#!/usr/bin/env python
#V1.0

from asyncio import tasks
from itertools import chain
from dotenv import load_dotenv
load_dotenv()

import argparse
import asyncio
import ipaddress
import json
import logging
import os
import sys
import time
import uvloop
import secrets
import pprint
import datetime
import functools
import uuid
import traceback

from logging.handlers import TimedRotatingFileHandler, WatchedFileHandler
from aiohttp import ClientSession, WSMessage, WSMsgType, log, web, ClientWebSocketResponse
from web3 import Web3
from web3.middleware import geth_poa_middleware
from eth_account.messages import encode_intended_validator
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from enum import IntEnum

ALLOWED_RPC_ACTIONS = [
    'service_subscribe', 'chain_info'
]

CLIENTS = {}

SERVICE = 'validator'
FILTER_CHAIN_ID = 'chain_id'
FILTERS = [FILTER_CHAIN_ID]
SUBS = 'subscriptions'

asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())
LOOP = asyncio.get_event_loop()

# Configuration arguments
parser = argparse.ArgumentParser(description="Raicoin Validator")
parser.add_argument('--host', type=str, help='Host to listen on (e.g. 127.0.0.1)', default='127.0.0.1')
parser.add_argument('-p', '--port', type=int, help='Port to listen on', default=7187)
parser.add_argument('--log-file', type=str, help='Log file location', default='rai_validator.log')
parser.add_argument('--key', help='Create a secure api key', action='store_true')

OPTIONS = parser.parse_args()
try:
    if OPTIONS.key:
        print(secrets.token_urlsafe())
        sys.exit(0)
    LISTEN_HOST = str(ipaddress.ip_address(OPTIONS.host))
    LISTEN_PORT = int(OPTIONS.port)
    if not os.path.exists('log'):
        os.makedirs('log')
    LOG_FILE = f'log/{OPTIONS.log_file}'
    SERVER_DESC = f'on {LISTEN_HOST} port {LISTEN_PORT}'
    print(f'Starting Raicoin Validator {SERVER_DESC}')
except Exception as e:
    parser.print_help()
    print(e)
    sys.exit(0)

# Enviroment variables
DEBUG_MODE = True if int(os.getenv('DEBUG', 0)) != 0 else False
TEST_MODE = True if int(os.getenv('TEST', 0)) != 0 else False
USE_CLOUDFLARE = True if int(os.getenv('USE_CLOUDFLARE', 0)) == 1 else False
USE_NGINX = True if int(os.getenv('USE_NGINX', 0)) == 1 else False

NODE_IP = os.getenv('NODE_IP', '127.0.0.1')
NODE_CALLBACK_KEY = os.getenv("NODE_CALLBACK_KEY", '')
if len(NODE_CALLBACK_KEY) != 43:
    print("Error found in .env: NODE_CALLBACK_KEY is missing or invalid, you can use 'python3 rai_validator.py --key' to generate a secure key")
    sys.exit(0)

RAI_TOKEN_URL = os.getenv('RAI_TOKEN_URL', 'wss://token.raicoin.org/')
if not RAI_TOKEN_URL.startswith('wss://'):
    print("Error found in .env: invalid RAI_TOKEN_URL")
    sys.exit(0)

EVM_CHAIN_CORE_ABI_FILE = os.getenv('EVM_CHAIN_CORE_ABI_FILE', 'evm_chain_core_abi.json')
EVM_CHAIN_CORE_ABI = None
with open(EVM_CHAIN_CORE_ABI_FILE) as f:
    EVM_CHAIN_CORE_ABI = json.load(f)
if not EVM_CHAIN_CORE_ABI:
    print(f'Failed to parse abi file:{EVM_CHAIN_CORE_ABI_FILE}')
    sys.exit(0)

EVM_CHAIN_VALIDATOR_ABI_FILE = os.getenv('EVM_CHAIN_VALIDATOR_ABI_FILE', 'evm_chain_validator_abi.json')
EVM_CHAIN_VALIDATOR_ABI = None
with open(EVM_CHAIN_VALIDATOR_ABI_FILE) as f:
    EVM_CHAIN_VALIDATOR_ABI = json.load(f)
if not EVM_CHAIN_VALIDATOR_ABI:
    print(f'Failed to parse abi file:{EVM_CHAIN_VALIDATOR_ABI_FILE}')
    sys.exit(0)

BSC_TEST_ENDPOINTS = os.getenv('BSC_TEST_ENDPOINTS', '')
if BSC_TEST_ENDPOINTS != '':
    try:
        endpoints = BSC_TEST_ENDPOINTS.split(',')
        BSC_TEST_ENDPOINTS = []
        for i in endpoints:
            if not i.strip():
                continue
            BSC_TEST_ENDPOINTS.append(i.strip())
        if not BSC_TEST_ENDPOINTS:
            print("Error found in .env: invalid BSC_TEST_ENDPOINTS config")
    except:
        print("Error found in .env: invalid BSC_TEST_ENDPOINTS config")
        sys.exit(0)
else:
    BSC_TEST_ENDPOINTS = [
        'https://data-seed-prebsc-1-s1.binance.org:8545/',
        'https://data-seed-prebsc-1-s2.binance.org:8545/',
        'https://data-seed-prebsc-1-s3.binance.org:8545/',
        'https://data-seed-prebsc-2-s1.binance.org:8545/',
        'https://data-seed-prebsc-2-s2.binance.org:8545/',
        'https://data-seed-prebsc-2-s3.binance.org:8545/',
    ]


class Util:
    def __init__(self, use_cf: bool, use_nginx : bool):
        self.use_cf = use_cf
        self.use_nginx = use_nginx

    def get_request_ip(self, r: web.Request) -> str:
        #X-FORWARDED-FOR not safe, don't use

        if self.use_cf:
            host = r.headers.get('CF-Connecting-IP', None)
            if host != None:
                return host
        if self.use_nginx:
            host = r.headers.get('X-Real-IP', None)
            if host != None:
                return host

        return self.get_connecting_ip(r)

    def get_connecting_ip(self, r: web.Request):
        peername = r.transport.get_extra_info('peername')
        if peername is not None:
            host, _ = peername
            return host
        return None


class ChainId(IntEnum):
    INVALID = 0
    RAICOIN = 1
    BITCOIN = 2
    ETHEREUM = 3
    BINANCE_SMART_CHAIN = 4

    RAICOIN_TEST = 10010
    BITCOIN_TEST = 10020
    ETHEREUM_TEST_GOERLI = 10033
    BINANCE_SMART_CHAIN_TEST = 10040

class EvmChainId(IntEnum):
    ETHEREUM = 1
    BINANCE_SMART_CHAIN = 56

    ETHEREUM_TEST_GOERLI = 5
    BINANCE_SMART_CHAIN_TEST = 97



def awaitable(func):
    @functools.wraps(func)
    async def wrapper(*args, **kw):
        thread_pool = ThreadPoolExecutor(1)
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(thread_pool, functools.partial(func, *args, **kw))
    return wrapper

class EvmChainValidator:
    def __init__(self, chain_id: ChainId, evm_chain_id: EvmChainId, core_address: str, validator_address: str, core_abi: dict, validator_abi: dict, endpoints: list):
        self.chain_id = chain_id
        self.evm_chain_id = evm_chain_id
        self.core_address = core_address
        self.validator_address = validator_address
        self.core_abi = core_abi
        self.validator_abi = validator_abi
        self.endpoints = endpoints
        self.endpoint_index = 0
        self.fee = None
        self.evm_chain_id_checked = False
        self.synced_height = 0

        if (len(self.endpoints) == 0):
            print("[ERROR] No endpoints found while constructing EvmChainValidator")
            sys.exit(0)

    def to_chain_info(self):
        info = {
            'chain_id': str(int(self.chain_id)),
            'fee': str(self.fee) if self.fee else '0',
        }
        return info

    async def check_evm_chain_id(self):
        for _ in range(len(self.endpoints)):
            chain_id = await self.get_chain_id()
            if chain_id != self.evm_chain_id:
                print(
                    f"[ERROR] Chain id mismatch, expected {self.evm_chain_id}, got {chain_id}, endpoint={self.get_endpoint()}")
                sys.exit(0)
            self.use_next_endpoint()
        self.evm_chain_id_checked  = True

    @awaitable
    def get_block_number(self) -> int:
        w3 = self.make_web3()
        try:
            return False, w3.eth.block_number
        except Exception as e:
            log.server_logger.error('get_block_number exception:%s', str(e))
            self.use_next_endpoint()
            return True, 0

    @awaitable
    def get_fee(self, block_number = 'latest'):
        w3 = self.make_web3()
        contract = w3.eth.contract(address=self.core_address, abi=self.core_abi)
        try:
            fee = contract.functions.getFee().call(block_identifier=block_number)
            return False, fee
        except Exception as e:
            log.server_logger.error('get_fee exception:%s', str(e))
            self.use_next_endpoint()
            return True, 0

    @awaitable
    def get_chain_id(self):
        w3 = self.make_web3()
        try:
            return w3.eth.chain_id
        except Exception as e:
            log.server_logger.error('get_chain_id exception:%s', str(e))
            return 0

    def use_next_endpoint(self):
        self.endpoint_index = (self.endpoint_index + 1) % len(self.endpoints)

    def get_endpoint(self):
        return self.endpoints[self.endpoint_index]

    def make_web3(self):
        w3 = Web3(Web3.HTTPProvider(self.get_endpoint()))
        w3.middleware_onion.inject(geth_poa_middleware, layer=0)
        return w3

    async def __call__(self):
        notify = False
        if not self.evm_chain_id_checked:
            await self.check_evm_chain_id()
        error, block_number = await self.get_block_number()
        if error or block_number <= self.synced_height:
            return
        print(f"block_number={block_number}")

        error, fee = await self.get_fee(block_number)
        if error:
            return
        if fee != self.fee:
            self.fee = fee
            notify = True
        print(f"fee={self.fee}")

        self.synced_height = block_number
        if notify:
            await notify_chain_info(self.chain_id, self.to_chain_info())
        # todo:

    def debug_json_encode(self) -> dict:
        result = {}
        result['chain_id'] = self.chain_id
        result['evm_chain_id'] = self.evm_chain_id
        result['core_address'] = self.core_address
        result['validator_address'] = self.validator_address
        result['endpoints'] = self.endpoints
        result['endpoint_index'] = self.endpoint_index
        result['fee'] = self.fee
        result['evm_chain_id_checked'] = self.evm_chain_id_checked
        return result


UTIL = Util(USE_CLOUDFLARE, USE_NGINX)


async def send_notify(message):
    if len(CLIENTS) == 0:
        return
    uids = list(CLIENTS.keys())
    filters = message['filters']
    targets = set()

    message_str = json.dumps(message)
    for uid in uids:
        if uid not in CLIENTS:
            continue
        client = CLIENTS[uid]
        for i in filters:
            k = i['key']
            v = i['value']
            if k not in client[SUBS] or v not in client[SUBS][k]:
                continue
            targets.add(uid)

    for uid in targets:
        try:
            await CLIENTS[uid]['ws'].send_str(message_str)
        except Exception as e:
            log.server_logger.error(
                'send_notify: message=%s, exception=%s', message_str, str(e))

async def register_service(ws):
    register = {
        'register': SERVICE,
        'filters': FILTERS,
        'actions': ALLOWED_RPC_ACTIONS
    }
    await ws.send_str(json.dumps(register))

def subscribe(uid, filters):
    global CLIENTS
    client = CLIENTS[uid]
    if len(filters) > 2:
        return True, 'invalid filters'
    for f in filters:
        k = f['key']
        v = str(f['value'])
        if k not in FILTERS:
            return True, 'invalid filter key'
        if k == FILTER_CHAIN_ID:
            try:
                v = ChainId(int(v))
                if v not in VALIDATORS:
                    return True, ' chain id not supported'
            except:
                return True, 'invalid filter value'
    for f in filters:
        k = f['key']
        v = str(f['value']).lower()
        if k not in client[SUBS]:
            client[SUBS][k] = {}
        if v not in  client[SUBS][k]:
            client[SUBS][k][v] = {}            
        client[SUBS][k][v]['timestamp'] = int(time.time())

    return False, None

async def chain_info(req, res):
    try:
        chain_id = int(req['chain_id'])
        if chain_id not in VALIDATORS:
            res['errror'] = 'chain not supported'
            return
        validator = VALIDATORS[chain_id]
        res.update(validator.to_chain_info())
    except Exception as e:
        res['error'] = 'exception'
        log.server_logger.exception('fee_query exception:%s', str(e))

async def notify_chain_info(chain_id: ChainId, info: dict):
    chain_id = str(int(chain_id))
    message = {
        'notify': 'chain_info',
        'service': SERVICE,
        'filters': [{'key': FILTER_CHAIN_ID, 'value': chain_id}],
    }
    message.update(info)
    await send_notify(message)

# Primary handler for all client websocket connections
async def handle_client_messages(r : web.Request, message : str, ws : web.WebSocketResponse):
    global CLIENTS
    ip = UTIL.get_request_ip(r)
    uid = ws.id
    log.server_logger.info('request: %s, %s, %s', message, ip, uid)

    try:
        res = {'service': SERVICE}
        request = json.loads(message)
        if 'request_id' in request:
            res['request_id'] = request['request_id']
        if 'client_id' in request:
            res['client_id'] =  request['client_id']

        if 'action' not in request:
            res['error'] = 'action missing'
            return res

        action = request['action']
        res['ack'] = action
        if action not in ALLOWED_RPC_ACTIONS:
            res.update({'error':'action not allowed'})
            return res

        if action == 'service_subscribe':
            filters = request['filters']
            error, info = subscribe(uid, filters)
            if error:
                res.update({'error':info})
            else:
                res.update({'success':''})
        elif action == 'chain_info':
            await chain_info(request, res)
        else:
            res.update({'error':'unknown action'})
        return res
    except Exception as e:
        log.server_logger.exception('uncaught error;%s;%s', ip, uid)
        res.update({
            'error':'unexpected error',
            'detail':str(sys.exc_info())
        })
        return res

async def client_handler(r : web.Request):
    global CLIENTS
    """Handler for websocket connections and messages"""
    ws = web.WebSocketResponse(heartbeat=30)
    try:
        await ws.prepare(r)
        # Connection Opened
    except:
        log.server_logger.error('Failed to prepare websocket: %s', UTIL.get_request_ip(r))
        return ws
    ws.id = str(uuid.uuid4())
    ip = UTIL.get_request_ip(r)
    CLIENTS[ws.id] = {'ws': ws, SUBS: {}}
    log.server_logger.info('new client connection;%s;%s;User-Agent:%s', ip, ws.id, str(
        r.headers.get('User-Agent')))

    try:
        await register_service(ws)
        async for msg in ws:
            if msg.type == WSMsgType.TEXT:
                if msg.data == 'close':
                    await ws.close()
                else:
                    res = await handle_client_messages(r, msg.data, ws=ws)
                    if res:
                        res = json.dumps(res)
                        log.server_logger.debug('Sending response %s to %s', res, ip)
                        await ws.send_str(res)
            elif msg.type == WSMsgType.CLOSE:
                log.server_logger.info('Client Connection closed normally')
                break
            elif msg.type == WSMsgType.ERROR:
                log.server_logger.info('Client Connection closed with error %s', ws.exception())
                break
        log.server_logger.info('Client connection closed normally')
    except:
        log.server_logger.exception('Client Closed with exception')
    finally:
        destory_client(r, ws.id)
    return ws

def destory_client(r : web.Request, client_id):
    if client_id not in CLIENTS:
        return
    del CLIENTS[client_id]

def debug_check_ip(r : web.Request):
    ip = UTIL.get_request_ip(r)
    if not ip or ip != '127.0.0.1':
        return True
    return False

class JsonEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, set):
            return list(obj)
        if isinstance(obj, web.WebSocketResponse):
            return 'WebSocketResponse object'
        if isinstance(obj, ClientWebSocketResponse):
            return 'ClientWebSocketResponse object'
        if isinstance(obj, EvmChainValidator):
            return obj.debug_json_encode()
        return json.JSONEncoder.default(self, obj)

DEBUG_DUMPS = partial(json.dumps, cls=JsonEncoder, indent=4)


async def debug_handler(r : web.Request):
    if debug_check_ip(r):
        log.server_logger.error('debug request from unauthorized ip: %s', UTIL.get_request_ip(r))
        return web.HTTPUnauthorized()

    try:
        request_json = await r.json()
        query = request_json['query']
        # todo:
        if query == 'validators':
            return web.json_response(VALIDATORS, dumps=DEBUG_DUMPS)
        else:
            pass
        return web.HTTPOk()
    except Exception:
        log.server_logger.exception('exception in debug request')
        return web.HTTPInternalServerError(reason="Something went wrong %s" % str(sys.exc_info()))


async def init_app():
    global BOOT_TIME
    # Setup logger
    if DEBUG_MODE:
        print("debug mode")
        logging.basicConfig(level=logging.DEBUG)
    else:
        root = logging.getLogger('aiohttp.server')
        logging.basicConfig(level=logging.WARN)
        handler = WatchedFileHandler(LOG_FILE)
        formatter = logging.Formatter("%(asctime)s;%(levelname)s;%(message)s", "%Y-%m-%d %H:%M:%S %z")
        handler.setFormatter(formatter)
        root.addHandler(handler)
        root.addHandler(TimedRotatingFileHandler(LOG_FILE, when="d", interval=1, backupCount=100))        

    app = web.Application()

    # Setup routes
    # todo:
    app.add_routes([web.get('/', client_handler)]) # All client WS requests
    app.add_routes([web.post('/debug', debug_handler)]) # local debug interface

    return app

APP = LOOP.run_until_complete(init_app())
if APP == None:
    print('Init app failed')
    sys.exit(0)

async def periodic(period, fn, *args):
    while True:
        begin = time.time()
        try:
            await fn(*args)
        except Exception as e:
            log.server_logger.error(f'periodic exception={str(e)}')
            traceback.print_tb(e.__traceback__)
        elapsed = time.time() - begin
        if elapsed < period:
            await asyncio.sleep(period - elapsed)

BSC_TEST_CORE_CONTRACT = '0xa8250D8f8cb583eD5c3dB52c496f79d788DAA64e'
BSC_TEST_VALIDATOR_CONTRACT = '0x8dc4A3dfE110a96ccB1b93b085885BB5dC9c74f5'

if TEST_MODE:
    VALIDATORS = {
        ChainId.BINANCE_SMART_CHAIN_TEST: {
            'period': 5,
            'validator': EvmChainValidator(ChainId.BINANCE_SMART_CHAIN_TEST, EvmChainId.BINANCE_SMART_CHAIN_TEST, BSC_TEST_CORE_CONTRACT, BSC_TEST_VALIDATOR_CONTRACT, EVM_CHAIN_CORE_ABI, EVM_CHAIN_VALIDATOR_ABI, BSC_TEST_ENDPOINTS)
        }
    }
else:
    VALIDATORS = {}

def main():
    global APP
    tasks = []
    # Start web/ws server
    async def start():
        runner = web.AppRunner(APP)
        await runner.setup()
        site = web.TCPSite(runner, LISTEN_HOST, LISTEN_PORT)
        await site.start()

    async def end():
        for task in tasks:
            task.cancel()
        await APP.shutdown()

    LOOP.run_until_complete(start())

    # Main program
    try:
        for i in VALIDATORS.values():
            tasks.append(LOOP.create_task(periodic(i['period'], i['validator'])))
        LOOP.run_forever()
    except KeyboardInterrupt:
        pass
    finally:
        LOOP.run_until_complete(end())

    LOOP.close()

if __name__ == "__main__":
    if TEST_MODE:
        print("***Run in test mode***")
    main()

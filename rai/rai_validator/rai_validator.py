#!/usr/bin/env python
#V1.0

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
from web3 import Web3, Account as EvmAccount
from web3.middleware import geth_poa_middleware
from eth_account.messages import encode_intended_validator
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from enum import IntEnum, Enum
from getpass import getpass
from hashlib import blake2b

ALLOWED_RPC_ACTIONS = [
    'service_subscribe', 'chain_info'
]

CLIENTS = {}
TXNS = {}

SERVICE = 'validator'
FILTER_CHAIN_ID = 'chain_id'
FILTERS = [FILTER_CHAIN_ID]
SUBS = 'subscriptions'
EPOCH_TIME = 72 * 3600
REWARD_TIME = 71 * 3600

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


def valid_evm_private_key(key):
    try:
        if (len(key) != 64):
            return False
        bytes.fromhex(key)
        return True
    except:
        return False

BSC_TEST_SIGNER_PRIVATE_KEY = os.getenv('BSC_TEST_SIGNER_PRIVATE_KEY', '')
if BSC_TEST_SIGNER_PRIVATE_KEY.lower() == 'input':
    BSC_TEST_SIGNER_PRIVATE_KEY = getpass(
        prompt='Input the private key of your signer account for BSC testnet: ')
    if valid_evm_private_key(BSC_TEST_SIGNER_PRIVATE_KEY):
        if len(BSC_TEST_SIGNER_PRIVATE_KEY) > 0:
            print('Error: invalid private key format')
        sys.exit(0)
elif BSC_TEST_SIGNER_PRIVATE_KEY != '':
    if not valid_evm_private_key(BSC_TEST_SIGNER_PRIVATE_KEY):
        print("Error found in .env: invalid BSC_TEST_SIGNER_PRIVATE_KEY")
        sys.exit(0)

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

ACCOUNT_LOOKUP = "13456789abcdefghijkmnopqrstuwxyz"
ACCOUNT_REVERSE = "~0~1234567~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~89:;<=>?@AB~CDEFGHIJK~LMNO~~~~~"
class Account:
    def char_encode(self, c):
        if c >= len(ACCOUNT_LOOKUP) or c < 0:
            return '~'
        return ACCOUNT_LOOKUP[c]

    def char_decode(self, d):
        if d not in ACCOUNT_LOOKUP:
            return 0xff
        return ord(ACCOUNT_REVERSE[ord(d) - 0x30]) - 0x30

    def decode(self, a):
        prefix = "rai_"
        if len(a) < 64 or not a.startswith(prefix):
            return True, None, None
        if ' ' in a or '\r' in a or '\n' in a or '\t' in a:
            return True, None, None
        if len(a) == 65 or (len(a) > 65 and a[64] != '_'):
            return True, None, None
            
        if (a[len(prefix)] != '1' and a[len(prefix)] != '3'):
            return True, None, None
        number = 0
        for i in range(len(prefix), 64):
            b = self.char_decode(a[i])
            if (0xff == b):
                return True, None, None
            number <<= 5
            number += b
        check = number & 0xffffffffff
        hex_str = "{0:064X}".format(number >> 40)
        ctx = blake2b(digest_size=5)
        ctx.update(bytes.fromhex(hex_str))
        validation = int.from_bytes(ctx.digest(), byteorder='little')
        if check != validation:
            print("decode: invalid check field")
            return True, None, None
        if len(a) == 64:
            return False, hex_str, None
        return False, hex_str, a[65:]

    def encode(self, b):
        if isinstance(b, str):
            if b.startswith("0x"):
                b = b[2:]
            try:
                b = bytes.fromhex(b)
            except:
                return True, None
        if not isinstance(b, bytes) or len(b) != 32:
            return True, None
        ctx = blake2b(digest_size=5)
        ctx.update(b)
        check = int.from_bytes(ctx.digest(), byteorder='little')
        raw = int.from_bytes(b, byteorder='big')
        number = raw * 2 ** 40 + check
        a = ''
        for i in range(60):
            c = number % 32
            number = number // 32
            e = self.char_encode(c)
            if e == '~':
                return True, None
            a += e
        a += '_iar'
        return False, a[::-1]
        
    def check(self, a):
        error, _, _ = self.decode(a)
        return error

ACCOUNT = Account()

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

class ChainStr(str, Enum):
    INVALID = 'invalid'
    RAICOIN = 'raicoin'
    BITCOIN = 'bitcoin'
    ETHEREUM = 'ethereum'
    BINANCE_SMART_CHAIN = 'binance smart chain'

    RAICOIN_TEST = 'raicoin testnet'
    BITCOIN_TEST = 'bitcoin testnet'
    ETHEREUM_TEST_GOERLI = 'ethereum goerli testnet'
    BINANCE_SMART_CHAIN_TEST = 'binance smart chain testnet'

def to_chain_str(chain_id: ChainId):
    return ChainStr[chain_id.name]

def to_chain_id(chain_str: ChainStr):
    return ChainId[chain_str.name]

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
    def __init__(self, chain_id: ChainId, evm_chain_id: EvmChainId, core_contract: str,
                 validator_contract: str, core_abi: dict, validator_abi: dict, endpoints: list,
                 signer_private_key: str, confirmations: int):
        self.chain_id = chain_id
        self.evm_chain_id = evm_chain_id
        self.core_contract = core_contract
        self.validator_contract = validator_contract
        self.core_abi = core_abi
        self.validator_abi = validator_abi
        self.genesis_validator = None
        self.genesis_signer = None
        self.endpoints = endpoints
        self.confirmations = confirmations
        self.signer = None
        if signer_private_key != '':
            self._account = EvmAccount.from_key(signer_private_key)
            self.signer = self._account.address

        # dynamic variables
        self.endpoint_index = 0
        self.fee = None
        self.evm_chain_id_checked = False
        self.synced_height = None
        self.validators = None  # list of ValidatorFullInfo, ordered by weight
        self.total_weight = self._total_weight = None
        self.validator_activities = {} # validator -> ValidatorActivity

        self.last_submit = None
        self.submission_interval = 300
        self.submission_state = self.SubmissionState.IDLE
        self.weights = {}
        self.signatures = {}

        if len(self.endpoints) == 0:
            print("[ERROR] No endpoints found while constructing EvmChainValidator")
            sys.exit(0)

    class ValidatorFullInfo:
        def __init__(self, validator: str, signer: str, weight: int, gas_price: int,
                     last_submit: int, epoch: int):
            self.validator = validator
            self.signer = signer
            self.weight = weight
            self.gas_price = gas_price
            self.last_submit = last_submit
            self.epoch = epoch

        def __lt__(self, other):
            return self.weight < other.weight

        def update(self, signer: str, weight: int, gas_price: int,
                    last_submit: int, epoch: int):
            updated = False
            if signer != self.signer:
                self.signer = signer
                updated = True
            if weight != self.weight:
                self.weight = weight
                updated = True
            if gas_price != self.gas_price:
                self.gas_price = gas_price
                updated = True
            if last_submit != self.last_submit:
                self.last_submit = last_submit
                updated = True
            if epoch != self.epoch:
                self.epoch = epoch
                updated = True
            return updated

    class Topics(str, Enum):
        ValidatorSubmitted = '0x8af4f119fc84662aea5dd2e38c11f64f244dd0dd79699db6778a9ce3bcb4e006'
        ValidatorPurged = '0xef1971845d41d94b4fa01bda8806e405a5433145ff767b4ce3d79f738c3624c7'

    class ValidatorActivity:
        def __init__(self, log_height: int, sync_height: int = 0):
            self.log_height = log_height
            self.sync_height = sync_height

    class SubmissionState(IntEnum):
        IDLE = 0
        WEIGHT_QUERY = 1
        COLLECT_SIGNATURES = 2

    def update_activity(self, log):
        validator = log.topics[1].hex()
        if validator not in self.validator_activities:
            self.validator_activities[validator] = self.ValidatorActivity(int(log.blockNumber))
        else:
            self.validator_activities[validator].log_height = int(log.blockNumber)

    def update_validator(self, validator: str, signer: str, weight: int, gas_price: int,
                         last_submit: int, epoch: int):
        updated = False
        existing = next((x for x in self.validators if x.validator == validator), None)
        if existing != None:
            updated = existing.update(signer, weight, gas_price, last_submit, epoch)
        else:
            updated = True
            self.validators.append(self.ValidatorFullInfo(validator, signer, weight, gas_price,
                                                          last_submit, epoch))
        if updated:
            self.validators.sort(reverse=True)
        return updated

    def to_chain_info(self):
        info = {
            'chain': str(to_chain_str(self.chain_id)),
            'chain_id': str(int(self.chain_id)),
            'fee': str(self.fee) if self.fee else '0',
            'total_weight': str(self.total_weight) if self.total_weight != None else '',
            'genesis_validator': self.genesis_validator,
            'genesis_signer': self.genesis_signer,
            'genesis_weight': str(self.total_weight) if self.total_weight != None else '', # todo:
        }
        if self.validators != None:
            info['validators'] = [{
                'validator': str(x.validator),
                'signer': str(x.signer),
                'weight': str(x.weight),
                'gas_price': str(x.gas_price),
                'last_submit': str(x.last_submit),
                'epoch': str(x.epoch),
            } for x in self.validators]
        else:
            info['validators'] = ''
        return info

    async def check_evm_chain_id(self):
        for _ in range(len(self.endpoints)):
            error, chain_id = await self.get_chain_id()
            if error:
                return
            if chain_id != self.evm_chain_id:
                print(
                    f"[ERROR] Chain id mismatch, expected {self.evm_chain_id}, got {chain_id}, endpoint={self.get_endpoint()}")
                sys.exit(0)
            self.use_next_endpoint()
        self.evm_chain_id_checked  = True

    @awaitable
    def get_block_number(self):
        w3 = self.make_web3()
        try:
            return False, w3.eth.block_number
        except Exception as e:
            log.server_logger.error('get_block_number exception:%s', str(e))
            self.use_next_endpoint()
            return True, 0

    @awaitable
    def get_genesis_validator(self):
        contract = self.make_validator_contract()
        try:
            return False, contract.functions.getGenesisValidator().call()
        except Exception as e:
            log.server_logger.error('get_genesis_validator exception:%s', str(e))
            self.use_next_endpoint()
            return True, ''

    @awaitable
    def get_genesis_signer(self):
        contract = self.make_validator_contract()
        try:
            return False, contract.functions.getGenesisSigner().call()
        except Exception as e:
            log.server_logger.error('get_genesis_signer exception:%s', str(e))
            self.use_next_endpoint()
            return True, ''

    @awaitable
    def get_fee(self, block_number = 'latest'):
        w3 = self.make_web3()
        contract = self.make_core_contract()
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
            return False, w3.eth.chain_id
        except Exception as e:
            log.server_logger.error('get_chain_id exception:%s', str(e))
            return True, 0

    @awaitable
    def get_total_weight(self, block_number='latest'):
        contract = self.make_validator_contract()
        try:
            return False, contract.functions.getTotalWeight().call(block_identifier=block_number)
        except Exception as e:
            log.server_logger.error('get_total_weight exception:%s', str(e))
            self.use_next_endpoint()
            return True, 0

    @awaitable
    def get_validator_count(self, block_number='latest'):
        contract = self.make_validator_contract()
        try:
            return False, contract.functions.getValidatorCount().call(block_identifier=block_number)
        except Exception as e:
            log.server_logger.error('get_validator_count exception:%s', str(e))
            self.use_next_endpoint()
            return True, 0

    @awaitable
    def get_validators(self, begin: int, end: int, block_number='latest'):
        contract = self.make_validator_contract()
        try:
            return False, contract.functions.getValidators(begin, end).call(block_identifier=block_number)
        except Exception as e:
            log.server_logger.error('get_validators exception:%s', str(e))
            self.use_next_endpoint()
            return True, []

    @awaitable
    def get_validator_logs(self, from_block: int, to_block: int):
        w3 = self.make_web3()
        filter_params = {
            'fromBlock': from_block,
            'toBlock': to_block,
            'address': self.validator_contract,
            'topics': [[self.Topics.ValidatorPurged, self.Topics.ValidatorSubmitted]]
        }
        try:
            return False, w3.eth.get_logs(filter_params)
        except Exception as e:
            log.server_logger.error('get_validator_logs exception:%s', str(e))
            self.use_next_endpoint()
            return True, None
    
    @awaitable
    def get_validator_info(self, validator: str, block_number='latest'):
        contract = self.make_validator_contract()
        try:
            return False, contract.functions.getValidatorInfo(validator).call(block_identifier=block_number)
        except Exception as e:
            log.server_logger.error('get_validator_info exception:%s', str(e))
            self.use_next_endpoint()
            return True, None

    @awaitable
    def get_weight(self, signer: str, block_number='latest'):
        contract = self.make_validator_contract()
        try:
            return False, contract.functions.getWeight(signer).call(block_identifier=block_number)
        except Exception as e:
            log.server_logger.error('get_weight exception:%s', str(e))
            self.use_next_endpoint()
            return True, None

    def use_next_endpoint(self):
        self.endpoint_index = (self.endpoint_index + 1) % len(self.endpoints)

    def get_endpoint(self):
        return self.endpoints[self.endpoint_index]

    def make_web3(self):
        w3 = Web3(Web3.HTTPProvider(self.get_endpoint()))
        w3.middleware_onion.inject(geth_poa_middleware, layer=0)
        return w3

    def make_core_contract(self):
        return self.make_web3().eth.contract(address=self.core_contract, abi=self.core_abi)
    
    def make_validator_contract(self):
        return self.make_web3().eth.contract(address=self.validator_contract, abi=self.validator_abi)

    async def sync_fee(self, block_number: int):
        error, fee = await self.get_fee(block_number)
        if error:
            return error, False
        updated = False
        if fee != self.fee:
            self.fee = fee
            updated = True
        return False, updated

    async def initialize_validators(self, block_number: int):
        updated = False
        error, count = await self.get_validator_count(block_number)
        if error:
            return error, updated
        validators = []
        begin = end = 0
        while True:
            begin = end
            if begin >= count:
                break
            end = begin + 1000
            if end > count:
                end = count
            error, array = await self.get_validators(begin, end, block_number)
            if error:
                return error, updated
            for i in array:
                validators.append(self.ValidatorFullInfo('0x' + i.validator.hex(), i.signer,
                    int(i.weight), int(i.gasPrice), int(i.lastSubmit), int(i.epoch)))
            await asyncio.sleep(0.1)
        validators.sort(reverse=True)
        self.validators = validators
        updated = True
        return False,  updated

    async def sync_activities(self, block_number: int):
        updated = False
        purgeable = []
        for validator, activity in self.validator_activities.items():
            if activity.sync_height >= activity.log_height \
                and block_number < activity.log_height + self.confirmations:
                continue
            error, info = await self.get_validator_info(validator, block_number)
            if error:
                return error, updated
            weight = 0
            if info.signer != '0x0000000000000000000000000000000000000000':
                error, weight = await self.get_weight(info.signer, block_number)
                if error:
                    return error, updated
            updated = self.update_validator(validator, info.signer, int(weight), int(info.gasPrice),
                int(info.lastSubmit), int(info.epoch)) or updated
            activity.sync_height = block_number
            if activity.sync_height >= activity.log_height + self.confirmations:
                purgeable.append(validator)
            await asyncio.sleep(0.1)
        for validator in purgeable:
            del self.validator_activities[validator]
        return False, updated

    async def sync_validators(self, block_number: int):
        updated = False
        error, total = await self.get_total_weight(block_number)
        if error:
            return error, updated
        if total != self._total_weight:
            updated = True
            self._total_weight = total
            min_weight = int(2e16)
            self.total_weight = min_weight if self._total_weight < min_weight else self._total_weight

        if self.validators == None:
            error, updated = await self.initialize_validators(block_number)
            if error:
                return error, updated
        previous_height = self.synced_height if self.synced_height else block_number
        error, logs = await self.get_validator_logs(previous_height - self.confirmations, block_number)
        if error:
            return error, updated
        for log in logs:
            self.update_activity(log)
  
        error, updated = await self.sync_activities(block_number)
        if error:
            return error, updated
        return False, updated

    def rewardable(self):
        if self.synced_height == None:
            return False

        if self.submission_state != self.SubmissionState.IDLE:
            return False

        if self.last_submit != None and self.last_submit + self.submission_interval > time.time():
            return False

        if self.signer == None or self.signer == self.genesis_signer:
            return False

        if NODE == None:
            return False
        validator = NODE['account_hex']
        if validator == self.genesis_validator:
            return False

        if validator not in self.validators:
            return True
        info = self.validators[validator]
        if info.epoch >= current_epoch():
            return False
        # todo:

    async def submit(self, block_number: int):
        if self.signer == None or self.signer == self.genesis_signer:
            return
        if NODE != None and NODE['account_hex'] == self.genesis_validator:
            return
        
        # todo:

    async def __call__(self):
        notify = False
        if not self.evm_chain_id_checked:
            await self.check_evm_chain_id()
        if not self.evm_chain_id_checked:
            return

        if self.genesis_validator == None:
            error, validator = await self.get_genesis_validator()
            if error:
                return
            self.genesis_validator = '0x' + validator.hex().lower()

        if self.genesis_signer == None:
            error, signer = await self.get_genesis_signer()
            if error:
                return
            self.genesis_signer =  Web3.toChecksumAddress(signer)

        error, block_number = await self.get_block_number()
        if error or block_number <= self.synced_height:
            return

        error, updated = await self.sync_fee(block_number)
        if error:
            return
        notify = updated

        error, updated = await self.sync_validators(block_number)
        if error:
            return
        notify = notify or updated
        # todo:



        self.synced_height = block_number
        if notify:
            await notify_chain_info(self.chain_id, self.to_chain_info())
        # todo:

    def debug_json_encode(self) -> dict:
        result = {}
        validators = None
        if self.validators != None:
            validators = []
            for validator in self.validators:
                validators.append({
                    'validator': validator.validator,
                    'signer': validator.signer,
                    'weight': validator.weight,
                    'gas_price': validator.gas_price,
                    'last_submit': validator.last_submit,
                    'epoch': validator.epoch,
                })
        result['validators'] = validators
        result['total_weight'] = self.total_weight
        result['chain_id'] = self.chain_id
        result['evm_chain_id'] = self.evm_chain_id
        result['core_contract'] = self.core_contract
        result['validator_contract'] = self.validator_contract
        result['genesis_validator'] = self.genesis_validator
        result['genesis_signer'] = self.genesis_signer
        result['endpoints'] = self.endpoints
        result['endpoint_index'] = self.endpoint_index
        result['fee'] = self.fee
        result['evm_chain_id_checked'] = self.evm_chain_id_checked
        result['signer'] = self.signer
        result['synced_height'] = self.synced_height
        result['confirmations'] = self.confirmations
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
            res['error'] = 'chain not supported'
            return
        validator = VALIDATORS[chain_id]['validator']
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

def callback_check_ip(r : web.Request):
    ip = UTIL.get_request_ip(r)
    if ip != NODE_IP:
        return True
    return False

def current_epoch():
    return int(time.time()) // EPOCH_TIME

def in_reward_time_range(last_submit, now):
    hour = 3600
    last_delay = int(last_submit) % EPOCH_TIME
    if last_delay > REWARD_TIME:
        last_delay = REWARD_TIME
    delay = (last_delay + REWARD_TIME - hour) % REWARD_TIME
    if delay > REWARD_TIME - hour:
        delay = REWARD_TIME - hour
    return (int(now) % EPOCH_TIME) >= delay

async def sync_with_node():
    if NODE == None:
        return
    if 'account' not in NODE:
        await NODE['ws'].send_str('{"action":"node_account"}')
    if 'snapshot' not in NODE or NODE['snapshot']['epoch'] != current_epoch():
        await NODE['ws'].send_str('{"action":"weight_snapshot"}')

async def handle_node_weight_snapshot_ack(r : dict):
    global NODE
    snapshot = {}
    snapshot['epoch'] = int(r['epoch'])
    snapshot['weights'] = {}
    for i in r['weights']:
        snapshot['weights'][i['representative']] = int(i['weight'])
    NODE['snapshot'] = snapshot

async def handle_node_messages(r : web.Request, message : str, ws : web.WebSocketResponse):
    """Process data sent by node"""
    ip = UTIL.get_request_ip(r)
    log.server_logger.info('node message; %s, %s', message, ip)

    global NODE
    try:
        r = json.loads(message)
        if 'action' not in r:
            log.server_logger.error('invalid node message;%s;%s', message, ip)
            return

        action = r['action']
        if action == 'bind_query_ack':
            pass
        elif action == 'cross_chain':
            pass
        elif action == 'node_account_ack':
            NODE['account'] = r['account']
            NODE['account_hex'] = '0x' + r['account_hex'].lower()
        elif action == 'weight_query_ack':
            pass
        elif action == 'weight_snapshot_ack':
            await handle_node_weight_snapshot_ack(r)
        else:
            log.server_logger.error('unexpected node message;%s;%s', message, ip)
    except Exception as e:
        log.server_logger.exception('uncaught error;%s;%s', str(e), ip)

NODE = None
async def node_handler(r : web.Request):
    global NODE
    if callback_check_ip(r):
        log.server_logger.error('callback from unauthorized ip: %s', UTIL.get_request_ip(r))
        return web.HTTPUnauthorized()
    
    ws = web.WebSocketResponse(heartbeat=30)
    try:
        await ws.prepare(r)
        # Connection Opened
    except:
        log.server_logger.error('Failed to prepare websocket: %s', UTIL.get_request_ip(r))
        return ws
     
    ip = UTIL.get_request_ip(r)
    NODE = {'ws':ws, 'ip':ip}
    log.server_logger.info('new node connection;%s;User-Agent:%s', ip, str(r.headers.get('User-Agent')))

    try:
        async for msg in ws:
            if msg.type == WSMsgType.TEXT:
                if msg.data == 'close':
                    await ws.close()
                else:
                    await handle_node_messages(r, msg.data, ws=ws)
            elif msg.type == WSMsgType.CLOSE:
                log.server_logger.info('Node connection closed normally')
                break
            elif msg.type == WSMsgType.ERROR:
                log.server_logger.info('Node connection closed with error %s', ws.exception())
                break

        log.server_logger.info('Node connection closed normally')
    except Exception as e:
        log.server_logger.exception('Node closed with exception=%s', e)
    finally:
        NODE = None
    return ws

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
        elif query == 'node':
            return web.json_response(NODE, dumps=DEBUG_DUMPS)
        else:
            pass
        return web.HTTPOk()
    except Exception:
        log.server_logger.exception('exception in debug request')
        return web.HTTPInternalServerError(reason="Something went wrong %s" % str(sys.exc_info()))


async def init_app():
    # Setup logger
    if DEBUG_MODE:
        print("debug mode")
        logging.basicConfig(level=logging.INFO)
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
    app.add_routes([web.get(f'/callback/{NODE_CALLBACK_KEY}', node_handler)]) # ws/wss callback from nodes
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
        except KeyboardInterrupt as e:
            raise e
        except Exception as e:
            log.server_logger.error(f'periodic exception={str(e)}')
            traceback.print_tb(e.__traceback__)
        elapsed = time.time() - begin
        if elapsed < period:
            await asyncio.sleep(period - elapsed)

BSC_TEST_CORE_CONTRACT = '0xC777f5b390E79c9634c9d07AF45Dc44b11893055'
BSC_TEST_VALIDATOR_CONTRACT = '0x98431eB42A087333F7E167d3586D3b47Bc4f28D3'
BSC_TEST_GENESIS_VALIDATOR = 'rai_1nxp5qgqhwxof81oardbyraygcp3s15rbo38adecwzbq8p3kpy76sq3999ia'
BSC_TEST_GENESIS_SIGNER = '0xd24EBE3D7879330509Bf1f9d8dffAF8CA8F32fdc'

if TEST_MODE:
    VALIDATORS = {
        ChainId.BINANCE_SMART_CHAIN_TEST: {
            'period': 5,
            'validator': EvmChainValidator(ChainId.BINANCE_SMART_CHAIN_TEST,
                                           EvmChainId.BINANCE_SMART_CHAIN_TEST,
                                           BSC_TEST_CORE_CONTRACT, BSC_TEST_VALIDATOR_CONTRACT,
                                           EVM_CHAIN_CORE_ABI, EVM_CHAIN_VALIDATOR_ABI,
                                           BSC_TEST_ENDPOINTS, BSC_TEST_SIGNER_PRIVATE_KEY, 30)
        },
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
            try:
                await task
            except:
                pass
        await APP.shutdown()

    LOOP.run_until_complete(start())

    # Main program
    try:
        for i in VALIDATORS.values():
            tasks.append(LOOP.create_task(periodic(i['period'], i['validator'])))
            v = i['validator']
            print(f'Start validator for {to_chain_str(v.chain_id)}, signer={v.signer}')
        tasks.append(LOOP.create_task(periodic(5, sync_with_node)))
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

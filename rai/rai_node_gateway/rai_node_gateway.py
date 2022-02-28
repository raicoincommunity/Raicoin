#!/usr/bin/env python
#V1.0.0

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
import uuid
import uvloop
import secrets
import random
import traceback

from logging.handlers import TimedRotatingFileHandler, WatchedFileHandler
from aiohttp import ClientSession, WSMessage, WSMsgType, log, web, ClientWebSocketResponse
from functools import partial

ALLOWED_RPC_ACTIONS = [
    'account_heads', 'active_account_heads', 'block_confirm', 'blocks_query', 'event_subscribe'
]

asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())
LOOP = asyncio.get_event_loop()

# Configuration arguments
parser = argparse.ArgumentParser(description="Raicoin Node Gateway")
parser.add_argument('--host', type=str, help='Host to listen on (e.g. 127.0.0.1)', default='127.0.0.1')
parser.add_argument('-p', '--port', type=int, help='Port to listen on', default=7178)
parser.add_argument('--log-file', type=str, help='Log file location', default='rai_node_gateway.log')
parser.add_argument('--limit', type=int, help='Max allowed requests per second from one IP', default=0)
parser.add_argument('-t', '--token', help='Create a secure url token', action='store_true')

options = parser.parse_args()

try:
    if options.token:
        print(secrets.token_urlsafe())
        sys.exit(0)
    LISTEN_HOST = str(ipaddress.ip_address(options.host))
    LISTEN_PORT = int(options.port)
    if not os.path.exists('log'):
        os.makedirs('log')
    LOG_FILE = f'log/{options.log_file}'
    server_desc = f'on {LISTEN_HOST} port {LISTEN_PORT}'
    print(f'Starting Raicoin Node Gateway {server_desc}')
    LIMIT = int(options.limit)
except Exception as e:
    parser.print_help()
    print(e)
    sys.exit(0)

# Environment configuration
CALLBACK_WHITELIST = os.getenv('CALLBACK_WHITELIST', '127.0.0.1')
if CALLBACK_WHITELIST != '127.0.0.1':
    try:
        ips = CALLBACK_WHITELIST.split(',')
        CALLBACK_WHITELIST = []
        for ip in ips:
            if not ip.strip():
                continue
            CALLBACK_WHITELIST.append(str(ipaddress.ip_address(ip.strip())))
        if not CALLBACK_WHITELIST:
            print("Error found in .env: invalid CALLBACK_WHITELIST config")
    except:
        print("Error found in .env: invalid CALLBACK_WHITELIST config")
        sys.exit(0)
DEBUG_MODE = True if int(os.getenv('DEBUG', 0)) != 0 else False

CALLBACK_TOKEN = os.getenv("CALLBACK_TOKEN", '')
if len(CALLBACK_TOKEN) != 43:
    print("Error found in .env: CALLBACK_TOKEN is missing or invalid, you can use 'python3 rai_node_gateway.py -t' to generate a secure token")
    sys.exit(0)

CHECK_CF_CONNECTING_IP = True if int(os.getenv('USE_CLOUDFLARE', 0)) == 1 else False

GS = {} # Global States
CLIENTS = GS['clients'] = {}
NODES = GS['nodes'] = []
LIMITS = GS['limits'] = {}
SUBS = GS['subscriptions'] = {}


class Util:
    def __init__(self, check_cf : bool):
        self.check_cf = check_cf

    def get_request_ip(self, r : web.Request) -> str:
        #X-FORWARDED-FOR not safe, don't use
        try:
            if self.check_cf:
                host = r.headers.get('CF-Connecting-IP', None) #Added by Cloudflare
                if host != None:
                    return host

            host = r.headers.get('X-Real-IP', None) #Added by Nginx
            if host != None:
                return host
                
            return self.get_connecting_ip(r)
        except:
            return '0.0.0.0'

    def get_connecting_ip(self, r : web.Request):
        peername = r.transport.get_extra_info('peername')
        if peername is not None:
            host, _ = peername
            return host
        return None

UTIL = Util(CHECK_CF_CONNECTING_IP)

def websocket_rate_limit(r : web.Request, ws : web.WebSocketResponse):
    global LIMITS
    if LIMIT == 0:
        return False
    burst_max = LIMIT * 100
    pps = LIMIT
    ip = UTIL.get_request_ip(r)
    now = int(time.time())
    if ip not in LIMITS:
        LIMITS[ip] = {'count':burst_max, 'ts':now}
    else:
        if LIMITS[ip]['ts'] < now:
            LIMITS[ip]['count'] += (now - LIMITS[ip]['ts']) * pps
            LIMITS[ip]['ts'] = now
            if LIMITS[ip]['count'] > burst_max:
                LIMITS[ip]['count'] = burst_max
    if LIMITS[ip]['count'] <= 0:
        log.server_logger.error('client messaging too quickly: %s; %s; User-Agent: %s', ip, ws.id, str(
            r.headers.get('User-Agent')))
        return True
    LIMITS[ip]['count'] -= 1
    return False

def sessions_count(ip):
    count = 0
    for client in CLIENTS.values():
        if client['ip'] == ip:
            count += 1
    return count

def random_node():
    if len(NODES) == 0:
        return None
    return NODES[random.randint(0, len(NODES) - 1)]

def main_node():
    if len(NODES) == 0:
        return None
    return NODES[0]

def is_main_node(node_id):
    node = main_node()
    if not node:
        return False
    return node['id'] == node_id

async def forward_to_node(r: web.Request, request_json : dict, uid):
    request_json['client_id'] = uid

    node = main_node() if request_json['action'] == "event_subscribe" else random_node()
    if not node:
        return {'error':'node is offline'}
    
    try:
        await node['ws'].send_str(json.dumps(request_json))
    except Exception as e:
        log.server_logger.error('rpc error;%s;%s', str(e), node['ip'])

# Primary handler for all client websocket connections
async def handle_client_messages(r : web.Request, message : str, ws : web.WebSocketResponse):
    """Process data sent by client"""
    if websocket_rate_limit(r, ws):
        return {'error': 'Messaging too quickly'}

    ip = UTIL.get_request_ip(r)
    uid = ws.id
    log.server_logger.info('request; %s, %s, %s', message, ip, uid)

    try:
        client = CLIENTS[uid]
        request_json = json.loads(message)
        action = request_json['action']

        res = {'ack':action}
        if 'request_id' in request_json and len(request_json['request_id']) <= 256:
            res['request_id'] = request_json['request_id']

        if request_json['action'] not in ALLOWED_RPC_ACTIONS:
            res.update({'error':'action not allowed'})
            return res

        # adjust counts so nobody can block the node with a huge request
        if 'count' in request_json:
            count = int(request_json['count'])
            if (count < 0) or (count > 1000):
                request_json['count'] = 1000

        # rpc: account_unsubscribe, check no other subscribers first
        if request_json['action'] == "event_subscribe":
            log.server_logger.info('event_subscribe request; %s; %s', ip, uid)
            event = str(request_json['event'])

            if  len(event) <= 32 and len(client['events']) <= 16:
                client['events'].add(event)
        try:
            return await forward_to_node(r, request_json, uid)
        except Exception as e:
            log.server_logger.error('rpc error;%s;%s;%s', str(e), ip, uid)
            res.update({
                'error':'rpc error',
                'detail': str(e)
            })
            return res
    except Exception as e:
        log.server_logger.exception('uncaught error;%s;%s', ip, uid)
        return {
            'error':'general error',
            'detail':str(sys.exc_info())
        }

async def client_handler(r : web.Request):
    """Handler for websocket connections and messages"""
    if not NODES:
        return web.HTTPBadGateway()

    ip = UTIL.get_request_ip(r)
    if (sessions_count(ip) >= 2):
        return web.HTTPForbidden()

    ws = web.WebSocketResponse(heartbeat=30)
    try:
        await ws.prepare(r)
        # Connection Opened
    except:
        log.server_logger.error('Failed to prepare websocket: %s', UTIL.get_request_ip(r))
        return ws
    ws.id = str(uuid.uuid4())

    global CLIENTS
    CLIENTS[ws.id] = {'ws':ws, 'events':set(), 'ip':ip}

    try:
        log.server_logger.info('new client connection;%s;%s;User-Agent:%s', ip, ws.id, str(
            r.headers.get('User-Agent')))
        
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
    except Exception:
        log.server_logger.exception('Client Closed with exception')
    finally:
        del CLIENTS[ws.id]
    return ws

# Primary handler for callback
def callback_check_ip(r : web.Request):
    ip = UTIL.get_request_ip(r)
    if not ip or ip not in CALLBACK_WHITELIST:
        return True
    return False

async def send_to_clients(message : str, event = None):
    ids = list(CLIENTS.keys())
    for i in ids:
        if i not in CLIENTS:
            continue
        client = CLIENTS[i]
        if event != None and event not in client['events']:
            continue
        try:
            await client['ws'].send_str(message)
        except:
            pass

async def send_to_client(message : str, client_id):
    if client_id not in CLIENTS:
        return
    try:
        await CLIENTS[client_id]['ws'].send_str(message)
    except:
        pass

# Primary handler for all node websocket connections
async def handle_node_messages(r : web.Request, message : str, ws : web.WebSocketResponse):
    """Process data sent by node"""
    ip = UTIL.get_request_ip(r)
    node_id = ws.id
    log.server_logger.info('request; %s, %s, %s', message, ip, node_id)

    try:
        request_json = json.loads(message)
        if 'notify' in request_json:
            notify = request_json['notify']
            if notify == 'block_confirm':
                await send_to_clients(message)
            else:
                await send_to_clients(message, notify)
            
        elif 'ack' in request_json:
            client_id = request_json['client_id']
            del request_json['client_id']
            await send_to_client(json.dumps(request_json), client_id)
        else:
            log.server_logger.error('unexpected node message;%s;%s;%s', message, ip, node_id)
    except Exception as e:
        log.server_logger.exception('uncaught error;%s;%s;%s', str(e), ip, node_id)

async def destroy_node(r: web.Request, node_id):
    main = is_main_node(node_id)

    for i in range(len(NODES)):
        if NODES[i]['id'] == node_id:
            NODES.pop(i)
            break
    
    message = {'notify':'node_offline', 'main':'true' if main else 'false'}
    await send_to_clients(json.dumps(message))

async def node_handler(r : web.Request):
    ip = UTIL.get_request_ip(r)
    if callback_check_ip(r):
        log.server_logger.error('callback from unauthorized ip: %s', ip)
        return web.HTTPUnauthorized()
    
    ws = web.WebSocketResponse(heartbeat=30)
    try:
        await ws.prepare(r)
        # Connection Opened
    except:
        log.server_logger.error('Failed to prepare websocket: %s', UTIL.get_request_ip(r))
        return ws
     
    ws.id = str(uuid.uuid4())
    NODES.append({'ws':ws, 'ip':ip, 'id':ws.id})

    try:
        log.server_logger.info('new node connection;%s;%s;User-Agent:%s', ip, ws.id, str(r.headers.get('User-Agent')))
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
        await destroy_node(r, ws.id)
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
        return json.JSONEncoder.default(self, obj)

DEBUG_DUMPS = partial(json.dumps, cls=JsonEncoder, indent=4)

async def debug_handler(r : web.Request):
    if debug_check_ip(r):
        log.server_logger.error('debug request from unauthorized ip: %s', UTIL.get_request_ip(r))
        return web.HTTPUnauthorized()

    try:
        request_json = await r.json()
        query = request_json['query']
        if query == 'subscriptions':
            return web.json_response(SUBS, dumps=DEBUG_DUMPS)
        elif query == 'nodes':
            return web.json_response(NODES, dumps=DEBUG_DUMPS)
        elif query == 'clients':
            return web.json_response(CLIENTS, dumps=DEBUG_DUMPS)
        elif query == 'limits':
            return web.json_response(LIMITS, dumps=DEBUG_DUMPS)
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
        logging.basicConfig(level=logging.DEBUG)
    else:
        root = logging.getLogger('aiohttp.server')
        logging.basicConfig(level=logging.WARN)
        handler = WatchedFileHandler(LOG_FILE)
        formatter = logging.Formatter("%(asctime)s;%(levelname)s;%(message)s", "%Y-%m-%d %H:%M:%S %z")
        handler.setFormatter(formatter)
        root.addHandler(handler)
        root.addHandler(TimedRotatingFileHandler(LOG_FILE, when="d", interval=1, backupCount=30))        

    app = web.Application()

    app.add_routes([web.get('/', client_handler)]) # All client WS requests
    app.add_routes([web.get(f'/callback/{CALLBACK_TOKEN}', node_handler)]) # ws/wss callback from nodes
    app.add_routes([web.post('/debug', debug_handler)]) # local debug interface

    return app

APP = LOOP.run_until_complete(init_app())

def main():
    global APP, GS
    # Start web/ws server
    async def start():
        runner = web.AppRunner(APP)
        await runner.setup()
        site = web.TCPSite(runner, LISTEN_HOST, LISTEN_PORT)
        await site.start()

    async def end():
        await APP.shutdown()

    LOOP.run_until_complete(start())

    # Main program
    try:
        LOOP.run_forever()
    except KeyboardInterrupt:
        pass
    finally:
        LOOP.run_until_complete(end())

    LOOP.close()

if __name__ == "__main__":
    main()

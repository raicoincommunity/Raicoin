#!/usr/bin/env python
#V1.5.0
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


from logging.handlers import TimedRotatingFileHandler, WatchedFileHandler
from aiohttp import ClientSession, WSMessage, WSMsgType, log, web

from rpc import Rpc
from util import Util

# whitelisted commands, disallow anything used for local node-based wallet as we may be using multiple back ends
ALLOWED_RPC_ACTIONS = [
    'account_info', 'account_subscribe', 'account_unsubscribe', 'account_forks', 'block_publish', 'block_query', 'receivables', 'current_timestamp'
]

asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())

# Configuration arguments
parser = argparse.ArgumentParser(description="Raicoin Wallet Server")
parser.add_argument('--host', type=str, help='Host to listen on (e.g. 127.0.0.1)', default='127.0.0.1')
parser.add_argument('-p', '--port', type=int, help='Port to listen on', default=7178)
parser.add_argument('--log-file', type=str, help='Log file location', default='rai_server.log')
parser.add_argument('--limit', type=int, help='Max allowed requests per second from one IP', default=0)
parser.add_argument('-t', '--token', help='Create a secure url token', action='store_true')

options = parser.parse_args()

try:
    if options.token:
        print(secrets.token_urlsafe())
        sys.exit(0)
    LISTEN_HOST = str(ipaddress.ip_address(options.host))
    LISTEN_PORT = int(options.port)
    LOG_FILE = options.log_file
    server_desc = f'on {LISTEN_HOST} port {LISTEN_PORT}'
    print(f'Starting Raicoin Wallet Server (RAI) {server_desc}')
    LIMIT = int(options.limit)
except Exception as e:
    parser.print_help()
    print(e)
    sys.exit(0)

# Environment configuration
NODE_URL = os.getenv('NODE_URL', 'http://127.0.0.1:7176')
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
DEBUG_MODE = True if int(os.getenv('DEBUG', 1)) != 0 else False

CALLBACK_TOKEN = os.getenv("CALLBACK_TOKEN", '')
if len(CALLBACK_TOKEN) != 43:
    print("Error found in .env: CALLBACK_TOKEN is missing or invalid, you can use 'python3 rai_wallet_server.py -t' to generate a secure token")
    sys.exit(0)

CHECK_CF_CONNECTING_IP = True if int(os.getenv('USE_CLOUDFLARE', 0)) == 1 else False

LOOP = asyncio.get_event_loop()
RPC = Rpc(NODE_URL)
UTIL = Util(CHECK_CF_CONNECTING_IP)
    
def websocket_rate_limit(r : web.Request, ws : web.WebSocketResponse):
    if LIMIT == 0:
        return False
    burst_max = LIMIT * 100
    pps = LIMIT
    ip = UTIL.get_request_ip(r)
    now = int(time.time())
    if ip not in r.app['limit']:
        r.app['limit'][ip] = {'count':burst_max, 'ts':now}
    else:
        if r.app['limit'][ip]['ts'] < now:
            r.app['limit'][ip]['count'] += (now - r.app['limit'][ip]['ts']) * pps
            r.app['limit'][ip]['ts'] = now
            if r.app['limit'][ip]['count'] > burst_max:
                r.app['limit'][ip]['count'] = burst_max
    if r.app['limit'][ip]['count'] <= 0:
        log.server_logger.error('client messaging too quickly: %s; %s; User-Agent: %s', ip, ws.id, str(
            r.headers.get('User-Agent')))
        return True
    r.app['limit'][ip]['count'] -= 1
    return False

async def websocket_forward_to_node(r: web.Request, request_json : dict, uid):
    request_json['client_id'] = uid

    account = ''
    if 'account' in request_json:
        account = request_json['account']
    elif 'block' in request_json and 'account' in request_json['block']:
        account = request_json['block']['account']
    
    if not account:
        node_id = random_node(r)
        if not node_id or node_id not in r.app['nodes']:
            return json.dumps({'error':'node is offline'})
        try:
            await r.app['nodes'][node_id]['ws'].send_str(json.dumps(request_json))
        except Exception as e:
            log.server_logger.error('rpc error;%s;%s', str(e), r.app['nodes'][node_id]['ip'])
        return
    
    node_id = ''
    if account in r.app['subscriptions']:
        node_id = r.app['subscriptions'][account]['node']
    else:
        node_id = random_node(r)            
    if not node_id or node_id not in r.app['nodes']:
        return json.dumps({'error':'node is offline'})
    subscribe(r, account, uid, node_id)

    node = r.app['nodes'][node_id]
    try:
        await node['ws'].send_str(json.dumps(request_json))
    except Exception as e:
        log.server_logger.error('rpc error;%s;%s', str(e), ip)

def subscribe(r : web.Request, account : str, uid : str, nid : str):
    subs = r.app['subscriptions']
    if account not in subs:
        subs[account] = { 'clients': set(), 'node': nid}
        log.server_logger.info('subscribe:%s', account)
    if uid in subs[account]['clients']:
        return
    subs[account]['clients'].add(uid)

def unsubscribe(r : web.Request, account : str, uid : str):
    subs = r.app['subscriptions']
    if account not in subs:
        return
    if uid in subs[account]['clients']:
        subs[account]['clients'].remove(uid)
    if len(subs[account]['clients']) == 0:
        log.server_logger.info('unsubscribe:%s', account)
        del subs[account]

# Primary handler for all client websocket connections
async def handle_client_messages(r : web.Request, message : str, ws : web.WebSocketResponse):
    """Process data sent by client"""
    if websocket_rate_limit(r, ws):
        reply = {'error': 'Messaging too quickly'}
        return json.dumps(reply)

    ip = UTIL.get_request_ip(r)
    uid = ws.id
    log.server_logger.info('request; %s, %s, %s', message, ip, uid)
    if message not in r.app['active_messages']:
        r.app['active_messages'].add(message)
    else:
        log.server_logger.error('request already active; %s; %s; %s', message, ip, uid)
        return

    try:
        clients = r.app['clients']
        subs = r.app['subscriptions']

        request_json = json.loads(message)
        if request_json['action'] not in ALLOWED_RPC_ACTIONS:
            return json.dumps({'error':'action not allowed'})

        # adjust counts so nobody can block the node with a huge request
        if 'count' in request_json:
            count = int(request_json['count'])
            if (count < 0) or (count > 1000):
                request_json['count'] = 1000

        # rpc: account_unsubscribe, check no other subscribers first
        if request_json['action'] == "account_unsubscribe":
            log.server_logger.info('unsubscribe request; %s; %s', ip, uid)
            account = request_json['account']

            if uid not in clients:
                return json.dumps({'error':'uid not found'})
            if 'accounts' not in clients[uid] or account not in clients[uid]['accounts']:
                return json.dumps({'error':'account not subscribed'})
            clients[uid]['accounts'].remove(account)
            
            unsubscribe(r, account, uid)
            return json.dumps({'success':'', 'ack':'account_unsubscribe'})
        elif request_json['action'] == "current_timestamp":
            return json.dumps({'timestamp': str(int(time.time())), 'ack': 'current_timestamp'})
        # rpc: defaut forward the request
        else:
            try:
                return await websocket_forward_to_node(r, request_json, uid)
            except Exception as e:
                log.server_logger.error('rpc error;%s;%s;%s', str(e), ip, uid)
                return json.dumps({
                    'error':'rpc error',
                    'detail': str(e)
                })
    except Exception as e:
        log.server_logger.exception('uncaught error;%s;%s', ip, uid)
        return json.dumps({
            'error':'general error',
            'detail':str(sys.exc_info())
        })
    finally:
        r.app['active_messages'].remove(message)

def random_node(r: web.Request):
    if not r.app['nodes']:
        return
    return random.choice(list(r.app['nodes'].keys()))

async def client_handler(r : web.Request):
    """Handler for websocket connections and messages"""
    if not r.app['nodes']:
        return web.HTTPBadGateway()

    ws = web.WebSocketResponse(heartbeat=30)
    try:
        await ws.prepare(r)
        # Connection Opened
    except:
        log.server_logger.error('Failed to prepare websocket: %s', UTIL.get_request_ip(r))
        return ws
    ws.id = str(uuid.uuid4())
    ip = UTIL.get_request_ip(r)
    r.app['clients'][ws.id] = {'ws':ws, 'accounts':set()}
    log.server_logger.info('new client connection;%s;%s;User-Agent:%s', ip, ws.id, str(
        r.headers.get('User-Agent')))

    try:
        async for msg in ws:
            if msg.type == WSMsgType.TEXT:
                if msg.data == 'close':
                    await ws.close()
                else:
                    reply = await handle_client_messages(r, msg.data, ws=ws)
                    if reply:
                        log.server_logger.debug('Sending response %s to %s', reply, ip)
                        await ws.send_str(reply)
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
        await destory_client(r, ws.id)
    return ws

async def destory_client(r : web.Request, client_id):
    if client_id not in r.app['clients']:
        return
    client = r.app['clients'][client_id]
    if 'accounts' in client:
        for account in client['accounts']:
            unsubscribe(r, account, client_id)
    try:
        await client['ws'].close()
    except:
        pass
    del r.app['clients'][client_id]

async def notify_account_unsubscribe(r : web.Request, uid, account):
    if uid not in r.app['clients']:
        return
    notify = { 'notify':'account_unsubscribe', 'account':account}
    try:
        await r.app['clients'][uid]['ws'].send_str(json.dumps(notify))
    except:
        pass

async def destroy_node(r: web.Request, node_id):
    if node_id in r.app['nodes']:
        try:
            await r.app['nodes'][node_id]['ws'].close()
        except:
            pass
        del r.app['nodes'][node_id]
    accounts = []
    for k,v in r.app['subscriptions'].items():
        if v['node'] == node_id:
            accounts.append(k)
    for account in accounts:
        for uid in r.app['subscriptions'][account]['clients']:
            await notify_account_unsubscribe(r, uid, account)
        del r.app['subscriptions'][account]

# Primary handler for callback
def callback_check_ip(r : web.Request):
    ip = UTIL.get_request_ip(r)
    if not ip or ip not in CALLBACK_WHITELIST:
        return True
    return False

# Primary handler for all node websocket connections
async def handle_node_messages(r : web.Request, message : str, ws : web.WebSocketResponse):
    """Process data sent by node"""
    ip = UTIL.get_request_ip(r)
    node_id = ws.id
    log.server_logger.info('request; %s, %s, %s', message, ip, node_id)

    clients = r.app['clients']
    subs = r.app['subscriptions']
    try:
        request_json = json.loads(message)
        if 'notify' in request_json:
            notify = request_json['notify']
            account = request_json['account']
            log.server_logger.debug('node message received: notify=%s, account=%s', notify, account)

            if account not in subs:
                return

            log.server_logger.info("Pushing to clients %s", str(subs[account]))
            for sub in subs[account]['clients']:
                try:
                    await r.app['clients'][sub]['ws'].send_str(message)
                except:
                    pass
        elif 'ack' in request_json:
            action = request_json['ack']
            client_id = request_json['client_id']
            del request_json['client_id']
            if client_id not in clients:
                log.server_logger.info('client missing:message=%s;', message)
                return
            if action == 'account_subscribe':
                if 'success' in request_json:
                    account = request_json['account']
                    clients[client_id]['accounts'].add(account)
            try:
                await clients[client_id]['ws'].send_str(json.dumps(request_json))
            except:
                pass
        else:
            log.server_logger.error('unexpected node message;%s;%s;%s', message, ip, node_id)
    except Exception as e:
        log.server_logger.exception('uncaught error;%s;%s;%s', str(e), ip, node_id)

async def node_handler(r : web.Request):
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
     
    ws.id = str(uuid.uuid4())
    ip = UTIL.get_request_ip(r)
    r.app['nodes'][ws.id] = {'ws':ws, 'ip':ip}
    log.server_logger.info('new node connection;%s;%s;User-Agent:%s', ip, ws.id, str(r.headers.get('User-Agent')))

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
    except Exception:
        log.server_logger.exception('Node closed with exception')
    finally:
        await destroy_node(r, ws.id)
    return ws

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
    # Global vars
    app['clients'] = {} # Keep track of connected clients
    app['nodes'] = {} # Keep track of connected nodes
    app['limit'] = {} # Limit messages based on IP
    app['active_messages'] = set() # Avoid duplicate messages from being processes simultaneously
    app['subscriptions'] = {} # Store subscription UUIDs, this is used for targeting callback accounts

    app.add_routes([web.get('/', client_handler)]) # All client WS requests
    app.add_routes([web.get(f'/callback/{CALLBACK_TOKEN}', node_handler)]) # ws/wss callback from nodes

    return app

APP = LOOP.run_until_complete(init_app())

def main():
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

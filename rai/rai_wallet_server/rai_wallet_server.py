#!/usr/bin/env python
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


from logging.handlers import TimedRotatingFileHandler, WatchedFileHandler
from aiohttp import ClientSession, WSMessage, WSMsgType, log, web

from rpc import RPC, allowed_rpc_actions
from util import Util

asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())

# Configuration arguments
parser = argparse.ArgumentParser(description="Raicoin Wallet Server")
parser.add_argument('--host', type=str, help='Host to listen on (e.g. 127.0.0.1)', default='127.0.0.1')
parser.add_argument('-p', '--port', type=int, help='Port to listen on', default=5076)
parser.add_argument('--log-file', type=str, help='Log file location', default='rai_server.log')
options = parser.parse_args()

try:
    listen_host = str(ipaddress.ip_address(options.host))
    listen_port = int(options.port)
    log_file = options.log_file
    server_desc = f'on {listen_host} port {listen_port}'
    print(f'Starting Raicoin Wallet Server (RAI) {server_desc}')
except Exception as e:
    parser.print_help()
    print(e)
    sys.exit(0)

# Environment configuration
rpc_url = os.getenv('RPC_URL', 'http://127.0.0.1:7176')
callback_whitelist = os.getenv('CALLBACK_WHITELIST', '127.0.0.1')
if callback_whitelist != '127.0.0.1':
    try:
        ips = callback_whitelist.split(',')
        callback_whitelist = []
        for ip in ips:
            if not ip.strip():
                continue
            callback_whitelist.append(str(ipaddress.ip_address(ip.strip())))
        if not callback_whitelist:
            print("Error found in .env: invalid CALLBACK_WHITELIST config")
    except:
        print("Error found in .env: invalid CALLBACK_WHITELIST config")
        sys.exit(0)
debug_mode = True if int(os.getenv('DEBUG', 1)) != 0 else False

loop = asyncio.get_event_loop()
rpc = RPC(rpc_url)
util = Util()
    
def websocket_rate_limit(r : web.Request, ws : web.WebSocketResponse):
    burst_max = 4096
    pps = 20
    ip = util.get_request_ip(r)
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

async def websocket_forward_to_node(request_json : dict, ip, uid):
    response = await rpc.json_post(request_json)
    if not response:
        log.server_logger.error('rpc error:%s;%s', ip, uid)
        response = {'error':'rpc error'}
    return response

def subscribe(r : web.Request, account : str, uid : str):
    subs = r.app['subscriptions']
    if account not in subs:
        subs[account] = set()
        log.server_logger.info('subscribe:%s', account)
    subs[account].add(uid)

def unsubscribe(r : web.Request, account : str, uid : str):
    subs = r.app['subscriptions']
    if account not in subs:
        return
    if uid in subs[account]:
        subs[account].remove(uid)
    if len(subs[account]) == 0:
        log.server_logger.info('unsubscribe:%s', account)
        del subs[account]

# Primary handler for all websocket connections
async def websocket_handle_messages(r : web.Request, message : str, ws : web.WebSocketResponse):
    """Process data sent by client"""
    if websocket_rate_limit(r, ws):
        reply = {'error': 'Messaging too quickly'}
        return json.dumps(reply)

    ip = util.get_request_ip(r)
    uid = ws.id
    log.server_logger.info('request; %s, %s, %s', message, ip, uid)
    if message not in r.app['active_messages']:
        r.app['active_messages'].add(message)
    else:
        log.server_logger.error('request already active; %s; %s; %s', message, ip, uid)
        return None

    try:
        clients = r.app['clients']
        subs = r.app['subscriptions']

        request_json = json.loads(message)
        if request_json['action'] not in allowed_rpc_actions:
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
                return json.dumps({'error':'uid not found'});
            if 'accounts' not in clients[uid] or account not in clients[uid]['accounts']:
                return json.dumps({'error':'account not subscribed'});
            clients[uid]['accounts'].remove(account);
            
            unsubscribe(r, account, uid)
            if account not in subs:
                response = await websocket_forward_to_node(request_json, ip, uid)
                return json.dumps(response)
            else:
                return json.dumps({'success':''})

        # rpc: defaut forward the request
        else:
            try:
                response = await websocket_forward_to_node(request_json, ip, uid)
                if request_json['action'] == 'account_subscribe' and 'success' in response:
                    account = request_json['account']
                    subscribe(r, account, uid)
                    clients[uid]['accounts'].add(account); 

                return json.dumps(response)
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

async def websocket_handler(r : web.Request):
    """Handler for websocket connections and messages"""
    ws = web.WebSocketResponse(heartbeat=30)
    await ws.prepare(r)
    # Connection Opened
    ws.id = str(uuid.uuid4())
    r.app['clients'][ws.id] = {'ws':ws, 'accounts':set()}
    log.server_logger.info('new connection;%s;%s;User-Agent:%s', util.get_request_ip(r), ws.id, str(
        r.headers.get('User-Agent')))

    try:
        async for msg in ws:
            if msg.type == WSMsgType.TEXT:
                if msg.data == 'close':
                    await ws.close()
                else:
                    reply = await websocket_handle_messages(r, msg.data, ws=ws)
                    if reply is not None:
                        log.server_logger.debug('Sending response %s to %s', reply, util.get_request_ip(r))
                        await ws.send_str(reply)
            elif msg.type == WSMsgType.CLOSE:
                log.server_logger.info('WS Connection closed normally')
                break
            elif msg.type == WSMsgType.ERROR:
                log.server_logger.info('WS Connection closed with error %s', ws.exception())
                break

        log.server_logger.info('WS connection closed normally')
    except Exception:
        log.server_logger.exception('WS Closed with exception')
    finally:
        if ws.id in r.app['clients']:
            if 'accounts' in r.app['clients'][ws.id]:
                for account in r.app['clients'][ws.id]['accounts']:
                    unsubscribe(r, account, ws.id)
            del r.app['clients'][ws.id]
        await ws.close()
    return ws

# Primary handler for callback
def callback_check_ip(r : web.Request):
    ip = util.get_connecting_ip(r)
    if not ip or ip not in callback_whitelist:
        return True
    return False

async def callback_handler(r : web.Request):
    if callback_check_ip(r):
        log.server_logger.error('callback from unauthorized ip: %s', util.get_connecting_ip(r))
        return web.HTTPUnauthorized()

    try:
        request_json = await r.json()
        notify = request_json['notify']
        account = request_json['account']
        log.server_logger.debug('callback received: notify=%s, account=%s', notify, account)

        if account not in r.app['subscriptions']:
            return

        log.server_logger.info("Pushing to clients %s", str(r.app['subscriptions'][account]))
        for sub in r.app['subscriptions'][account]:
            await r.app['clients'][sub]['ws'].send_str(json.dumps(request_json))
        
        return web.HTTPOk()
    except Exception:
        log.server_logger.exception('received exception in callback')
        return web.HTTPInternalServerError(reason="Something went wrong %s" % str(sys.exc_info()))


async def init_app():
    # Setup logger
    if debug_mode:
        print("debug mode")
        logging.basicConfig(level=logging.DEBUG)
    else:
        root = logging.getLogger('aiohttp.server')
        logging.basicConfig(level=logging.WARN)
        handler = WatchedFileHandler(log_file)
        formatter = logging.Formatter("%(asctime)s;%(levelname)s;%(message)s", "%Y-%m-%d %H:%M:%S %z")
        handler.setFormatter(formatter)
        root.addHandler(handler)
        root.addHandler(TimedRotatingFileHandler(log_file, when="d", interval=1, backupCount=100))        

    app = web.Application()
    # Global vars
    app['clients'] = {} # Keep track of connected clients
    app['limit'] = {} # Limit messages based on IP
    app['active_messages'] = set() # Avoid duplicate messages from being processes simultaneously
    app['subscriptions'] = {} # Store subscription UUIDs, this is used for targeting callback accounts

    app.add_routes([web.get('/', websocket_handler)]) # All WS requests
    app.add_routes([web.post('/callback', callback_handler)]) # HTTP Callback from node

    return app

app = loop.run_until_complete(init_app())


def main():
    # Start web/ws server
    async def start():
        runner = web.AppRunner(app)
        await runner.setup()
        site = web.TCPSite(runner, listen_host, listen_port)
        await site.start()

    async def end():
        await app.shutdown()

    loop.run_until_complete(start())

    # Main program
    try:
        loop.run_forever()
    except KeyboardInterrupt:
        pass
    finally:
        loop.run_until_complete(end())

    loop.close()

if __name__ == "__main__":
    main()

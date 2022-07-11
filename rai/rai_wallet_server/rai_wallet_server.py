#!/usr/bin/env python
#V1.6.0
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
import dns.resolver
import dns.rrset

from logging.handlers import TimedRotatingFileHandler, WatchedFileHandler
from aiohttp import ClientSession, WSMessage, WSMsgType, log, web, ClientWebSocketResponse
from functools import partial
from dns.asyncresolver import Resolver

# whitelisted commands, disallow anything used for local node-based wallet as we may be using multiple back ends
ALLOWED_RPC_ACTIONS = [
    'account_info', 'account_subscribe', 'account_unsubscribe', 'account_forks', 'block_publish', 'block_query', 'receivables', 'current_timestamp', 'service_subscribe', 'dns_verify'
]

asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())
LOOP = asyncio.get_event_loop()

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
    print("Error found in .env: CALLBACK_TOKEN is missing or invalid, you can use 'python3 rai_wallet_server.py -t' to generate a secure token")
    sys.exit(0)

CHECK_CF_CONNECTING_IP = True if int(os.getenv('USE_CLOUDFLARE', 0)) == 1 else False

BSC_BRIDGE_URL = os.getenv('BSC_BRIDGE_URL', 'wss://bsc-bridge.raicoin.org/')
if not BSC_BRIDGE_URL.startswith('wss://'):
    print("Error found in .env: invalid BSC_BRIDGE_URL")
    sys.exit(0)

PANCAKESWAP_LP_REWARDER_URL = os.getenv(
    'PANCAKESWAP_LP_REWARD_URL', 'wss://pancakeswap-lp-reward.raicoin.org')
if not PANCAKESWAP_LP_REWARDER_URL.startswith('wss://'):
    print("Error found in .env: invalid PANCAKESWAP_LP_REWARDER_URL")
    sys.exit(0)

BSC_FAUCET_URL = os.getenv('BSC_FAUCET_URL', 'wss://bsc-faucet.raicoin.org/')
if not BSC_FAUCET_URL.startswith('wss://'):
    print("Error found in .env: invalid BSC_FAUCET_URL")
    sys.exit(0)

ALIAS_URL = os.getenv('ALIAS_URL', 'wss://alias.raicoin.org/')
if not ALIAS_URL.startswith('wss://'):
    print("Error found in .env: invalid ALIAS_URL")
    sys.exit(0)

TOKEN_URL = os.getenv('TOKEN_URL', 'wss://token.raicoin.org/')
if not TOKEN_URL.startswith('wss://'):
    print("Error found in .env: invalid TOKEN_URL")
    sys.exit(0)

VALIDATOR_URL = os.getenv('VALIDATOR_URL', 'wss://validator.raicoin.org/')
if not VALIDATOR_URL.startswith('wss://'):
    print("Error found in .env: invalid VALIDATOR_URL")
    sys.exit(0)

SRV_PROVIDERS = 'service_providers'
SRV_SUBS = 'service_subscriptions'
MAX_FILTERS_PER_SUBSCRIPTION = 1024
MAX_FILTER_VALUE_SIZE = 256
DNS_VERIFICATION_PREFIX = '_raicoin-verification'


GS = {} # Global States

TASKS = [
    {
        'desc': 'BSC bridge',
        'url': BSC_BRIDGE_URL,
    },
    {
        'desc': 'Pancakeswap LP rewarder',
        'url': PANCAKESWAP_LP_REWARDER_URL,
    },
    {
        'desc': 'BSC faucet',
        'url': BSC_FAUCET_URL,
    },
    {
        'desc': 'RAI alias',
        'url': ALIAS_URL,
    },
    {
        'desc': 'RAI token',
        'url': TOKEN_URL,
    },
    {
        'desc': 'RAI validator',
        'url': VALIDATOR_URL,
    },
]
GS['tasks'] = TASKS

class Util:
    def __init__(self, check_cf : bool):
        self.check_cf = check_cf

    def get_request_ip(self, r : web.Request) -> str:
        #X-FORWARDED-FOR not safe, don't use

        if self.check_cf:
            host = r.headers.get('CF-Connecting-IP', None) #Added by Cloudflare
            if host != None:
                return host

        host = r.headers.get('X-Real-IP', None) #Added by Nginx
        if host != None:
            return host
            
        return self.get_connecting_ip(r)

    def get_connecting_ip(self, r : web.Request):
        peername = r.transport.get_extra_info('peername')
        if peername is not None:
            host, _ = peername
            return host
        return None

    async def json_post(self, url: str, request : dict = None, headers : dict = None, timeout : int = 10) -> dict:
        try:
            async with ClientSession() as session:
                async with session.post(url, json=request, headers=headers, timeout=timeout) as resp:
                    return await resp.json(content_type=None)
        except Exception:
            log.server_logger.exception()
            return None

    async def json_get(self, url: str, params : dict = None, headers : dict = None, timeout : int = 10) -> dict:
        try:
            async with ClientSession() as session:
                async with session.get(url, params=params, headers=headers, timeout=timeout) as resp:
                    return await resp.json(content_type=None)
        except Exception:
            log.server_logger.exception()
            return None

UTIL = Util(CHECK_CF_CONNECTING_IP)

async def dns_query(domain: str, rtype: str = 'TXT', **kwargs):
    try:
        resolver = Resolver()
        response = await resolver.resolve(domain, rdtype=rtype, **kwargs)
        return response.rrset
    except:
        return None

async def dns_query_from_cloudflare(domain: str, rtype: str = 'TXT'):
    url = 'https://cloudflare-dns.com/dns-query'
    headers = {'Accept': 'application/dns-json'}
    params = {'name': domain, 'type': rtype}
    return await UTIL.json_get(url, params, headers)

async def dns_verify(domain: str, account: str):
    try:
        if not domain.startswith('.'):
            domain = '.' + domain
        domain = DNS_VERIFICATION_PREFIX + domain
        res = await asyncio.gather(dns_query_from_cloudflare(domain), dns_query(domain))

        count = 0
        for i in res[0]['Answer']:
            if i['data'].replace('"', '') == account:
                count += 1
                break

        for i in res[1]:
            if i.to_text().replace('"', '') == account:
                count += 1
                break

        if count < 2:
            return True

        return False
    except Exception as e:
        log.server_logger.error('dns_verify error;%s;%s', domain, str(e))
        return True

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

async def forward_to_node(r: web.Request, request_json : dict, uid):
    request_json['client_id'] = uid

    account = ''
    if 'account' in request_json:
        account = request_json['account']
    elif 'block' in request_json and 'account' in request_json['block']:
        account = request_json['block']['account']
    
    if not account:
        node_id = random_node(r)
        if not node_id or node_id not in r.app['nodes']:
            return {'error':'node is offline'}
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
        return {'error':'node is offline'}
    subscribe(r, account, uid, node_id)

    node = r.app['nodes'][node_id]
    try:
        await node['ws'].send_str(json.dumps(request_json))
    except Exception as e:
        log.server_logger.error('rpc error;%s;%s', str(e), node['ip'])

async def forward_to_service_provider(r: web.Request, request_json : dict, uid):
    action  = request_json['action']
    service = request_json['service']
    res = {'ack':action, 'service':service}

    if 'request_id' in request_json:
        res['request_id'] = request_json['request_id']

    if service not in r.app[SRV_PROVIDERS]:
        res.update({'error': 'service not found'})
        return res
    if 'actions' not in r.app[SRV_PROVIDERS][service]:
        res.update({'error': 'provider not found'})
        return res
    if action not in r.app[SRV_PROVIDERS][service]['actions']:
        res.update({'error': 'invalid action'})
        return res
    request_json['client_id'] = uid

    try:
        await r.app[SRV_PROVIDERS][service]['ws'].send_str(json.dumps(request_json))
    except Exception as e:
        log.server_logger.error('service %s rpc error:%s', service, str(e))
        res.update({'error': 'rpc error'})
        return res

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

def filter_exists(filters, search):
    for i in filters:
        if i['key'] == search['key'] and i['value'] == search['value']:
            return True
    return False

def all_filters_exist(filters, searchs):
    for i in searchs:
        if not filter_exists(filters, i):
            return False
    return True

def merge_filters(dest, src):
    for i in src:
        if not filter_exists(dest, i):
            dest.append(i)
    return dest

def service_subscribe(r : web.Request, uid, service, filters):
    providers = r.app[SRV_PROVIDERS]
    subs = r.app[SRV_SUBS]
    if service not in providers:
        return True
    if len(filters) > MAX_FILTERS_PER_SUBSCRIPTION:
        return True
        
    for f in filters:
        if f['key'] not in providers[service]['filters']:
            return True
        if len(str(f['value'])) > MAX_FILTER_VALUE_SIZE:
            return True

    for f in filters:
        k = f['key']
        v = str(f['value'])
        if service not in subs:
            subs[service] = {}
        if k not in subs[service]:
            subs[service][k] = {}
        if v not in subs[service][k]:
            subs[service][k][v] = set()
        
        subs[service][k][v].add(uid)
    return False

def service_unsubscribe(r: web.Request, uid, service = None):
    clients = r.app['clients']
    if uid not in clients:
        return
    client = r.app['clients'][uid]

    subs = r.app[SRV_SUBS]
    services = []
    if service == None:
        services = client[SRV_SUBS].keys()
    else:
        services.append(service)

    for service in services:
        if service not in client[SRV_SUBS]:
            continue
        filters = client[SRV_SUBS][service]
        for f in filters:
            k = f['key']
            v = str(f['value'])
            if service not in subs or k not in subs[service] or v not in subs[service][k]:
                continue
            if uid not in subs[service][k][v]:
                continue
            subs[service][k][v].remove(uid)
            if len(subs[service][k][v]) == 0:
                del subs[service][k][v]
            if len(subs[service][k]) == 0:
                del subs[service][k]
            if len(subs[service]) == 0:
                del subs[service]

def handle_client_service_subscribe(r : web.Request, uid, service, filters):
    clients =  r.app['clients']
    if uid not in clients or SRV_SUBS not in clients[uid]:
        return False, None
    srv_subs = clients[uid][SRV_SUBS]
    if service in srv_subs:
        if all_filters_exist(srv_subs[service], filters):
            return False, None
        service_unsubscribe(r, uid, service)
        merge_filters(srv_subs[service], filters)
    else:
        srv_subs[service] = filters
    error = service_subscribe(r, uid, service, srv_subs[service])
    if error:
        del r.app['clients'][uid][SRV_SUBS][service]
        return True, 'wallet server: failed to subscribe service'
    return False, None

async def handle_client_dns_verify(r : web.Request, request, response):
    if len(request['items']) > 10:
        response['error'] = 'too many items'
        return
    coros = [dns_verify(i['dns'], i['account']) for i in request['items']]
    result = await asyncio.gather(*coros)
    items = []
    for i, fail in enumerate(result):
        item = {}
        item['account'] = request['items'][i]['account']
        item['dns'] = request['items'][i]['dns']
        item['valid'] = 'false' if fail else 'true'
        items.append(item)
    response['items'] = items

# Primary handler for all client websocket connections
async def handle_client_messages(r : web.Request, message : str, ws : web.WebSocketResponse):
    """Process data sent by client"""
    if websocket_rate_limit(r, ws):
        return {'error': 'Messaging too quickly'}

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
        request_json = json.loads(message)
        action = request_json['action']

        res = {'ack':action}
        if 'request_id' in request_json and len(request_json['request_id']) <= 256:
            res['request_id'] = request_json['request_id']

        if 'service' in request_json:
            service = request_json['service']
            res['service'] = service
            if action == 'service_subscribe':
                filters = request_json['filters']
                error, info = handle_client_service_subscribe(r, uid, service, filters)
                if error:
                    res.update({'error': info})
                    return res
            elif action == 'dns_verify':
                await handle_client_dns_verify(r, request_json, res)
                return res
            return await forward_to_service_provider(r, request_json, uid)

        if request_json['action'] not in ALLOWED_RPC_ACTIONS:
            res.update({'error':'action not allowed'})
            return res

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
                res.update({'error':'uid not found'})
                return res
            if 'accounts' not in clients[uid] or account not in clients[uid]['accounts']:
                res.update({'error':'account not subscribed'})
                return res
            clients[uid]['accounts'].remove(account)
            
            unsubscribe(r, account, uid)
            res.update({'success':''})
            return res
        elif request_json['action'] == "current_timestamp":
            res.update({'timestamp': str(int(time.time()))})
            return res
        # rpc: defaut forward the request
        else:
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
    r.app['clients'][ws.id] = {'ws':ws, 'accounts':set(), SRV_SUBS:{}}
    log.server_logger.info('new client connection;%s;%s;User-Agent:%s', ip, ws.id, str(
        r.headers.get('User-Agent')))

    try:
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
        await destory_client(r, ws.id)
    return ws

async def destory_client(r : web.Request, client_id):
    accounts = list(r.app['subscriptions'].keys())
    for account in accounts:
        unsubscribe(r, account, client_id)
    if client_id not in r.app['clients']:
        return
    service_unsubscribe(r, client_id)
    client = r.app['clients'][client_id]
    client['accounts'].clear()
    try:
        await client['ws'].close()
    except:
        pass
    del r.app['clients'][client_id]

async def notify_account_unsubscribe(r : web.Request, uid, account):
    if uid not in r.app['clients']:
        return
    client = r.app['clients'][uid]
    if account not in client['accounts']:
        return
    client['accounts'].remove(account)
    notify = {'notify':'account_unsubscribe', 'account':account}
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
        if account not in r.app['subscriptions'] or r.app['subscriptions'][account]['node'] != node_id:
            continue
        uids = r.app['subscriptions'][account]['clients'].copy()
        for uid in uids:
            await notify_account_unsubscribe(r, uid, account)
        if account in r.app['subscriptions'] and r.app['subscriptions'][account]['node'] == node_id:
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
            if 'node' not in subs[account] or subs[account]['node'] != node_id:
                return

            log.server_logger.info("Pushing to clients %s", str(subs[account]))

            sub_clients = subs[account]['clients'].copy()
            for sub in sub_clients:
                try:
                    if sub not in r.app['clients']:
                        continue
                    if account not in r.app['clients'][sub]['accounts']:
                        continue
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
                if 'account' in request_json:
                    account = request_json['account']
                    if 'success' in request_json:
                        clients[client_id]['accounts'].add(account)
                    else:
                        unsubscribe(r, account, client_id)
                        if account in clients[client_id]['accounts']:
                            clients[client_id]['accounts'].remove(account)
                else:
                    log.server_logger.error('unexpected account_subscribe ack:message=%s;', message)
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
    except Exception as e:
        log.server_logger.exception('Node closed with exception=%s', e)
    finally:
        await destroy_node(r, ws.id)
    return ws

async def handle_service_provider_message(app, msg_str, ws):
    log.server_logger.info('message from service provider: %s', msg_str)

    try:
        msg = json.loads(msg_str)
        if 'register' in msg:
            service = msg['register']
            app[SRV_PROVIDERS][service] = {'ws':ws, 'filters': msg['filters']}
            if 'actions' in msg:
                app[SRV_PROVIDERS][service]['actions'] = msg['actions']

        elif 'notify' in msg:
            service = msg['service']
            filters = msg['filters']
            subs = app[SRV_SUBS]

            if service not in subs:
                log.server_logger.error('service %s missing', service)
                return

            uids = set()
            for f in filters:
                k = f['key']
                v = f['value']
                if k not in subs[service] or v not in subs[service][k]:
                    continue
                for uid in subs[service][k][v]:
                    uids.add(uid)
            
            for uid in uids:
                try:
                    await app['clients'][uid]['ws'].send_str(msg_str)
                except:
                    pass

        elif 'ack' in msg:
            if 'client_id' not in msg:
                log.server_logger.error('handle_service_provider_message: client_id missing')
                return
            uid = msg['client_id']
            if uid not in app['clients']:
                return
            del msg['client_id']
            try:
                await app['clients'][uid]['ws'].send_str(json.dumps(msg))
            except:
                pass
    except Exception as e:
        log.server_logger.exception('handle_service_provider_message: uncaught exception=%s', e)

async def destroy_service_provider(app, ws_id):
    keys = []
    for k, v in app[SRV_PROVIDERS].items():
        if v['ws'].id == ws_id:
            if not v['ws'].closed:
                try:
                    await v['ws'].close()
                except:
                    pass
            keys.append(k)
    for k in keys:
        del app[SRV_PROVIDERS][k]

async def connect_to_service_provider(task):
    global APP
    if 'active' in task and task['active']:
        return
    task['active'] = True
    desc = task['desc']

    ws_id = ''
    try:
        session = ClientSession()
        async with session.ws_connect(task['url'], heartbeat=30) as ws:
            ws_id = ws.id = str(uuid.uuid4())
            async for msg in ws:
                if msg.type == WSMsgType.TEXT:
                    if msg.data == 'close':
                        await ws.close()
                    else:
                        await handle_service_provider_message(APP, msg.data, ws)
                elif msg.type == WSMsgType.CLOSE:
                    log.server_logger.info('%s connection closed normally', desc)
                    break
                elif msg.type == WSMsgType.ERROR:
                    log.server_logger.info('%s connection closed with error %s', desc, ws.exception())
                    break
        
        log.server_logger.info('%s connection closed normally', desc)
    except Exception as e:
        log.server_logger.exception('%s connection closed with exception: %s', desc, e)
    except:
        log.server_logger.exception('%s uncaught exception!', desc)
    finally:
        if not session.closed:
            try:
                await session.close()
            except:
                pass

        if ws_id:
            await destroy_service_provider(APP, ws_id)
        task['active'] = False

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
            return web.json_response(r.app['subscriptions'], dumps=DEBUG_DUMPS)
        elif query == 'nodes':
            return web.json_response(r.app['nodes'], dumps=DEBUG_DUMPS)
        elif query == 'clients':
            return web.json_response(r.app['clients'], dumps=DEBUG_DUMPS)
        elif query == SRV_PROVIDERS:
            return web.json_response(r.app[SRV_PROVIDERS], dumps=DEBUG_DUMPS)
        elif query == SRV_SUBS:
            return web.json_response(r.app[SRV_SUBS], dumps=DEBUG_DUMPS)
        else:
            pass
        return web.HTTPOk()
    except Exception:
        log.server_logger.exception('exception in debug request')
        return web.HTTPInternalServerError(reason="Something went wrong %s" % str(sys.exc_info()))

async def init_app():
    global GS
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
    app[SRV_SUBS] = {}
    app[SRV_PROVIDERS] = {}

    app.add_routes([web.get('/', client_handler)]) # All client WS requests
    app.add_routes([web.get(f'/callback/{CALLBACK_TOKEN}', node_handler)]) # ws/wss callback from nodes
    app.add_routes([web.post('/debug', debug_handler)]) # local debug interface

    return app

APP = LOOP.run_until_complete(init_app())

async def periodic(period, fn, *args):
    while True:
        begin = time.time()
        try:
            await fn(*args)
        except Exception as e:
            log.server_logger.error(f'periodic exception={str(e)}')
            traceback.print_tb(e.__traceback__)
        elapsed = time.time() - begin
        await asyncio.sleep(period - elapsed)

def main():
    global APP, GS
    # Start web/ws server
    async def start():
        runner = web.AppRunner(APP)
        await runner.setup()
        site = web.TCPSite(runner, LISTEN_HOST, LISTEN_PORT)
        await site.start()

    async def end():
        for task in GS['tasks']:
            task['inst'].cancel()
        await APP.shutdown()

    LOOP.run_until_complete(start())

    # Main program
    try:
        for task in GS['tasks']:
            task['inst'] = LOOP.create_task(periodic(5, connect_to_service_provider, task))
        LOOP.run_forever()
    except KeyboardInterrupt:
        pass
    finally:
        LOOP.run_until_complete(end())

    LOOP.close()

if __name__ == "__main__":
    main()

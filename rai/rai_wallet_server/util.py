from aiohttp import web

class Util:

    def __init__(self):
        pass

    def get_request_ip(self, r : web.Request) -> str:
        host = r.headers.get('CF-Connecting-IP', None) #X-FORWARDED-FOR not safe
        if host is None:
            peername = r.transport.get_extra_info('peername')
            if peername is not None:
                host, _ = peername
        return host

    def get_connecting_ip(self, r : web.Request):
        peername = r.transport.get_extra_info('peername')
        if peername is not None:
            host, _ = peername
            return host
        return None
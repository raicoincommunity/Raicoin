from aiohttp import web

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
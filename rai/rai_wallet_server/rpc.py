import json
import sys
import time

from aiohttp import ClientSession, log, web

class Rpc:
    def __init__(self, node_url : str):
        self.node_url = node_url

    async def json_post(self, request_json : dict, timeout : int = 30) -> dict:
        try:
            async with ClientSession() as session:
                async with session.post(self.node_url, json=request_json, timeout=timeout) as resp:
                    return await resp.json(content_type=None)
        except Exception:
            log.server_logger.exception()
            return None


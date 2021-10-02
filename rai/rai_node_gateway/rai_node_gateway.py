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


from logging.handlers import TimedRotatingFileHandler, WatchedFileHandler
from aiohttp import ClientSession, WSMessage, WSMsgType, log, web
from functools import partial

ALLOWED_RPC_ACTIONS = [
    'account_heads', 'active_account_heads', 'block_confirm', 'blocks_query', 'event_subscribe'
]

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
import struct

from typing import Tuple, Union, Callable
from logging.handlers import TimedRotatingFileHandler, WatchedFileHandler
from aiohttp import ClientSession, WSMessage, WSMsgType, log, web, ClientWebSocketResponse
from web3 import Web3, Account as EvmAccount
from web3.middleware import geth_poa_middleware, construct_sign_and_send_raw_middleware
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from enum import IntEnum, Enum
from getpass import getpass
from hashlib import blake2b
from io import BytesIO
from eth_account._utils.structured_data.hashing import (
    hash_domain,
    hash_message as hash_eip712_message
)
from eth_account.messages import SignableMessage
from eth_account._utils.structured_data.validation import validate_structured_data
from collections import OrderedDict

ALLOWED_RPC_ACTIONS = [
    'service_subscribe', 'chain_info', 'sign_transfer', 'sign_creation', 'token_symbol',
    'token_name', 'token_type', 'token_decimals'
]

CLIENTS = {}

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

ERC20_ABI_FILE = os.getenv('EVM_CHAIN_CORE_ABI_FILE', 'erc20_abi.json')
ERC20_ABI = None
with open(ERC20_ABI_FILE, 'r') as f:
    ERC20_ABI = json.load(f)
if not ERC20_ABI:
    print(f'Failed to read abi file:{ERC20_ABI_FILE}')
    sys.exit(0)

ERC721_ABI_FILE = os.getenv('EVM_CHAIN_CORE_ABI_FILE', 'erc721_abi.json')
ERC721_ABI = None
with open(ERC721_ABI_FILE, 'r') as f:
    ERC721_ABI = json.load(f)
if not ERC721_ABI:
    print(f'Failed to read abi file:{ERC721_ABI_FILE}')
    sys.exit(0)

EVM_CHAIN_CORE_ABI_FILE = os.getenv('EVM_CHAIN_CORE_ABI_FILE', 'evm_chain_core_abi.json')
EVM_CHAIN_CORE_ABI = None
with open(EVM_CHAIN_CORE_ABI_FILE) as f:
    EVM_CHAIN_CORE_ABI = json.load(f)
if not EVM_CHAIN_CORE_ABI:
    print(f'Failed to read abi file:{EVM_CHAIN_CORE_ABI_FILE}')
    sys.exit(0)

EVM_CHAIN_VALIDATOR_ABI_FILE = os.getenv('EVM_CHAIN_VALIDATOR_ABI_FILE', 'evm_chain_validator_abi.json')
EVM_CHAIN_VALIDATOR_ABI = None
with open(EVM_CHAIN_VALIDATOR_ABI_FILE) as f:
    EVM_CHAIN_VALIDATOR_ABI = json.load(f)
if not EVM_CHAIN_VALIDATOR_ABI:
    print(f'Failed to read abi file:{EVM_CHAIN_VALIDATOR_ABI_FILE}')
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

    def uint256_to_hex(self, value: int) -> str:
        return f'{value:#066x}'

    def uint256_to_bytes(self, value: int) -> bytes:
        return bytes.fromhex(self.uint256_to_hex(value)[2:])

    def hex_to_bytes(self, hex_str: str) -> bytes:
        if hex_str.startswith('0x'):
            hex_str = hex_str[2:]
        return bytes.fromhex(hex_str)
    
    def bytes_to_hex(value: bytes) -> str:
        return f'0x{value.hex()}'.lower()

UTIL = Util(USE_CLOUDFLARE, USE_NGINX)

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

class CallbackHelper:
    def __init__(self):
        self.__data = OrderedDict()

    def add(self, callback) -> int:
        key = secrets.randbits(256)
        data = {'callback': callback, 'time': time.time()}
        self.__data[key] = data
        return key

    async def callback(self, key: int, *args, **kwargs):
        if key not in self.__data:
            return
        await self.__data[key]['callback'](args, kwargs)
        del self.__data[key]

    async def __call__(self):
        while True:
            key = next(iter(self.__data), None)
            if key is None:
                break
            if self.__data[key]['time'] + 10 > time.time():
                break
            del self.__data[key]
CALLBACK_HELPER = CallbackHelper()

class Serializer:
    @staticmethod
    def read_bool(stream: BytesIO) -> Union[None, bool]:
        u8 = Serializer.read_uint8(stream)
        if u8 == None or u8 > 1:
            return None
        return u8 == 1

    @staticmethod
    def read_uint8(stream: BytesIO) -> Union[None, int]:
        b = stream.read(1)
        if len(b) < 1:
            return None
        return struct.unpack(">B", b)[0]

    @staticmethod
    def read_uint16(stream: BytesIO) -> Union[None, int]:
        b = stream.read(2)
        if len(b) < 2:
            return None
        return struct.unpack('>H', b)[0]
    
    @staticmethod
    def read_uint32(stream: BytesIO) -> Union[None, int]:
        b = stream.read(4)
        if len(b) < 4:
            return None
        return struct.unpack('>I', b)[0]
    
    @staticmethod
    def read_uint64(stream: BytesIO) -> Union[None, int]:
        b = stream.read(8)
        if len(b) < 8:
            return None
        return struct.unpack('>Q', b)[0]

    @staticmethod
    def read_uint128(stream: BytesIO) -> Union[None, int]:
        b = stream.read(16)
        if len(b) < 16:
            return None
        u = struct.unpack('>QQ', b)
        return u[0] * (2 ** 64) + u[1]

    @staticmethod
    def read_uint256(stream: BytesIO) -> Union[None, int]:
        b = stream.read(32)
        if len(b) < 32:
            return None
        u = struct.unpack('>QQQQ', b)
        return u[0] * (2 ** 192) + u[1] * (2 ** 128) + u[2] * (2 ** 64) + u[3]

    @staticmethod
    def read_string(stream: BytesIO) -> Union[None, str]:
        length = Serializer.read_uint16(stream)
        if length == None:
            return None
        if length == 0:
            return ''
        b = stream.read(length)
        if len(b) < length:
            return None
        try:
            return b.decode('utf-8')
        except:
            return None

    @staticmethod
    def read_bytes(stream: BytesIO) -> Union[None, bytes]:
        length = Serializer.read_uint16(stream)
        if length == None:
            return None
        if length == 0:
            return b''
        b = stream.read(length)
        if len(b) < length:
            return None
        return b

    @staticmethod
    def write_bool(stream: BytesIO, b: bool):
        if b:
            Serializer.write_uint8(stream, 1)
        else:
            Serializer.write_uint8(stream, 0)

    @staticmethod
    def write_uint8(stream: BytesIO, value: int) -> None:
        if value >= 2 ** 8:
            raise ValueError("value out of range")
        stream.write(struct.pack(">B", value))

    @staticmethod
    def write_uint16(stream: BytesIO, value: int):
        if value >= 2 ** 16:
            raise ValueError("value too large")
        stream.write(struct.pack('>H', value))

    @staticmethod
    def write_uint32(stream: BytesIO, value: int):
        if value >= 2 ** 32:
            raise ValueError("value too large")
        stream.write(struct.pack('>I', value))
    
    @staticmethod
    def write_uint64(stream: BytesIO, value: int):
        if value >= 2 ** 64:
            raise ValueError("value too large")
        stream.write(struct.pack('>Q', value))

    @staticmethod
    def write_uint128(stream: BytesIO, value: int):
        if value >= 2 ** 128:
            raise ValueError("value too large")
        stream.write(struct.pack('>QQ', value >> 64, value & 0xffffffffffffffff))

    @staticmethod
    def write_uint256(stream: BytesIO, value: Union[int, str]):
        if isinstance(value, str):
            if value.startswith('rai_'):
                error, value, _ = ACCOUNT.decode(value)
                if error:
                    raise ValueError("invalid rai account")
            value = int(value, 16)
        if value >= 2 ** 256:
            raise ValueError("value too large")
        stream.write(struct.pack('>QQQQ', value >> 192, value >> 128 & 0xffffffffffffffff,
            value >> 64 & 0xffffffffffffffff, value & 0xffffffffffffffff))

    @staticmethod
    def write_string(stream: BytesIO, value: str):
        if value == '':
            Serializer.write_uint16(stream, 0)
            return
        try:
            b = value.encode('utf-8')
        except:
            raise ValueError("invalid string")
        Serializer.write_uint16(stream, len(b))
        stream.write(b)

    @staticmethod
    def write_bytes(stream: BytesIO, value: Union[bytes, str]):
        if isinstance(value, str):
            value = value[2:] if value.startswith("0x") else value
            value = bytes.fromhex(value)
        if value == b'':
            Serializer.write_uint16(stream, 0)
            return
        Serializer.write_uint16(stream, len(value))
        stream.write(value)

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

class CrossChainMessage:
    def __init__(self, source: str, destination: str, chain_id: ChainId,
        source_signer: str):
        self.source = source
        self.destination = destination
        self.chain_id = chain_id
        self.source_signer = source_signer
    
    def serialize(self) -> str:
        raise NotImplementedError
    def deserialize(self, payload: str):
        raise NotImplementedError

class WeightSignMessage(CrossChainMessage):
    def __init__(self, source: str, destination: str, chain_id: ChainId = ChainId.INVALID,
        source_signer: str = '', is_request: bool = True, validator: str = '', signer: str = '',
        weight: int = 0, epoch: int = 0, signature: bytes = b''):
        super().__init__(source, destination, chain_id, source_signer)
        self.is_request = is_request
        self.validator = validator
        self.signer = signer
        self.weight = weight
        self.epoch = epoch
        self.signature = signature

    def serialize(self, stream: BytesIO) -> str:
        Serializer.write_uint8(stream, CrossChainMessageType.WeightSign)
        Serializer.write_bool(stream, self.is_request)
        Serializer.write_uint256(stream, self.validator)
        Serializer.write_uint256(stream, self.signer)
        Serializer.write_uint128(stream, self.weight)
        Serializer.write_uint32(stream, self.epoch)
        if not self.is_request:
            Serializer.write_bytes(stream, self.signature)

    def deserialize(self, stream: BytesIO) -> bool:
        msg_type = Serializer.read_uint8(stream)
        if msg_type != CrossChainMessageType.WeightSign:
            return True
        self.is_request = Serializer.read_bool(stream)
        if self.is_request == None:
            return True
        validator = Serializer.read_uint256(stream)
        if validator == None:
            return True
        self.validator = UTIL.uint256_to_hex(validator)
        signer = Serializer.read_uint256(stream)
        if signer == None:
            return True
        self.signer = UTIL.uint256_to_hex(signer)
        self.weight = Serializer.read_uint128(stream)
        if self.weight == None:
            return True
        self.epoch = Serializer.read_uint32(stream)
        if self.epoch == None:
            return True
        if not self.is_request:
            self.signature = Serializer.read_bytes(stream)
            if self.signature == None:
                return True
        return False

class CrossChainOp(str, Enum):
    MAP = 'map'
    UNMAP = 'unmap'
    WRAP = 'wrap'
    UNWRAP = 'unwrap'

class CrossChainOpcode(IntEnum):
    MAP = 1
    UNMAP = 2
    WRAP = 3
    UNWRAP = 4

class TransferSignMessage(CrossChainMessage):
    def __init__(self, source: str, destination: str, chain_id: ChainId = ChainId.INVALID,
        source_signer: str = '', is_request: bool = True, account: str = '', height: int = 0,
        opcode: CrossChainOpcode = 0, request_id: int = 0, signature: bytes = b''):
        super().__init__(source, destination, chain_id, source_signer)
        self.is_request = is_request
        self.account = account
        self.height = height
        self.opcode = opcode
        self.request_id = request_id
        self.signature = signature

    def serialize(self, stream: BytesIO) -> str:
        Serializer.write_uint8(stream, CrossChainMessageType.TransferSign)
        Serializer.write_bool(stream, self.is_request)
        Serializer.write_uint256(stream, self.account)
        Serializer.write_uint64(stream, self.height)
        Serializer.write_uin8(stream, int(self.opcode))
        Serializer.write_uint256(stream, self.request_id)
        if not self.is_request:
            Serializer.write_bytes(stream, self.signature)

    def deserialize(self, stream: BytesIO) -> bool:
        msg_type = Serializer.read_uint8(stream)
        if msg_type != CrossChainMessageType.TransferSign:
            return True
        self.is_request = Serializer.read_bool(stream)
        if self.is_request == None:
            return True
        account = Serializer.read_uint256(stream)
        if account == None:
            return True
        error, self.account = ACCOUNT.encode(UTIL.uint256_to_hex(account))
        if error:
            return True
        self.height = Serializer.read_uint64(stream)
        if self.height == None:
            return True
        opcode = Serializer.read_uint8(stream)
        if opcode == None:
            return True
        try:
            self.opcode = CrossChainOpcode(opcode)
        except:
            return True
        self.request_id = Serializer.read_uint256(stream)
        if self.request_id == None:
            return True
        if not self.is_request:
            self.signature = Serializer.read_bytes(stream)
            if self.signature == None:
                return True
        return False

class CreationSignMessage(CrossChainMessage):
    def __init__(self, source: str, destination: str, chain_id: ChainId = ChainId.INVALID,
        source_signer: str = '', is_request: bool = True, original_chain_id: int = 0,
        original_contract: str = '', request_id: int = 0, signature: bytes = b''):
        super().__init__(source, destination, chain_id, source_signer)
        self.is_request = is_request
        self.original_chain_id = original_chain_id
        self.original_contract = original_contract
        self.request_id = request_id
        self.signature = signature

    def serialize(self, stream: BytesIO) -> str:
        Serializer.write_uint8(stream, CrossChainMessageType.CreationSign)
        Serializer.write_bool(stream, self.is_request)
        Serializer.write_uint32(stream, self.original_chain_id)
        Serializer.write_uint256(stream, self.original_contract)
        Serializer.write_uint256(stream, self.request_id)
        if not self.is_request:
            Serializer.write_bytes(stream, self.signature)

    def deserialize(self, stream: BytesIO) -> bool:
        msg_type = Serializer.read_uint8(stream)
        if msg_type != CrossChainMessageType.CreationSign:
            return True
        self.is_request = Serializer.read_bool(stream)
        if self.is_request == None:
            return True
        self.original_chain_id = Serializer.read_uint32(stream)
        if self.original_chain_id == None:
            return True
        self.original_contract = Serializer.read_uint256(stream)
        if self.original_contract == None:
            return True
        self.request_id = Serializer.read_uint256(stream)
        if self.request_id == None:
            return True
        if not self.is_request:
            self.signature = Serializer.read_bytes(stream)
            if self.signature == None:
                return True
        return False

class CrossChainMessageType(IntEnum):
    WeightSign = 1
    TransferSign = 2
    CreationSign = 3

async def send_cross_chain_message(data: CrossChainMessage):
    stream = BytesIO()
    data.serialize(stream)
    stream.seek(0)
    payload = stream.read()

    message = {
        'action': 'cross_chain',
        'source': data.source,
        'destination': data.destination,
        'chain_id': str(int(data.chain_id)),
        'payload': payload.hex(),
    }
    await send_to_node(message)

class EvmChainValidator:
    def __init__(self, chain_id: ChainId, evm_chain_id: EvmChainId, core_contract: str,
                 validator_contract: str, core_abi: dict, validator_abi: dict, endpoints: list,
                 signer_private_key: str, confirmations: int, tag: str):
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
        self.tag = tag
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
        self.total_weight = self.__total_weight = None
        self.validator_activities = {} # validator -> ValidatorActivity

        self.bound_signer = None
        self.binding_status_synced = False

        self.last_submit = None
        self.submission_interval = 300
        self.submission_state = self.SubmissionState.IDLE
        self.submission_epoch = 0
        self.weights = {} # WeightInfo
        self.submission_weight = None
        self.weight_query_count = 0
        self.signatures = {}
        self.signature_collect_count = 0

        self.token_symbols = {}
        self.token_names = {}
        self.token_types = {}
        self.token_decimals = {}

        if len(self.endpoints) == 0:
            print("[ERROR] No endpoints found while constructing EvmChainValidator")
            sys.exit(0)

    def to_checksum_address(self, address: Union[str, int]) -> str:
        if isinstance(address, int):
            address = UTIL.uint256_to_hex(address)
        if len(address) > 42:
            if address.startswith('0x'):
                address = address[2:]
            if len(address) != 64 or not address.startswith('000000000000000000000000'):
                raise ValueError("Invalid address")
            address = '0x' + address[24:]
        return Web3.toChecksumAddress(address)

    async def process_bind_query_ack(self, validator: str, signer: str):
        if NODE == None:
            return
        if validator != NODE['account_hex']:
            return
        self.bound_signer = signer
        self.binding_status_synced = True

    async def process_weight_query_ack(self, validator: str, epoch: int, weight: int, replier: str):
        if validator != NODE['account_hex']:
            return
        if self.submission_state != self.SubmissionState.WEIGHT_QUERY:
            return
        if epoch != self.submission_epoch:
            return
        threshold = self.weight_threshold(0.99)
        replier_weight = self.weight_of_validator(replier)
        if replier_weight < threshold:
            return True
        self.weights[replier] = self.WeightInfo(replier, epoch, weight)

    async def process_weight_sign_message(self, message: WeightSignMessage):
        global NODE
        if NODE == None:
            return
        if message.is_request:
            if self.signer == None:
                return
            if message.source_signer != message.signer:
                return
            if message.epoch != NODE['snapshot']['epoch']:
                return
            snapshot_weight = node_snapshot_weight(message.validator)
            if snapshot_weight == None or message.weight > snapshot_weight:
                return
            signature = self.signSubmitValidator(message.validator, message.signer,
                message.weight, message.epoch)
            response = WeightSignMessage(NODE['account_hex'], i.source, self.chain_id, '', False,
                message.validator, message.signer, message.weight, message.epoch, signature)
            await send_cross_chain_message(response)
        else:
            if self.submission_state != self.SubmissionState.COLLECT_SIGNATURES:
                return
            if self.submission_epoch != message.epoch or self.submission_weight != message.weight:
                return
            if self.check_validator(message.validator, message.signer):
                return
            if self.verifySubmitValidator(message.validator, message.signer, message.weight,
                message.epoch, message.signature):
                return
            self.signatures[message.validator] = self.SignatureInfo(message.validator, message.signer,
                message.epoch, message.signature)

    def is_native_token(self, address: Union[str, int]) -> bool:
        if isinstance(address, str):
            address = int(address, 16)
        return address == 1

    async def process_token_service_transfer_query_ack(self, sign: TransferSignMessage, ack: dict):
        if 'error' in ack:
            return
        token = ack['token']
        error, account, _ = ACCOUNT.decode(ack['account'])
        height = int(ack['height'])
        if error:
            log.server_logger.error('failed to decode account;%s', ack['account'])
            return
        signature = None
        if sign.opcode == CrossChainOpcode.UNMAP and ack['ack'] == 'token_unmap_info':
            if self.chain_id != int(token['chain_id']):
                return
            to = self.to_checksum_address(ack['to'])
            value = int(ack['value'])
            txn_hash = ack['source_transaction']
            if self.is_native_token(token['address_raw']):
                signature = self.signUnmapETH(account, to, txn_hash, height, value)
            elif token['type'] == '20':
                signature = self.signUnmapERC20(account, to, txn_hash, height, value)
            elif token['type'] == '721':
                signature = self.signUnmapERC721(account, to, txn_hash, height, value)
            else:
                return
        elif sign.opcode == CrossChainOpcode.WRAP and ack['ack'] == 'token_wrap_info':
            if self.chain_id != int(ack['to_chain_id']):
                return
            token_chain_id = int(token['chain_id'])
            token_address = token['address_raw']
            to = self.to_checksum_address(ack['to_account'])
            value = int(ack['value'])
            txn_hash = ack['source_transaction']
            if token['type'] == '20':
                signature = self.signWrapERC20Token(token_chain_id, token_address, account, to,
                    txn_hash, height, value)
            elif token['type'] == '721':
                signature = self.signWrapERC721Token(token_chain_id, token_address, account, to,
                    txn_hash, height, value)
            else:
                return
        else:
            return
        if signature == None:
            return
        response = TransferSignMessage(NODE['account_hex'], sign.source, self.chain_id, '', False,
            sign.account, sign.height, sign.opcode, sign.request_id, signature)
        await send_cross_chain_message(response)

    async def process_transfer_sign_message(self, message: TransferSignMessage):
        global NODE
        if NODE == None:
            return
        if message.is_request:
            async def callback(ack: dict):
                await self.process_token_service_transfer_query_ack(message, ack)
            query = {
                'account': message.account,
                'height': str(message.height),
            }
            if message.opcode == CrossChainOpcode.UNMAP:
                query['action'] = 'token_unmap_info'
            elif message.opcode == CrossChainOpcode.WRAP:
                query['action'] = 'token_wrap_info'
            else:
                log.server_logger.error('invalid transfer sign message;%s', message)
                return
            await send_to_token_service(query, callback)
        else:
            await CALLBACK_HELPER.callback(message.request_id, message)

    async def process_token_service_token_info_query_ack(self, sign: CreationSignMessage, ack: dict):
        if 'error' in ack or ack['ack'] != 'token_info':
            return
        if ack['type'] not in ('20', '721'):
            log.server_logger.error('process_token_service_token_info_query_ack unknown type:%s',
                ack['type'])
            return
        signature = None
        address = self.to_checksum_address(ack['address_raw'])
        original_chain = to_chain_str(ChainId(sign.original_chain_id)).value
        result = self.token_symbol(None, {'address': address})
        if 'symbol' not in result:
            return
        symbol = result['symbol']
        name = f'RAI wrapped {symbol}<{self.tag}>'
        symbol = 'r' + symbol
        if ack['type'] == '20':
            signature = self.signCreateWrappedERC20Token(name, symbol, original_chain,
                sign.original_chain_id, sign.original_contract, int(ack['decimals']))
        elif ack['type'] == '721':
            signature = self.signCreateWrappedERC721Token(name, symbol, original_chain,
                sign.original_chain_id, sign.original_contract)
        else:
            return
        if signature == None:
            return
        response = CreationSignMessage(NODE['account_hex'], sign.source, self.chain_id, '', False,
            sign.original_chain_id, sign.original_contract, sign.request_id, signature)
        await send_cross_chain_message(response)

    async def process_creation_sign_message(self, message: CreationSignMessage):
        global NODE
        if NODE == None:
            return
        if message.is_request:
            if self.chain_id == message.original_chain_id:
                return
            async def callback(ack: dict):
                await self.process_token_service_token_info_query_ack(message, ack)
            address = message.original_contract
            if address.startswith('0x'):
                address = address[2:]
            query = {
                'chain_id': str(int(message.original_chain_id)),
                'address_raw': address,
            }
            query['action'] = 'token_info'
            await send_to_token_service(query, callback)
        else:
            await CALLBACK_HELPER.callback(message.request_id, message)

    async def process_cross_chain_message(self, message: Union[WeightSignMessage, None]):
        if isinstance(message, WeightSignMessage):
            await self.process_weight_sign_message(message)
        elif isinstance(message, TransferSignMessage):
            await self.process_transfer_sign_message(message)
        elif isinstance(message, CreationSignMessage):
            await self.process_creation_sign_message(message)
        else:
            pass

    def encode_structured_data(self, data: dict) -> SignableMessage:
        validate_structured_data(data)
        structured_data = data
        return SignableMessage(
            b'\x01',
            hash_domain(structured_data),
            hash_eip712_message(structured_data),
        )

    def eip721_base(self, contract: str):
        return {
            "types": {
                "EIP712Domain": [
                    {"name": "name", "type": "string"},
                    {"name": "version", "type": "string"},
                    {"name": "chainId", "type": "uint256"},
                    {"name": "verifyingContract", "type": "address"},
                ],
            },
            "domain": {
                "name": "Raicoin",
                "version": "1.0",
                "chainId": self.evm_chain_id,
                "verifyingContract": contract,
            },
        }

    def eip721_submit_validator(self, validator: str, signer: str, weight: int, epoch: int) -> SignableMessage:
        data = self.eip721_base(self.validator_contract)
        data['types']['SubmitValidator'] = [
            {"name": "validator", "type": "bytes32"},
            {"name": "signer", "type": "address"},
            {"name": "weight", "type": "uint256"},
            {"name": "epoch", "type": "uint32"},
        ]
        data['primaryType'] = 'SubmitValidator'
        data['message'] = {
            'validator': UTIL.hex_to_bytes(validator),
            'signer': signer,
            'weight': weight,
            'epoch': epoch,
        }
        return self.encode_structured_data(data)

    def signSubmitValidator(self, validator: str, signer: str, weight: int, epoch: int) -> bytes:
        signable = self.eip721_submit_validator(validator, signer, weight, epoch)
        signed = self._account.sign_message(signable)
        return UTIL.uint256_to_bytes(signed.r) + UTIL.uint256_to_bytes(signed.s) + bytes((signed.v, ))

    def verifySubmitValidator(self, validator: str, signer: str, weight: int, epoch: int,
        signature: bytes) -> bool:
        if len(signature) != 65:
            return True
        signable = self.eip721_submit_validator(validator, signer, weight, epoch)
        return self._account.recover_message(signable, signature) != self.to_checksum_address(signer)

    def eip721_unmap_erc20(self, token: str, sender: str, recipient: str, txn_hash: str,
        txn_height: int, share: int) -> SignableMessage:
        data = self.eip721_base(self.core_contract)
        data['types']['UnmapERC20'] = [
            {"name": "token", "type": "address"},
            {"name": "sender", "type": "bytes32"},
            {"name": "recipient", "type": "address"},
            {"name": "txnHash", "type": "bytes32"},
            {"name": "txnHeight", "type": "uint64"},
            {"name": "share", "type": "uint256"},
        ]
        data['primaryType'] = 'UnmapERC20'
        data['message'] = {
            'token': token,
            'sender': UTIL.hex_to_bytes(sender),
            'recipient': recipient,
            'txnHash': UTIL.hex_to_bytes(txn_hash),
            'txnHeight': txn_height,
            'share': share,
        }
        return self.encode_structured_data(data)

    def signUnmapERC20(self, token: str, sender: str, recipient: str, txn_hash: str,
        txn_height: int, share: int):
        signable = self.eip721_unmap_erc20(token, sender, recipient, txn_hash, txn_height, share)
        signed = self._account.sign_message(signable)
        return UTIL.uint256_to_bytes(signed.r) + UTIL.uint256_to_bytes(signed.s) + bytes((signed.v, ))

    def eip721_unmap_erc721(self, token: str, sender: str, recipient: str, txn_hash: str,
        txn_height: int, token_id: int) -> SignableMessage:
        data = self.eip721_base(self.core_contract)
        data['types']['UnmapERC721'] = [
            {"name": "token", "type": "address"},
            {"name": "sender", "type": "bytes32"},
            {"name": "recipient", "type": "address"},
            {"name": "txnHash", "type": "bytes32"},
            {"name": "txnHeight", "type": "uint64"},
            {"name": "tokenId", "type": "uint256"},
        ]
        data['primaryType'] = 'UnmapERC721'
        data['message'] = {
            'token': token,
            'sender': UTIL.hex_to_bytes(sender),
            'recipient': recipient,
            'txnHash': UTIL.hex_to_bytes(txn_hash),
            'txnHeight': txn_height,
            'tokenId': token_id,
        }
        return self.encode_structured_data(data)

    def signUnmapERC721(self, token: str, sender: str, recipient: str, txn_hash: str,
        txn_height: int, token_id: int):
        signable = self.eip721_unmap_erc721(token, sender, recipient, txn_hash, txn_height, token_id)
        signed = self._account.sign_message(signable)
        return UTIL.uint256_to_bytes(signed.r) + UTIL.uint256_to_bytes(signed.s) + bytes((signed.v, ))

    def eip721_unmap_eth(self, sender: str, recipient: str, txn_hash: str, txn_height: int,
        amount: int) -> SignableMessage:
        data = self.eip721_base(self.core_contract)
        data['types']['UnmapETH'] = [
            {"name": "sender", "type": "bytes32"},
            {"name": "recipient", "type": "address"},
            {"name": "txnHash", "type": "bytes32"},
            {"name": "txnHeight", "type": "uint64"},
            {"name": "amount", "type": "uint256"},
        ]
        data['primaryType'] = 'UnmapETH'
        data['message'] = {
            'sender': UTIL.hex_to_bytes(sender),
            'recipient': recipient,
            'txnHash': UTIL.hex_to_bytes(txn_hash),
            'txnHeight': txn_height,
            'amount': amount,
        }
        return self.encode_structured_data(data)

    def signUnmapETH(self, sender: str, recipient: str, txn_hash: str, txn_height: int,
        amount: int):
        signable = self.eip721_unmap_eth(sender, recipient, txn_hash, txn_height, amount)
        signed = self._account.sign_message(signable)
        return UTIL.uint256_to_bytes(signed.r) + UTIL.uint256_to_bytes(signed.s) + bytes((signed.v, ))

    def eip721_create_wrapped_erc20(self, name: str, symbol: str, original_chain: str,
        original_chain_id: int, original_contract: str, decimals: int):
        data = self.eip721_base(self.core_contract)
        data['types']['CreateWrappedERC20Token'] = [
            {"name": "name", "type": "string"},
            {"name": "symbol", "type": "string"},
            {"name": "originalChain", "type": "string"},
            {"name": "originalChainId", "type": "uint32"},
            {"name": "originalContract", "type": "bytes32"},
            {"name": "decimals", "type": "uint8"},
        ]
        data['primaryType'] = 'CreateWrappedERC20Token'
        data['message'] = {
            'name': name,
            'symbol': symbol,
            'originalChain': original_chain,
            'originalChainId': original_chain_id,
            'originalContract': UTIL.hex_to_bytes(original_contract),
            'decimals': decimals,
        }
        return self.encode_structured_data(data)

    def signCreateWrappedERC20Token(self, name: str, symbol: str, original_chain: str,
        original_chain_id: int, original_contract: str, decimals: int):
        signable = self.eip721_create_wrapped_erc20(name, symbol, original_chain, original_chain_id,
            original_contract, decimals)
        signed = self._account.sign_message(signable)
        return UTIL.uint256_to_bytes(signed.r) + UTIL.uint256_to_bytes(signed.s) + bytes((signed.v, ))

    def eip721_create_wrapped_erc721(self, name: str, symbol: str, original_chain: str,
        original_chain_id: int, original_contract: str):
        data = self.eip721_base(self.core_contract)
        data['types']['CreateWrappedERC721Token'] = [
            {"name": "name", "type": "string"},
            {"name": "symbol", "type": "string"},
            {"name": "originalChain", "type": "string"},
            {"name": "originalChainId", "type": "uint32"},
            {"name": "originalContract", "type": "bytes32"},
        ]
        data['primaryType'] = 'CreateWrappedERC721Token'
        data['message'] = {
            'name': name,
            'symbol': symbol,
            'originalChain': original_chain,
            'originalChainId': original_chain_id,
            'originalContract': UTIL.hex_to_bytes(original_contract),
        }
        return self.encode_structured_data(data)

    def signCreateWrappedERC721Token(self, name: str, symbol: str, original_chain: str,
        original_chain_id: int, original_contract: str):
        signable = self.eip721_create_wrapped_erc721(name, symbol, original_chain, original_chain_id,
            original_contract)
        signed = self._account.sign_message(signable)
        return UTIL.uint256_to_bytes(signed.r) + UTIL.uint256_to_bytes(signed.s) + bytes((signed.v, ))

    def eip721_wrap_erc20(self, original_chain_id: int, original_contract: str, sender: str,
        recipient: str, txn_hash: str, txn_height: int, amount: int):
        data = self.eip721_base(self.core_contract)
        data['types']['WrapERC20Token'] = [
            {"name": "originalChainId", "type": "uint32"},
            {"name": "originalContract", "type": "bytes32"},
            {"name": "sender", "type": "bytes32"},
            {"name": "recipient", "type": "address"},
            {"name": "txnHash", "type": "bytes32"},
            {"name": "txnHeight", "type": "uint64"},
            {"name": "amount", "type": "uint256"},
        ]
        data['primaryType'] = 'WrapERC20Token'
        data['message'] = {
            'originalChainId': original_chain_id,
            'originalContract': UTIL.hex_to_bytes(original_contract),
            'sender': UTIL.hex_to_bytes(sender),
            'recipient': recipient,
            'txnHash': UTIL.hex_to_bytes(txn_hash),
            'txnHeight': txn_height,
            'amount': amount,
        }
        return self.encode_structured_data(data)

    def signWrapERC20Token(self, original_chain_id: int, original_contract: str, sender: str,
        recipient: str, txn_hash: str, txn_height: int, amount: int):
        signable = self.eip721_wrap_erc20(original_chain_id, original_contract, sender, recipient,
            txn_hash, txn_height, amount)
        signed = self._account.sign_message(signable)
        return UTIL.uint256_to_bytes(signed.r) + UTIL.uint256_to_bytes(signed.s) + bytes((signed.v, ))

    def eip721_wrap_erc721(self, original_chain_id: int, original_contract: str, sender: str,
        recipient: str, txn_hash: str, txn_height: int, token_id: int):
        data = self.eip721_base(self.core_contract)
        data['types']['WrapERC721Token'] = [
            {"name": "originalChainId", "type": "uint32"},
            {"name": "originalContract", "type": "bytes32"},
            {"name": "sender", "type": "bytes32"},
            {"name": "recipient", "type": "address"},
            {"name": "txnHash", "type": "bytes32"},
            {"name": "txnHeight", "type": "uint64"},
            {"name": "tokenId", "type": "uint256"},
        ]
        data['primaryType'] = 'WrapERC721Token'
        data['message'] = {
            'originalChainId': original_chain_id,
            'originalContract': UTIL.hex_to_bytes(original_contract),
            'sender': UTIL.hex_to_bytes(sender),
            'recipient': recipient,
            'txnHash': UTIL.hex_to_bytes(txn_hash),
            'txnHeight': txn_height,
            'tokenId': token_id,
        }
        return self.encode_structured_data(data)

    def signWrapERC721Token(self, original_chain_id: int, original_contract: str, sender: str,
        recipient: str, txn_hash: str, txn_height: int, token_id: int):
        signable = self.eip721_wrap_erc721(original_chain_id, original_contract, sender, recipient,
            txn_hash, txn_height, token_id)
        signed = self._account.sign_message(signable)
        return UTIL.uint256_to_bytes(signed.r) + UTIL.uint256_to_bytes(signed.s) + bytes((signed.v, ))

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

    class InterfaceId(str, Enum):
        INVALID = '0xffffffff'
        ERC165 = '0x01ffc9a7'
        ERC20 = '0x36372b07'
        ERC721 = '0x80ac58cd'

    class ValidatorActivity:
        def __init__(self, log_height: int, sync_height: int = 0):
            self.log_height = log_height
            self.sync_height = sync_height

    class SubmissionState(IntEnum):
        IDLE = 0
        WEIGHT_QUERY = 1
        COLLECT_SIGNATURES = 2

    class WeightInfo:
        def __init__(self, replier: str, epoch: int, weight: int):
            self.replier = replier
            self.epoch = epoch
            self.weight = weight

    class SignatureInfo:
        def __init__(self, validator: str, signer: str, epoch: int, signature: bytes):
            self.validator = validator
            self.signer = signer
            self.epoch = epoch
            self.signature = signature

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
            self.update_validator_indices()
        return updated

    def update_validator_indices(self):
        indices = {}
        for index, info in enumerate(self.validators):
            indices[info.validator] = index
        self.validator_indices = indices

    def check_validator(self, validator: str, signer: str) -> bool:
        if validator not in self.validator_indices:
            return True
        info = self.validator_indices[validator]
        if info.signer != signer:
            return True
        threshold = self.weight_threshold(0.99)
        if info.weight < threshold:
            return True
        return False

    def to_chain_info(self):
        info = {
            'chain': to_chain_str(self.chain_id).value,
            'chain_id': str(int(self.chain_id)),
            'fee': str(self.fee) if self.fee else '0',
            'total_weight': str(self.total_weight) if self.total_weight != None else '',
            'genesis_validator': self.genesis_validator,
            'genesis_signer': self.genesis_signer,
            'genesis_weight': str(self.genesis_weight()),
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

    @awaitable
    def sumbit_validator(self, validator: str, signer: str, weight: int, epoch: int, reward_to: str,
        signatures: bytes):
        contract = self.make_validator_contract()
        try:
            contract.functions.submitValidator(validator, signer, weight, epoch, reward_to, signatures).transact({
                'from': signer
            })
        except Exception as e:
            log.server_logger.error('sumbit_validator exception:%s', str(e))
            self.use_next_endpoint()

    @awaitable
    def supports_interface(self, address: str, interface: str, block_number='latest') -> Tuple[bool, bool]:
        contract = self.make_erc721_contract(address)
        try:
            return False, contract.functions.supportsInterface(interface).call(block_identifier=block_number)
        except Exception as e:
            log.server_logger.error('supports_interface exception:%s', str(e))
            self.use_next_endpoint()
            return True, False

    @awaitable
    def get_erc20_name(self, address: str, block_number='latest') -> Tuple[bool, str]:
        contract = self.make_erc20_contract(address)
        try:
            return False, contract.functions.name().call(block_identifier=block_number)
        except Exception as e:
            log.server_logger.error('get_name exception:%s', str(e))
            self.use_next_endpoint()
            return True, ''

    @awaitable
    def get_erc20_symbol(self, address: str, block_number='latest') -> Tuple[bool, str]:
        contract = self.make_erc20_contract(address)
        try:
            return False, contract.functions.symbol().call(block_identifier=block_number)
        except Exception as e:
            log.server_logger.error('get_symbol exception:%s', str(e))
            self.use_next_endpoint()
            return True, ''

    @awaitable
    def get_erc20_decimals(self, address: str, block_number='latest') -> Tuple[bool, int]:
        contract = self.make_erc20_contract(address)
        try:
            return False, contract.functions.decimals().call(block_identifier=block_number)
        except Exception as e:
            log.server_logger.error('get_decimals exception:%s', str(e))
            self.use_next_endpoint()
            return True, 0

    @awaitable
    def get_erc20_allowance(self, address: str, owner: str, spender: str, block_number='latest') -> Tuple[bool, int]:
        contract = self.make_erc20_contract(address)
        try:
            return False, contract.functions.allowance(owner, spender).call(block_identifier=block_number)
        except Exception as e:
            log.server_logger.error('get_allowance exception:%s', str(e))
            self.use_next_endpoint()
            return True, 0

    @awaitable
    def get_erc721_name(self, address: str, block_number='latest') -> Tuple[bool, str]:
        contract = self.make_erc721_contract(address)
        try:
            return False, contract.functions.name().call(block_identifier=block_number)
        except Exception as e:
            log.server_logger.error('get_name exception:%s', str(e))
            self.use_next_endpoint()
            return True, ''

    @awaitable
    def get_erc721_symbol(self, address: str, block_number='latest') -> Tuple[bool, str]:
        contract = self.make_erc721_contract(address)
        try:
            return False, contract.functions.symbol().call(block_identifier=block_number)
        except Exception as e:
            log.server_logger.error('get_symbol exception:%s', str(e))
            self.use_next_endpoint()
            return True, ''

    def use_next_endpoint(self):
        self.endpoint_index = (self.endpoint_index + 1) % len(self.endpoints)

    def get_endpoint(self):
        return self.endpoints[self.endpoint_index]

    def make_web3(self):
        w3 = Web3(Web3.HTTPProvider(self.get_endpoint()))
        w3.middleware_onion.inject(geth_poa_middleware, layer=0)
        if self.signer != None:
            w3.middleware_onion.add(construct_sign_and_send_raw_middleware(self._account))
            w3.eth.default_account = self.signer
        return w3

    def make_core_contract(self):
        return self.make_web3().eth.contract(address=self.core_contract, abi=self.core_abi)
    
    def make_validator_contract(self):
        return self.make_web3().eth.contract(address=self.validator_contract, abi=self.validator_abi)

    def make_erc20_contract(self, address: str):
        return self.make_web3().eth.contract(address=address, abi=ERC20_ABI)

    def make_erc721_contract(self, address: str):
        return self.make_web3().eth.contract(address=address, abi=ERC721_ABI)

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
        self.update_validator_indices()
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
        if total != self.__total_weight:
            updated = True
            self.__total_weight = total
            min_weight = int(2e16)
            self.total_weight = min_weight if self.__total_weight < min_weight else self.__total_weight

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
        if not self.binding_status_synced or self.signer != self.bound_signer:
            return False

        if NODE == None:
            return False
        validator = NODE['account_hex']
        if validator == self.genesis_validator:
            return False
        current_weight = node_snapshot_weight(NODE['account'])
        if current_weight == None:
            return

        if validator not in self.validators:
            return current_weight != 0
        info = self.validators[validator]
        if info.weight == 0 and current_weight == 0:
            return False
        if info.epoch >= current_epoch():
            return False
        return in_reward_time_range(info.last_submit, int(time.time() - 10))

    def genesis_weight(self):
        if self.total_weight == None or self.__total_weight == None:
            return 0
        if self.total_weight <= self.__total_weight:
            return 0
        return self.total_weight - self.__total_weight

    def top_validators(self, percent: int):
        if self.validators == None:
            return []

        if len(self.validators) == 0:
            return [self.genesis_validator]

        genesis_weight = self.genesis_weight()
        validators = []
        weight = 0
        genesis_included = False
        for i in self.validators:
            if i.weight < genesis_weight and not genesis_included:
                genesis_included = True
                weight += genesis_weight
                validators.append(self.genesis_validator)
                if weight / self.total_weight >= percent:
                    break
            if i.weight == 0:
                break
            weight += i.weight
            validators.append(i.validator)
            if weight / self.total_weight >= percent:
                break
        return validators

    def weight_threshold(self, percent: float):
        genesis_weight = self.genesis_weight()
        if not self.validators:
            return genesis_weight
        weight = 0
        genesis_included = False
        for i in self.validators:
            if i.weight < genesis_weight and not genesis_included:
                genesis_included = True
                weight += genesis_weight
                if weight / self.total_weight >= percent:
                    return genesis_weight
            if i.weight == 0:
                break
            weight += i.weight
            if weight / self.total_weight >= percent:
                return i.weight
        return 0

    def weight_of_validator(self, validator: str):
        if validator == self.genesis_validator:
            return self.genesis_weight()
        if validator not in self.validator_indices:
            return 0
        index = self.validator_indices[validator]
        return self.validators[index].weight

    def calc_submission_weight(self):
        if not self.weights:
            return None
        threshold = self.weight_threshold(0.99)
        l = list(self.weights.values())
        l.sort(key = lambda x: x.weight, reverse = True)

        sum = 0
        for i in l:
            weight = self.weight_of_validator(i.replier)
            if weight < threshold:
                continue
            sum += weight
            if sum >= self.total_weight * 2 / 3:
                return i.weight
        return None

    async def node_weight_query(self):
        if NODE == None:
            return
        self.weight_query_count += 1
        count = self.weight_query_count 
        if count <= 3:
            percent = 0.8
        elif count <= 6:
            percent = 0.9
        else:
            percent = 0.99

        validators = self.top_validators(percent)
        for validator in validators:
            if validator in self.weights:
                continue
            message = {
                'action': 'weight_query',
                'request_id': f'{int(self.chain_id):064X}',
                'representative': NODE['account'],
                'replier': validator,
            }
            await send_to_node(message)

    async def collect_weight_signatures(self):
        if NODE == None:
            return
        source = NODE['account_hex']

        self.signature_collect_count += 1
        threshold = self.weight_threshold(0.99)
        for i in self.weights:
            if i.replier in self.signatures:
                continue
            weight = self.weight_of_validator(i.replier)
            if weight < threshold:
                continue
            message = WeightSignMessage(source, i.replier, self.chain_id, '', True, source,
                self.signer, self.submission_weight, self.submission_epoch)
            await send_cross_chain_message(message)

    def purge_outdated_weights(self):
        epoch = current_epoch()
        purgeable = []
        for validator, info in self.weights.items():
            if info.epoch != epoch:
                purgeable.append(validator)
        for validator in purgeable:
            del self.weights[validator]

    def enough_signatures(self):
        l = list(self.signatures.values())
        l.sort(key = lambda x: self.weight_of_validator(x.replier), reverse = True)
        signatures = []
        weight = 0
        for i in l:
            weight += self.weight_of_validator(i.replier)
            signatures.append(i)
            if weight > self.total_weight * 0.51:
                return signatures
        return None

    async def bind(self):
        if self.signer == None or self.signer == self.genesis_signer:
            return
        if not self.binding_status_synced:
            message = {
                'action': 'bind_query',
                'chain_id': str(int(self.chain_id)),
                'validator': NODE['account_hex'],

            }
            await send_to_node(message)
            return
        if self.signer == self.bound_signer:
            return
        message = {
            'action': 'bind',
            'chain_id': str(int(self.chain_id)),
            'signer': UTIL.uint256_to_hex(int(self.signer, 16)),
        }
        self.binding_status_synced = False
        await send_to_node(message)

    async def submit(self):
        if NODE == None:
            return
        validator = NODE['account_hex']

        state = self.submission_state
        if state == self.SubmissionState.IDLE:
            if not self.rewardable():
                return
            self.submission_epoch = current_epoch()
            self.weight_query_count = 0
            self.weights = {}
            self.submission_state = self.SubmissionState.WEIGHT_QUERY
            await self.node_weight_query()
        elif state == self.SubmissionState.WEIGHT_QUERY:
            if self.submission_epoch != current_epoch():
                self.submission_state = self.SubmissionState.IDLE
                return
            self.purge_outdated_weights()
            self.submission_weight = self.calc_submission_weight()
            if self.submission_weight == None:
                await self.node_weight_query()
                return
            self.signature_collect_count = 0
            self.signatures = {}
            self.submission_state = self.SubmissionState.COLLECT_SIGNATURES
            await self.collect_weight_signatures()
        elif state == self.SubmissionState.COLLECT_SIGNATURES:
            if self.submission_epoch != current_epoch():
                self.submission_state = self.SubmissionState.IDLE
                return
            signatures = self.enough_signatures()
            if signatures == None or len(signatures) > 100:
                if self.signature_collect_count >= 10:
                    self.submission_state = self.SubmissionState.IDLE
                else:
                    await self.collect_weight_signatures()
                return
            signatures.sort(key = lambda x: x.signer.lower())
            pack = b''
            for i in signatures:
                pack += i.r + i.s + i.v
            await self.sumbit_validator(validator, self.signer, self.submission_weight,
                self.submission_epoch, self.signer, pack)
            self.last_submit = time.time()
            self.submission_state = self.SubmissionState.IDLE
        else:
            log.server_logger.error('unexpected submission state:%s', state.name)

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
        if error or (self.synced_height != None and block_number <= self.synced_height):
            return

        error, updated = await self.sync_fee(block_number)
        if error:
            return
        notify = updated

        error, updated = await self.sync_validators(block_number)
        if error:
            return
        notify = notify or updated

        self.synced_height = block_number
        if notify:
            await notify_chain_info(self.chain_id, self.to_chain_info())

        await self.bind()
        await self.submit()

    async def sign_transfer(self, uid: str, req: dict) -> dict:
        if NODE == None:
            return {'error': 'node offline'}
        source = NODE['account_hex']
        destination = req['validator']
        account = req['account']
        height = int(req['height'])
        op = req['operation']
        try:
            opcode = CrossChainOpcode[CrossChainOp(op).name]
        except:
            return {'error': 'invalid operation'}
        async def callback(sign: TransferSignMessage):
            try:
                resp = {
                    'validator': sign.source,
                    'signer': sign.source_signer,
                    'account': sign.account,
                    'height': sign.height,
                    'operation': CrossChainOp[sign.opcode.name],
                    'signature': UTIL.bytes_to_hex(sign.signature),
                }
                await send_ack(uid, req, resp)
            except Exception as e:
                log.server_logger.exception('sign_transfer.callback: uncaught exception=%s', e)
        request_id = CallbackHelper.add(callback)
        message = TransferSignMessage(source, destination, self.chain_id, '', True, account,
            height, opcode, request_id)

        await send_cross_chain_message(message)
        return {'pending': ''}

    async def sign_creation(self, uid: str, req: dict) -> dict:
        if NODE == None:
            return {'error': 'node offline'}
        source = NODE['account_hex']
        destination = req['validator']
        original_chain_id = int(req['original_chain_id'])
        original_contract = req['original_contract']
        async def callback(sign: CreationSignMessage):
            try:
                resp = {
                    'validator': sign.source,
                    'signer': sign.source_signer,
                    'original_chain_id': str(sign.original_chain_id),
                    'original_contract': sign.original_contract,
                    'signature': UTIL.bytes_to_hex(sign.signature),
                }
                await send_ack(uid, req, resp)
            except Exception as e:
                log.server_logger.exception('sign_creation.callback: uncaught exception=%s', e)
        request_id = CallbackHelper.add(callback)
        message = CreationSignMessage(source, destination, self.chain_id, '', True,
            original_chain_id, original_contract, request_id)
        await send_cross_chain_message(message)
        return {'pending': ''}

    async def token_symbol(self, _: str, req: dict) -> dict:
        address = req['address']
        if address in self.token_symbols:
            return {'symbol': self.token_symbols[address]}
        error, block_number = await self.get_block_number()
        if error:
            return {'error': 'failed to get block_number'}
        block_number -= self.confirmations
        error, symbol = await self.get_erc20_symbol(address, block_number)
        if error:
            return {'error': 'failed to get token symbol'}
        self.token_symbols[address] = symbol
        return {'symbol': symbol}

    async def token_name(self, _: str, req: dict) -> dict:
        address = req['address']
        result = {'address': address, 'address_raw': req['address_raw']}
        if address in self.token_names:
            result['name'] = self.token_names[address]
            return result
        error, block_number = await self.get_block_number()
        if error:
            return {'error': 'failed to get block_number'}
        block_number -= self.confirmations
        error, name = await self.get_erc20_name(address, block_number)
        if error:
            result['error'] = 'failed to get token name'
            return result
        self.token_names[address] = name
        result['name'] = self.token_names[address]
        return result

    async def is_erc20_token(self, address: str, block_number: int) -> bool:
        error, _ = await self.get_erc20_decimals(address, block_number)
        if error:
            return False
        error, _ = await self.get_erc20_allowance(address, self.core_contract, self.validator_contract, block_number)
        if error:
            return False
        return True

    async def is_erc721_token(self, address: str, block_number: int) -> bool:
        error,  yes = await self.supports_interface(address, self.InterfaceId.ERC165, block_number)
        if error or not yes:
            return False
        error, yes = await self.supports_interface(address, self.InterfaceId.INVALID, block_number)
        if error or yes:
            return False
        error, yes = await self.supports_interface(address, self.InterfaceId.ERC721, block_number)
        if error or not yes:
            return False
        return True

    async def token_type(self, _: str, req: dict) -> dict:
        address = req['address']
        if address in self.token_types:
            return {'type': self.token_types[address]}
        error, block_number = await self.get_block_number()
        if error:
            return {'error': 'failed to get block_number'}
        block_number -= self.confirmations

        yes = await self.is_erc20_token(address, block_number)
        if yes:
            self.token_types[address] = '20'
            return {'type': '20'}
        yes = await self.is_erc721_token(address, block_number)
        if yes:
            self.token_types[address] = '721'
            return {'type': '721'}
        return {'type': ''}

    async def token_decimals(self, _: str, req: dict) -> dict:
        address = req['address']
        if address in self.token_decimals:
            return {'decimals': self.token_decimals[address]}
        error, block_number = await self.get_block_number()
        if error:
            return {'error': 'failed to get block_number'}
        block_number -= self.confirmations
        error, decimals = await self.get_erc20_decimals(address, block_number)
        if error:
            return {'error': 'failed to get token decimals'}
        self.token_decimals[address] = decimals
        return {'decimals': decimals}

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
        result['bound_signer'] = self.bound_signer
        result['binding_status_synced'] = self.binding_status_synced
        result['weights'] = self.weights
        result['signatures'] = self.signatures
        result['submission_state'] = self.submission_state.name
        result['submission_weight'] = self.submission_weight
        result['submission_epoch'] = self.submission_epoch
        result['weight_query_count'] = self.weight_query_count
        result['signature_collect_count'] = self.signature_collect_count
        result['last_submit'] = self.last_submit
        return result

async def send_ack(uid, req: dict, resp: dict):
    if uid not in CLIENTS:
        return
    message = {'service': SERVICE}
    if 'request_id' in req:
        message['request_id'] = req['request_id']
    if 'client_id' in req:
        message['client_id'] =  req['client_id']

    if 'action' not in req:
        return
    message['ack'] = req['action']
    message.update(resp)
    try:
        await CLIENTS[uid]['ws'].send_str(json.dumps(message))
    except Exception as e:
        log.server_logger.error(
            'send_ack: message=%s, exception=%s', message, str(e))

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

async def sign_transfer(uid, req, res):
    try:
        chain_id = int(req['chain_id'])
        if chain_id not in VALIDATORS:
            res['error'] = 'chain not supported'
            return
        validator = VALIDATORS[chain_id]['validator']
        res.update(await validator.sign_transfer(uid, req))
    except Exception as e:
        res['error'] = 'exception'
        log.server_logger.exception('sign_transfer exception:%s', str(e))

async def sign_creation(uid, req, res):
    try:
        chain_id = int(req['chain_id'])
        if chain_id not in VALIDATORS:
            res['error'] = 'chain not supported'
            return
        validator = VALIDATORS[chain_id]['validator']
        res.update(await validator.sign_creation(uid, req))
    except Exception as e:
        res['error'] = 'exception'
        log.server_logger.exception('sign_creation exception:%s', str(e))

async def token_symbol(uid, req, res):
    try:
        if 'chain' in req:
            res['chain'] = req['chain']
        res['chain_id'] = req['chain_id']
        if 'address' in req:
            res['address'] = req['address']
        if 'address_raw' in req:
            res['address_raw'] = req['address_raw']
        chain_id = int(req['chain_id'])
        if chain_id not in VALIDATORS:
            res['error'] = 'chain not supported'
            return
        validator = VALIDATORS[chain_id]['validator']
        res.update(await validator.token_symbol(uid, req))
    except Exception as e:
        res['error'] = 'exception'
        log.server_logger.exception('token_symbol exception:%s', str(e))

async def token_name(uid, req, res):
    try:
        if 'chain' in req:
            res['chain'] = req['chain']
        res['chain_id'] = req['chain_id']
        if 'address' in req:
            res['address'] = req['address']
        if 'address_raw' in req:
            res['address_raw'] = req['address_raw']
        chain_id = int(req['chain_id'])
        if chain_id not in VALIDATORS:
            res['error'] = 'chain not supported'
            return
        validator = VALIDATORS[chain_id]['validator']
        res.update(await validator.token_name(uid, req))
    except Exception as e:
        res['error'] = 'exception'
        log.server_logger.exception('token_name exception:%s', str(e))

async def token_type(uid, req, res):
    try:
        if 'chain' in req:
            res['chain'] = req['chain']
        res['chain_id'] = req['chain_id']
        if 'address' in req:
            res['address'] = req['address']
        if 'address_raw' in req:
            res['address_raw'] = req['address_raw']
        chain_id = int(req['chain_id'])
        if chain_id not in VALIDATORS:
            res['error'] = 'chain not supported'
            return
        validator = VALIDATORS[chain_id]['validator']
        res.update(await validator.token_type(uid, req))
    except Exception as e:
        res['error'] = 'exception'
        log.server_logger.exception('token_type exception:%s', str(e))

async def token_decimals(uid, req, res):
    try:
        if 'chain' in req:
            res['chain'] = req['chain']
        res['chain_id'] = req['chain_id']
        if 'address' in req:
            res['address'] = req['address']
        if 'address_raw' in req:
            res['address_raw'] = req['address_raw']
        chain_id = int(req['chain_id'])
        if chain_id not in VALIDATORS:
            res['error'] = 'chain not supported'
            return
        validator = VALIDATORS[chain_id]['validator']
        res.update(await validator.token_decimals(uid, req))
    except Exception as e:
        res['error'] = 'exception'
        log.server_logger.exception('token_decimals exception:%s', str(e))

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
        elif action == 'sign_transfer':
            await sign_transfer(uid, request, res)
        elif action == 'sign_creation':
            await sign_creation(uid, request, res)
        elif action == 'token_symbol':
            await token_symbol(uid, request, res)
        elif action == 'token_name':
            await token_name(uid, request, res)
        elif action == 'token_type':
            await token_type(uid, request, res)
        elif action == 'token_decimals':
            await token_decimals(uid, request, res)
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

async def send_to_node(message):
    if NODE == None:
        return
    await NODE['ws'].send_str(json.dumps(message))

async def handle_node_weight_snapshot_ack(r : dict):
    global NODE
    snapshot = {}
    snapshot['epoch'] = int(r['epoch'])
    snapshot['weights'] = {}
    for i in r['weights']:
        snapshot['weights'][i['representative']] = int(i['weight'])
    NODE['snapshot'] = snapshot

async def handle_node_cross_chain_message(r : dict):
    source = '0x' + r['source_hex'].lower()
    destination = '0x' + r['destination_hex'].lower()
    source_signer = '0x' + r['source_signer'].lower()
    chain_id = int(r['chain_id'])
    payload = bytes.fromhex(r['payload'])

    if len(payload) == 0:
        log.server_logger.error('invalid cross chain message;%s', r)
        return

    stream = BytesIO(payload[:1])
    message_type = Serializer.read_uint8(stream)
    stream = BytesIO(payload)

    if message_type == CrossChainMessageType.WeightSign:
        message = WeightSignMessage(source, destination, chain_id, source_signer)
    elif message_type == CrossChainMessageType.TransferSign:
        message = TransferSignMessage(source, destination, chain_id, source_signer)
    elif message_type == CrossChainMessageType.CreationSign:
        message = CreationSignMessage(source, destination, chain_id, source_signer)
    else:
        pass
    # todo:

    error = message.deserialize(stream)
    if error:
        log.server_logger.error('invalid cross chain message;%s', r)
        return
    if chain_id not in VALIDATORS:
        return
    await VALIDATORS[chain_id]['validator'].process_cross_chain_message(message)

async def handle_node_weight_query_ack(r : dict):
    global NODE
    chain_id = int(r['request_id'], 16)
    validator = '0x' + r['representative_hex'].lower()
    epoch = int(r['epoch'])
    weight = int(r['weight'])
    replier = '0x' + r['replier_hex'].lower()
    if chain_id not in VALIDATORS:
        return
    await VALIDATORS[chain_id]['validator'].process_weight_query_ack(validator, epoch, weight, replier)

async def handle_node_bind_query_ack(r : dict):
    chain_id = int(r['chain_id'])
    validator = '0x' + r['validator_hex'].lower()
    signer = '0x' + r['signer'].lower()
    if chain_id not in VALIDATORS:
        return
    await VALIDATORS[chain_id]['validator'].process_bind_query_ack(validator, signer)

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
            await handle_node_bind_query_ack(r)
        elif action == 'cross_chain':
            await handle_node_cross_chain_message(r)
        elif action == 'node_account_ack':
            NODE['account'] = r['account']
            NODE['account_hex'] = '0x' + r['account_hex'].lower()
        elif action == 'weight_query_ack':
            await handle_node_weight_query_ack(r)
        elif action == 'weight_snapshot_ack':
            await handle_node_weight_snapshot_ack(r)
        else:
            log.server_logger.error('unexpected node message;%s;%s', message, ip)
    except Exception as e:
        log.server_logger.exception('uncaught error;%s;%s', str(e), ip)

NODE = None
def node_snapshot_weight(account: str) -> Union[int, None]:
    if NODE == None:
        return None
    if not account.startswith('rai_'):
        error, account = ACCOUNT.encode(account)
        if error:
            return None
    if account not in NODE['snapshot']['weights']:
        return 0
    return NODE['snapshot']['weights'][account]

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

TOKEN_SERVICE = None

async def send_to_token_service(message : dict, callback: Callable[[dict], None]):
    global TOKEN_SERVICE, CALLBACK_HELPER
    if TOKEN_SERVICE == None:
        return
    key = CALLBACK_HELPER.add(callback)
    message['request_id'] = UTIL.uint256_to_hex(key)[2:]
    await TOKEN_SERVICE['ws'].send_str(json.dumps(message))

async def handle_token_service_message(msg_str, ws):
    global TOKEN_SERVICE
    log.server_logger.info('message from token service: %s', msg_str)
    try:
        msg = json.loads(msg_str)
        if 'register' in msg:
            service = msg['register']
            if service != 'token':
                return
            TOKEN_SERVICE = {'ws':ws, 'filters': msg['filters']}
            if 'actions' in msg:
                TOKEN_SERVICE['actions'] = msg['actions']
        elif 'notify' in msg:
            pass
        elif 'ack' in msg:
            if 'request_id' not in msg:
                log.server_logger.error('handle_token_service_message: request_id missing')
                return
            key = int(msg['request_id'], 16)
            await CALLBACK_HELPER.callback(key, msg)
    except Exception as e:
        log.server_logger.exception('handle_token_service_message: uncaught exception=%s', e)

async def connect_to_token_service(task):
    global TOKEN_SERVICE

    try:
        session = ClientSession()
        async with session.ws_connect(RAI_TOKEN_URL, heartbeat=30) as ws:
            async for msg in ws:
                if msg.type == WSMsgType.TEXT:
                    if msg.data == 'close':
                        await ws.close()
                    else:
                        await handle_token_service_message(msg.data, ws)
                elif msg.type == WSMsgType.CLOSE:
                    log.server_logger.info('token service connection closed normally')
                    break
                elif msg.type == WSMsgType.ERROR:
                    log.server_logger.info('token service connection closed with error %s', ws.exception())
                    break
        
        log.server_logger.info('token service connection closed normally')
    except Exception as e:
        log.server_logger.exception('token service connection closed with exception: %s', e)
    except:
        log.server_logger.exception('token service uncaught exception!')
    finally:
        if not session.closed:
            try:
                await session.close()
            except:
                pass
        TOKEN_SERVICE = None

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
                                           BSC_TEST_ENDPOINTS, BSC_TEST_SIGNER_PRIVATE_KEY, 30, 'bsc_test')
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
        tasks.append(LOOP.create_task(periodic(10, CALLBACK_HELPER)))
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

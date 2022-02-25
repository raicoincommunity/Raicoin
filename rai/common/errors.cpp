#include <rai/common/errors.hpp>

#include <string>

std::string rai::ErrorString(rai::ErrorCode error_code)
{
    switch (error_code)
    {
        case rai::ErrorCode::SUCCESS:
        {
            return "Success";
        }
        case rai::ErrorCode::GENERIC:
        {
            return "Unknown error";
        }
        case rai::ErrorCode::STREAM:
        {
            return "Invalid stream";
        }
        case rai::ErrorCode::BLOCK_TYPE:
        {
            return "Invalid block type";
        }
        case rai::ErrorCode::BLOCK_OPCODE:
        {
            return "Invalid block opcode";
        }
        case rai::ErrorCode::EXTENSIONS_LENGTH:
        {
            return "The extensions exceed maximum size";
        }
        case rai::ErrorCode::UNKNOWN_COMMAND:
        {
            return "Unknown command";
        }
        case rai::ErrorCode::DATA_PATH:
        {
            return "Failed to create data directory, please check <data_path>";
        }
        case rai::ErrorCode::CONFIG_VERSION:
        {
            return "Unknown daemon config version";
        }
        case rai::ErrorCode::OPEN_OR_CREATE_FILE:
        {
            return "Failed to open or create file";
        }
        case rai::ErrorCode::WRITE_FILE:
        {
            return "Failed to write file";
        }
        case rai::ErrorCode::CONFIG_NODE_VERSION:
        {
            return "Unknown node config version";
        }
        case rai::ErrorCode::CONFIG_LOG_VERSION:
        {
            return "Unknown log config version";
        }
        case rai::ErrorCode::PASSWORD_LENGTH:
        {
            return "Error: password length must be 8 to 256";
        }
        case rai::ErrorCode::PASSWORD_MATCH:
        {
            return "Error: passwords do not match";
        }
        case rai::ErrorCode::KEY_FILE_EXIST:
        {
            return "The specified key file already exists";
        }
        case rai::ErrorCode::DERIVE_KEY:
        {
            return "Failed to derive key from password";
        }
        case rai::ErrorCode::CMD_MISS_KEY:
        {
            return "Please specify key parameter to run the command";
        }
        case rai::ErrorCode::KEY_FILE_NOT_EXIST:
        {
            return "Can not find the specified key file";
        }
        case rai::ErrorCode::KEY_FILE_VERSION:
        {
            return "Unknown key file format";
        }
        case rai::ErrorCode::INVALID_KEY_FILE:
        {
            return "Invalid key file";
        }
        case rai::ErrorCode::PASSWORD_ERROR:
        {
            return "Password incorrect";
        }
        case rai::ErrorCode::MAGIC_NUMBER:
        {
            return "Invalid magic number";
        }
        case rai::ErrorCode::INVALID_MESSAGE:
        {
            return "Invalid message type";
        }
        case rai::ErrorCode::UNKNOWN_MESSAGE:
        {
            return "Unknown message type";
        }
        case rai::ErrorCode::HANDSHAKE_TYPE:
        {
            return "Unknow handshake type";
        }
        case rai::ErrorCode::OUTDATED_VERSION:
        {
            return "Outdated message version";
        }
        case rai::ErrorCode::UNKNOWN_VERSION:
        {
            return "Unknown message version";
        }
        case rai::ErrorCode::SIGNATURE:
        {
            return "Bad signature";
        }
        case rai::ErrorCode::KEEPLIVE_PEERS:
        {
            return "More than maximum allowed peers in keeplive message";
        }
        case rai::ErrorCode::KEEPLIVE_REACHABLE_PEERS:
        {
            return "Reachable peers more than total peers in keeplive message";
        }
        case rai::ErrorCode::CONFIG_RPC_VERSION:
        {
            return "Unknow rpc config version";
        }
        case rai::ErrorCode::PUBLISH_NEED_CONFIRM:
        {
            return "Invalid extension field in publish message";
        }
        case rai::ErrorCode::BLOCK_CREDIT:
        {
            return "Invalid block credit field";
        }
        case rai::ErrorCode::BLOCK_COUNTER:
        {
            return "Invalid block counter field";
        }
        case rai::ErrorCode::BLOCK_TIMESTAMP:
        {
            return "Invalid block timestamp field";
        }
        case rai::ErrorCode::MDB_ENV_CREATE:
        {
            return "Failed to create MDB environment";
        }
        case rai::ErrorCode::MDB_ENV_SET_MAXDBS:
        {
            return "Failed to set MDB environment max datebases";
        }
        case rai::ErrorCode::MDB_ENV_SET_MAPSIZE:
        {
            return "Failed to set MDB environment map size";
        }
        case rai::ErrorCode::MDB_ENV_OPEN:
        {
            return "Failed to open MDB environment";
        }
        case rai::ErrorCode::MDB_TXN_BEGIN:
        {
            return "Failed to begin MDB transaction";
        }
        case rai::ErrorCode::MDB_DBI_OPEN:
        {
            return "Failed to open MDB database";
        }
        case rai::ErrorCode::MESSAGE_QUERY_BY:
        {
            return "Query message with invalid query_by field";
        }
        case rai::ErrorCode::MESSAGE_QUERY_STATUS:
        {
            return "Query message with invalid query_status field";
        }
        case rai::ErrorCode::MESSAGE_QUERY_BLOCK:
        {
            return "Query message with invalid block";
        }
        case rai::ErrorCode::MESSAGE_CONFIRM_SIGNATURE:
        {
            return "Confirm message with invalid signature";
        }
        case rai::ErrorCode::MESSAGE_FORK_ACCOUNT:
        {
            return "Fork message with unequal accounts";
        }
        case rai::ErrorCode::MESSAGE_FORK_HEIGHT:
        {
            return "Fork message with unequal heights";
        }
        case rai::ErrorCode::MESSAGE_FORK_BLOCK_EQUAL:
        {
            return "Fork message with same blocks";
        }
        case rai::ErrorCode::MESSAGE_CONFLICT_BLOCK:
        {
            return "Conflict message with invalid block";
        }
        case rai::ErrorCode::MESSAGE_CONFLICT_SIGNATURE:
        {
            return "Conflict message with invalid signature";
        }
        case rai::ErrorCode::MESSAGE_CONFLICT_TIMESTAMP:
        {
            return "Conflict message with invalid timestamp";
        }
        case rai::ErrorCode::ACCOUNT_LIMITED:
        {
            return "Account is limited";
        }
        case rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET:
        {
            return "Failed to get account infomation from ledger";;
        }
        case rai::ErrorCode::LEDGER_BLOCK_GET:
        {
            return "Failed to get block from ledger";
        }
        case rai::ErrorCode::CMD_MISS_HASH:
        {
            return "Please specify hash parameter to run the command";
        }
        case rai::ErrorCode::CMD_INVALID_HASH:
        {
            return "Invalid hash parameter";
        }
        case rai::ErrorCode::LEDGER_RECEIVABLE_INFO_COUNT:
        {
            return "Failed to get receivable count from ledger";
        }
        case rai::ErrorCode::LEDGER_BLOCK_COUNT:
        {
            return "Failed to get block count from ledger";
        }
        case rai::ErrorCode::LEDGER_ACCOUNT_COUNT:
        {
            return "Failed to get account count from ledger";
        }
        case rai::ErrorCode::CONFIG_WALLET_VERSION:
        {
            return "Unknow wallet config version";
        }
        case rai::ErrorCode::WALLET_PASSWORD:
        {
            return "Wallet password incorrect";
        }
        case rai::ErrorCode::WALLET_LOCKED:
        {
            return "Wallet is locked, unlock it first";
        }
        case rai::ErrorCode::WALLET_INFO_PUT:
        {
            return "Failed to put wallet infomation to ledger";
        }
        case rai::ErrorCode::WALLET_ACCOUNT_INFO_PUT:
        {
            return "Failed to put account infomation to ledger";
        }
        case rai::ErrorCode::WALLET_GET:
        {
            return "Failed to get wallet"; 
        }
        case rai::ErrorCode::WALLET_ACCOUNT_GET:
        {
            return "Failed to get account";
        }
        case rai::ErrorCode::WALLET_ACCOUNT_EXISTS:
        {
            return "The account already exists";
        }
        case rai::ErrorCode::DNS_RESOLVE:
        {
            return "Failed to resolve dns to ip";
        }
        case rai::ErrorCode::TCP_CONNECT:
        {
            return "Failed to make tcp connection";
        }
        case rai::ErrorCode::HTTP_POST:
        {
            return "Http post failed";
        }
        case rai::ErrorCode::LOGIC_ERROR:
        {
            return "Logic error";
        }
        case rai::ErrorCode::MESSAGE_CONFIRM_TIMESTAMP:
        {
            return "Confirm message with invalid timestamp";
        }
        case rai::ErrorCode::ELECTION_TALLY:
        {
            return "Election tally failed";
        }
        case rai::ErrorCode::UDP_RECEIVE:
        {
            return "Failed to receive udp packet";
        }
        case rai::ErrorCode::RESERVED_IP:
        {
            return "Message from reserved ip";
        }
        case rai::ErrorCode::REWARD_TO_ACCOUNT:
        {
            return "Invalid forward_reward_to account in config file";
        }
        case rai::ErrorCode::WALLET_ACCOUNT_IN_SYNC:
        {
            return "The account is synchronizing, please try later";
        }
        case rai::ErrorCode::EXTENSION_APPEND:
        {
            return "Failed to append extension to block";
        }
        case rai::ErrorCode::ACCOUNT_RESTRICTED:
        {
            return "Account is restricted";
        }
        case rai::ErrorCode::LEDGER_SOURCE_PUT:
        {
            return "Failed to put source to ledger";
        }
        case rai::ErrorCode::LEDGER_VERSION_PUT:
        {
            return "Failed to put version to ledger";
        }
        case rai::ErrorCode::LEDGER_UNKNOWN_VERSION:
        {
            return "Unknown ledger version";
        }
        case rai::ErrorCode::API_KEY:
        {
            return "The rai_api_key field is missing or invalid";
        }
        case rai::ErrorCode::BLOCK_AMOUNT_GET:
        {
            return "Failed to get block amount";
        }
        case rai::ErrorCode::WALLET_TIME_SYNC:
        {
            return "Wallet's time not synchronized";
        }
        case rai::ErrorCode::ELECTION_CONFLICT:
        {
            return "Conflict vote found";
        }
        case rai::ErrorCode::UNEXPECTED:
        {
            return "Encounter unexpected error";
        }
        case rai::ErrorCode::INVALID_PRIVATE_KEY:
        {
            return "Invalid private key";
        }
        case rai::ErrorCode::NODE_STATUS:
        {
            return "The node is offline or unsynchronized";
        }
        case rai::ErrorCode::BLOCK_HEIGHT_DUPLICATED:
        {
            return "Block with duplicated height";
        }
        case rai::ErrorCode::KEEPLIVE_ACK:
        {
            return "Invalid keeplive ack";
        }
        case rai::ErrorCode::SUBSCRIBE_TIMESTAMP:
        {
            return "Invalid subscription timestamp";
        }
        case rai::ErrorCode::SUBSCRIBE_SIGNATURE:
        {
            return "Invalid subscription signature";
        }
        case rai::ErrorCode::SUBSCRIBE_NO_CALLBACK:
        {
            return "Callback url not configured";
        }
        case rai::ErrorCode::ACCOUNT_ACTION_TOO_QUICKLY:
        {
            return "Create block too quickly";
        }
        case rai::ErrorCode::ACCOUNT_ACTION_CREDIT:
        {
            return "Account daily transactions limit exceeded";
        }
        case rai::ErrorCode::ACCOUNT_ACTION_BALANCE:
        {
            return "Not enough balance";
        }
        case rai::ErrorCode::LEDGER_RECEIVABLES_GET:
        {
            return "Failed to get receivables from ledger";
        }
        case rai::ErrorCode::WALLET_NOT_SELECTED_ACCOUNT:
        {
            return "Not selected account";
        }
        case rai::ErrorCode::LEDGER_RECEIVABLE_INFO_GET:
        {
            return "Failed to get receivable info from ledger";
        }
        case rai::ErrorCode::WALLET_RECEIVABLE_LESS_THAN_CREDIT:
        {
            return "Receivable amount less than credit price";
        }
        case rai::ErrorCode::REWARDER_REWARDABLE_LESS_THAN_CREDIT:
        {
            return "Rewardable amount less than credit price";
        }
        case rai::ErrorCode::LEDGER_REWARDABLE_INFO_GET:
        {
            return "Failed to get rewardable info from ledger";
        }
        case rai::ErrorCode::REWARDER_TIMESTAMP:
        {
            return "Not rewardable yet";
        }
        case rai::ErrorCode::REWARDER_AMOUNT:
        {
            return "Invalid rewardable amount";
        }
        case rai::ErrorCode::ACCOUNT_ZERO_BALANCE:
        {
            return "Account balance is zero";
        }
        case rai::ErrorCode::PEER_QUERY:
        {
            return "Failed to get peer";
        }
        case rai::ErrorCode::WALLET_EXISTS:
        {
            return "The wallet already exists";
        }
        case rai::ErrorCode::ACCOUNT_MAX_CREDIT:
        {
            return "Account's max allowed daily transactions limit is 1310700";
        }
        case rai::ErrorCode::INVALID_URL:
        {
            return "Invalid URL";
        }
        case rai::ErrorCode::HTTP_CLIENT_USED:
        {
            return "Http client is being used";
        }
        case rai::ErrorCode::LOAD_CERT:
        {
            return "Failed to load cacert.pem";
        }
        case rai::ErrorCode::SET_SSL_SNI:
        {
            return "Failed to set SSL SNI";
        }
        case rai::ErrorCode::SSL_HANDSHAKE:
        {
            return "SSL handshake failed";
        }
        case rai::ErrorCode::WRITE_STREAM:
        {
            return "Failed to write stream";
        }
        case rai::ErrorCode::HTTP_GET:
        {
            return "Http get failed";
        }
        case rai::ErrorCode::NODE_ACCOUNT_DUPLICATED:
        {
            return "Duplicated node account found";
        }
        case rai::ErrorCode::SUBSCRIPTION_EVENT:
        {
            return "Unknown event";
        }
        case rai::ErrorCode::EXTENSIONS_BROKEN_STREAM:
        {
            return "Broken stream";
        }
        case rai::ErrorCode::EXTENSION_TYPE:
        {
            return "Invalid extension type";
        }
        case rai::ErrorCode::EXTENSION_PARSE_UNKNOWN:
        {
            return "Unknown extension type";
        }
        case rai::ErrorCode::CONFIG_APP_VERSION:
        {
            return "Unknow app config version";
        }
        case rai::ErrorCode::WEBSOCKET_ACCEPT:
        {
            return "Websocket accept failed";
        }
        case rai::ErrorCode::WEBSOCKET_CLOSE:
        {
            return "Websocket close failed";
        }
        case rai::ErrorCode::WEBSOCKET_SEND:
        {
            return "Websocket send failed";
        }
        case rai::ErrorCode::WEBSOCKET_RECEIVE:
        {
            return "Websocket receive failed";
        }
        case rai::ErrorCode::WEBSOCKET_QUEUE_OVERFLOW:
        {
            return "Websocket message queue overflow";
        }
        case rai::ErrorCode::UTF8_CHECK:
        {
            return "Not utf-8 encoding";
        }
        case rai::ErrorCode::UTF8_CONTROL_CHARACTER:
        {
            return "Control character not allowed";
        }
        case rai::ErrorCode::HANDSHAKE_TIMESTAMP:
        {
            return "Invalid handshake timestamp";
        }
        case rai::ErrorCode::LEDGER_VERSION:
        {
            return "Invalid ledger version";
        }
        case rai::ErrorCode::EXTENSION_VALUE:
        {
            return "Invalid extension value";
        }
        case rai::ErrorCode::RECEIVABLE_AMOUNT:
        {
            return "Invalid receivable amount";
        }
        case rai::ErrorCode::SEND_AMOUNT:
        {
            return "Invalid send amount";
        }
        case rai::ErrorCode::JSON_GENERIC:
        {
            return "Failed to parse json";
        }
        case rai::ErrorCode::JSON_BLOCK_TYPE:
        {
            return "Failed to parse block type from json";
        }
        case rai::ErrorCode::JSON_BLOCK_OPCODE:
        {
            return "Failed to parse block opcode from json";
        }
        case rai::ErrorCode::JSON_BLOCK_CREDIT:
        {
            return "Failed to parse block credit from json";
        }
        case rai::ErrorCode::JSON_BLOCK_COUNTER:
        {
            return "Failed to parse block counter from json";
        }
        case rai::ErrorCode::JSON_BLOCK_TIMESTAMP:
        {
            return "Failed to parse block timestamp from json";
        }
        case rai::ErrorCode::JSON_BLOCK_HEIGHT:
        {
            return "Failed to parse block height from json";
        }
        case rai::ErrorCode::JSON_BLOCK_ACCOUNT:
        {
            return "Failed to parse block account from json";
        }
        case rai::ErrorCode::JSON_BLOCK_PREVIOUS:
        {
            return "Failed to parse block previous from json";
        }
        case rai::ErrorCode::JSON_BLOCK_REPRESENTATIVE:
        {
            return "Failed to parse block representative from json";
        }
        case rai::ErrorCode::JSON_BLOCK_BALANCE:
        {
            return "Failed to parse block balance from json";
        }
        case rai::ErrorCode::JSON_BLOCK_LINK:
        {
            return "Failed to parse block link from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSIONS_LENGTH:
        {
            return "Failed to parse block extensions length from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSIONS_EMPTY:
        {
            return "Extensions empty";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TYPE:
        {
            return "Failed to parse block extension type from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_VALUE:
        {
            return "Failed to parse block extension value from json";
        }
        case rai::ErrorCode::JSON_BLOCK_SIGNATURE:
        {
            return "Failed to parse block signature from json";
        }
        case rai::ErrorCode::JSON_BLOCK_PRICE:
        {
            return "Failed to parse credit price from json";
        }
        case rai::ErrorCode::JSON_BLOCK_BEGIN_TIME:
        {
            return "Failed to parse begin time from json";
        }
        case rai::ErrorCode::JSON_BLOCK_END_TIME:
        {
            return "Failed to parse end time from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSIONS_RAW:
        {
            return "Failed to parse block extensions_raw from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_FORMAT:
        {
            return "Invalid block extensions format in json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_ALIAS_OP:
        {
            return "Failed to parse alias extension op from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_ALIAS_OP_VALUE:
        {
            return "Failed to parse alias extension op_value from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_OP:
        {
            return "Failed to parse token extension op from json";;
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TYPE:
        {
            return "Failed to parse token extension type from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_NAME:
        {
            return "Failed to parse token extension name from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SYMBOL:
        {
            return "Failed to parse token extension symbol from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_INIT_SUPPLY:
        {
            return "Failed to parse token extension init_supply from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_CAP_SUPPLY:
        {
            return "Failed to parse token extension cap_supply from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_DECIMALS:
        {
            return "Failed to parse token extension decimals from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_BURNABLE:
        {
            return "Failed to parse token extension burnable from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_MINTABLE:
        {
            return "Failed to parse token extension mintable from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_CIRCULABLE:
        {
            return "Failed to parse token extension circulable from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_BASE_URI:
        {
            return "Failed to parse token extension base_uri from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TO:
        {
            return "Failed to parse token extension to from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_VALUE:
        {
            return "Failed to parse token extension value from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_URI:
        {
            return "Failed to parse token extension uri from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_CHAIN:
        {
            return "Failed to parse token extension chain from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_ADDRESS:
        {
            return "Failed to parse token extension address from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SOURCE:
        {
            return "Failed to parse token extension source from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_FROM:
        {
            return "Failed to parse token extension from from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_BLOCK_HEIGHT:
        {
            return "Failed to parse token extension block_height from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TX_HASH:
        {
            return "Failed to parse token extension transaction hash from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_SUBOP:
        {
            return "Failed to parse token extension swap sub_op from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TOKEN_OFFER:
        {
            return "Failed to parse token extension swap token_offer from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TOKEN_WANT:
        {
            return "Failed to parse token extension swap token_want from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_VALUE_OFFER:
        {
            return "Failed to parse token extension swap value_offer from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_VALUE_WANT:
        {
            return "Failed to parse token extension swap value_want from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_MIN_OFFER:
        {
            return "Failed to parse token extension swap min_offer from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_MAX_OFFER:
        {
            return "Failed to parse token extension swap max_offer from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TIMEOUT:
        {
            return "Failed to parse token extension swap timeout from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_MAKER:
        {
            return "Failed to parse token extension swap maker from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_ORDER_HEIGHT:
        {
            return "Failed to parse token extension swap order_height from "
                   "json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TRADE_HEIGHT:
        {
            return "Failed to parse token extension swap trade_height from "
                   "json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_VALUE:
        {
            return "Failed to parse token extension swap value from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_SHARE:
        {
            return "Failed to parse token extension swap share from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TAKER:
        {
            return "Failed to parse token extension swap taker from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_INQUIRY_HEIGHT:
        {
            return "Failed to parse token extension swap inquiry_height from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_SIGNATURE:
        {
            return "Failed to parse token extension swap signature from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TAKE_HEIGHT:
        {
            return "Failed to parse token extension swap take_height from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TO_CHAIN:
        {
            return "Failed to parse token extension to_chain from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TO_ACCOUNT:
        {
            return "Failed to parse token extension to_account from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_UNWRAP_CHAIN:
        {
            return "Failed to parse token extension unwrap_chain from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_MAIN_ACCOUNT:
        {
            return "Failed to parse token extension main_account from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_UNMAP_EXTRA_DATA:
        {
            return "Failed to parse token extension unmap extra_data from json";
        }
        case rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_ACK_HEIGHT:
        {
            return "Failed to parse token extension swap ack_height from json";
        }
        case rai::ErrorCode::JSON_CONFIG_VERSION:
        {
            return "Failed to parse version from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC:
        {
            return "Failed to parse rpc from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_NODE:
        {
            return "Failed to parse node from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_NODE_VERSION:
        {
            return "Failed to parse node version from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG:
        {
            return "Failed to parse log from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_VERSION:
        {
            return "Failed to parse log version from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_TO_CERR:
        {
            return "Failed to parse log_to_cerr from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_MAX_SIZE:
        {
            return "Failed to parse log max_size from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_ROTATION_SIZE:
        {
            return "Failed to parse log rotation_size from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_FLUSH:
        {
            return "Failed to parse log flush from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_NODE_PORT:
        {
            return "Failed to parse node port from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_NODE_IO_THREADS:
        {
            return "Failed to parse node io_threads from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_NETWORK:
        {
            return "Failed to parse log network from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_NETWORK_SEND:
        {
            return "Failed to parse log network_send from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_NETWORK_RECEIVE:
        {
            return "Failed to parse log network_receive from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_MESSAGE:
        {
            return "Failed to parse log message from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_MESSAGE_HANDSHAKE:
        {
            return "Failed to parse log message_handshake from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_PRECONFIGURED_PEERS:
        {
            return "Failed to parse node preconfigured_peers from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC_ENABLE:
        {
            return "Failed to parse rpc enable from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC_VERSION:
        {
            return "Failed to parse rpc version from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC_ADDRESS:
        {
            return "Failed to parse rpc address from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC_PORT:
        {
            return "Failed to parse rpc port from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC_ENABLE_CONTROL:
        {
            return "Failed to parse rpc enable_control from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_RPC_WHITELIST:
        {
            return "Failed to parse rpc whitelist from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_LOG_RPC:
        {
            return "Failed to parse log rpc from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_WALLET_VERSION:
        {
            return "Failed to parse version from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_WALLET_SERVER:
        {
            return "Failed to parse server from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_CALLBACK_URL:
        {
            return "Failed to parse callback_url from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_WALLET_PRECONFIGURED_REP:
        {
            return "Failed to parse preconfigured_representatives from "
                   "config file";
        }
        case rai::ErrorCode::JSON_CONFIG_REWARD_TO:
        {
            return "Failed to parse reward_to from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_DAILY_REWARD_TIMES:
        {
            return "Failed to parse daily_reward_times from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_ONLINE_STATS_URL:
        {
            return "Failed to parse online_statistics_url from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_INVITED_REPS_URL:
        {
            return "Failed to parse invited_representatives_url from config "
                   "file";
        }
        case rai::ErrorCode::JSON_CONFIG_WALLET:
        {
            return "Failed to parse wallet config";
        }
        case rai::ErrorCode::JSON_CONFIG_AIRDROP_MISS:
        {
            return "Missing config file";
        }
        case rai::ErrorCode::JSON_CONFIG_AUTO_CREDIT:
        {
            return "Failed to parse auto_credit from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_AUTO_RECEIVE:
        {
            return "Failed to parse auto_receive from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_RECEIVE_MINIMUM:
        {
            return "Failed to parse receive_minimum from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_ENABLE_RICH_LIST:
        {
            return "Failed to parse enable_rich_list from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_ENABLE_DELEGATOR_LIST:
        {
            return "Failed to parse enable_delegator_list from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_APP_VERSION:
        {
            return "Failed to parse app.version from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_APP_NODE_GATEWAY:
        {
            return "Failed to parse app.node_gateway from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_APP_WS_ADDRESS:
        {
            return "Failed to parse app.websocket.address from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_APP_WS_PORT:
        {
            return "Failed to parse app.websocket.port from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_APP_WS:
        {
            return "Failed to parse app.websocket from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_APP_WS_ENABLE:
        {
            return "Failed to parse app.websocket.enable from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_APP:
        {
            return "Failed to parse app object from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_NODE_ADDRESS:
        {
            return "Failed to parse node address from config file";
        }
        case rai::ErrorCode::JSON_CONFIG_ELECTION_CONCURRENCY:
        {
            return "Failed to parse election concurrency from config file";
        }
        case rai::ErrorCode::RPC_GENERIC:
        {
            return "[RPC] Internal server error";
        }
        case rai::ErrorCode::RPC_JSON:
        {
            return "[RPC] Invalid JSON format";
        }
        case rai::ErrorCode::RPC_JSON_DEPTH:
        {
            return "[RPC] Max JSON depth exceeded";
        }
        case rai::ErrorCode::RPC_HTTP_BODY_SIZE:
        {
            return "[RPC] Max http body size exceeded";
        }
        case rai::ErrorCode::RPC_NOT_LOCALHOST:
        {
            return "[RPC] The action is only allowed on localhost(127.0.0.1)";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_BLOCK:
        {
            return "[RPC] The block field is missing";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_ACCOUNT:
        {
            return "[RPC] The account field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_ACCOUNT:
        {
            return "[RPC] Invalid account field";
        }
        case rai::ErrorCode::RPC_ACCOUNT_NOT_EXIST:
        {
            return "[RPC] The account does not exist";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_TYPE:
        {
            return "[RPC] The type field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_TYPE:
        {
            return "[RPC] Invalid type field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_HEIGHT:
        {
            return "[RPC] The height field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_HEIGHT:
        {
            return "[RPC] Invalid height field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_PREVIOUS:
        {
            return "[RPC] The previous field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_PREVIOUS:
        {
            return "[RPC] Invalid previous field";
        }
        case rai::ErrorCode::RPC_UNKNOWN_ACTION:
        {
            return "[RPC] Unknown action";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_TIMESTAMP:
        {
            return "[RPC] The timestamp field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_TIMESTAMP:
        {
            return "[RPC] Invalid timestamp field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_SIGNATURE:
        {
            return "[RPC] The signature field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_SIGNATURE:
        {
            return "[RPC] Invalid signature field";
        }
        case rai::ErrorCode::RPC_ENABLE_CONTROL:
        {
            return "[RPC] RPC control is disabled";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_COUNT:
        {
            return "[RPC] The count field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_COUNT:
        {
            return "[RPC] Invalid count field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_HASH:
        {
            return "[RPC] The hash field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_HASH:
        {
            return "[RPC] Invalid hash field";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_ROOT:
        {
            return "[RPC] Invalid root field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_TO:
        {
            return "[RPC] The to field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_TO:
        {
            return "[RPC] Invalid to field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_AMOUNT:
        {
            return "[RPC] The amount field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_AMOUNT:
        {
            return "[RPC] Invalid amount field";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_REP:
        {
            return "[RPC] Invalid representative field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_EVENT:
        {
            return "[RPC] The event field is missing";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_NEXT:
        {
            return "[RPC] The next field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_NEXT:
        {
            return "[RPC] Invalid next field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_ACCOUNT_TYPES:
        {
            return "[RPC] The account_types field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_ACCOUNT_TYPES:
        {
            return "[RPC] Invalid account_types field";
        }
        case rai::ErrorCode::RPC_ACTION_NOT_ALLOWED:
        {
            return "[RPC] Action not allowed";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_CLIENT_ID:
        {
            return "[RPC] Invalid client_id field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_SERVICE:
        {
            return "[RPC] The service field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_SERVICE:
        {
            return "[RPC] Invalid service field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_FILTERS:
        {
            return "[RPC] The filters field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_FILTERS:
        {
            return "[RPC] Invalid filters field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_NAME:
        {
            return "[RPC] The name field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_NAME:
        {
            return "[RPC] Invalid name field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_DNS:
        {
            return "[RPC] The dns field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_DNS:
        {
            return "[RPC] Invalid dns field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_CHAIN:
        {
            return "[RPC] The chain field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_CHAIN:
        {
            return "[RPC] Invalid chain field";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_ADDRESS_RAW:
        {
            return "[RPC] Invalid address_raw field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_ADDRESS:
        {
            return "[RPC] The address field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_ADDRESS:
        {
            return "[RPC] Invalid address field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_TOKEN_ID:
        {
            return "[RPC] The token_id field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_TOKEN_ID:
        {
            return "[RPC] Invalid token_id field";
        }
        case rai::ErrorCode::RPC_MISS_FIELD_TOKENS:
        {
            return "[RPC] The tokens field is missing";
        }
        case rai::ErrorCode::RPC_INVALID_FIELD_TOKENS:
        {
            return "[RPC] Invalid tokens field";
        }
        case rai::ErrorCode::BLOCK_PROCESS_GENERIC:
        {
            return "Error in block processor";
        }
        case rai::ErrorCode::BLOCK_PROCESS_SIGNATURE:
        {
            return "Invalid block signature";
        }
        case rai::ErrorCode::BLOCK_PROCESS_EXISTS:
        {
            return "Block has already been processed";
        }
        case rai::ErrorCode::BLOCK_PROCESS_GAP_PREVIOUS:
        {
            return "Previous block does not exist";
        }
        case rai::ErrorCode::BLOCK_PROCESS_GAP_RECEIVE_SOURCE:
        {
            return "Receive source block does not exist";
        }
        case rai::ErrorCode::BLOCK_PROCESS_GAP_REWARD_SOURCE:
        {
            return "Reward source block does not exist";
        }
        case rai::ErrorCode::BLOCK_PROCESS_PREVIOUS:
        {
            return "Invalid block previous";
        }
        case rai::ErrorCode::BLOCK_PROCESS_OPCODE:
        {
            return "Invalid block opcode";
        }
        case rai::ErrorCode::BLOCK_PROCESS_CREDIT:
        {
            return "Invalid block credit";
        }
        case rai::ErrorCode::BLOCK_PROCESS_COUNTER:
        {
            return "Invalid block counter";
        }
        case rai::ErrorCode::BLOCK_PROCESS_TIMESTAMP:
        {
            return "Invalid block timestamp";
        }
        case rai::ErrorCode::BLOCK_PROCESS_BALANCE:
        {
            return "Invalid block balance";
        }
        case rai::ErrorCode::BLOCK_PROCESS_UNRECEIVABLE:
        {
            return "Source block is unreceivable";
        }
        case rai::ErrorCode::BLOCK_PROCESS_UNREWARDABLE:
        {
            return "Source block is unrewardable";
        }
        case rai::ErrorCode::BLOCK_PROCESS_PRUNED:
        {
            return "Block has been pruned";
        }
        case rai::ErrorCode::BLOCK_PROCESS_FORK:
        {
            return "Fork detected";
        }
        case rai::ErrorCode::BLOCK_PROCESS_TYPE_MISMATCH:
        {
            return "Block type mismatch";
        }
        case rai::ErrorCode::BLOCK_PROCESS_REPRESENTATIVE:
        {
            return "Invalid representative";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LINK:
        {
            return "Invalid block link";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_PUT:
        {
            return "Failed to put block to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_SUCCESSOR_SET:
        {
            return "Failed to set block successor to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET:
        {
            return "Failed to get block from ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_TYPE_UNKNOWN:
        {
            return "Unknown block type";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_PUT:
        {
            return "Failed to put receivable infomation to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_DEL:
        {
            return "Failed to delete receivable infomation from ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ACCOUNT_EXCEED_TRANSACTIONS:
        {
            return "Account exceeds maximum transactions";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_PUT:
        {
            return "Failed to put rewardable infomation to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT:
        {
            return "Failed to put account infomation to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT:
        {
            return "Data inconsistency in the ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_DEL:
        {
            return "Failed to delete block from ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ROLLBACK_BLOCK_PUT:
        {
            return "Failed to put rollback block to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_DEL:
        {
            return "Failed to delete account infomation from ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_FORK_DEL:
        {
            return "Failed to delete fork entry from ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_FORK_GET:
        {
            return "Failed to get fork entry from ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_FORK_PUT:
        {
            return "Failed to put fork to ledger";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_REWARDED:
        {
            return "Rollback rewarded block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_CONFIRM_BLOCK_MISS:
        {
            return "Confirm missing block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_SOURCE_PRUNED:
        {
            return "Rollback block with pruned source";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_NOT_EQUAL_TO_HEAD:
        {
            return "Rollback block not equal to head block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_RECEIVED:
        {
            return "Rollback received block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_IGNORE:
        {
            return "Rollback operation ignored";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_NON_HEAD:
        {
            return "Rollback non-head block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_TAIL:
        {
            return "Rollback tail block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_PREPEND_IGNORE:
        {
            return "Prepend operation ignored";
        }
        case rai::ErrorCode::BLOCK_PROCESS_UNKNOWN_OPERATION:
        {
            return "Unknown block operation";
        }
        case rai::ErrorCode::BLOCK_PROCESS_CONTINUE:
        {
            return "Continue processing block";
        }
        case rai::ErrorCode::BLOCK_PROCESS_WAIT:
        {
            return "Wait missing block";
        }
        case rai::ErrorCode::BOOTSTRAP_GENERIC:
        {
            return "Bootstrap error";
        }
        case rai::ErrorCode::BOOTSTRAP_PEER:
        {
            return "Failed to get peer for bootstrap";
        }
        case rai::ErrorCode::BOOTSTRAP_RESET:
        {
            return "Bootstrap has been reset";
        }
        case rai::ErrorCode::BOOTSTRAP_ATTACK:
        {
            return "Bootstrap attack is detected";
        }
        case rai::ErrorCode::BOOTSTRAP_CONNECT:
        {
            return "Failed to connect to peer";
        }
        case rai::ErrorCode::BOOTSTRAP_TYPE:
        {
            return "Invalid bootstrap type";
        }
        case rai::ErrorCode::BOOTSTRAP_SEND:
        {
            return "Failed to send data to peer";
        }
        case rai::ErrorCode::BOOTSTRAP_RECEIVE:
        {
            return "Failed to receive data from peer";
        }
        case rai::ErrorCode::BOOTSTRAP_ACCOUNT:
        {
            return "Invalid bootstrap account";
        }
        case rai::ErrorCode::BOOTSTRAP_FORK_LENGTH:
        {
            return "Invalid bootstrap fork message length";
        }
        case rai::ErrorCode::BOOTSTRAP_FORK_BLOCK:
        {
            return "Invalid block in bootstrap fork message";
        }
        case rai::ErrorCode::BOOTSTRAP_SIZE:
        {
            return "Bootstrap message exceeds maximum size";
        }
        case rai::ErrorCode::BOOTSTRAP_MESSAGE_TYPE:
        {
            return "Invalid bootstrap message";
        }
        case rai::ErrorCode::BOOTSTRAP_SLOW_CONNECTION:
        {
            return "Slow connection";
        }
        case rai::ErrorCode::APP_GENERIC:
        {
            return "App generic error";
        }
        case rai::ErrorCode::APP_INVALID_GATEWAY_URL:
        {
            return "Invalid gateway url";
        }
        case rai::ErrorCode::APP_PROCESS_GAP_PREVIOUS:
        {
            return "Previous block does not exist";
        }
        case rai::ErrorCode::APP_PROCESS_LEDGER_ACCOUNT_PUT:
        {
            return "Failed to put account infomation to ledger";
        }
        case rai::ErrorCode::APP_PROCESS_LEDGER_BLOCK_PUT:
        {
            return "Failed to put block to ledger";
        }
        case rai::ErrorCode::APP_PROCESS_PRUNED:
        {
            return "Block has been pruned";
        }
        case rai::ErrorCode::APP_PROCESS_PREVIOUS_MISMATCH:
        {
            return "Block previous mismatch";
        }
        case rai::ErrorCode::APP_PROCESS_LEDGER_SUCCESSOR_SET:
        {
            return "Failed to set block successor to ledger";
        }
        case rai::ErrorCode::APP_PROCESS_FORK:
        {
            return "Fork detected";
        }
        case rai::ErrorCode::APP_PROCESS_CONFIRMED_FORK:
        {
            return "Confirmed fork detected";
        }
        case rai::ErrorCode::APP_PROCESS_EXIST:
        {
            return "Block exists";
        }
        case rai::ErrorCode::APP_PROCESS_CONFIRMED:
        {
            return "Block confirmed";
        }
        case rai::ErrorCode::APP_PROCESS_ROLLBACK_ACCOUNT_MISS:
        {
            return "Rollback account miss";
        }
        case rai::ErrorCode::APP_PROCESS_ROLLBACK_NON_HEAD:
        {
            return "Rollback non-head block";
        }
        case rai::ErrorCode::APP_PROCESS_ROLLBACK_CONFIRMED:
        {
            return "Rollback confirmed block";
        }
        case rai::ErrorCode::APP_PROCESS_ROLLBACK_BLOCK_PUT:
        {
            return "Failed to put rollback block";
        }
        case rai::ErrorCode::APP_PROCESS_LEDGER_BLOCK_DEL:
        {
            return "Failed to delete block from ledger";
        }
        case rai::ErrorCode::APP_PROCESS_LEDGER_ACCOUNT_DEL:
        {
            return "Failed to delete account from ledger";
        }
        case rai::ErrorCode::APP_PROCESS_CONFIRM_REQUIRED:
        {
            return "Block should be confirmed";
        }
        case rai::ErrorCode::APP_PROCESS_LEDGER_PUT:
        {
            return "Failed to put app data to ledger";
        }
        case rai::ErrorCode::APP_PROCESS_LEDGER_DEL:
        {
            return "Failed to delete app data from ledger";
        }
        case rai::ErrorCode::APP_PROCESS_HALT:
        {
            return "The app encounters fatal error and is halted";
        }
        case rai::ErrorCode::APP_PROCESS_WAITING:
        {
            return "Wait for source block";
        }
        case rai::ErrorCode::APP_RPC_MISS_FIELD_TRACE:
        {
            return "The trace field is missing";
        }
        case rai::ErrorCode::APP_RPC_INVALID_FIELD_TRACE:
        {
            return "Invalid trace field";
        }
        case rai::ErrorCode::ALIAS_GENERIC:
        {
            return "Alias generic error";
        }
        case rai::ErrorCode::ALIAS_RESERVED_CHARACTOR_AT:
        {
            return "Contain reserved charactor '@'";
        }
        case rai::ErrorCode::ALIAS_RESERVED_CHARACTOR_UNDERSCORE:
        {
            return "Contain reserved charactor '_'";
        }
        case rai::ErrorCode::ALIAS_OP_INVALID:
        {
            return "Invalid alias operation";
        }
        case rai::ErrorCode::ALIAS_OP_UNKNOWN:
        {
            return "Unknown alias operation";
        }
        case rai::ErrorCode::ALIAS_MULTI_EXTENSIONS:
        {
            return "One block can contain only one alias extension";
        }
        case rai::ErrorCode::ALIAS_LEDGER_GET:
        {
            return "Failed to get data from ledger";
        }
        case rai::ErrorCode::ALIAS_LEDGER_PUT:
        {
            return "Failed to put data to ledger";
        }
        case rai::ErrorCode::TOKEN_GENERIC:
        {
            return "Token generic error";
        }
        case rai::ErrorCode::TOKEN_OP_INVALID:
        {
            return "Invalid token operation";
        }
        case rai::ErrorCode::TOKEN_OP_UNKNOWN:
        {
            return "Unknown token operation";
        }
        case rai::ErrorCode::TOKEN_TYPE_INVALID:
        {
            return "Invalid token type";
        }
        case rai::ErrorCode::TOKEN_TYPE_UNKNOWN:
        {
            return "Unknown token type";
        }
        case rai::ErrorCode::TOKEN_NAME_UTF8_CHECK:
        {
            return "The token name isn't a valid UTF-8 string";
        }
        case rai::ErrorCode::TOKEN_SYMBOL_UTF8_CHECK:
        {
            return "The token symbol isn't a valid UTF-8 string";
        }
        case rai::ErrorCode::TOKEN_CAP_SUPPLY_EXCEEDED:
        {
            return "The token cap supply is exceeded";
        }
        case rai::ErrorCode::TOKEN_BASE_URI_UTF8_CHECK:
        {
            return "The token base uri isn't a valid UTF-8 string";
        }
        case rai::ErrorCode::TOKEN_URI_UTF8_CHECK:
        {
            return "The token uri isn't a valid UTF-8 string";
        }
        case rai::ErrorCode::TOKEN_CHAIN_INVALID:
        {
            return "Invalid chain";
        }
        case rai::ErrorCode::TOKEN_CHAIN_UNKNOWN:
        {
            return "Unknown chain";
        }
        case rai::ErrorCode::TOKEN_SOURCE_INVALID:
        {
            return  "Invalid token source";
        }
        case rai::ErrorCode::TOKEN_SOURCE_UNKNOWN:
        {
            return "Unknown token source";
        }
        case rai::ErrorCode::TOKEN_CREATION_DATA_MISS:
        {
            return "Token creation data is missing";
        }
        case rai::ErrorCode::TOKEN_SWAP_SUB_DATA_MISS:
        {
            return "Token swap sub-data is missing";
        }
        case rai::ErrorCode::TOKEN_SWAP_VALUE_OFFER:
        {
            return "Invalid token value offered";
        }
        case rai::ErrorCode::TOKEN_SWAP_VALUE_WANT:
        {
            return "Invalid token value wanted";
        }
        case rai::ErrorCode::TOKEN_SWAP_MIN_OFFER:
        {
            return "Invalid minimum tradable value";
        }
        case rai::ErrorCode::TOKEN_SWAP_MAX_OFFER:
        {
            return "Invalid maximum tradable value";
        }
        case rai::ErrorCode::TOKEN_SWAP_PAIR_EQUAL:
        {
            return "Invalid token pair";
        }
        case rai::ErrorCode::TOKEN_SWAP_SUB_OP_INVALID:
        {
            return "Invalid token swap operation";
        }
        case rai::ErrorCode::TOKEN_SWAP_SUB_OP_UNKNOWN:
        {
            return "Unknown token swap operation";
        }
        case rai::ErrorCode::TOKEN_SWAP_MAKER:
        {
            return "Invalid token swap maker";
        }
        case rai::ErrorCode::TOKEN_SWAP_SHARE:
        {
            return "Invalid token swap share public key";
        }
        case rai::ErrorCode::TOKEN_SWAP_ORDER_HEIGHT:
        {
            return "Invalid token swap order_height";
        }
        case rai::ErrorCode::TOKEN_SWAP_TRADE_HEIGHT:
        {
            return "Invalid token swap trade_height";
        }
        case rai::ErrorCode::TOKEN_SWAP_INQUIRY_HEIGHT:
        {
            return "Invalid token swap inquiry_height";
        }
        case rai::ErrorCode::TOKEN_SWAP_TAKER:
        {
            return "Invalid token swap taker";
        }
        case rai::ErrorCode::TOKEN_SWAP_TAKE_HEIGHT:
        {
            return "Invalid token swap take_height";
        }
        case rai::ErrorCode::TOKEN_UNMAP_CHAIN:
        {
            return "Unmap token to invalid chain";
        }
        case rai::ErrorCode::TOKEN_UNMAP_TO:
        {
            return "Unmap token to invalid destination address";
        }
        case rai::ErrorCode::TOKEN_WRAP_CHAIN:
        {
            return "Wrap token to invalid chain";
        }
        case rai::ErrorCode::TOKEN_WRAP_TO:
        {
            return "Wrap token to invalid destination address";
        }
        case rai::ErrorCode::TOKEN_VALUE:
        {
            return "Invalid token value";
        }
        case rai::ErrorCode::TOKEN_MINT_TO:
        {
            return "Mint token to invalid destination address";
        }
        case rai::ErrorCode::TOKEN_SEND_TO:
        {
            return "Send token to invalid destination address";
        }
        case rai::ErrorCode::TOKEN_BLOCK_HEIGHT:
        {
            return "Token invalid block height";
        }
        case rai::ErrorCode::TOKEN_UNWRAP_CHAIN:
        {
            return "Unwrap from invalid chain";
        }
        case rai::ErrorCode::TOKEN_SWAP_MAIN_ACCOUNT:
        {
            return "Invalid token swap main account";
        }
        case rai::ErrorCode::TOKEN_CAP_SUPPLY:
        {
            return "Invalid token cap supply";
        }
        case rai::ErrorCode::TOKEN_LEDGER_GET:
        {
            return "Failed to get data from ledger";
        }
        case rai::ErrorCode::TOKEN_LEDGER_PUT:
        {
            return "Failed to put data to ledger";
        }
        case rai::ErrorCode::TOKEN_SWAP_ACK_HEIGHT:
        {
            return "Invalid swap ack height";
        }
        case rai::ErrorCode::TOKEN_MULTI_EXTENSIONS:
        {
            return "Contain more than one token extension in the block";
        }
        case rai::ErrorCode::TOKEN_RECREATE:
        {
            return "Recreate an existing token";
        }
        case rai::ErrorCode::TOKEN_NOT_CREATED:
        {
            return "Token not created";
        }
        case rai::ErrorCode::TOKEN_TOTAL_SUPPLY_OVERFLOW:
        {
            return "Token total supply overflow";
        }
        case rai::ErrorCode::TOKEN_UNMINTABLE:
        {
            return "Token unmintable";
        }
        case rai::ErrorCode::TOKEN_ID_REMINT:
        {
            return "Remint an existing token id";
        }
        case rai::ErrorCode::TOKEN_UNBURNABLE:
        {
            return "Token unburnable";
        }
        case rai::ErrorCode::TOKEN_BURN_MORE_THAN_BALANCE:
        {
            return "Burn more than balance";
        }
        case rai::ErrorCode::TOKEN_ID_NOT_OWNED:
        {
            return "Doesn't own the token id";
        }
        case rai::ErrorCode::TOKEN_BALANCE_IS_EMPTY:
        {
            return "The token balance is empty";
        }
        case rai::ErrorCode::TOKEN_BALANCE_IS_ZERO:
        {
            return "The token balance is 0";
        }
        case rai::ErrorCode::TOKEN_UNCIRCULABLE:
        {
            return "The token is uncirculable, only allowed to transfer or swap"
                   " with token issuer";
        }
        case rai::ErrorCode::TOKEN_SEND_MORE_THAN_BALANCE:
        {
            return "Send more than balance";
        }
        case rai::ErrorCode::TOKEN_UNRECEIVABLE:
        {
            return "Token unreceivable";
        }
        case rai::ErrorCode::TOKEN_SOURCE_TXN_HASH:
        {
            return "Invalid source transaction hash";
        }
        case rai::ErrorCode::TOKEN_RECEIVE_FROM:
        {
            return "Receive from invalid account";
        }
        case rai::ErrorCode::TOKEN_RECEIVE_BLOCK_HEIGHT:
        {
            return "Receive from invalid block height";
        }
        case rai::ErrorCode::TOKEN_RECEIVE_VALUE:
        {
            return "Receive with invalid token value or id";
        }
        case rai::ErrorCode::TOKEN_RECEIVE_SOURCE:
        {
            return "Receive with invalid token source";
        }
        case rai::ErrorCode::TOKEN_BALANCE_OVERFLOW:
        {
            return "Token balance overflow";
        }
        case rai::ErrorCode::TOKEN_SWAP_TIMESTAMP:
        {
            return "Invalid timestamp";
        }
        case rai::ErrorCode::TOKEN_SWAP_NO_ENOUGH_BALANCE:
        {
            return "Have no enough token";
        }
        case rai::ErrorCode::TOKEN_SWAP_MAIN_ACCOUNT_EMPTY:
        {
            return "Token swap main account not set";
        }
        case rai::ErrorCode::TOKEN_SWAP_ORDERS_EXCEEDED:
        {
            return "The account's max allowed active orders is exceeded";
        }
        case rai::ErrorCode::TOKEN_SWAP_WITH_SELF:
        {
            return "Swap token with self";
        }
        case rai::ErrorCode::TOKEN_SWAP_ORDER_MISS:
        {
            return "The order doesn't exist";
        }
        case rai::ErrorCode::TOKEN_SWAP_INQUIRY_VALUE:
        {
            return "Swap inquiry with invalid token value or id";
        }
        case rai::ErrorCode::TOKEN_SWAP_WITH_MAIN_ACCOUNT:
        {
            return "Swap token with main account";
        }
        case rai::ErrorCode::TOKEN_SWAP_INQUIRY_WAITING_MISS:
        {
            return "The swap inquiry request doesn't exist or has been replied";
        }
        case rai::ErrorCode::TOKEN_SWAP_TIMEOUT:
        {
            return "Swap timeout";
        }
        case rai::ErrorCode::TOKEN_SWAP_STATUS_UNEXPECTED:
        {
            return "Unexpected swap status";
        }
        case rai::ErrorCode::TOKEN_SWAP_TAKE_WAITING_MISS:
        {
            return "The swap take request doesn't exist or has been replied";
        }
        case rai::ErrorCode::TOKEN_SWAP_TAKE_ACK_VALUE:
        {
            return "Swap take ack with invalid token value or id";
        }
        case rai::ErrorCode::TOKEN_SWAP_ORDER_FINISHED:
        {
            return "The order has already been fulfilled or canceled";
        }
        case rai::ErrorCode::TOKEN_SWAP_MISS:
        {
            return "The swap doesn't exist";
        }
        case rai::ErrorCode::TOKEN_SWAP_ACCOUNT_NOT_MAKER:
        {
            return "The current account is not the swap maker";
        }
        case rai::ErrorCode::TOKEN_SWAP_TAKE_NACK_HEIGHT:
        {
            return "Invalid take nack height";
        }
        default:
        {
            return "Invalid error code";
        }
    }
}
#include <rai/common/chain.hpp>

std::string rai::ChainToString(rai::Chain chain)
{
    switch (chain)
    {
        case rai::Chain::INVALID:
        {
            return "invalid";
        }
        case rai::Chain::RAICOIN:
        {
            return "raicoin";
        }
        case rai::Chain::BITCOIN:
        {
            return "bitcoin";
        }
        case rai::Chain::ETHEREUM:
        {
            return "ethereum";
        }
        case rai::Chain::BINANCE_SMART_CHAIN:
        {
            return "binance smart chain";
        }
        case rai::Chain::RAICOIN_TEST:
        {
            return "raicoin testnet";
        }
        case rai::Chain::BITCOIN_TEST:
        {
            return "bitcoin testnet";
        }
        case rai::Chain::ETHEREUM_TEST_ROPSTEN:
        {
            return "ethereum ropsten testnet";
        }
        case rai::Chain::ETHEREUM_TEST_KOVAN:
        {
            return "ethereum kovan testnet";
        }
        case rai::Chain::ETHEREUM_TEST_RINKEBY:
        {
            return "ethereum rinkeby testnet";
        }
        case rai::Chain::ETHEREUM_TEST_GOERLI:
        {
            return "ethereum goerli testnet";
        }
        case rai::Chain::ETHEREUM_TEST_SEPOLIA:
        {
            return "ethereum sepolia testnet";
        }
        case rai::Chain::BINANCE_SMART_CHAIN_TEST:
        {
            return "binance smart chain testnet";
        }
        default:
        {
            return std::to_string(static_cast<uint32_t>(chain));
        }
    }
}

rai::Chain rai::StringToChain(const std::string& str)
{
    if ("raicoin" == str)
    {
        return rai::Chain::RAICOIN;
    }
    else if ("bitcoin" == str)
    {
        return rai::Chain::BITCOIN;
    }
    else if ("ethereum" == str)
    {
        return rai::Chain::ETHEREUM;
    }
    else if ("binance smart chain" == str)
    {
        return rai::Chain::BINANCE_SMART_CHAIN;
    }
    else if ("raicoin testnet" == str)
    {
        return rai::Chain::RAICOIN_TEST;
    }
    else if ("bitcoin testnet" == str)
    {
        return rai::Chain::BITCOIN_TEST;
    }
    else if ("ethereum ropsten testnet" == str)
    {
        return rai::Chain::ETHEREUM_TEST_ROPSTEN;
    }
    else if ("ethereum kovan testnet" == str)
    {
        return rai::Chain::ETHEREUM_TEST_KOVAN;
    }
    else if ("ethereum rinkeby testnet" == str)
    {
        return rai::Chain::ETHEREUM_TEST_RINKEBY;
    }
    else if ("ethereum goerli testnet" == str)
    {
        return rai::Chain::ETHEREUM_TEST_GOERLI;
    }
    else if ("ethereum sepolia testnet" == str)
    {
        return rai::Chain::ETHEREUM_TEST_SEPOLIA;
    }
    else if ("binance smart chain testnet" == str)
    {
        return rai::Chain::BINANCE_SMART_CHAIN_TEST;
    }
    else
    {
        return rai::Chain::INVALID;
    }
}

rai::ErrorCode rai::CheckWrapToChain(Chain chain)
{
    switch (chain)
    {
        case rai::Chain::INVALID:
        {
            return rai::ErrorCode::TOKEN_WRAP_TO_INVALID_CHAIN;
        }
        case rai::Chain::RAICOIN:
        case rai::Chain::BITCOIN:
        {
            return rai::ErrorCode::TOKEN_WRAP_TO_UNSUPPORTED_CHAIN;
        }
        case rai::Chain::ETHEREUM:
        case rai::Chain::BINANCE_SMART_CHAIN:
        {
            return rai::ErrorCode::SUCCESS;
        }
        case rai::Chain::RAICOIN_TEST:
        case rai::Chain::BITCOIN_TEST:
        {
            return rai::ErrorCode::TOKEN_WRAP_TO_UNSUPPORTED_CHAIN;
        }
        case rai::Chain::ETHEREUM_TEST_ROPSTEN:
        case rai::Chain::ETHEREUM_TEST_KOVAN:
        case rai::Chain::ETHEREUM_TEST_RINKEBY:
        case rai::Chain::ETHEREUM_TEST_GOERLI:
        case rai::Chain::ETHEREUM_TEST_SEPOLIA:
        case rai::Chain::BINANCE_SMART_CHAIN_TEST:
        {
            return rai::ErrorCode::SUCCESS;
        }
        default:
        {
            return rai::ErrorCode::TOKEN_WRAP_TO_UNKNOWN_CHAIN;
        }
    }
}

uint64_t rai::ChainSinceHeight(Chain chain)
{
    switch (chain)
    {
        case rai::Chain::INVALID:
        {
            return 0;
        }
        case rai::Chain::RAICOIN:
        case rai::Chain::BITCOIN:
        {
            return 0;
        }
        case rai::Chain::ETHEREUM:
        {
            return 15000000;
        }
        case rai::Chain::BINANCE_SMART_CHAIN:
        {
            return 19000000;
        }
        case rai::Chain::RAICOIN_TEST:
        case rai::Chain::BITCOIN_TEST:
        {
            return 0;
        }
        case rai::Chain::ETHEREUM_TEST_ROPSTEN:
        case rai::Chain::ETHEREUM_TEST_KOVAN:
        case rai::Chain::ETHEREUM_TEST_RINKEBY:
        case rai::Chain::ETHEREUM_TEST_SEPOLIA:
        {
            return 0;
        }
        case rai::Chain::ETHEREUM_TEST_GOERLI:
        {
            return 7080000;
        }
        case rai::Chain::BINANCE_SMART_CHAIN_TEST:
        {
            return 20300000;
        }
        default:
        {
            return 0;
        }
    }
}
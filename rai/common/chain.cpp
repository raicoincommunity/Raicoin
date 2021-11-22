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
        case rai::Chain::BINANCE_SMART_CHAIN_TEST:
        {
            return "binance smart chain testnet";
        }
        default:
        {
            return "unknown";
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
    else if ("binance smart chain testnet" == str)
    {
        return rai::Chain::BINANCE_SMART_CHAIN_TEST;
    }
    else
    {
        return rai::Chain::INVALID;
    }
}

#include <rai/rai_token/parsers/evm.hpp>

bool rai::GetEvmChainId(rai::Chain chain, rai::EvmChainId& evm_chain)
{
    switch (chain)
    {
        case rai::Chain::ETHEREUM:
        {
            evm_chain = 1;
            break;
        }
        case rai::Chain::BINANCE_SMART_CHAIN:
        {
            evm_chain = 56;
            break;
        }
        case rai::Chain::ETHEREUM_TEST_ROPSTEN:
        {
            evm_chain = 3;
            break;
        }
        case rai::Chain::ETHEREUM_TEST_KOVAN:
        {
            evm_chain = 42;
            break;
        }
        case rai::Chain::ETHEREUM_TEST_RINKEBY:
        {
            evm_chain = 4;
            break;
        }
        case rai::Chain::ETHEREUM_TEST_GOERLI:
        {
            evm_chain = 5;
            break;
        }
        case rai::Chain::ETHEREUM_TEST_SEPOLIA:
        {
            evm_chain = 11155111;
            break;
        }
        case rai::Chain::BINANCE_SMART_CHAIN_TEST:
        {
            evm_chain = 97;
            break;
        }
        default:
        {
            return true;
        }
    }

    return false;
}


rai::EvmEndpoint::EvmEndpoint(uint64_t index, const rai::Url& url)
    : index_(index),
      url_(url),
      batch_(rai::EvmEndpoint::BATCH_MAX),
      consecutive_errors_(0),
      checked_(false)
{
}



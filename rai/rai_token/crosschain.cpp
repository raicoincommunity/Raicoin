#include <rai/rai_token/crosschain.hpp>


namespace
{
std::vector<std::string> DefaultEndpoints(rai::Chain chain)
{
    std::vector<std::string> result;
    switch (chain)
    {
        case rai::Chain::ETHEREUM:
        {
            result.push_back("https://rpc.ankr.com/eth");
            result.push_back("https://eth-rpc.gateway.pokt.network");
            break;
        }
        case rai::Chain::BINANCE_SMART_CHAIN:
        {
            result.push_back("https://bsc-dataseed.binance.org/");
            result.push_back("https://bsc-dataseed1.binance.org/");
            result.push_back("https://bsc-dataseed2.binance.org/");
            result.push_back("https://bsc-dataseed3.binance.org/");
            result.push_back("https://bsc-dataseed4.binance.org/");
            result.push_back("https://bsc-dataseed1.defibit.io/");
            result.push_back("https://bsc-dataseed2.defibit.io/");
            result.push_back("https://bsc-dataseed3.defibit.io/");
            result.push_back("https://bsc-dataseed4.defibit.io/");
            result.push_back("https://bsc-dataseed1.ninicoin.io/");
            result.push_back("https://bsc-dataseed2.ninicoin.io/");
            result.push_back("https://bsc-dataseed3.ninicoin.io/");
            result.push_back("https://bsc-dataseed4.ninicoin.io/");
            result.push_back("https://bsc.nodereal.io");
            break;
        }
        case rai::Chain::BINANCE_SMART_CHAIN_TEST:
        {
            result.push_back("https://data-seed-prebsc-1-s1.binance.org:8545/");
            result.push_back("https://data-seed-prebsc-2-s1.binance.org:8545");
            result.push_back("https://data-seed-prebsc-1-s2.binance.org:8545/");
            result.push_back("https://data-seed-prebsc-2-s2.binance.org:8545/");
            result.push_back("https://data-seed-prebsc-1-s3.binance.org:8545/");
            result.push_back("https://data-seed-prebsc-2-s3.binance.org:8545/");
            break;
        }
        default:
        {
            break;
        }
    }
    return result;
}
}  // namespace

rai::CrossChainConfig::CrossChainConfig()
{
    // todo:
}

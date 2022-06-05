#pragma once

#include <cstdint>
#include <string>
#include <rai/common/numbers.hpp>

namespace rai
{
enum class Chain : uint32_t
{
    INVALID                 = 0,
    RAICOIN                 = 1,
    BITCOIN                 = 2,
    ETHEREUM                = 3,
    BINANCE_SMART_CHAIN     = 4,

    RAICOIN_TEST                = 10010,
    BITCOIN_TEST                = 10020,
    ETHEREUM_TEST_ROPSTEN       = 10030,
    ETHEREUM_TEST_KOVAN         = 10031,
    ETHEREUM_TEST_RINKEBY       = 10032,
    ETHEREUM_TEST_GOERLI        = 10033,
    BINANCE_SMART_CHAIN_TEST    = 10040,

    MAX
};
std::string ChainToString(Chain);
rai::Chain StringToChain(const std::string&);
inline bool IsRaicoin(Chain chain)
{
    return chain == Chain::RAICOIN || chain == Chain::RAICOIN_TEST;
}

rai::Chain GetChainFromBlockLink(const rai::uint256_union&);

}  // namespace rai
#pragma once
#include <string>
#include <rai/common/numbers.hpp>

namespace rai
{
enum class RaiNetworks
{
    // Low work parameters, publicly known genesis key, test IP ports
    TEST,
    // Normal work parameters, secret beta genesis key, beta IP ports
    BETA,
    // Normal work parameters, secret live key, live IP ports
    LIVE,
};
rai::RaiNetworks constexpr RAI_NETWORK = RaiNetworks::ACTIVE_NETWORK;

const std::string TEST_PRIVATE_KEY =
    "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4";
// rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo
const std::string TEST_PUBLIC_KEY =
    "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";

const std::string TEST_GENESIS_BLOCK = R"%%%({
    "type": "representative",
    "opcode": "receive",
    "credit": "512",
    "counter": "1",
    "timestamp": "1564272000",
    "height": "0",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "10000000000000000",
    "link": "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
    "signature": "7CF2CDCC44E7765DF8C0B8B30E1B207489DCBB06C594EBC44D9E3D3E08AE4CCF6A1FF4D4BB19615FFEE5E6496C664E63B3430E6B9F6E8DBBD247195B95712206"
})%%%";

uint64_t constexpr EPOCH_TIMESTAMP = 1564272000;
uint64_t constexpr MAX_TIMESTAMP_DIFF = 300;
uint64_t constexpr MIN_CONFIRM_INTERVAL = 5;
uint16_t constexpr TRANSACTIONS_PER_CREDIT = 20;
uint32_t constexpr CONFIRM_WEIGHT_PERCENTAGE = 75;
uint32_t constexpr FORK_ELECTION_ROUNDS_THRESHOLD = 5;


// Votes from qualified representatives will be broadcasted
const rai::Amount QUALIFIED_REP_WEIGHT = rai::Amount(256 * rai::RAI);

rai::Amount CreditPrice(uint64_t);
rai::Amount RewardRate(uint64_t);
rai::Amount RewardAmount(const rai::Amount&, uint64_t, uint64_t);
uint64_t RewardTimestamp(uint64_t, uint64_t);
uint16_t MaxAllowedForks(uint64_t);
}  // namespace rai

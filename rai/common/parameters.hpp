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
rai::RaiNetworks constexpr RAI_NETWORK = rai::RaiNetworks::ACTIVE_NETWORK;

uint64_t constexpr TEST_EPOCH_TIMESTAMP = 1577836800;
const std::string TEST_PRIVATE_KEY =
    "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4";
// rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo
const std::string TEST_PUBLIC_KEY =
    "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";

const std::string TEST_GENESIS_BLOCK = R"%%%({
    "type": "transaction",
    "opcode": "receive",
    "credit": "512",
    "counter": "1",
    "timestamp": "1577836800",
    "height": "0",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_1nwbq4dzmo7oe8kzz6ox3bdp75n6chhfrd344yforc8bo4n9mbi66oswoac9",
    "balance": "10000000000000000",
    "link": "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
    "note_length": "0",
    "signature": "67DF01C204603C0715CAA3B1CB01B1CE1ED84E499F3432D85D01B1509DE9C51D4267FEAB2E376903A625B106818B0129FAC19B78C2F5631F8CAB48A7DF502602"
})%%%";

uint64_t constexpr MAX_TIMESTAMP_DIFF      = 300;
uint64_t constexpr MIN_CONFIRM_INTERVAL    = 10;
uint32_t constexpr TRANSACTIONS_PER_CREDIT = 20;
uint16_t constexpr MAX_ACCOUNT_CREDIT      = 65535;
uint32_t constexpr MAX_ACCOUNT_DAILY_TRANSACTIONS =
    MAX_ACCOUNT_CREDIT * TRANSACTIONS_PER_CREDIT;
uint32_t constexpr CONFIRM_WEIGHT_PERCENTAGE      = 75;
uint32_t constexpr FORK_ELECTION_ROUNDS_THRESHOLD = 5;
uint32_t constexpr AIRDROP_ACCOUNTS = 10000;
uint64_t constexpr AIRDROP_INVITED_ONLY_DURATION = 180 * 86400;
uint64_t constexpr AIRDROP_DURATION = 4 * 360 * 86400;

// Votes from qualified representatives will be broadcasted
const rai::Amount QUALIFIED_REP_WEIGHT = rai::Amount(256 * rai::RAI);

uint64_t EpochTimestamp();
std::string GenesisPublicKey();
std::string GenesisBlock();

rai::Amount CreditPrice(uint64_t);
rai::Amount RewardRate(uint64_t);
rai::Amount RewardAmount(const rai::Amount&, uint64_t, uint64_t);
uint64_t RewardTimestamp(uint64_t, uint64_t);
uint16_t MaxAllowedForks(uint64_t);
}  // namespace rai

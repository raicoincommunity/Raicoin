#pragma once
#include <string>
#include <rai/common/numbers.hpp>
#include <rai/common/chain.hpp>

namespace rai
{
#define XSTR(x) VER_STR(x)
#define VER_STR(x) #x
const char * const RAI_VERSION_STRING = XSTR(TAG_VERSION_STRING);

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
std::string NetworkString();

uint64_t constexpr TEST_EPOCH_TIMESTAMP = 1577836800;
uint64_t constexpr TEST_GENESIS_BALANCE = 10000000; //unit: RAI
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
    "signature": "67DF01C204603C0715CAA3B1CB01B1CE1ED84E499F3432D85D01B1509DE9C51D4267FEAB2E376903A625B106818B0129FAC19B78C2F5631F8CAB48A7DF502602"
})%%%";

uint64_t constexpr BETA_EPOCH_TIMESTAMP = 1585699200;
uint64_t constexpr BETA_GENESIS_BALANCE = 10010000; //unit: RAI
const std::string BETA_PUBLIC_KEY =
    "4681DC26DA5C34B59D5B320D8053D957D5FC824CE39B239B101892658F180F08";
const std::string BETA_GENESIS_BLOCK = R"%%%({
    "type": "transaction",
    "opcode": "receive",
    "credit": "512",
    "counter": "1",
    "timestamp": "1585699200",
    "height": "0",
    "account": "rai_1jn3uimfnq3nppgopeifi3bxkoyozk36srwu6gfj186kep9ji5rawm6ok3rr",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_1jngsd4mnzfb4h5j8eji5ynqbigaz16dmjjdz8us6run1ziyzuqfhz8yz3uf",
    "balance": "10010000000000000",
    "link": "4681DC26DA5C34B59D5B320D8053D957D5FC824CE39B239B101892658F180F08",
    "signature": "D265320853B6D9533EEB48E857E0C68DF3E32C5CF1B1178236EB2E0453A68B8F927415ACE8FC1E5C16C8DEB7C11DD43EFD86CA3DDB826243400AAF85DCC8D506"
})%%%";

uint64_t constexpr LIVE_EPOCH_TIMESTAMP = 1595894400;
uint64_t constexpr LIVE_GENESIS_BALANCE = 10010000; //unit: RAI
const std::string LIVE_PUBLIC_KEY =
    "0EF328F20F9B722A42A5A0873D7F4F05620E8E9D9C5BF0F61403B98022448828";
const std::string LIVE_GENESIS_BLOCK = R"%%%({
    "type": "transaction",
    "opcode": "receive",
    "credit": "512",
    "counter": "1",
    "timestamp": "1595894400",
    "height": "0",
    "account": "rai_15qm75s1z8uk7b3cda699ozny3d43t9bu94uy5u3a1xsi1j6b43apftwsfs9",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_3h5s5bgaf1jp1rofe5umxan84kiwxj3ppeuyids7zzaxahsohzchcyxqzwp6",
    "balance": "10010000000000000",
    "link": "0EF328F20F9B722A42A5A0873D7F4F05620E8E9D9C5BF0F61403B98022448828",
    "signature": "ACA0AC37F02B2C9D40929EAD69FAE1828E89728896FB06EEA8274852CDA647E9F9AB9D5868DB379253CAD2E0D011BEC4EA4FF2BBC9A369354D4D8154C796920F"
})%%%";


uint64_t constexpr MAX_TIMESTAMP_DIFF      = 300;
uint64_t constexpr MIN_CONFIRM_INTERVAL    = 10;
uint32_t constexpr TRANSACTIONS_PER_CREDIT = 20;
uint16_t constexpr MAX_ACCOUNT_CREDIT      = 65535;
uint32_t constexpr MAX_ACCOUNT_DAILY_TRANSACTIONS =
    MAX_ACCOUNT_CREDIT * TRANSACTIONS_PER_CREDIT;
uint32_t constexpr CONFIRM_WEIGHT_PERCENTAGE = 80;
uint32_t constexpr FORK_ELECTION_ROUNDS_THRESHOLD = 5;
uint32_t constexpr AIRDROP_ACCOUNTS = 10000;
uint64_t constexpr AIRDROP_INVITED_ONLY_DURATION = 180 * 86400;
uint64_t constexpr AIRDROP_DURATION = 4 * 360 * 86400;
size_t constexpr MAX_EXTENSIONS_SIZE = 256;

// Votes from qualified representatives will be broadcasted
const rai::Amount QUALIFIED_REP_WEIGHT = rai::Amount(256 * rai::RAI);

uint64_t EpochTimestamp();
std::string GenesisPublicKey();
rai::Amount GenesisBalance();
std::string GenesisBlock();

rai::Amount CreditPrice(uint64_t);
rai::Amount RewardRate(uint64_t);
rai::Amount RewardAmount(const rai::Amount&, uint64_t, uint64_t);
uint64_t RewardTimestamp(uint64_t, uint64_t);
uint16_t MaxAllowedForks(uint64_t);
rai::Chain CurrentChain();
}  // namespace rai

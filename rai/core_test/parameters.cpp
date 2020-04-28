#include <rai/common/parameters.hpp>
#include <gtest/gtest.h>
#include <rai/core_test/config.hpp>
#include <rai/core_test/test_util.hpp>
#include <rai/common/blocks.hpp>

namespace
{
void DumpSignature(const rai::Block& block)
{
    rai::RawKey raw_key;
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    rai::PublicKey public_key;
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    rai::uint512_union signature;
    signature = rai::SignMessage(raw_key, public_key, block.Hash());
    std::cout << signature.StringHex() << std::endl;
}
}

TEST(parameters, RewardAmount)
{
    uint64_t constexpr day = 24 * 60 * 60;
    rai::Amount expect(0);
    rai::Amount result = rai::RewardAmount(rai::RAI, rai::EpochTimestamp() - 1,
                                          std::numeric_limits<uint64_t>::max());
    ASSERT_EQ(expect, result);

    result = rai::RewardAmount(rai::RAI, rai::EpochTimestamp() + 2,
                               rai::EpochTimestamp() + 1);
    ASSERT_EQ(expect, result);

    expect = 7800 * rai::uRAI;
    result = rai::RewardAmount(rai::RAI, rai::EpochTimestamp(),
                               rai::EpochTimestamp() + day);
    ASSERT_EQ(expect, result);

    expect = 4600  * rai::uRAI;
    result = rai::RewardAmount(rai::RAI, rai::EpochTimestamp() + 89 * day,
                               rai::EpochTimestamp() + 90 * day);
    ASSERT_EQ(expect, result);

    expect = 3000  * rai::uRAI;
    result = rai::RewardAmount(rai::RAI, rai::EpochTimestamp() + 360 * day,
                               rai::EpochTimestamp() + 362 * day);
    ASSERT_EQ(expect, result);

    expect = 270  * rai::uRAI;
    result =
        rai::RewardAmount(rai::RAI, rai::EpochTimestamp() + (4 * 360 - 2) * day,
                          rai::EpochTimestamp() + (4 * 360 - 1) * day);
    ASSERT_EQ(expect, result);

    expect = 540 * rai::uRAI;
    result = rai::RewardAmount(2 * rai::RAI,
                               rai::EpochTimestamp() + (4 * 360 - 2) * day,
                               rai::EpochTimestamp() + (4 * 360 - 1) * day);
    ASSERT_EQ(expect, result);

    expect = 140 * rai::uRAI;
    result = rai::RewardAmount(rai::RAI,
                               rai::EpochTimestamp() + 4 * 360 * day,
                               rai::EpochTimestamp() + (4 * 360 + 1) * day);
    ASSERT_EQ(expect, result);

    expect = 70 * rai::uRAI;
    result = rai::RewardAmount(rai::RAI,
                               rai::EpochTimestamp() + 4 * 360 * day,
                               rai::EpochTimestamp() + 4 * 360 * day + day / 2);
    ASSERT_EQ(expect, result);

    expect = 1;
    result = rai::RewardAmount(rai::RAI / (70 * rai::uRAI) + 1,
                               rai::EpochTimestamp() + 4 * 360 * day,
                               rai::EpochTimestamp() + 4 * 360 * day + day / 2);
    ASSERT_EQ(expect, result);

    expect = 1;
    result = rai::RewardAmount(rai::RAI / 90, rai::EpochTimestamp(),
                               rai::EpochTimestamp() + 1);
    ASSERT_EQ(expect, result);

    expect = 0;
    result = rai::RewardAmount(rai::RAI / 91, rai::EpochTimestamp(),
                               rai::EpochTimestamp() + 1);
    ASSERT_EQ(expect, result);

    expect = 1;
    result = rai::RewardAmount(11076924, rai::EpochTimestamp(),
                               rai::EpochTimestamp() + 1);
    ASSERT_EQ(expect, result);

    expect = 0;
    result = rai::RewardAmount(11076923, rai::EpochTimestamp(),
                               rai::EpochTimestamp() + 1);
    ASSERT_EQ(expect, result);

    expect = 1;
    result = rai::RewardAmount(617142858, rai::EpochTimestamp() + 4 * 360 * day,
                               rai::EpochTimestamp() + 4 * 360 * day + 1);
    ASSERT_EQ(expect, result);

    expect = 0;
    result = rai::RewardAmount(617142857, rai::EpochTimestamp() + 4 * 360 * day,
                               rai::EpochTimestamp() + 4 * 360 * day + 1);
    ASSERT_EQ(expect, result);
}

TEST(parameters, RewardTimestamp)
{
    uint64_t constexpr day = 24 * 60 * 60;
    uint64_t begin = rai::EpochTimestamp() - 1;
    uint64_t end = rai::EpochTimestamp() + 1;

    uint64_t ret = rai::RewardTimestamp(begin, end);
    ASSERT_EQ(0, ret);

    begin = rai::EpochTimestamp() + 2;
    end = rai::EpochTimestamp() + 1;
    ret = rai::RewardTimestamp(begin, end);
    ASSERT_EQ(0, ret);

    begin = rai::EpochTimestamp();
    end = rai::EpochTimestamp() + 1;
    ret = rai::RewardTimestamp(begin, end);
    ASSERT_EQ(rai::EpochTimestamp() + day + 1, ret);

    begin = rai::EpochTimestamp();
    end = rai::EpochTimestamp() + 2;
    ret = rai::RewardTimestamp(begin, end);
    ASSERT_EQ(rai::EpochTimestamp() + day + 1, ret);

    begin = rai::EpochTimestamp();
    end = rai::EpochTimestamp() + 2 * day - 4;
    ret = rai::RewardTimestamp(begin, end);
    ASSERT_EQ(rai::EpochTimestamp() + 2 * day - 2, ret);

    begin = rai::EpochTimestamp();
    end = rai::EpochTimestamp() + 2 * day + 2;
    ret = rai::RewardTimestamp(begin, end);
    ASSERT_EQ(rai::EpochTimestamp() + 2 * day + 2, ret);

    begin = rai::EpochTimestamp();
    end = rai::EpochTimestamp() + 3 * day;
    ret = rai::RewardTimestamp(begin, end);
    ASSERT_EQ(rai::EpochTimestamp() + 3 * day, ret);
}

TEST(parameters, TEST_GENESIS_BLOCK)
{
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream(rai::TEST_GENESIS_BLOCK);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block(error_code, ptree);
    
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(false, block.CheckSignature());
    ASSERT_EQ(block.Balance(), rai::Amount(10000000 * rai::RAI));
    ASSERT_EQ(rai::TEST_EPOCH_TIMESTAMP, block.Timestamp());

    rai::PublicKey public_key;
    bool error = public_key.DecodeHex(rai::TEST_PUBLIC_KEY);
    ASSERT_EQ(false, error);
    ASSERT_EQ(public_key, block.Account());
    ASSERT_EQ(true, block.Previous().IsZero());
    ASSERT_EQ(512, block.Credit());
    ASSERT_EQ(0, block.Height());
    ASSERT_EQ(1, block.Counter());
    ASSERT_EQ(public_key, block.Link());
    ASSERT_EQ(rai::BlockOpcode::RECEIVE, block.Opcode());
}


TEST(parameters, BETA_GENESIS_BLOCK)
{
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream(rai::BETA_GENESIS_BLOCK);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block(error_code, ptree);
    
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(false, block.CheckSignature());
    ASSERT_EQ(block.Balance(), rai::Amount(10010000 * rai::RAI));
    ASSERT_EQ(rai::BETA_EPOCH_TIMESTAMP, block.Timestamp());

    rai::PublicKey public_key;
    bool error = public_key.DecodeHex(rai::BETA_PUBLIC_KEY);
    ASSERT_EQ(false, error);
    ASSERT_EQ(public_key, block.Account());
    ASSERT_EQ(true, block.Previous().IsZero());
    ASSERT_EQ(512, block.Credit());
    ASSERT_EQ(0, block.Height());
    ASSERT_EQ(1, block.Counter());
    ASSERT_EQ(public_key, block.Link());
    ASSERT_EQ(rai::BlockOpcode::RECEIVE, block.Opcode());
}
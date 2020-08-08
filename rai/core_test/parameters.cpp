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

TEST(parameters, CreditPrice)
{
    uint64_t constexpr quarter = 90 * 24 * 60 * 60;
    rai::Amount expect(0);
    rai::Amount result = rai::CreditPrice(rai::EpochTimestamp() - 1);
    ASSERT_EQ(expect, result);

    expect = 1000 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp());
    ASSERT_EQ(expect, result);

    result = rai::CreditPrice(rai::EpochTimestamp() + quarter);
    ASSERT_EQ(expect, result);

    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 2);
    ASSERT_EQ(expect, result);

    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 3);
    ASSERT_EQ(expect, result);

    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 4 - 1);
    ASSERT_EQ(expect, result);

    expect = 900 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 4);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 5 - 1);
    ASSERT_EQ(expect, result);

    expect = 800 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 5);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 6 - 1);
    ASSERT_EQ(expect, result);

    expect = 700 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 6);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 7 - 1);
    ASSERT_EQ(expect, result);

    expect = 600 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 7);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 8 - 1);
    ASSERT_EQ(expect, result);

    expect = 500 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 8);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 10 - 1);
    ASSERT_EQ(expect, result);

    expect = 400 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 10);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 12 - 1);
    ASSERT_EQ(expect, result);

    expect = 300 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 12);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 14 - 1);
    ASSERT_EQ(expect, result);

    expect = 200 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 14);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 16 - 1);
    ASSERT_EQ(expect, result);

    expect = 100 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 16);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 18 - 1);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 18);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 20 - 1);
    ASSERT_EQ(expect, result);

    expect = 90 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 20);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 21 - 1);
    ASSERT_EQ(expect, result);

    expect = 80 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 21);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 22 - 1);
    ASSERT_EQ(expect, result);

    expect = 70 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 22);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 23 - 1);
    ASSERT_EQ(expect, result);

    expect = 60 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 23);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 24 - 1);
    ASSERT_EQ(expect, result);

    expect = 50 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 24);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 26 - 1);
    ASSERT_EQ(expect, result);

    expect = 40 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 26);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 28 - 1);
    ASSERT_EQ(expect, result);

    expect = 30 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 28);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 30 - 1);
    ASSERT_EQ(expect, result);

    expect = 20 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 30);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 32 - 1);
    ASSERT_EQ(expect, result);

    expect = 10 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 32);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 34 - 1);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 34);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 36 - 1);
    ASSERT_EQ(expect, result);

    expect = 9 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 36);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 37 - 1);
    ASSERT_EQ(expect, result);

    expect = 8 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 37);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 38 - 1);
    ASSERT_EQ(expect, result);

    expect = 7 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 38);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 39 - 1);
    ASSERT_EQ(expect, result);

    expect = 6 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 39);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 40 - 1);
    ASSERT_EQ(expect, result);

    expect = 5 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 40);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 42 - 1);
    ASSERT_EQ(expect, result);

    expect = 4 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 42);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 44 - 1);
    ASSERT_EQ(expect, result);

    expect = 3 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 44);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 46 - 1);
    ASSERT_EQ(expect, result);

    expect = 2 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 46);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 48 - 1);
    ASSERT_EQ(expect, result);

    expect = 1 * rai::mRAI;
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 48);
    ASSERT_EQ(expect, result);
    result = rai::CreditPrice(rai::EpochTimestamp() + quarter * 50 - 1);
    ASSERT_EQ(expect, result);
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

TEST(parameters, LIVE_GENESIS_BLOCK)
{
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream(rai::LIVE_GENESIS_BLOCK);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block(error_code, ptree);
    
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(false, block.CheckSignature());
    ASSERT_EQ(block.Balance(), rai::Amount(10010000 * rai::RAI));
    ASSERT_EQ(rai::LIVE_EPOCH_TIMESTAMP, block.Timestamp());

    rai::PublicKey public_key;
    bool error = public_key.DecodeHex(rai::LIVE_PUBLIC_KEY);
    ASSERT_EQ(false, error);
    ASSERT_EQ(public_key, block.Account());
    ASSERT_EQ(true, block.Previous().IsZero());
    ASSERT_EQ(512, block.Credit());
    ASSERT_EQ(0, block.Height());
    ASSERT_EQ(1, block.Counter());
    ASSERT_EQ(public_key, block.Link());
    ASSERT_EQ(rai::BlockOpcode::RECEIVE, block.Opcode());
}
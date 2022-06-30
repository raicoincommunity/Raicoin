#include <chrono>
#include <thread>
#include <gtest/gtest.h>
#include <rai/core_test/test_util.hpp>
#include <rai/common/blocks.hpp>
#include <rai/rai_token/chainwaiting.hpp>


TEST(ChainWaiting, Basic)
{
    rai::ChainWaiting waiting;
    rai::Account account1, account2, account3;
    account1.Random();
    account2.Random();
    account3.Random();
    rai::TxBlockBuilder builder1(account1, rai::Account(1), nullptr);
    rai::TxBlockBuilder builder2(account2, rai::Account(2), nullptr);
    rai::TxBlockBuilder builder3(account3, rai::Account(3), nullptr);
    std::shared_ptr<rai::Block> block;
    rai::ErrorCode error_code =
        builder1.Receive(block, rai::BlockHash(1), 100 * rai::RAI,
                         rai::CurrentTimestamp() - 10, rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(0, waiting.Size());
    waiting.Insert(rai::Chain::ETHEREUM, 100, block);
    EXPECT_EQ(1, waiting.Size());

    error_code = builder1.Send(block, rai::Account(1), 10 * rai::RAI,
                               rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(rai::Chain::BINANCE_SMART_CHAIN, 100, block);
    EXPECT_EQ(2, waiting.Size());
    error_code =
        builder2.Receive(block, rai::BlockHash(2), 200 * rai::RAI,
                         rai::CurrentTimestamp() - 20, rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(rai::Chain::BINANCE_SMART_CHAIN, 101, block);
    EXPECT_EQ(3, waiting.Size());
    error_code = builder2.Send(block, rai::Account(2), 100 * rai::RAI,
                               rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(rai::Chain::BINANCE_SMART_CHAIN, 101, block);
    EXPECT_EQ(4, waiting.Size());

    error_code =
        builder3.Receive(block, rai::BlockHash(3), 300 * rai::RAI,
                         rai::CurrentTimestamp() - 30, rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(rai::Chain::BINANCE_SMART_CHAIN, 102, block);
    EXPECT_EQ(5, waiting.Size());
    error_code =
        builder3.Change(block, rai::Account(5), rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(rai::Chain::BINANCE_SMART_CHAIN, 102, block);
    EXPECT_EQ(6, waiting.Size());


    auto removed = waiting.Remove(rai::Chain::BINANCE_SMART_CHAIN, 99);
    EXPECT_EQ(0, removed.size());
    EXPECT_EQ(6, waiting.Size());

    removed = waiting.Remove(rai::Chain::BINANCE_SMART_CHAIN, 101);
    EXPECT_EQ(3, removed.size());
    EXPECT_EQ(3, waiting.Size());
    EXPECT_EQ(rai::Chain::BINANCE_SMART_CHAIN, removed[0].chain_);
    EXPECT_EQ(100, removed[0].height_);
    EXPECT_EQ(account1, removed[0].block_->Account());
    EXPECT_EQ(1, removed[0].block_->Height());
    EXPECT_EQ(account2, removed[1].block_->Account());
    EXPECT_EQ(true, removed[1].block_->Height() <= 1);

    removed = waiting.Remove(rai::Chain::ETHEREUM, 200);
    EXPECT_EQ(1, removed.size());
    EXPECT_EQ(2, waiting.Size());

    removed = waiting.Remove(rai::Chain::BINANCE_SMART_CHAIN, 200);
    EXPECT_EQ(2, removed.size());
    EXPECT_EQ(0, waiting.Size());
}
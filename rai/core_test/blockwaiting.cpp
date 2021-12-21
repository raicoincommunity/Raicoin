#include <chrono>
#include <thread>
#include <gtest/gtest.h>
#include <rai/core_test/test_util.hpp>
#include <rai/common/blocks.hpp>
#include <rai/app/blockwaiting.hpp>

TEST(BlockWaiting, Basic)
{
    rai::BlockWaiting waiting;
    rai::Account account1, account2, account3, account4, account5;
    account1.Random();
    account2.Random();
    account3.Random();
    account4.Random();
    account5.Random();

    rai::TxBlockBuilder builder1(account1, rai::Account(1), nullptr);
    rai::TxBlockBuilder builder2(account2, rai::Account(2), nullptr);
    rai::TxBlockBuilder builder3(account3, rai::Account(3), nullptr);
    std::shared_ptr<rai::Block> block;
    rai::ErrorCode error_code =
        builder1.Receive(block, rai::BlockHash(1), 100 * rai::RAI,
                         rai::CurrentTimestamp() - 10, rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(0, waiting.Size());
    waiting.Insert(account4, 1, block, true);
    EXPECT_EQ(1, waiting.Size());

    error_code = builder1.Send(block, rai::Account(1), 10 * rai::RAI,
                               rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(account4, 2, block, false);
    EXPECT_EQ(2, waiting.Size());
    error_code =
        builder2.Receive(block, rai::BlockHash(2), 200 * rai::RAI,
                         rai::CurrentTimestamp() - 20, rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(account4, 2, block, true);
    EXPECT_EQ(3, waiting.Size());
    error_code = builder2.Send(block, rai::Account(2), 100 * rai::RAI,
                               rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(account4, 3, block, true);
    EXPECT_EQ(4, waiting.Size());

    error_code =
        builder3.Receive(block, rai::BlockHash(3), 300 * rai::RAI,
                         rai::CurrentTimestamp() - 30, rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(account5, 4, block, true);
    EXPECT_EQ(5, waiting.Size());
    error_code =
        builder3.Change(block, rai::Account(5), rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(account5, 3, block, true);
    EXPECT_EQ(6, waiting.Size());

    bool exists = waiting.Exists(account3, 1);
    EXPECT_EQ(false, exists);

    exists = waiting.Exists(account4, 1);
    EXPECT_EQ(true, exists);

    exists = waiting.Exists(account4, 2);
    EXPECT_EQ(true, exists);

    exists = waiting.Exists(account4, 3);
    EXPECT_EQ(true, exists);

    exists = waiting.Exists(account4, 4);
    EXPECT_EQ(false, exists);

    exists = waiting.Exists(account5, 3);
    EXPECT_EQ(true, exists);

    exists = waiting.Exists(account5,4);
    EXPECT_EQ(true, exists);

    exists = waiting.Exists(account5,4);
    EXPECT_EQ(true, exists);

    auto removed = waiting.Remove(account3, 10);
    EXPECT_EQ(0, removed.size());
    EXPECT_EQ(6, waiting.Size());

    removed = waiting.Remove(account4, 2);
    EXPECT_EQ(3, removed.size());
    EXPECT_EQ(3, waiting.Size());
    EXPECT_EQ(account4, removed[0].for_.account_);
    EXPECT_EQ(1, removed[0].for_.height_);
    EXPECT_EQ(true, removed[0].confirmed_);
    EXPECT_EQ(account1, removed[0].block_->Account());
    EXPECT_EQ(0, removed[0].block_->Height());
    EXPECT_EQ(account4, removed[1].for_.account_);
    EXPECT_EQ(2, removed[1].for_.height_);
    EXPECT_EQ(false, removed[1].confirmed_);
    EXPECT_EQ(account4, removed[2].for_.account_);
    EXPECT_EQ(2, removed[2].for_.height_);
    EXPECT_EQ(true, removed[2].confirmed_);
    if (removed[1].block_->Account() == account1)
    {
        EXPECT_EQ(1, removed[1].block_->Height());
        EXPECT_EQ(0, removed[2].block_->Height());
        EXPECT_EQ(account2, removed[2].block_->Account());
    }
    else if (removed[1].block_->Account() == account2)
    {
        EXPECT_EQ(0, removed[1].block_->Height());
        EXPECT_EQ(1, removed[2].block_->Height());
        EXPECT_EQ(account1, removed[2].block_->Account());
    }
    else
    {
        ASSERT_EQ(false, true);
    }

    removed = waiting.Remove(account5, 3);
    EXPECT_EQ(1, removed.size());
    EXPECT_EQ(2, waiting.Size());
    EXPECT_EQ(account5, removed[0].for_.account_);
    EXPECT_EQ(3, removed[0].for_.height_);
    EXPECT_EQ(true, removed[0].confirmed_);

    auto left = waiting.List();
    EXPECT_EQ(2, left.size());
    if (left[0].for_.account_ == account4)
    {
        EXPECT_EQ(3, left[0].for_.height_);
        EXPECT_EQ(4, left[1].for_.height_);
    }
    else
    {
        EXPECT_EQ(4, left[0].for_.height_);
        EXPECT_EQ(3, left[1].for_.height_);
    }
}

TEST(BlockWaiting, Age)
{
    rai::BlockWaiting waiting;
    rai::Account account1, account2, account3, account4;
    account1.Random();
    account2.Random();
    account3.Random();
    account4.Random();
    rai::TxBlockBuilder builder1(account1, rai::Account(1), nullptr);
    rai::TxBlockBuilder builder2(account2, rai::Account(2), nullptr);
    std::shared_ptr<rai::Block> block;
    rai::ErrorCode error_code =
        builder1.Receive(block, rai::BlockHash(1), 100 * rai::RAI,
                         rai::CurrentTimestamp() - 10, rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(account3, 1, block, true);

    error_code =
        builder2.Receive(block, rai::BlockHash(2), 200 * rai::RAI,
                         rai::CurrentTimestamp() - 20, rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(account4, 2, block, true);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    error_code = builder1.Send(block, rai::Account(1), 10 * rai::RAI,
                               rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(account3, 3, block, false);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    error_code = builder2.Send(block, rai::Account(2), 100 * rai::RAI,
                               rai::CurrentTimestamp());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    waiting.Insert(account4, 4, block, true);

    EXPECT_EQ(4, waiting.Size());

    waiting.Age(3);
    EXPECT_EQ(2, waiting.Size());
    EXPECT_EQ(false, waiting.Exists(account3, 1));
    EXPECT_EQ(false, waiting.Exists(account4, 2));
    EXPECT_EQ(true, waiting.Exists(account3, 3));
    EXPECT_EQ(true, waiting.Exists(account4, 4));
    EXPECT_EQ(2, waiting.Size());

    waiting.Age(1);
    EXPECT_EQ(1, waiting.Size());
    EXPECT_EQ(false, waiting.Exists(account3, 3));
    EXPECT_EQ(true, waiting.Exists(account4, 4));
    EXPECT_EQ(1, waiting.Size());
}
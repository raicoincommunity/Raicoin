#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <rai/core_test/config.hpp>
#include <rai/core_test/test_util.hpp>
#include <rai/secure/ledger.hpp>

TEST(Ledger, Abort)
{
    auto data_file = boost::filesystem::current_path() / "ledger_test.ldb";
    auto lock_file = boost::filesystem::current_path() / "ledger_test.ldb-lock";

    if (boost::filesystem::exists(data_file))
    {
        boost::filesystem::remove(data_file);
    }
    if (boost::filesystem::exists(lock_file))
    {
        boost::filesystem::remove(lock_file);
    }

    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Store store(error_code, data_file);
        rai::Ledger ledger(error_code, store, rai::LedgerType::NODE);

        {
            rai::Transaction transaction(error_code, ledger, true);
            EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
            rai::AccountInfo info;
            bool error =
                ledger.AccountInfoPut(transaction, rai::Account(1), info);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoPut(transaction, rai::Account(2), info);
            EXPECT_EQ(false, error);
            error = ledger.VersionPut(transaction, 1);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(1), info);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(2), info);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(3), info);
            EXPECT_EQ(true, error);
            uint32_t version = 0;
            error = ledger.VersionGet(transaction, version);
            EXPECT_EQ(false, error);
            EXPECT_EQ(1, version);
        }

        {
            rai::Transaction transaction(error_code, ledger, true);
            EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
            rai::AccountInfo info;
            bool error =
                ledger.AccountInfoPut(transaction, rai::Account(3), info);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoPut(transaction, rai::Account(4), info);
            EXPECT_EQ(false, error);
            error = ledger.VersionPut(transaction, 2);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(1), info);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(2), info);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(3), info);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(4), info);
            EXPECT_EQ(false, error);
            uint32_t version = 0;
            error = ledger.VersionGet(transaction, version);
            EXPECT_EQ(false, error);
            EXPECT_EQ(2, version);
            transaction.Abort();
        }

        {
            rai::Transaction transaction(error_code, ledger, true);
            EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
            rai::AccountInfo info;
            bool error =
                ledger.AccountInfoGet(transaction, rai::Account(1), info);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(2), info);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(3), info);
            EXPECT_EQ(true, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(4), info);
            EXPECT_EQ(true, error);
            uint32_t version = 0;
            error = ledger.VersionGet(transaction, version);
            EXPECT_EQ(false, error);
            EXPECT_EQ(1, version);
        }

        {
            rai::Transaction transaction(error_code, ledger, false);
            EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
            rai::AccountInfo info;
            bool error =
                ledger.AccountInfoGet(transaction, rai::Account(1), info);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(2), info);
            EXPECT_EQ(false, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(3), info);
            EXPECT_EQ(true, error);
            error = ledger.AccountInfoGet(transaction, rai::Account(4), info);
            EXPECT_EQ(true, error);
            uint32_t version = 0;
            error = ledger.VersionGet(transaction, version);
            EXPECT_EQ(false, error);
            EXPECT_EQ(1, version);
        }
    }

    boost::filesystem::remove(data_file);
    boost::filesystem::remove(lock_file);
}
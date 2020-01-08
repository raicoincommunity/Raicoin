#include <rai/secure/store.hpp>

rai::Store::Store(rai::ErrorCode& error_code,
                  const boost::filesystem::path& path)
    : env_(error_code, path, 128),
      accounts_(0),
      blocks_(0),
      blocks_index_(0),
      meta_(0),
      receivables_(0),
      rewardables_(0),
      rollbacks_(0),
      forks_(0),
      wallets_(0)
{
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    rai::MdbTransaction transaction(error_code, env_, nullptr, true);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    auto ret = mdb_dbi_open(transaction, "accounts", MDB_CREATE, &accounts_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "blocks", MDB_CREATE, &blocks_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "blocks_index", MDB_CREATE, &blocks_index_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "meta", MDB_CREATE, &meta_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "receivables", MDB_CREATE, &receivables_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "rewardables", MDB_CREATE, &rewardables_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "rollbacks", MDB_CREATE, &rollbacks_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "forks", MDB_CREATE, &forks_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "wallets", MDB_CREATE, &wallets_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }
}

bool rai::Store::Put(MDB_txn* txn, MDB_dbi dbi, MDB_val* key, MDB_val* value)
{
    auto ret = mdb_put(txn, dbi, key, value, 0);
    if (ret != MDB_SUCCESS)
    {
        return true;
    }

    return false;
}

bool rai::Store::Get(MDB_txn* txn, MDB_dbi dbi, MDB_val* key,
                     MDB_val* value) const
{
    auto ret = mdb_get(txn, dbi, key, value);
    assert(ret == MDB_SUCCESS || ret == MDB_NOTFOUND);
    if (ret != MDB_SUCCESS)
    {
        return true;
    }

    return false;
}

bool rai::Store::Del(MDB_txn* txn, MDB_dbi dbi, MDB_val* key, MDB_val* value)
{
    auto ret = mdb_del(txn, dbi, key, value);
    assert(ret == MDB_SUCCESS || ret == MDB_NOTFOUND);
    if (ret != MDB_SUCCESS)
    {
        return true;
    }

    return false;
}



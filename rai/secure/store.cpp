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
      wallets_(0),
      sources_(0),
      alias_(0),
      alias_block_(0),
      alias_index_(0),
      alias_dns_index_(0),
      account_tokens_info_(0),
      token_block_(0),
      account_token_link_(0),
      account_token_info_(0),
      token_info_(0),
      token_receivable_(0),
      token_id_info_(0),
      token_holders_(0),
      account_token_id_(0),
      token_transfer_(0),
      token_id_transfer_(0),
      swap_main_account_(0),
      account_swap_info_(0),
      order_info_(0),
      order_index_(0),
      swap_info_(0),
      inquiry_waiting_(0),
      take_waiting_(0),
      order_swap_index_(0),
      token_swap_index_(0),
      take_nack_block_(0),
      maker_swap_index_(0),
      binding_count_(0),
      binding_entries_(0)
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

    ret = mdb_dbi_open(transaction, "sources", MDB_CREATE, &sources_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "alias", MDB_CREATE, &alias_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "alias_block", MDB_CREATE, &alias_block_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "alias_index", MDB_CREATE, &alias_index_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "alias_dns_index", MDB_CREATE,
                       &alias_dns_index_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "account_tokens_info", MDB_CREATE,
                       &account_tokens_info_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "token_block", MDB_CREATE,
                       &token_block_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "account_token_link", MDB_CREATE,
                       &account_token_link_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "account_token_info", MDB_CREATE,
                       &account_token_info_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "token_info", MDB_CREATE, &token_info_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "token_receivable", MDB_CREATE,
                       &token_receivable_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret =
        mdb_dbi_open(transaction, "token_id_info", MDB_CREATE, &token_id_info_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret =
        mdb_dbi_open(transaction, "token_holders", MDB_CREATE, &token_holders_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "account_token_id", MDB_CREATE,
                       &account_token_id_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "token_transfer", MDB_CREATE,
                       &token_transfer_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "token_id_transfer", MDB_CREATE,
                       &token_id_transfer_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "swap_main_account", MDB_CREATE,
                       &swap_main_account_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "account_swap_info", MDB_CREATE,
                       &account_swap_info_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "order_info", MDB_CREATE, &order_info_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "order_index", MDB_CREATE, &order_index_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "swap_info", MDB_CREATE, &swap_info_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "inquiry_waiting", MDB_CREATE,
                       &inquiry_waiting_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "take_waiting", MDB_CREATE, &take_waiting_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "order_swap_index", MDB_CREATE,
                       &order_swap_index_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "token_swap_index", MDB_CREATE,
                       &token_swap_index_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "take_nack_block", MDB_CREATE,
                       &take_nack_block_);
    if (ret != MDB_SUCCESS)
    {
        error_code = rai::ErrorCode::MDB_DBI_OPEN;
        return;
    }

    ret = mdb_dbi_open(transaction, "maker_swap_index", MDB_CREATE,
                       &maker_swap_index_);
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



#pragma once
#include <boost/filesystem.hpp>
#include <lmdb/libraries/liblmdb/lmdb.h>
#include <rai/common/errors.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/util.hpp>
#include <rai/secure/lmdb.hpp>

namespace rai
{

class Store
{
public:
    Store(rai::ErrorCode&, const boost::filesystem::path&);
    Store(const rai::Store&) = delete;
    bool Put(MDB_txn*, MDB_dbi, MDB_val*, MDB_val*);
    bool Get(MDB_txn*, MDB_dbi, MDB_val*, MDB_val*) const;
    bool Del(MDB_txn*, MDB_dbi, MDB_val*, MDB_val*);

    rai::MdbEnv env_;

    /***************************************************************************
     Key: rai::Account
     Value: rai::AccountInfo
     **************************************************************************/
    MDB_dbi accounts_;

    /***************************************************************************
     Key: rai::BlockHash
     Value: rai::Block,rai::BlockHash
     **************************************************************************/
    MDB_dbi blocks_;

    /***************************************************************************
     Key: rai::Account, uint64_t
     Value: rai::BlockHash
     **************************************************************************/
    MDB_dbi blocks_index_;

    /***************************************************************************
     Meta information, such as version.
     Key: uint32_t
     Value: uint32_t
     **************************************************************************/
     MDB_dbi meta_;

    /***************************************************************************
     Key: rai::Account, rai::BlockHash
     Value: rai::ReceivableInfo
     **************************************************************************/
    MDB_dbi receivables_;

    /***************************************************************************
     Key: rai::Account,rai::BlockHash
     Value: rai::RewardableInfo
     **************************************************************************/
    MDB_dbi rewardables_;

    /***************************************************************************
     Key: rai::BlockHash
     Value: rai::Block
     **************************************************************************/
    MDB_dbi rollbacks_;

    /***************************************************************************
     Key: rai::Account, uint64_t
     Value: rai::Block,rai::Block
     **************************************************************************/
    MDB_dbi forks_;

    /***************************************************************************
     Key: uint32_t,uint32_t
     Value: rai::WalletInfo
     **************************************************************************/
    MDB_dbi wallets_;
};
} // namespace rai
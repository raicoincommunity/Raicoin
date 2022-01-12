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

    /***************************************************************************
     Key: rai::BlockHash
     Value: rai::Block/junk
     **************************************************************************/
    MDB_dbi sources_;

    /***************************************************************************
     Key: rai::Account
     Value: rai::AliasInfo
     **************************************************************************/
    MDB_dbi alias_;

    /***************************************************************************
     Key: rai::Account, uint64_t
     Value: rai::AliasBlock
     **************************************************************************/
    MDB_dbi alias_block_;

    /***************************************************************************
     Key: rai::AliasIndex
     Value: uint8_t
     **************************************************************************/
    MDB_dbi alias_index_;

    /***************************************************************************
     Key: rai::AliasIndex
     Value: uint8_t
     **************************************************************************/
    MDB_dbi alias_dns_index_;

    /***************************************************************************
     Key: rai::Account
     Value: rai::AccountTokensInfo
     **************************************************************************/
    MDB_dbi account_tokens_info_;

    /***************************************************************************
     Key: rai::Account, uint64_t
     Value: rai::TokenBlock
     **************************************************************************/
    MDB_dbi token_block_;

    /***************************************************************************
     Key: rai::AccountTokenLinkKey
     Value: uint64_t
     **************************************************************************/
    MDB_dbi account_token_link_;

    /***************************************************************************
     Key: rai::Account, rai::Chain, rai::TokenAddress
     Value: rai::AccountTokenInfo
     **************************************************************************/
    MDB_dbi account_token_info_;

    /***************************************************************************
     Key: rai::TokenKey
     Value: rai::TokenInfo
     **************************************************************************/
    MDB_dbi token_info_;

    /***************************************************************************
     Key: rai::TokenKey
     Value: rai::TokenInfo
     **************************************************************************/
    MDB_dbi token_receivable_;

    /***************************************************************************
     Key: rai::TokenKey, rai::TokenValue
     Value: rai::TokenIdInfo
     **************************************************************************/
    MDB_dbi token_id_info_;

    /***************************************************************************
     Key: rai::TokenKey, rai::TokenValue, rai::Account
     Value: rai::TokenValue
     **************************************************************************/
    MDB_dbi token_holders_;

    /***************************************************************************
     Key: rai::Account, rai::TokenKey, rai::TokenValue
     Value: uint64_t
     **************************************************************************/
    MDB_dbi account_token_id_;

    /***************************************************************************
     Key: rai::TokenKey, uint64_t, rai::Account, uint64_t
     Value: uint64_t
     **************************************************************************/
    MDB_dbi token_transfer_;

    /***************************************************************************
     Key: rai::TokenTransferKey
     Value: rai::Account, uint64_t
     **************************************************************************/
    MDB_dbi token_id_transfer_;

    /***************************************************************************
     Key: rai::Account
     Value: rai::Account
     **************************************************************************/
    MDB_dbi swap_main_account_;

    /***************************************************************************
     Key: rai::Account
     Value: rai::AccountSwapInfo
     **************************************************************************/
    MDB_dbi account_swap_info_;

    /***************************************************************************
     Key: rai::Account, uint64_t
     Value: rai::OrderInfo
     **************************************************************************/
    MDB_dbi order_info_;

    /***************************************************************************
     Key: rai::OrderIndex
     Value: uint8_t
     **************************************************************************/
    MDB_dbi order_index_;

    /***************************************************************************
     Key: rai::Account, uint64_t
     Value: rai::SwapInfo
     **************************************************************************/
    MDB_dbi swap_info_;

    /***************************************************************************
     Key: rai::InquiryWaiting
     Value: uint8_t
     **************************************************************************/
    MDB_dbi inquiry_waiting_;

    /***************************************************************************
     Key: rai::TakeWaiting
     Value: uint8_t
     **************************************************************************/
    MDB_dbi take_waiting_;

    /***************************************************************************
     Key: rai::OrderSwapIndex
     Value: uint64_t
     **************************************************************************/
    MDB_dbi order_swap_index_;

    /***************************************************************************
     Key: rai::TokenSwapIndex
     Value: uint64_t
     **************************************************************************/
    MDB_dbi token_swap_index_;
};
} // namespace rai
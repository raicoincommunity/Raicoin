#pragma once
#include <unordered_map>
#include <string>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/blocks.hpp>
#include <rai/common/token.hpp>
#include <rai/common/chain.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/numbers.hpp>
#include <rai/secure/util.hpp>
#include <rai/secure/store.hpp>

namespace rai
{
class Ledger;

class RepWeightOpration
{
public:
    RepWeightOpration(bool, const rai::Account&, const rai::Amount&);

private:
    friend class rai::Ledger;

    bool add_;
    rai::Account representative_;
    rai::Amount weight_;
};

class Transaction
{
public:
    Transaction(rai::ErrorCode&, rai::Ledger&, bool);
    Transaction(const rai::Transaction&) = delete;
    ~Transaction();
    rai::Transaction& operator=(const rai::Transaction&) = delete;
    void Abort();

private:
    friend class rai::Ledger;

    rai::Ledger& ledger_;
    bool write_;
    bool aborted_;
    rai::MdbTransaction mdb_transaction_;
    std::vector<rai::RepWeightOpration> rep_weight_operations_;

};

class Iterator
{
public:
    Iterator();
    Iterator(rai::StoreIterator&&);
    Iterator(const rai::Iterator&) = delete;
    Iterator(rai::Iterator&&);

    rai::Iterator& operator++();
    rai::Iterator& operator=(rai::Iterator&&);
    rai::Iterator& operator=(const rai::Iterator&) = delete;
    bool operator==(const rai::Iterator&) const;
    bool operator!=(const rai::Iterator&) const;

private:
    friend class rai::Ledger;

    rai::StoreIterator store_it_;

};

class AccountInfo
{
public:
    AccountInfo();
    AccountInfo(rai::BlockType, const rai::BlockHash&); // first block
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);
    bool Confirmed(uint64_t) const;
    bool Valid() const;
    bool Restricted() const;

    rai::BlockType type_;
    uint16_t forks_;
    uint64_t head_height_;
    uint64_t tail_height_;
    uint64_t confirmed_height_;
    rai::BlockHash head_;
    rai::BlockHash tail_;
};

class AliasInfo
{
public:
    AliasInfo();
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    uint64_t head_;
    uint64_t name_valid_;
    uint64_t dns_valid_;
    std::vector<uint8_t> name_;
    std::vector<uint8_t> dns_;
};

class AliasBlock
{
public:
    AliasBlock();
    AliasBlock(uint64_t, const rai::BlockHash&, int32_t);
    AliasBlock(uint64_t, const rai::BlockHash&, int32_t, uint8_t,
               const std::vector<uint8_t>&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    uint64_t previous_;
    rai::BlockHash hash_;
    int32_t status_; // rai::ErrorCode
    uint8_t op_;
    std::vector<uint8_t> value_;
};

class AliasIndex
{
public:
    AliasIndex() = default;
    AliasIndex(const rai::Prefix&, const rai::Account&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::Prefix prefix_;
    rai::Account account_;
};

class TokenKey
{
public:
    TokenKey();
    TokenKey(rai::Chain, const rai::TokenAddress&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);
    bool Valid() const;

    rai::Chain chain_;
    rai::TokenAddress address_;
};

class TokenTransfer
{
public:
    TokenTransfer();
    TokenTransfer(const rai::TokenKey&, uint64_t, const rai::Account&,
                  uint64_t);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::TokenKey token_;
    uint64_t ts_comp_; // timestamp's complement
    rai::Account account_;
    uint64_t height_;
};

class TokenIdTransferKey
{
public:
    TokenIdTransferKey();
    TokenIdTransferKey(const rai::TokenKey&, const rai::TokenValue&, uint64_t);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::TokenKey token_;
    rai::TokenValue id_;
    uint64_t index_comp_; // index's complement
};

class TokenInfo
{
public:
    TokenInfo();
    TokenInfo(rai::TokenType, const std::string&, const std::string&, uint8_t,
              bool, bool, bool, const rai::TokenValue&, uint64_t);
    TokenInfo(rai::TokenType, const std::string&, const std::string&, bool,
              bool, const rai::TokenValue&, const std::string, uint64_t);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::TokenType type_;
    std::string symbol_;
    std::string name_;
    uint8_t decimals_;
    bool burnable_;
    bool mintable_;
    bool circulable_;
    uint64_t holders_;
    uint64_t transfers_;
    uint64_t swaps_;
    uint64_t created_at_;
    rai::TokenValue total_supply_;
    rai::TokenValue cap_supply_;
    rai::TokenValue local_supply_;
    std::string base_uri_;
};

class TokenBlock
{
public:
    enum class ValueOp : uint8_t
    {
        NONE = 0,
        INCREASE = 1,
        DECREASE = 2,
    };

    TokenBlock();
    TokenBlock(uint64_t, const rai::BlockHash&, rai::ErrorCode,
               const rai::TokenValue&, rai::TokenBlock::ValueOp,
               const rai::TokenKey&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);


    uint64_t previous_;
    rai::BlockHash hash_;
    rai::ErrorCode status_;
    rai::TokenValue value_;
    ValueOp value_op_;
    rai::TokenKey token_;
};

class TokenHolder
{
public:
    TokenHolder();
    TokenHolder(rai::Chain, const rai::TokenAddress&, const rai::TokenValue&,
                const rai::Account&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::TokenKey token_;
    rai::TokenValue balance_comp_; // balance's complement
    rai::Account account_;
};

class TokenReceivableKey
{
public:
    TokenReceivableKey();
    TokenReceivableKey(const rai::Account&, const rai::TokenKey&, rai::Chain,
                       const rai::BlockHash&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::Account to_;
    rai::TokenKey token_;
    rai::Chain chain_;
    rai::BlockHash tx_hash_;
};

class TokenReceivable
{
public:
    TokenReceivable();
    TokenReceivable(const rai::Account&, const rai::TokenValue&, uint64_t,
                    rai::TokenType, rai::TokenSource);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::Account from_;
    rai::TokenValue value_;
    uint64_t block_height_;
    rai::TokenType token_type_;
    rai::TokenSource source_;
};

class TokenIdInfo
{
public:
    TokenIdInfo();
    TokenIdInfo(const std::string&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    bool burned_;
    bool unmapped_;
    bool wrapped_;
    uint64_t transfers_;
    std::string uri_;
};

class AccountTokensInfo
{
public:
    AccountTokensInfo();
    AccountTokensInfo(uint64_t, uint64_t);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    uint64_t head_;
    uint64_t blocks_;
};

class AccountTokenInfo
{
public:
    AccountTokenInfo();
    AccountTokenInfo(uint64_t, uint64_t, const rai::TokenValue&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    uint64_t head_;
    uint64_t blocks_;
    rai::TokenValue balance_;
};

class AccountTokenId
{
public:
    AccountTokenId();
    AccountTokenId(const rai::Account&, const rai::TokenKey&,
                   const rai::TokenValue&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::Account account_;
    rai::TokenKey token_;
    rai::TokenValue id_;
};

class AccountTokenLink
{
public:
    AccountTokenLink();
    AccountTokenLink(const rai::Account&, rai::Chain, const rai::TokenAddress&,
                     uint64_t);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::Account account_;
    rai::TokenKey token_;
    uint64_t height_;
};

class OrderIndex
{
public:
    OrderIndex();
    OrderIndex(rai::Chain, const rai::TokenAddress&, rai::Chain,
               const rai::TokenAddress&, const rai::SwapRate&,
               const rai::Account&, uint64_t);
    OrderIndex(rai::Chain, const rai::TokenAddress&, const rai::TokenValue&,
               rai::Chain, const rai::TokenAddress&, const rai::SwapRate&,
               const rai::Account&, uint64_t);
    OrderIndex(rai::Chain, const rai::TokenAddress&, rai::Chain,
               const rai::TokenAddress&, const rai::TokenValue&,
               const rai::SwapRate&, const rai::Account&, uint64_t);
    OrderIndex(rai::Chain, const rai::TokenAddress&, const rai::TokenValue&,
               rai::Chain, const rai::TokenAddress&, const rai::TokenValue&,
               const rai::SwapRate&, const rai::Account&, uint64_t);
    OrderIndex(const rai::TokenKey&, rai::TokenType, const rai::TokenKey&,
               rai::TokenType);
    OrderIndex(const rai::TokenKey&, rai::TokenType, const rai::TokenKey&,
               rai::TokenType, const rai::TokenValue&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::TokenKey token_offer_;
    rai::TokenType type_offer_;
    rai::TokenValue id_offer_;
    rai::TokenKey token_want_;
    rai::TokenType type_want_;
    rai::TokenValue id_want_;
    rai::SwapRate rate_;
    rai::Account maker_;
    uint64_t height_;
};

class OrderInfo
{
public:
    OrderInfo();
    OrderInfo(const rai::Account&, rai::Chain, const rai::TokenAddress&,
              rai::TokenType, rai::Chain, const rai::TokenAddress&,
              rai::TokenType, const rai::TokenValue&, const rai::TokenValue&,
              uint64_t, const rai::BlockHash&);
    OrderInfo(const rai::Account&, rai::Chain, const rai::TokenAddress&,
              rai::TokenType, rai::Chain, const rai::TokenAddress&,
              rai::TokenType, const rai::TokenValue&, const rai::TokenValue&,
              const rai::TokenValue&, const rai::TokenValue&, uint64_t,
              const rai::BlockHash&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);
    bool Finished() const;

    enum class FinishedBy : uint8_t
    {
        INVALID = 0,
        CANCEL  = 1,
        FULFILL = 2,
        MAX
    };

    rai::Account main_;
    rai::TokenKey token_offer_;
    rai::TokenType type_offer_;
    rai::TokenKey token_want_;
    rai::TokenType type_want_;
    rai::TokenValue value_offer_;
    rai::TokenValue value_want_;
    rai::TokenValue min_offer_;
    rai::TokenValue max_offer_;
    rai::TokenValue left_;
    uint64_t timeout_;
    uint64_t finished_height_;
    FinishedBy finished_by_;
    rai::BlockHash hash_;
};

class OrderSwapIndex
{
public:
    OrderSwapIndex();
    OrderSwapIndex(const rai::Account&, uint64_t, const rai::Account&, uint64_t,
                   uint64_t);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::Account maker_;
    uint64_t order_height_;
    uint64_t ts_comp_;
    rai::Account taker_;
    uint64_t inquiry_height_;
};

class TokenSwapIndex
{
public:
    TokenSwapIndex();
    TokenSwapIndex(const rai::TokenKey&, const rai::Account&, uint64_t,
                   uint64_t);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::TokenKey token_;
    uint64_t ts_comp_;
    rai::Account taker_;
    uint64_t inquiry_height_;
};

class SwapInfo
{
public:
    SwapInfo();
    SwapInfo(const rai::Account&, uint64_t, uint64_t, uint64_t,
             const rai::TokenValue&, const rai::PublicKey&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    enum class Status : uint8_t
    {
        INVALID         = 0,
        INQUIRY         = 1,
        INQUIRY_ACK     = 2,
        INQUIRY_NACK    = 3,
        TAKE            = 4,
        TAKE_ACK        = 5,
        TAKE_NACK       = 6,
        MAX
    };

    static std::string StatusToString(Status);

    Status status_;
    rai::Account maker_;
    uint64_t order_height_;
    uint64_t inquiry_ack_height_;
    uint64_t take_height_;
    uint64_t trade_height_;
    uint64_t timeout_;
    rai::TokenValue value_;
    rai::PublicKey taker_share_;
    rai::PublicKey maker_share_;
    rai::Signature maker_signature_;
    rai::BlockHash trade_previous_;
};

class AccountSwapInfo
{
public:
    AccountSwapInfo();
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    uint64_t active_orders_;
    uint64_t total_orders_;
    uint64_t active_swaps_;
    uint64_t total_swaps_;
    uint64_t trusted_;
    uint64_t blocked_;
};

class InquiryWaiting
{
public:
    InquiryWaiting();
    InquiryWaiting(const rai::Account&, uint64_t, const rai::Account&,
                   uint64_t);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::Account main_;
    uint64_t ack_height_;
    rai::Account taker_;
    uint64_t inquiry_height_;
};

class TakeWaiting
{
public:
    TakeWaiting();
    TakeWaiting(const rai::Account&, uint64_t, const rai::Account&, uint64_t);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::Account maker_;
    uint64_t trade_height_;
    rai::Account taker_;
    uint64_t inquiry_height_;
};

class ReceivableInfo
{
public:
    ReceivableInfo() = default;
    ReceivableInfo(const rai::Account&, const rai::Amount&, uint64_t);
    bool operator>(const rai::ReceivableInfo&) const;
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::Account source_;
    rai::Amount amount_;
    uint64_t timestamp_;
};

class RewardableInfo
{
public:
    RewardableInfo() = default;
    RewardableInfo(const rai::Account&, const rai::Amount&, uint64_t);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);
    
    rai::Account source_;
    rai::Amount amount_;
    uint64_t valid_timestamp_;
};

class WalletInfo
{
public:
    WalletInfo();
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    uint32_t version_;
    uint32_t index_;
    uint32_t selected_account_id_;
    rai::uint256_union salt_;
    rai::uint256_union key_;
    rai::uint256_union seed_;
    rai::uint256_union check_;
};

class WalletAccountInfo
{
public:
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    uint32_t index_;
    rai::PrivateKey private_key_;
    rai::PublicKey public_key_;
};

enum class MetaKey : uint32_t
{
    VERSION            = 0,
    SELECTED_WALLET_ID = 1,
};

typedef std::multimap<rai::ReceivableInfo, rai::BlockHash,
                      std::greater<rai::ReceivableInfo>>
    ReceivableInfos;

typedef std::multimap<rai::ReceivableInfo,
                      std::pair<rai::Account, rai::BlockHash>,
                      std::greater<rai::ReceivableInfo>>
    ReceivableInfosAll;

enum class ReceivableInfosType
{
    CONFIRMED     = 0,
    NOT_CONFIRMED = 1,
    ALL           = 2
};

class RichListEntry
{
public:
    rai::Account account_;
    rai::Amount balance_;
};

typedef boost::multi_index_container<
    rai::RichListEntry,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_non_unique<
            boost::multi_index::member<rai::RichListEntry, rai::Amount,
                                       &rai::RichListEntry::balance_>,
            std::greater<rai::Amount>>,
        boost::multi_index::hashed_unique<boost::multi_index::member<
            rai::RichListEntry, rai::Account, &rai::RichListEntry::account_>>>>
    RichList;

class DelegatorListEntry
{
public:
    rai::Account account_;
    rai::Account rep_;
    rai::Amount weight_;
    rai::BlockType type_;
};

typedef boost::multi_index_container<
    rai::DelegatorListEntry,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<
            boost::multi_index::member<rai::DelegatorListEntry, rai::Account,
                                       &rai::DelegatorListEntry::account_>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::member<rai::DelegatorListEntry, rai::Account,
                                       &rai::DelegatorListEntry::rep_>>>>
    DelegatorList;

enum class LedgerType : uint32_t
{
    INVALID = 0,
    NODE    = 1,
    WALLET  = 2,
    APP     = 3,
};

class Ledger
{
public:
    Ledger(rai::ErrorCode&, rai::Store&, rai::LedgerType, bool = false,
           bool = false);

    bool AccountInfoPut(rai::Transaction&, const rai::Account&,
                        const rai::AccountInfo&);
    bool AccountInfoGet(rai::Transaction&, const rai::Account&,
                        rai::AccountInfo&) const;
    bool AccountInfoGet(const rai::Iterator&, rai::Account&,
                        rai::AccountInfo&) const;
    bool AccountInfoDel(rai::Transaction&, const rai::Account&);
    rai::Iterator AccountInfoBegin(rai::Transaction&);
    rai::Iterator AccountInfoEnd(rai::Transaction&);
    bool AccountCount(rai::Transaction&, size_t&) const;
    bool NextAccountInfo(rai::Transaction&, rai::Account&,
                         rai::AccountInfo&) const;
    bool AliasInfoPut(rai::Transaction&, const rai::Account&,
                      const rai::AliasInfo&);
    bool AliasInfoGet(rai::Transaction&, const rai::Account&,
                      rai::AliasInfo&) const;
    bool AliasBlockPut(rai::Transaction&, const rai::Account&, uint64_t,
                       const rai::AliasBlock&);
    bool AliasBlockGet(rai::Transaction&, const rai::Account&, uint64_t,
                       rai::AliasBlock&) const;
    bool AliasIndexPut(rai::Transaction&, const rai::AliasIndex&);
    bool AliasIndexDel(rai::Transaction&, const rai::AliasIndex&);
    bool AliasIndexGet(rai::Transaction&, rai::AliasIndex&) const;
    bool AliasIndexGet(const rai::Iterator&, rai::AliasIndex&) const;
    rai::Iterator AliasIndexLowerBound(rai::Transaction&,
                                       const rai::AliasIndex&);
    rai::Iterator AliasIndexUpperBound(rai::Transaction&,
                                       const rai::AliasIndex&);
    rai::Iterator AliasIndexLowerBound(rai::Transaction&, const rai::Prefix&);
    rai::Iterator AliasIndexUpperBound(rai::Transaction&, const rai::Prefix&,
                                       size_t = 64);
    bool AliasDnsIndexPut(rai::Transaction&, const rai::AliasIndex&);
    bool AliasDnsIndexDel(rai::Transaction&, const rai::AliasIndex&);
    bool AliasDnsIndexGet(rai::Transaction&, rai::AliasIndex&) const;
    bool AliasDnsIndexGet(const rai::Iterator&, rai::AliasIndex&) const;
    rai::Iterator AliasDnsIndexLowerBound(rai::Transaction&,
                                          const rai::AliasIndex&);
    rai::Iterator AliasDnsIndexUpperBound(rai::Transaction&,
                                          const rai::AliasIndex&);
    rai::Iterator AliasDnsIndexLowerBound(rai::Transaction&,
                                          const rai::Prefix&);
    rai::Iterator AliasDnsIndexUpperBound(rai::Transaction&, const rai::Prefix&,
                                          size_t = 64);
    bool AccountTokenInfoPut(rai::Transaction&, const rai::Account&, rai::Chain,
                             const rai::TokenAddress&,
                             const rai::AccountTokenInfo&);
    bool AccountTokenInfoGet(rai::Transaction&, const rai::Account&, rai::Chain,
                             const rai::TokenAddress&,
                             rai::AccountTokenInfo&) const;
    bool AccountTokenInfoGet(const rai::Iterator&, rai::Account&, rai::Chain&,
                             rai::TokenAddress&, rai::AccountTokenInfo&) const;
    rai::Iterator AccountTokenInfoLowerBound(rai::Transaction&,
                                             const rai::Account&) const;
    rai::Iterator AccountTokenInfoUpperBound(rai::Transaction&,
                                             const rai::Account&) const;
    bool AccountTokenLinkPut(rai::Transaction&, const rai::AccountTokenLink&,
                             uint64_t);
    bool AccountTokenLinkPut(rai::Transaction&, const rai::AccountTokenLink&,
                             uint64_t, uint64_t);
    bool AccountTokenLinkGet(rai::Transaction&, const rai::AccountTokenLink&,
                             uint64_t&) const;
    bool AccountTokenLinkGet(rai::Transaction&, const rai::AccountTokenLink&,
                             uint64_t&, uint64_t&) const;
    bool AccountTokenLinkSuccessorSet(rai::Transaction&,
                                      const rai::AccountTokenLink&, uint64_t);
    bool AccountTokenLinkSuccessorGet(rai::Transaction&,
                                      const rai::AccountTokenLink&,
                                      uint64_t&) const;
    bool AccountTokensInfoPut(rai::Transaction&, const rai::Account&,
                              rai::AccountTokensInfo&);
    bool AccountTokensInfoGet(rai::Transaction&, const rai::Account&,
                              rai::AccountTokensInfo&) const;
    bool AccountTokenIdPut(rai::Transaction&, const rai::AccountTokenId&,
                           uint64_t);
    bool AccountTokenIdGet(rai::Transaction&, const rai::AccountTokenId&,
                           uint64_t&) const;
    bool AccountTokenIdGet(rai::Iterator&, rai::AccountTokenId&,
                           uint64_t&) const;
    bool AccountTokenIdDel(rai::Transaction&, const rai::AccountTokenId&);
    bool AccountTokenIdExist(rai::Transaction&,
                             const rai::AccountTokenId&) const;
    rai::Iterator AccountTokenIdLowerBound(rai::Transaction&,
                                           const rai::Account&,
                                           const rai::TokenKey&) const;
    rai::Iterator AccountTokenIdLowerBound(rai::Transaction&,
                                           const rai::AccountTokenId&) const;
    rai::Iterator AccountTokenIdUpperBound(rai::Transaction&,
                                           const rai::Account&,
                                           const rai::TokenKey&) const;
    bool TokenBlockPut(rai::Transaction&, const rai::Account&, uint64_t,
                       const rai::TokenBlock&);
    bool TokenBlockPut(rai::Transaction&, const rai::Account&, uint64_t,
                       const rai::TokenBlock&, uint64_t);
    bool TokenBlockGet(rai::Transaction&, const rai::Account&, uint64_t,
                       rai::TokenBlock&) const;
    bool TokenBlockGet(rai::Transaction&, const rai::Account&, uint64_t,
                       rai::TokenBlock&, uint64_t&) const;
    bool TokenBlockSuccessorSet(rai::Transaction&, const rai::Account&,
                                uint64_t, uint64_t);
    bool TokenBlockSuccessorGet(rai::Transaction&, const rai::Account&,
                                uint64_t, uint64_t&) const;
    bool TokenInfoPut(rai::Transaction&, const rai::TokenKey&,
                      const rai::TokenInfo&);
    bool TokenInfoGet(rai::Transaction&, const rai::TokenKey&,
                      rai::TokenInfo&) const;
    bool TokenReceivablePut(rai::Transaction&, const rai::TokenReceivableKey&,
                            const rai::TokenReceivable&);
    bool TokenReceivableGet(rai::Transaction&, const rai::TokenReceivableKey&,
                            rai::TokenReceivable&) const;
    bool TokenReceivableGet(const rai::Iterator&, rai::TokenReceivableKey&,
                            rai::TokenReceivable&) const;
    bool TokenReceivableDel(rai::Transaction&, const rai::TokenReceivableKey&);
    rai::Iterator TokenReceivableLowerBound(rai::Transaction&,
                                           const rai::Account&) const;
    rai::Iterator TokenReceivableUpperBound(rai::Transaction&,
                                           const rai::Account&) const;
    rai::Iterator TokenReceivableLowerBound(rai::Transaction&,
                                            const rai::Account&,
                                            const rai::TokenKey&) const;
    rai::Iterator TokenReceivableUpperBound(rai::Transaction&,
                                            const rai::Account&,
                                            const rai::TokenKey&) const;
    bool TokenIdInfoPut(rai::Transaction&, const rai::TokenKey&,
                        const rai::TokenValue&, const rai::TokenIdInfo&);
    bool TokenIdInfoGet(rai::Transaction&, const rai::TokenKey&,
                        const rai::TokenValue&, rai::TokenIdInfo&);
    bool MaxTokenIdGet(rai::Transaction&, const rai::TokenKey&,
                       rai::TokenValue&) const;
    bool TokenHolderPut(rai::Transaction&, const rai::TokenKey&,
                        const rai::Account&, const rai::TokenValue&);
    bool TokenHolderExist(rai::Transaction&, const rai::TokenKey&,
                          const rai::Account&, const rai::TokenValue&) const;
    bool TokenHolderDel(rai::Transaction&, const rai::TokenKey&,
                        const rai::Account&, const rai::TokenValue&);
    bool TokenTransferPut(rai::Transaction&, const rai::TokenTransfer&);
    bool TokenIdTransferPut(rai::Transaction&, const rai::TokenIdTransferKey&,
                            const rai::Account&, uint64_t);
    bool SwapMainAccountPut(rai::Transaction&, const rai::Account&,
                            const rai::Account&);
    bool SwapMainAccountGet(rai::Transaction&, const rai::Account&,
                            rai::Account&) const;
    bool AccountSwapInfoPut(rai::Transaction&, const rai::Account&,
                            const rai::AccountSwapInfo&);
    bool AccountSwapInfoGet(rai::Transaction&, const rai::Account&,
                            rai::AccountSwapInfo&) const;
    bool OrderInfoPut(rai::Transaction&, const rai::Account&, uint64_t,
                      const rai::OrderInfo&);
    bool OrderInfoGet(rai::Transaction&, const rai::Account&, uint64_t,
                      rai::OrderInfo&) const;
    bool OrderIndexPut(rai::Transaction&, const rai::OrderIndex&);
    bool OrderIndexDel(rai::Transaction&, const rai::OrderIndex&);
    bool OrderIndexGet(const rai::Iterator&, rai::OrderIndex&) const;
    rai::Iterator OrderIndexLowerBound(rai::Transaction&, const rai::TokenKey&,
                                       rai::TokenType, const rai::TokenKey&,
                                       rai::TokenType) const;
    rai::Iterator OrderIndexUpperBound(rai::Transaction&, const rai::TokenKey&,
                                       rai::TokenType, const rai::TokenKey&,
                                       rai::TokenType) const;
    rai::Iterator OrderIndexLowerBound(rai::Transaction&, const rai::TokenKey&,
                                       rai::TokenType, const rai::TokenKey&,
                                       rai::TokenType,
                                       const rai::TokenValue&) const;
    rai::Iterator OrderIndexUpperBound(rai::Transaction&, const rai::TokenKey&,
                                       rai::TokenType, const rai::TokenKey&,
                                       rai::TokenType,
                                       const rai::TokenValue&) const;
    bool OrderCount(rai::Transaction&, size_t&) const;
    bool SwapInfoPut(rai::Transaction&, const rai::Account&, uint64_t,
                     const rai::SwapInfo&);
    bool SwapInfoGet(rai::Transaction&, const rai::Account&, uint64_t,
                     rai::SwapInfo&) const;
    bool InquiryWaitingPut(rai::Transaction&, const rai::InquiryWaiting&);
    bool InquiryWaitingGet(const rai::Iterator&, rai::InquiryWaiting&) const;
    bool InquiryWaitingDel(rai::Transaction&, const rai::InquiryWaiting&);
    bool InquiryWaitingExist(rai::Transaction&,
                             const rai::InquiryWaiting&) const;
    bool InquiryWaitingExist(rai::Transaction&, const rai::Account&,
                             uint64_t) const;
    rai::Iterator InquiryWaitingLowerBound(rai::Transaction&,
                                           const rai::Account&, uint64_t) const;
    rai::Iterator InquiryWaitingUpperBound(rai::Transaction&,
                                           const rai::Account&, uint64_t) const;
    bool TakeWaitingPut(rai::Transaction&, const rai::TakeWaiting&);
    bool TakeWaitingGet(const rai::Iterator&, rai::TakeWaiting&) const;
    bool TakeWaitingDel(rai::Transaction&, const rai::TakeWaiting&);
    bool TakeWaitingExist(rai::Transaction&, const rai::TakeWaiting&) const;
    bool TakeWaitingExist(rai::Transaction&, const rai::Account&,
                          uint64_t) const;
    rai::Iterator TakeWaitingLowerBound(rai::Transaction&, const rai::Account&,
                                        uint64_t) const;
    rai::Iterator TakeWaitingUpperBound(rai::Transaction&, const rai::Account&,
                                        uint64_t) const;
    bool OrderSwapIndexPut(rai::Transaction&, const rai::OrderSwapIndex&);
    bool TokenSwapIndexPut(rai::Transaction&, const rai::TokenSwapIndex&);
    bool BlockPut(rai::Transaction&, const rai::BlockHash&, const rai::Block&);
    bool BlockPut(rai::Transaction&, const rai::BlockHash&, const rai::Block&,
                  const rai::BlockHash&);
    bool BlockGet(rai::Transaction&, const rai::BlockHash&,
                  std::shared_ptr<rai::Block>&) const;
    bool BlockGet(rai::Transaction&, const rai::BlockHash&,
                  std::shared_ptr<rai::Block>&, rai::BlockHash&) const;
    bool BlockGet(rai::Transaction&, const rai::Account&, uint64_t,
                  std::shared_ptr<rai::Block>&) const;
    bool BlockGet(rai::Transaction&, const rai::Account&, uint64_t,
                  std::shared_ptr<rai::Block>&, rai::BlockHash&) const;
    bool BlockDel(rai::Transaction&, const rai::BlockHash&);
    bool BlockCount(rai::Transaction&, size_t&) const;
    bool BlockExists(rai::Transaction&, const rai::BlockHash&) const;
    bool BlockSuccessorSet(rai::Transaction&, const rai::BlockHash&,
                           const rai::BlockHash&);
    bool BlockSuccessorGet(rai::Transaction&, const rai::BlockHash&,
                           rai::BlockHash&) const;
    bool Empty(rai::Transaction&) const;
    bool ForkPut(rai::Transaction&, const rai::Account&, uint64_t,
                 const rai::Block&, const rai::Block&);
    bool ForkGet(rai::Transaction&, const rai::Account&, uint64_t,
                 std::shared_ptr<rai::Block>&,
                 std::shared_ptr<rai::Block>&) const;
    bool ForkGet(const rai::Iterator&, std::shared_ptr<rai::Block>&,
                 std::shared_ptr<rai::Block>&) const;
    bool ForkDel(rai::Transaction&, const rai::Account&);
    bool ForkDel(rai::Transaction&, const rai::Account&, uint64_t);
    bool ForkExists(rai::Transaction&, const rai::Account&, uint64_t) const;
    rai::Iterator ForkLowerBound(rai::Transaction&, const rai::Account&);
    rai::Iterator ForkUpperBound(rai::Transaction&, const rai::Account&);
    bool NextFork(rai::Transaction&, rai::Account&, uint64_t&,
                  std::shared_ptr<rai::Block>&,
                  std::shared_ptr<rai::Block>&) const;
    bool ReceivableInfoPut(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&, const rai::ReceivableInfo&);
    bool ReceivableInfoGet(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&, rai::ReceivableInfo&) const;
    bool ReceivableInfoExists(rai::Transaction&, const rai::Account&,
                              const rai::BlockHash&) const;
    bool ReceivableInfoGet(const rai::Iterator&, rai::Account&, rai::BlockHash&,
                           rai::ReceivableInfo&) const;

    bool ReceivableInfosGet(rai::Transaction&, rai::ReceivableInfosType,
                            rai::ReceivableInfosAll&,
                            size_t = std::numeric_limits<size_t>::max());
    bool ReceivableInfosGet(rai::Transaction&, const rai::Account&,
                            rai::ReceivableInfosType, rai::ReceivableInfos&,
                            size_t = std::numeric_limits<size_t>::max());
    bool ReceivableInfoDel(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&);
    bool ReceivableInfoCount(rai::Transaction&, size_t&) const;
    rai::Iterator ReceivableInfoLowerBound(rai::Transaction&,
                                           const rai::Account&);
    rai::Iterator ReceivableInfoUpperBound(rai::Transaction&,
                                           const rai::Account&);
    bool RewardableInfoPut(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&, const rai::RewardableInfo&);
    bool RewardableInfoGet(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&, rai::RewardableInfo&) const;
    bool RewardableInfoGet(const rai::Iterator&, rai::Account&, rai::BlockHash&,
                           rai::RewardableInfo&) const;
    bool RewardableInfoDel(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&);
    rai::Iterator RewardableInfoLowerBound(rai::Transaction&,
                                           const rai::Account&);
    rai::Iterator RewardableInfoUpperBound(rai::Transaction&,
                                           const rai::Account&);
    bool RollbackBlockPut(rai::Transaction&, const rai::BlockHash&,
                          const rai::Block&);
    bool RollbackBlockGet(rai::Transaction&, const rai::BlockHash&,
                          std::shared_ptr<rai::Block>&) const;

    void RepWeightAdd(rai::Transaction&, const rai::Account&,
                      const rai::Amount&);
    void RepWeightSub(rai::Transaction&, const rai::Account&,
                      const rai::Amount&);
    bool RepWeightGet(const rai::Account&, rai::Amount&) const;
    void RepWeightTotalGet(rai::Amount&) const;
    void RepWeightsGet(rai::Amount&,
                       std::unordered_map<rai::Account, rai::Amount>&) const;
    bool SourcePut(rai::Transaction&, const rai::BlockHash&);
    bool SourcePut(rai::Transaction&, const rai::BlockHash&, const rai::Block&);
    bool SourceGet(rai::Transaction&, const rai::BlockHash&,
                   std::shared_ptr<rai::Block>&) const;
    bool SourceDel(rai::Transaction&, const rai::BlockHash&);
    bool SourceExists(rai::Transaction&, const rai::BlockHash&) const;
    bool WalletInfoPut(rai::Transaction&, uint32_t, const rai::WalletInfo&);
    bool WalletInfoGet(rai::Transaction&, uint32_t, rai::WalletInfo&) const;
    bool WalletInfoGetAll(
        rai::Transaction&,
        std::vector<std::pair<uint32_t, rai::WalletInfo>>&) const;
    bool WalletAccountInfoPut(rai::Transaction&, uint32_t, uint32_t,
                              const rai::WalletAccountInfo&);
    bool WalletAccountInfoGet(rai::Transaction&, uint32_t, uint32_t,
                              rai::WalletAccountInfo&) const;
    bool WalletAccountInfoGetAll(
        rai::Transaction&, uint32_t,
        std::vector<std::pair<uint32_t, rai::WalletAccountInfo>>&) const;
    bool SelectedWalletIdPut(rai::Transaction&, uint32_t);
    bool SelectedWalletIdGet(rai::Transaction&, uint32_t&) const;
    bool VersionPut(rai::Transaction&, uint32_t);
    bool VersionGet(rai::Transaction&, uint32_t&) const;
    void UpdateRichList(const rai::Block&);
    std::vector<rai::RichListEntry> GetRichList(uint64_t);
    void UpdateDelegatorList(const rai::Block&);
    std::vector<rai::DelegatorListEntry> GetDelegatorList(const rai::Account&,
                                                          uint64_t);

    rai::ErrorCode UpgradeWallet(rai::Transaction&);
    rai::ErrorCode UpgradeWalletV1V2(rai::Transaction&);

private:
    friend class rai::Transaction;

    bool BlockIndexPut_(rai::Transaction&, const rai::Account&, uint64_t,
                        const rai::BlockHash&);
    bool BlockIndexGet_(rai::Transaction&, const rai::Account&, uint64_t,
                        rai::BlockHash&) const;
    bool BlockIndexDel_(rai::Transaction&, const rai::Account&, uint64_t);
    void RepWeightsCommit_(const std::vector<rai::RepWeightOpration>&);
    rai::ErrorCode InitMemoryTables_(rai::Transaction&);
    void UpdateRichList_(const rai::Account&, const rai::Amount&);
    void UpdateDelegatorList_(const rai::Account&, const rai::Account&,
                              const rai::Amount&, rai::BlockType);

    static uint32_t constexpr BLOCKS_PER_INDEX = 8;
    const rai::Amount RICH_LIST_MINIMUM = rai::Amount(10 * rai::RAI);

    rai::Store& store_;
    mutable std::mutex rep_weights_mutex_;
    rai::Amount total_rep_weight_;
    std::unordered_map<rai::Account, rai::Amount> rep_weights_;

    bool enable_rich_list_;
    mutable std::mutex rich_list_mutex_;
    rai::RichList rich_list_;

    bool enable_delegator_list_;
    mutable std::mutex delegator_list_mutex_;
    rai::DelegatorList delegator_list_;
};
}  // namespace rai
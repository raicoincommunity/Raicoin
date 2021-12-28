#include <rai/secure/ledger.hpp>

rai::RepWeightOpration::RepWeightOpration(bool add,
                                          const rai::Account& representative,
                                          const rai::Amount& weight)
    : add_(add), representative_(representative), weight_(weight)
{
}

rai::Transaction::Transaction(rai::ErrorCode& error_code, rai::Ledger& ledger,
                              bool write)
    : ledger_(ledger),
      write_(write),
      aborted_(false),
      mdb_transaction_(error_code, ledger.store_.env_, nullptr, write)
{
}

rai::Transaction::~Transaction()
{
    if (aborted_)
    {
        return;
    }
    ledger_.RepWeightsCommit_(rep_weight_operations_);
}

void rai::Transaction::Abort()
{
    aborted_ = true;
    mdb_transaction_.Abort();
}

rai::Iterator::Iterator(rai::StoreIterator&& store_it)
    : store_it_(std::move(store_it))
{
}

rai::Iterator::Iterator(rai::Iterator&& other)
    : store_it_(std::move(other.store_it_))
{
}

rai::Iterator& rai::Iterator::operator++()
{
    ++store_it_;
    return *this;
}

bool rai::Iterator::operator==(const rai::Iterator& other) const
{
    return store_it_ == other.store_it_;
}
bool rai::Iterator::operator!=(const rai::Iterator& other) const
{
    return store_it_ != other.store_it_;
}

rai::AccountInfo::AccountInfo()
    : type_(rai::BlockType::INVALID),
      forks_(0),
      head_height_(rai::Block::INVALID_HEIGHT),
      tail_height_(rai::Block::INVALID_HEIGHT),
      confirmed_height_(rai::Block::INVALID_HEIGHT)
{
}

rai::AccountInfo::AccountInfo(rai::BlockType type, const rai::BlockHash& hash)
    : type_(type),
      forks_(0),
      head_height_(0),
      tail_height_(0),
      confirmed_height_(rai::Block::INVALID_HEIGHT),
      head_(hash),
      tail_(hash)
{
}

void rai::AccountInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, type_);
    rai::Write(stream, forks_);
    rai::Write(stream, head_height_);
    rai::Write(stream, tail_height_);
    rai::Write(stream, confirmed_height_);
    rai::Write(stream, head_.bytes);
    rai::Write(stream, tail_.bytes);
}

bool rai::AccountInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error      = rai::Read(stream, type_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, forks_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, head_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, tail_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, confirmed_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, head_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, tail_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

bool rai::AccountInfo::Confirmed(uint64_t height) const
{
    if (!Valid())
    {
        return false;
    }

    if (confirmed_height_ == rai::Block::INVALID_HEIGHT)
    {
        return false;
    }

    if (confirmed_height_ >= height)
    {
        return true;
    }

    return false;
}

bool rai::AccountInfo::Valid() const
{
    if (head_height_ == rai::Block::INVALID_HEIGHT
        || tail_height_ == rai::Block::INVALID_HEIGHT)
    {
        return false;
    }

    if (head_.IsZero() || tail_.IsZero())
    {
        return false;
    }

    return true;
}

bool rai::AccountInfo::Restricted() const
{
    return forks_ > rai::MaxAllowedForks(rai::CurrentTimestamp());
}

rai::AliasInfo::AliasInfo()
    : head_(rai::Block::INVALID_HEIGHT),
      name_valid_(rai::Block::INVALID_HEIGHT),
      dns_valid_(rai::Block::INVALID_HEIGHT)
{
}

void rai::AliasInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, head_);
    rai::Write(stream, name_valid_);
    rai::Write(stream, dns_valid_);
    uint16_t size = static_cast<uint16_t>(name_.size());
    rai::Write(stream, size);
    rai::Write(stream, size, name_);
    size = static_cast<uint16_t>(dns_.size());
    rai::Write(stream, size);
    rai::Write(stream, size, dns_);
}

bool rai::AliasInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, head_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, name_valid_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, dns_valid_);
    IF_ERROR_RETURN(error, true);
    uint16_t size = 0;
    error = rai::Read(stream, size);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, size, name_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, size);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, size, dns_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::AliasBlock::AliasBlock()
    : previous_(rai::Block::INVALID_HEIGHT), hash_(0), status_(0), op_(0)
{
}

rai::AliasBlock::AliasBlock(uint64_t previous, const rai::BlockHash& hash,
                            int32_t status)
    : previous_(previous), hash_(hash), status_(status), op_(0)
{
}

rai::AliasBlock::AliasBlock(uint64_t previous, const rai::BlockHash& hash,
                            int32_t status, uint8_t op,
                            const std::vector<uint8_t>& value)
    : previous_(previous), hash_(hash), status_(status), op_(op), value_(value)
{
}

void rai::AliasBlock::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, previous_);
    rai::Write(stream, hash_.bytes);
    rai::Write(stream, status_);
    rai::Write(stream, op_);
    uint16_t size = static_cast<uint16_t>(value_.size());
    rai::Write(stream, size);
    rai::Write(stream, size, value_);
}

bool rai::AliasBlock::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, previous_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, hash_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, status_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, op_);
    IF_ERROR_RETURN(error, true);
    uint16_t size = 0;
    error = rai::Read(stream, size);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, size, value_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::AliasIndex::AliasIndex(const rai::Prefix& prefix,
                            const rai::Account& account)
    : prefix_(prefix), account_(account)
{
    prefix_.ToLower();
}

void rai::AliasIndex::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, prefix_.bytes);
    rai::Write(stream, account_.bytes);
}

bool rai::AliasIndex::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, prefix_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, account_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::TokenKey::TokenKey() : chain_(rai::Chain::INVALID), address_(0)
{
}

rai::TokenKey::TokenKey(rai::Chain chain, const rai::TokenAddress& address)
    : chain_(chain), address_(address)
{
}

void rai::TokenKey::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, address_.bytes);
    rai::Write(stream, chain_);
}

bool rai::TokenKey::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, address_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, chain_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::TokenIndex::TokenIndex() : token_(), ts_comp_(0), account_(0), height_(0)
{
}

rai::TokenIndex::TokenIndex(rai::Chain chain, const rai::TokenAddress& address,
                            uint64_t timestamp, const rai::Account& account,
                            uint64_t height)
    : token_(chain, address),
      ts_comp_(~timestamp),
      account_(account),
      height_(height)
{
}

void rai::TokenIndex::Serialize(rai::Stream& stream) const
{
    token_.Serialize(stream);
    rai::Write(stream, ts_comp_);
    rai::Write(stream, account_.bytes);
    rai::Write(stream, height_);
}

bool rai::TokenIndex::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = token_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, ts_comp_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, account_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, height_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::TokenInfo::TokenInfo()
    : type_(rai::TokenType::INVALID),
      decimals_(0),
      burnable_(false),
      mintable_(false),
      circulable_(true),
      holders_(0),
      transfers_(0),
      swaps_(0),
      total_supply_(0),
      cap_supply_(0)
{
}

rai::TokenInfo::TokenInfo(rai::TokenType type, const std::string& symbol,
                          const std::string& name, uint8_t decimals,
                          bool burnable, bool mintable, bool circulable,
                          const rai::TokenValue& cap_supply)
    : type_(type),
      symbol_(symbol),
      name_(name),
      decimals_(decimals),
      burnable_(burnable),
      mintable_(mintable),
      circulable_(circulable),
      holders_(0),
      transfers_(0),
      swaps_(0),
      total_supply_(0),
      cap_supply_(cap_supply)
{
}

rai::TokenInfo::TokenInfo(rai::TokenType type, const std::string& symbol,
                          const std::string& name, bool burnable,
                          bool circulable, const rai::TokenValue& cap_supply,
                          const std::string base_uri)
    : type_(type),
      symbol_(symbol),
      name_(name),
      decimals_(0),
      burnable_(burnable),
      mintable_(true),
      circulable_(circulable),
      holders_(0),
      transfers_(0),
      swaps_(0),
      total_supply_(0),
      cap_supply_(cap_supply),
      base_uri_(base_uri)
{
}

void rai::TokenInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, type_);
    rai::Write(stream, symbol_);
    rai::Write(stream, name_);
    rai::Write(stream, decimals_);
    rai::Write(stream, burnable_);
    rai::Write(stream, mintable_);
    rai::Write(stream, circulable_);
    rai::Write(stream, holders_);
    rai::Write(stream, transfers_);
    rai::Write(stream, swaps_);
    rai::Write(stream, total_supply_.bytes);
    rai::Write(stream, cap_supply_.bytes);
    rai::Write(stream, base_uri_);
}

bool rai::TokenInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, type_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, symbol_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, name_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, decimals_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, burnable_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, mintable_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, circulable_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, holders_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, transfers_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, swaps_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, total_supply_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, cap_supply_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, base_uri_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::TokenBlock::TokenBlock()
    : previous_(rai::Block::INVALID_HEIGHT),
      hash_(0),
      status_(rai::ErrorCode::SUCCESS)
{
}

rai::TokenBlock::TokenBlock(uint64_t previous, const rai::BlockHash& hash,
                            rai::ErrorCode status)
    : previous_(previous), hash_(hash), status_(status)
{
}

void rai::TokenBlock::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, previous_);
    rai::Write(stream, hash_.bytes);
    rai::Write(stream, status_);
}

bool rai::TokenBlock::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, previous_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, hash_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, status_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::TokenHolder::TokenHolder() : token_(), balance_comp_(0), account_(0)
{
}

rai::TokenHolder::TokenHolder(rai::Chain chain,
                              const rai::TokenAddress& address,
                              const rai::TokenValue& balance,
                              const rai::Account& account)
    : token_(chain, address), balance_comp_(~balance), account_(account)
{
}

void rai::TokenHolder::Serialize(rai::Stream& stream) const
{
    token_.Serialize(stream);
    rai::Write(stream, balance_comp_.bytes);
    rai::Write(stream, account_.bytes);
}

bool rai::TokenHolder::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = token_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, balance_comp_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, account_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::AccountTokensInfo::AccountTokensInfo()
    : head_(rai::Block::INVALID_HEIGHT), blocks_(0), last_active_(0), main_(0)
{
}

rai::AccountTokensInfo::AccountTokensInfo(uint64_t head, uint64_t blocks,
                                          uint64_t last_active)
    : head_(head), blocks_(blocks), last_active_(last_active), main_(0)
{
}

rai::AccountTokensInfo::AccountTokensInfo(const rai::Account& account,
                                          uint64_t last_active)
    : head_(rai::Block::INVALID_HEIGHT),
      blocks_(0),
      last_active_(last_active),
      main_(account)
{
}

void rai::AccountTokensInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, head_);
    rai::Write(stream, blocks_);
    rai::Write(stream, last_active_);
    rai::Write(stream, main_.bytes);
}

bool rai::AccountTokensInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, head_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, blocks_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, last_active_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, main_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::AccountTokenInfo::AccountTokenInfo()
    : head_(rai::Block::INVALID_HEIGHT), blocks_(0), balance_(0)
{
}

rai::AccountTokenInfo::AccountTokenInfo(uint64_t head, uint64_t blocks,
                                        const rai::TokenValue& balance)
    : head_(head), blocks_(blocks), balance_(balance)
{
}

void rai::AccountTokenInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, head_);
    rai::Write(stream, blocks_);
    rai::Write(stream, balance_.bytes);
}

bool rai::AccountTokenInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, head_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, blocks_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, balance_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::AccountTokenId::AccountTokenId() : account_(0), token_(), id_(0)
{
}

rai::AccountTokenId::AccountTokenId(const rai::Account& account,
                                    const rai::TokenKey& token,
                                    const rai::TokenValue& id)
    : account_(account), token_(token), id_(id)
{
}

void rai::AccountTokenId::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, account_.bytes);
    token_.Serialize(stream);
    rai::Write(stream, id_.bytes);
}

bool rai::AccountTokenId::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, account_.bytes);
    IF_ERROR_RETURN(error, true);
    error = token_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, id_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::AccountTokenLink::AccountTokenLink()
    : account_(0), token_(), height_(rai::Block::INVALID_HEIGHT)
{
}

rai::AccountTokenLink::AccountTokenLink(const rai::Account& account,
                                        rai::Chain chain,
                                        const rai::TokenAddress& address,
                                        uint64_t height)
    : account_(account), token_(chain, address), height_(height)
{
}

void rai::AccountTokenLink::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, account_.bytes);
    token_.Serialize(stream);
    rai::Write(stream, height_);
}

bool rai::AccountTokenLink::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, account_.bytes);
    IF_ERROR_RETURN(error, true);
    error = token_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, height_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::OrderIndex::OrderIndex()
    : token_offer_(),
      token_want_(),
      rate_(0),
      maker_(0),
      height_(rai::Block::INVALID_HEIGHT)
{
}

rai::OrderIndex::OrderIndex(rai::Chain chain_offer,
                            const rai::TokenAddress& address_offer,
                            rai::Chain chain_want,
                            const rai::TokenAddress& address_want,
                            const rai::SwapRate& rate,
                            const rai::Account& maker, uint64_t height)
    : token_offer_(chain_offer, address_offer),
      token_want_(chain_want, address_want),
      rate_(rate),
      maker_(maker),
      height_(height)
{
}

void rai::OrderIndex::Serialize(rai::Stream& stream) const
{
    token_offer_.Serialize(stream);
    token_want_.Serialize(stream);
    rai::Write(stream, rate_.bytes);
    rai::Write(stream, maker_.bytes);
    rai::Write(stream, height_);
}

bool rai::OrderIndex::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = token_offer_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = token_want_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, rate_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, maker_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, height_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::OrderInfo::OrderInfo()
    : main_(0),
      token_offer_(),
      token_want_(),
      value_offer_(0),
      value_want_(0),
      min_offer_(0),
      max_offer_(0),
      left_(0),
      finished_height_(rai::Block::INVALID_HEIGHT),
      has_nft_(false),
      finished_by_(rai::OrderInfo::FinishedBy::INVALID)
{
}

rai::OrderInfo::OrderInfo(const rai::Account& main_account,
                          rai::Chain chain_offer,
                          const rai::TokenAddress& address_offer,
                          rai::Chain chain_want,
                          const rai::TokenAddress& address_want,
                          const rai::TokenValue& value_offer,
                          const rai::TokenValue& value_want, uint64_t timeout)
    : main_(main_account),
      token_offer_(chain_offer, address_offer),
      token_want_(chain_want, address_want),
      value_offer_(value_offer),
      value_want_(value_want),
      min_offer_(0),
      max_offer_(0),
      left_(0),
      timeout_(timeout),
      finished_height_(rai::Block::INVALID_HEIGHT),
      has_nft_(true),
      finished_by_(rai::OrderInfo::FinishedBy::INVALID)
{
}

rai::OrderInfo::OrderInfo(
    const rai::Account& main_account, rai::Chain chain_offer,
    const rai::TokenAddress& address_offer, rai::Chain chain_want,
    const rai::TokenAddress& address_want, const rai::TokenValue& value_offer,
    const rai::TokenValue& value_want, const rai::TokenValue& min_offer,
    const rai::TokenValue& max_offer, uint64_t timeout)
    : main_(main_account),
      token_offer_(chain_offer, address_offer),
      token_want_(chain_want, address_want),
      value_offer_(value_offer),
      value_want_(value_want),
      min_offer_(min_offer),
      max_offer_(max_offer),
      left_(max_offer),
      timeout_(timeout),
      finished_height_(rai::Block::INVALID_HEIGHT),
      has_nft_(false),
      finished_by_(rai::OrderInfo::FinishedBy::INVALID)
{
}

void rai::OrderInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, main_.bytes);
    token_offer_.Serialize(stream);
    token_want_.Serialize(stream);
    rai::Write(stream, value_offer_.bytes);
    rai::Write(stream, value_want_.bytes);
    rai::Write(stream, min_offer_.bytes);
    rai::Write(stream, max_offer_.bytes);
    rai::Write(stream, left_.bytes);
    rai::Write(stream, timeout_);
    rai::Write(stream, finished_height_);
    rai::Write(stream, has_nft_);
    rai::Write(stream, finished_by_);
}

bool rai::OrderInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, main_.bytes);
    IF_ERROR_RETURN(error, true);
    error = token_offer_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = token_want_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, value_offer_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, value_want_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, min_offer_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, max_offer_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, left_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, timeout_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, finished_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, has_nft_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, finished_by_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::OrderSwapIndex::OrderSwapIndex()
    : maker_(0),
      order_height_(rai::Block::INVALID_HEIGHT),
      ts_comp_(0),
      taker_(0),
      inquiry_height_(rai::Block::INVALID_HEIGHT)
{
}

rai::OrderSwapIndex::OrderSwapIndex(const rai::Account& maker,
                                    uint64_t order_height,
                                    const rai::Account& taker,
                                    uint64_t inquiry_height, uint64_t timestamp)
    : maker_(maker),
      order_height_(order_height),
      ts_comp_(~timestamp),
      taker_(taker),
      inquiry_height_(inquiry_height)
{
}

void rai::OrderSwapIndex::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, maker_.bytes);
    rai::Write(stream, order_height_);
    rai::Write(stream, ts_comp_);
    rai::Write(stream, taker_.bytes);
    rai::Write(stream, inquiry_height_);
}

bool rai::OrderSwapIndex::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, maker_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, order_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, ts_comp_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, taker_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, inquiry_height_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::TokenSwapIndex::TokenSwapIndex()
    : token_(),
      ts_comp_(0),
      taker_(0),
      inquiry_height_(rai::Block::INVALID_HEIGHT)
{
}

rai::TokenSwapIndex::TokenSwapIndex(rai::Chain chain,
                                    const rai::TokenAddress& address,
                                    const rai::Account& taker,
                                    uint64_t inquiry_height, uint64_t timestamp)
    : token_(chain, address),
      ts_comp_(~timestamp),
      taker_(taker),
      inquiry_height_(inquiry_height)
{
}

void rai::TokenSwapIndex::Serialize(rai::Stream& stream) const
{
    token_.Serialize(stream);
    rai::Write(stream, ts_comp_);
    rai::Write(stream, taker_.bytes);
    rai::Write(stream, inquiry_height_);
}

bool rai::TokenSwapIndex::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = token_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, ts_comp_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, taker_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, inquiry_height_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::SwapInfo::SwapInfo()
    : status_(rai::SwapInfo::Status::INVALID),
      maker_(0),
      order_height_(rai::Block::INVALID_HEIGHT),
      inquiry_ack_height_(rai::Block::INVALID_HEIGHT),
      take_height_(rai::Block::INVALID_HEIGHT),
      trade_height_(rai::Block::INVALID_HEIGHT),
      timeout_(0),
      value_(0)
{
}

rai::SwapInfo::SwapInfo(const rai::Account& maker, uint64_t order_height,
                        uint64_t inquiry_ack_height, uint64_t timeout,
                        const rai::TokenValue& value,
                        const rai::PublicKey& take_share)
    : status_(rai::SwapInfo::Status::INQUIRY),
      maker_(maker),
      order_height_(order_height),
      inquiry_ack_height_(inquiry_ack_height),
      take_height_(rai::Block::INVALID_HEIGHT),
      trade_height_(rai::Block::INVALID_HEIGHT),
      timeout_(timeout),
      value_(value),
      taker_share_(take_share)
{
}

void rai::SwapInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, status_);
    rai::Write(stream, maker_.bytes);
    rai::Write(stream, order_height_);
    rai::Write(stream, inquiry_ack_height_);
    rai::Write(stream, take_height_);
    rai::Write(stream, trade_height_);
    rai::Write(stream, timeout_);
    rai::Write(stream, value_.bytes);
    rai::Write(stream, taker_share_.bytes);
    rai::Write(stream, maker_share_.bytes);
    rai::Write(stream, maker_signature_.bytes);
}

bool rai::SwapInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, status_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, maker_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, order_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, inquiry_ack_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, take_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, trade_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, timeout_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, value_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, taker_share_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, maker_share_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, maker_signature_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::ReceivableInfo::ReceivableInfo(const rai::Account& source,
                                    const rai::Amount& amount,
                                    uint64_t timestamp)
    : source_(source), amount_(amount), timestamp_(timestamp)
{
}

bool rai::ReceivableInfo::operator>(const rai::ReceivableInfo& other) const
{
    return amount_ > other.amount_;
}

void rai::ReceivableInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, source_.bytes);
    rai::Write(stream, amount_.bytes);
    rai::Write(stream, timestamp_);
}

bool rai::ReceivableInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, source_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, amount_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, timestamp_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::RewardableInfo::RewardableInfo(const rai::Account& source,
                                    const rai::Amount& amount,
                                    uint64_t valid_timestamp)
    : source_(source), amount_(amount), valid_timestamp_(valid_timestamp)
{
}

void rai::RewardableInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, source_.bytes);
    rai::Write(stream, amount_.bytes);
    rai::Write(stream, valid_timestamp_);
}

bool rai::RewardableInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, source_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, amount_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, valid_timestamp_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::WalletInfo::WalletInfo()
    : version_(0), index_(0), salt_(0), key_(0), seed_(0), check_(0)
{
}

void rai::WalletInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, version_);
    rai::Write(stream, index_);
    rai::Write(stream, selected_account_id_);
    rai::Write(stream, salt_.bytes);
    rai::Write(stream, key_.bytes);
    rai::Write(stream, seed_.bytes);
    rai::Write(stream, check_.bytes);
}

bool rai::WalletInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, version_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, index_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, selected_account_id_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, salt_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, key_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, seed_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, check_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}


void rai::WalletAccountInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, index_);
    rai::Write(stream, private_key_.bytes);
    rai::Write(stream, public_key_.bytes);
}

bool rai::WalletAccountInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, index_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, private_key_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, public_key_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::Ledger::Ledger(rai::ErrorCode& error_code, rai::Store& store,
                    rai::LedgerType type, bool enable_rich_list,
                    bool enable_delegator_list)
    : store_(store),
      total_rep_weight_(0),
      enable_rich_list_(enable_rich_list),
      enable_delegator_list_(enable_delegator_list)
{
    IF_NOT_SUCCESS_RETURN_VOID(error_code);
    rai::Transaction transaction(error_code, *this, true);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);
    if (type == rai::LedgerType::NODE)
    {
        InitMemoryTables_(transaction);
    }
    else if (type == rai::LedgerType::WALLET)
    {
        error_code = UpgradeWallet(transaction);
    }
    else
    {
    }

    if (error_code != rai::ErrorCode::SUCCESS)
    {
        transaction.Abort();
    }
}

bool rai::Ledger::AccountInfoPut(rai::Transaction& transaction,
                                 const rai::Account& account,
                                 const rai::AccountInfo& account_info)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        account_info.Serialize(stream);
    }
    rai::MdbVal key(account);
    rai::MdbVal value(bytes.size(), bytes.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.accounts_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::AccountInfoGet(rai::Transaction& transaction,
                                 const rai::Account& account,
                                 rai::AccountInfo& account_info) const
{
    rai::MdbVal key(account);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.accounts_, key, value);
    if (error)
    {
        return true;
    }
    rai::BufferStream stream(value.Data(), value.Size());
    return account_info.Deserialize(stream);
}

bool rai::Ledger::AccountInfoGet(const rai::Iterator& it, rai::Account& account,
                                 rai::AccountInfo& info) const
{
    if (it.store_it_->first.Data() == nullptr
        || it.store_it_->first.Size() == 0)
    {
        return true;
    }
    account = it.store_it_->first.uint256_union();

    auto data = it.store_it_->second.Data();
    auto size = it.store_it_->second.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream(data, size);
    return info.Deserialize(stream);
}

bool rai::Ledger::AccountInfoDel(rai::Transaction& transaction,
                                 const rai::Account& account)
{
    if (!transaction.write_)
    {
        return true;
    }

    rai::MdbVal key(account);
    return store_.Del(transaction.mdb_transaction_, store_.accounts_, key,
                      nullptr);
}

rai::Iterator rai::Ledger::AccountInfoBegin(rai::Transaction& transaction)
{

    rai::StoreIterator store_it(transaction.mdb_transaction_, store_.accounts_);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::AccountInfoEnd(rai::Transaction& transaction)
{
    rai::StoreIterator store_it(nullptr);
    return rai::Iterator(std::move(store_it));
}

bool rai::Ledger::AccountCount(rai::Transaction& transaction,
                               size_t& count) const
{
    MDB_stat stat;
    auto ret = mdb_stat(transaction.mdb_transaction_, store_.accounts_, &stat);
    if (ret != MDB_SUCCESS)
    {
        assert(0);
        return true;
    }

    count = stat.ms_entries;
    return false;
}

bool rai::Ledger::NextAccountInfo(rai::Transaction& transaction,
                                  rai::Account& account,
                                  rai::AccountInfo& info) const
{
    rai::MdbVal key(account);
    rai::StoreIterator store_it(transaction.mdb_transaction_, store_.accounts_,
                                key);
    if (store_it->first.Data() == nullptr
        || store_it->first.Size() == 0)
    {
        return true;
    }
    account = store_it->first.uint256_union();

    auto data = store_it->second.Data();
    auto size = store_it->second.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream(data, size);
    return info.Deserialize(stream);
}

bool rai::Ledger::AliasInfoPut(rai::Transaction& transaction,
                               const rai::Account& account,
                               const rai::AliasInfo& info)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        info.Serialize(stream);
    }
    rai::MdbVal key(account);
    rai::MdbVal value(bytes.size(), bytes.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.alias_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::AliasInfoGet(rai::Transaction& transaction,
                               const rai::Account& account,
                               rai::AliasInfo& info) const
{
    rai::MdbVal key(account);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.alias_, key, value);
    if (error)
    {
        return true;
    }
    rai::BufferStream stream(value.Data(), value.Size());
    return info.Deserialize(stream);
}

bool rai::Ledger::AliasBlockPut(rai::Transaction& transaction,
                                const rai::Account& account, uint64_t height,
                                const rai::AliasBlock& block)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        block.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_, store_.alias_block_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::AliasBlockGet(rai::Transaction& transaction,
                                const rai::Account& account, uint64_t height,
                                rai::AliasBlock& block) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_, store_.alias_block_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    return block.Deserialize(stream);
}

bool rai::Ledger::AliasIndexPut(rai::Transaction& transaction,
                                const rai::AliasIndex& index)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        index.Serialize(stream);
    }
    uint8_t ignore = 0;

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(sizeof(ignore), &ignore);
    bool error = store_.Put(transaction.mdb_transaction_, store_.alias_index_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::AliasIndexDel(rai::Transaction& transaction,
                                const rai::AliasIndex& index)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        index.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.alias_index_, key,
                      nullptr);
}

bool rai::Ledger::AliasIndexGet(rai::Transaction& transaction,
                                rai::AliasIndex& index) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        index.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_, store_.alias_index_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::AliasIndexGet(const rai::Iterator& it,
                                rai::AliasIndex& index) const
{
    if (it.store_it_->first.Data() == nullptr
        || it.store_it_->first.Size() == 0)
    {
        return true;
    }
    auto data = it.store_it_->first.Data();
    auto size = it.store_it_->first.Size();
    rai::BufferStream stream(data, size);
    return index.Deserialize(stream);
}

rai::Iterator rai::Ledger::AliasIndexLowerBound(rai::Transaction& transaction,
                                                const rai::AliasIndex& index)
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        index.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.alias_index_, key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::AliasIndexUpperBound(rai::Transaction& transaction,
                                                const rai::AliasIndex& index)
{
    rai::AliasIndex index_l(index);
    index_l.account_ += 1;
    if (index_l.account_.IsZero())
    {
        index_l.prefix_ += 1;
        if (index_l.prefix_.IsZero())
        {
            return rai::Iterator(rai::StoreIterator(nullptr));
        }
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        index_l.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.alias_index_, key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::AliasIndexLowerBound(rai::Transaction& transaction,
                                                const rai::Prefix& prefix)
{
    rai::AliasIndex index{prefix, rai::Account(0)};
    return AliasIndexLowerBound(transaction, index);
}

rai::Iterator rai::Ledger::AliasIndexUpperBound(rai::Transaction& transaction,
                                                const rai::Prefix& prefix,
                                                size_t size)
{
    if (size <= 0)
    {
        return rai::Iterator(rai::StoreIterator(nullptr));
    }

    rai::AliasIndex index{prefix, rai::Account(0)};
    rai::Prefix offset(static_cast<uint8_t>(1), size - 1);
    index.prefix_ += offset;
    if (index.prefix_.IsZero())
    {
        return rai::Iterator(rai::StoreIterator(nullptr));
    }
    return AliasIndexLowerBound(transaction, index);  // lower bound !
}

bool rai::Ledger::AliasDnsIndexPut(rai::Transaction& transaction,
                                   const rai::AliasIndex& index)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        index.Serialize(stream);
    }
    uint8_t ignore = 0;

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(sizeof(ignore), &ignore);
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.alias_dns_index_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::AliasDnsIndexDel(rai::Transaction& transaction,
                                   const rai::AliasIndex& index)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        index.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.alias_dns_index_,
                      key, nullptr);
}

bool rai::Ledger::AliasDnsIndexGet(rai::Transaction& transaction,
                                   rai::AliasIndex& index) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        index.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.alias_dns_index_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::AliasDnsIndexGet(const rai::Iterator& it,
                                   rai::AliasIndex& index) const
{
    if (it.store_it_->first.Data() == nullptr
        || it.store_it_->first.Size() == 0)
    {
        return true;
    }
    auto data = it.store_it_->first.Data();
    auto size = it.store_it_->first.Size();
    rai::BufferStream stream(data, size);
    return index.Deserialize(stream);
}

rai::Iterator rai::Ledger::AliasDnsIndexLowerBound(
    rai::Transaction& transaction, const rai::AliasIndex& index)
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        index.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.alias_dns_index_, key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::AliasDnsIndexUpperBound(
    rai::Transaction& transaction, const rai::AliasIndex& index)
{
    rai::AliasIndex index_l(index);
    index_l.account_ += 1;
    if (index_l.account_.IsZero())
    {
        index_l.prefix_ += 1;
        if (index_l.prefix_.IsZero())
        {
            return rai::Iterator(rai::StoreIterator(nullptr));
        }
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        index_l.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.alias_dns_index_, key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::AliasDnsIndexLowerBound(
    rai::Transaction& transaction, const rai::Prefix& prefix)
{
    rai::AliasIndex index{prefix, rai::Account(0)};
    return AliasDnsIndexLowerBound(transaction, index);
}

rai::Iterator rai::Ledger::AliasDnsIndexUpperBound(
    rai::Transaction& transaction, const rai::Prefix& prefix, size_t size)
{
    if (size <= 0)
    {
        return rai::Iterator(rai::StoreIterator(nullptr));
    }

    rai::AliasIndex index{prefix, rai::Account(0)};
    rai::Prefix offset(static_cast<uint8_t>(1), size - 1);
    index.prefix_ += offset;
    if (index.prefix_.IsZero())
    {
        return rai::Iterator(rai::StoreIterator(nullptr));
    }
    return AliasDnsIndexLowerBound(transaction, index);  // lower bound !
}

bool rai::Ledger::AccountTokenInfoPut(rai::Transaction& transaction,
                                      const rai::Account& account,
                                      rai::Chain chain,
                                      const rai::TokenAddress& address,
                                      const rai::AccountTokenInfo& info)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, chain);
        rai::Write(stream, address.bytes);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        info.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.account_token_info_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::AccountTokenInfoGet(rai::Transaction& transaction,
                                      const rai::Account& account,
                                      rai::Chain chain,
                                      const rai::TokenAddress& address,
                                      rai::AccountTokenInfo& info) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, chain);
        rai::Write(stream, address.bytes);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.account_token_info_, key, value);
    IF_ERROR_RETURN(error, true);
    rai::BufferStream stream(value.Data(), value.Size());
    return info.Deserialize(stream);
}

bool rai::Ledger::AccountTokenLinkPut(rai::Transaction& transaction,
                                      const rai::AccountTokenLink& link,
                                      uint64_t previous)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        link.Serialize(stream);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        rai::Write(stream, previous);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.account_token_link_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::AccountTokenLinkGet(rai::Transaction& transaction,
                                      const rai::AccountTokenLink& link,
                                      uint64_t& previous) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        link.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.account_token_link_, key, value);
    IF_ERROR_RETURN(error, true);
    rai::BufferStream stream(value.Data(), value.Size());
    return rai::Read(stream, previous);
}

bool rai::Ledger::AccountTokensInfoPut(rai::Transaction& transaction,
                                       const rai::Account& account,
                                       rai::AccountTokensInfo& info)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        info.Serialize(stream);
    }
    rai::MdbVal key(account);
    rai::MdbVal value(bytes.size(), bytes.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.account_tokens_info_, key, value);
    IF_ERROR_RETURN(error, true);

    return false;
}

bool rai::Ledger::AccountTokensInfoGet(rai::Transaction& transaction,
                                       const rai::Account& account,
                                       rai::AccountTokensInfo& info) const
{
    rai::MdbVal key(account);
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.account_tokens_info_, key, value);
    IF_ERROR_RETURN(error, true);
    rai::BufferStream stream(value.Data(), value.Size());
    return info.Deserialize(stream);
}

bool rai::Ledger::TokenBlockPut(rai::Transaction& transaction,
                                const rai::Account& account, uint64_t height,
                                const rai::TokenBlock& block)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        block.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_, store_.token_block_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::BlockPut(rai::Transaction& transaction,
                           const rai::BlockHash& hash, const rai::Block& block)
{
    return BlockPut(transaction, hash, block, rai::BlockHash(0));
}

bool rai::Ledger::BlockPut(rai::Transaction& transaction,
                           const rai::BlockHash& hash, const rai::Block& block,
                           const rai::BlockHash& successor)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
        rai::Write(stream, successor.bytes);
    }
    rai::MdbVal key(hash);
    rai::MdbVal value(bytes.size(), bytes.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.blocks_, key, value);
    IF_ERROR_RETURN(error, error);

    if (block.Height() % rai::Ledger::BLOCKS_PER_INDEX == 0)
    {
        error = BlockIndexPut_(transaction, block.Account(), block.Height(),
                               block.Hash());
        IF_ERROR_RETURN(error, error);
    }

    return false;
}

bool rai::Ledger::BlockGet(rai::Transaction& transaction,
                           const rai::BlockHash& hash,
                           std::shared_ptr<rai::Block>& block) const
{
    rai::MdbVal key(hash);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.blocks_, key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockUnverify(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    return false;
}

bool rai::Ledger::BlockGet(rai::Transaction& transaction,
                           const rai::BlockHash& hash,
                           std::shared_ptr<rai::Block>& block,
                           rai::BlockHash& successor) const
{
    rai::MdbVal key(hash);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.blocks_, key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockUnverify(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }
    error = rai::Read(stream, successor.bytes);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::BlockGet(rai::Transaction& transaction,
                           const rai::Account& account, uint64_t height,
                           std::shared_ptr<rai::Block>& block) const
{
    rai::AccountInfo info;
    bool error = AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN(error, error);

    if (height < info.tail_height_ || height > info.head_height_)
    {
        return true;
    }

    uint64_t start = (height / rai::Ledger::BLOCKS_PER_INDEX)
                     * rai::Ledger::BLOCKS_PER_INDEX;
    uint64_t end = start + rai::Ledger::BLOCKS_PER_INDEX;
    if (start < info.tail_height_)
    {
        start = info.tail_height_;
    }
    if (end > info.head_height_)
    {
        end = info.head_height_;
    }

    if (height - start < end - height)
    {
        rai::BlockHash hash(info.tail_);
        if (start != info.tail_height_)
        {
            error = BlockIndexGet_(transaction, account, start, hash);
            assert(error == false);
            IF_ERROR_RETURN(error, error);
        }

        for (uint64_t i = start; i <= height; ++i)
        {
            std::shared_ptr<rai::Block> block_l(nullptr);
            error = BlockGet(transaction, hash, block_l, hash);
            assert(error == false);
            IF_ERROR_RETURN(error, true);
            if (height == block_l->Height() && account == block_l->Account())
            {
                block = block_l;
                return false;
            }
        }
    }
    else
    {
        rai::BlockHash hash(info.head_);
        if (end != info.head_height_)
        {
            error = BlockIndexGet_(transaction, account, end, hash);
            assert(error == false);
            IF_ERROR_RETURN(error, error);
        }

        for (uint64_t i = height; i <= end; ++i)
        {
            std::shared_ptr<rai::Block> block_l(nullptr);
            error = BlockGet(transaction, hash, block_l);
            assert(error == false);
            IF_ERROR_RETURN(error, error);
            if (height == block_l->Height() && account == block_l->Account())
            {
                block = block_l;
                return false;
            }
            hash = block_l->Previous();
        }
    }

    assert(0);
    return true;
}

bool rai::Ledger::BlockGet(rai::Transaction& transaction,
                           const rai::Account& account, uint64_t height,
                           std::shared_ptr<rai::Block>& block,
                           rai::BlockHash& successor) const
{
    rai::AccountInfo info;
    bool error = AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN(error, error);

    if (height < info.tail_height_ || height > info.head_height_)
    {
        return true;
    }

    uint64_t start = (height / rai::Ledger::BLOCKS_PER_INDEX)
                     * rai::Ledger::BLOCKS_PER_INDEX;
    uint64_t end = start + rai::Ledger::BLOCKS_PER_INDEX;
    if (start < info.tail_height_)
    {
        start = info.tail_height_;
    }
    if (end > info.head_height_)
    {
        end = info.head_height_;
    }

    if (height - start < end - height)
    {
        rai::BlockHash hash(info.tail_);
        if (start != info.tail_height_)
        {
            error = BlockIndexGet_(transaction, account, start, hash);
            assert(error == false);
            IF_ERROR_RETURN(error, error);
        }

        for (uint64_t i = start; i <= height; ++i)
        {
            std::shared_ptr<rai::Block> block_l(nullptr);
            error = BlockGet(transaction, hash, block_l, hash);
            assert(error == false);
            IF_ERROR_RETURN(error, true);
            if (height == block_l->Height() && account == block_l->Account())
            {
                block = block_l;
                successor = hash;
                return false;
            }
        }
    }
    else
    {
        rai::BlockHash hash(info.head_);
        if (end != info.head_height_)
        {
            error = BlockIndexGet_(transaction, account, end, hash);
            assert(error == false);
            IF_ERROR_RETURN(error, error);
        }

        for (uint64_t i = height; i <= end; ++i)
        {
            std::shared_ptr<rai::Block> block_l(nullptr);
            error = BlockGet(transaction, hash, block_l, hash);
            assert(error == false);
            IF_ERROR_RETURN(error, error);
            if (height == block_l->Height() && account == block_l->Account())
            {
                block = block_l;
                successor = hash;
                return false;
            }
            hash = block_l->Previous();
        }
    }

    assert(0);
    return true;
}

bool rai::Ledger::BlockDel(rai::Transaction& transaction,
                           const rai::BlockHash& hash)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    bool error = BlockGet(transaction, hash, block);
    IF_ERROR_RETURN(error, error);
    if (block->Height() % rai::Ledger::BLOCKS_PER_INDEX == 0)
    {
        error = BlockIndexDel_(transaction, block->Account(), block->Height());
        IF_ERROR_RETURN(error, error);
    }

    rai::MdbVal key(hash);
    return store_.Del(transaction.mdb_transaction_, store_.blocks_, key,
                      nullptr);
}

bool rai::Ledger::BlockCount(rai::Transaction& transaction, size_t& count) const
{
    MDB_stat stat;
    auto ret =
        mdb_stat(transaction.mdb_transaction_, store_.blocks_, &stat);
    if (ret != MDB_SUCCESS)
    {
        assert(0);
        return true;
    }

    count = stat.ms_entries;
    return false;
}

bool rai::Ledger::BlockExists(rai::Transaction& transaction,
                              const rai::BlockHash& hash) const
{
    rai::MdbVal key(hash);
    rai::MdbVal junk;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.blocks_, key, junk);
    if (error)
    {
        return false;
    }
    return true;
}

bool rai::Ledger::BlockSuccessorSet(rai::Transaction& transaction,
                                    const rai::BlockHash& hash,
                                    const rai::BlockHash& successor)
{
    if (!transaction.write_)
    {
        return true;
    }

    rai::MdbVal key(hash);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.blocks_, key, value);
    IF_ERROR_RETURN(error, error);
    std::vector<uint8_t> bytes(value.Data(), value.Data() + value.Size());
    std::copy(successor.bytes.begin(), successor.bytes.end(),
              bytes.end() - successor.bytes.size());

    rai::MdbVal value_new(bytes.size(), bytes.data());
    error = store_.Put(transaction.mdb_transaction_, store_.blocks_, key,
                       value_new);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::BlockSuccessorGet(rai::Transaction& transaction,
                                    const rai::BlockHash& hash,
                                    rai::BlockHash& successor) const
{
    rai::MdbVal key(hash);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.blocks_, key, value);
    IF_ERROR_RETURN(error, error);
    std::copy(value.Data() + value.Size() - successor.bytes.size(),
              value.Data() + value.Size(), successor.bytes.begin());
    return false;
}

bool rai::Ledger::Empty(rai::Transaction& transaction) const
{
    MDB_stat stat;
    auto ret = mdb_stat(transaction.mdb_transaction_, store_.accounts_, &stat);
    if (ret != MDB_SUCCESS)
    {
        assert(0);
        return false;
    }
    if (stat.ms_entries > 0)
    {
        return false;
    }

    ret = mdb_stat(transaction.mdb_transaction_, store_.blocks_, &stat);
    if (ret != MDB_SUCCESS)
    {
        assert(0);
        return false;
    }
    if (stat.ms_entries > 0)
    {
        return false;
    }

    return true;
}

bool rai::Ledger::ForkPut(rai::Transaction& transaction,
                          const rai::Account& account, uint64_t height,
                          const rai::Block& first, const rai::Block& second)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        first.Serialize(stream);
        second.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.forks_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::ForkGet(rai::Transaction& transaction,
                          const rai::Account& account, uint64_t height,
                          std::shared_ptr<rai::Block>& first,
                          std::shared_ptr<rai::Block>& second) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.forks_, key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    first = rai::DeserializeBlockUnverify(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }
    second = rai::DeserializeBlockUnverify(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    return false;
}

bool rai::Ledger::ForkGet(const rai::Iterator& it,
                          std::shared_ptr<rai::Block>& first,
                          std::shared_ptr<rai::Block>& second) const
{
    auto data = it.store_it_->second.Data();
    auto size = it.store_it_->second.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }

    rai::BufferStream stream(data, size);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    first = rai::DeserializeBlockUnverify(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }
    second = rai::DeserializeBlockUnverify(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    return false;
}

bool rai::Ledger::ForkDel(rai::Transaction& transaction,
                          const rai::Account& account)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint64_t> heights;
    rai::Iterator i = ForkLowerBound(transaction, account);
    rai::Iterator n = ForkUpperBound(transaction, account);
    for (; i != n; ++i)
    {
        std::shared_ptr<rai::Block> first(nullptr);
        std::shared_ptr<rai::Block> second(nullptr);
        bool error = ForkGet(i, first, second);
        if (error || first->Account() != account)
        {
            return true;
        }
        heights.push_back(first->Height());
    }

    for (uint64_t height : heights)
    {
        bool error = ForkDel(transaction, account, height);
        IF_ERROR_RETURN(error, true);
    }

    return false;
}

bool rai::Ledger::ForkDel(rai::Transaction& transaction,
                          const rai::Account& account, uint64_t height)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.forks_, key,
                      nullptr);
}

bool rai::Ledger::ForkExists(rai::Transaction& transaction,
                             const rai::Account& account, uint64_t height) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.forks_, key, value);
    IF_ERROR_RETURN(error, false);

    return true;
}

rai::Iterator rai::Ledger::ForkLowerBound(rai::Transaction& transaction,
                                          const rai::Account& account)
{
    uint64_t height = 0;
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_, store_.forks_,
                                key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::ForkUpperBound(rai::Transaction& transaction,
                                          const rai::Account& account)
{
    uint64_t height = std::numeric_limits<uint64_t>::max();
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_, store_.forks_,
                                key);
    return rai::Iterator(std::move(store_it));
}

bool rai::Ledger::NextFork(rai::Transaction& transaction, rai::Account& account,
                           uint64_t& height, std::shared_ptr<rai::Block>& first,
                           std::shared_ptr<rai::Block>& second) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_, store_.forks_,
                                key);
    auto data = store_it->first.Data();
    auto size = store_it->first.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream key_stream(data, size);
    bool error = rai::Read(key_stream, account.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(key_stream, height);
    IF_ERROR_RETURN(error, true);

    data = store_it->second.Data();
    size = store_it->second.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream data_stream(data, size);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    first = rai::DeserializeBlockUnverify(error_code, data_stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }
    second = rai::DeserializeBlockUnverify(error_code, data_stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    return false;
}

bool rai::Ledger::ReceivableInfoPut(rai::Transaction& transaction,
                                    const rai::Account& destination,
                                    const rai::BlockHash& hash,
                                    const rai::ReceivableInfo& info)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, destination.bytes);
        rai::Write(stream, hash.bytes);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        info.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_, store_.receivables_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::ReceivableInfoGet(rai::Transaction& transaction,
                                    const rai::Account& destination,
                                    const rai::BlockHash& hash,
                                    rai::ReceivableInfo& info) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, destination.bytes);
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_, store_.receivables_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    return info.Deserialize(stream);
}

bool rai::Ledger::ReceivableInfoExists(rai::Transaction& transaction,
                                       const rai::Account& destination,
                                       const rai::BlockHash& hash) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, destination.bytes);
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_, store_.receivables_,
                            key, value);
    return !error;
}

bool rai::Ledger::ReceivableInfoGet(const rai::Iterator& it,
                                    rai::Account& destination,
                                    rai::BlockHash& hash,
                                    rai::ReceivableInfo& info) const
{
    auto data = it.store_it_->first.Data();
    auto size = it.store_it_->first.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream_key(data, size);
    bool error = rai::Read(stream_key, destination.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream_key, hash.bytes);
    IF_ERROR_RETURN(error, true);

    data = it.store_it_->second.Data();
    size = it.store_it_->second.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream_value(data, size);
    error = info.Deserialize(stream_value);
    IF_ERROR_RETURN(error, true);

    return false;
}

bool rai::Ledger::ReceivableInfosGet(rai::Transaction& transaction,
                                     rai::ReceivableInfosType type,
                                     rai::ReceivableInfosAll& receivables,
                                     size_t max_size)
{
    rai::StoreIterator store_i(transaction.mdb_transaction_,
                               store_.receivables_);
    rai::Iterator i(std::move(store_i));
    rai::Iterator n(rai::StoreIterator(nullptr));
    for (; i != n; ++i)
    {
        rai::Account account;
        rai::BlockHash hash;
        rai::ReceivableInfo info;
        bool error = ReceivableInfoGet(i, account, hash, info);
        if (error)
        {
            // log
            return true;
        }

        if (type != rai::ReceivableInfosType::ALL)
        {
            std::shared_ptr<rai::Block> block;
            error = BlockGet(transaction, hash, block);
            if (error || block == nullptr)
            {
                // log
                return true;
            }

            rai::AccountInfo account_info;
            error = AccountInfoGet(transaction, block->Account(), account_info);
            if (error || !account_info.Valid())
            {
                // log
                return true;
            }

            if (account_info.Confirmed(block->Height()))
            {
                if (type != rai::ReceivableInfosType::CONFIRMED)
                {
                    continue;
                }
            }
            else
            {
                if (type != rai::ReceivableInfosType::NOT_CONFIRMED)
                {
                    continue;
                }
            }
        }

        receivables.emplace(info, std::make_pair(account, hash));
        if (receivables.size() > max_size)
        {
            receivables.erase(std::prev(receivables.end()));
        }
    }

    return false;
}

bool rai::Ledger::ReceivableInfosGet(rai::Transaction& transaction,
                                     const rai::Account& account,
                                     rai::ReceivableInfosType type,
                                     rai::ReceivableInfos& receivables,
                                     size_t max_size)
{
    rai::Iterator i = ReceivableInfoLowerBound(transaction, account);
    rai::Iterator n = ReceivableInfoUpperBound(transaction, account);
    for (; i != n; ++i)
    {
        rai::Account ignore;
        rai::BlockHash hash;
        rai::ReceivableInfo info;
        bool error = ReceivableInfoGet(i, ignore, hash, info);
        if (error)
        {
            // log
            return true;
        }

        if (type != rai::ReceivableInfosType::ALL)
        {
            std::shared_ptr<rai::Block> block;
            error = BlockGet(transaction, hash, block);
            if (error || block == nullptr)
            {
                // log
                return true;
            }

            rai::AccountInfo account_info;
            error = AccountInfoGet(transaction, block->Account(), account_info);
            if (error || !account_info.Valid())
            {
                // log
                return true;
            }

            if (account_info.Confirmed(block->Height()))
            {
                if (type != rai::ReceivableInfosType::CONFIRMED)
                {
                    continue;
                }
            }
            else
            {
                if (type != rai::ReceivableInfosType::NOT_CONFIRMED)
                {
                    continue;
                }
            }
        }

        receivables.emplace(info, hash);
        if (receivables.size() > max_size)
        {
            receivables.erase(std::prev(receivables.end()));
        }
    }

    return false;
}

bool rai::Ledger::ReceivableInfoDel(rai::Transaction& transaction,
                                    const rai::Account& destination,
                                    const rai::BlockHash& hash)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, destination.bytes);
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.receivables_, key,
                      nullptr);
}

bool rai::Ledger::ReceivableInfoCount(rai::Transaction& transaction,
                                      size_t& count) const
{
    MDB_stat stat;
    auto ret =
        mdb_stat(transaction.mdb_transaction_, store_.receivables_, &stat);
    if (ret != MDB_SUCCESS)
    {
        assert(0);
        return true;
    }

    count = stat.ms_entries;
    return false;
}

rai::Iterator rai::Ledger::ReceivableInfoLowerBound(
    rai::Transaction& transaction, const rai::Account& account)
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::BlockHash hash(0);
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.receivables_, key);
    return rai::Iterator(std::move(store_it));
}


rai::Iterator rai::Ledger::ReceivableInfoUpperBound(
    rai::Transaction& transaction, const rai::Account& account)
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::BlockHash hash(std::numeric_limits<rai::uint256_t>::max());
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.receivables_, key);
    return rai::Iterator(std::move(store_it));
}

bool rai::Ledger::RewardableInfoPut(rai::Transaction& transaction,
                                    const rai::Account& representative,
                                    const rai::BlockHash& hash,
                                    const rai::RewardableInfo& info)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, representative.bytes);
        rai::Write(stream, hash.bytes);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        info.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_, store_.rewardables_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::RewardableInfoGet(rai::Transaction& transaction,
                                    const rai::Account& representative,
                                    const rai::BlockHash& hash,
                                    rai::RewardableInfo& info) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, representative.bytes);
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_, store_.rewardables_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    return info.Deserialize(stream);
}


bool rai::Ledger::RewardableInfoGet(const rai::Iterator& it,
                                    rai::Account& destination,
                                    rai::BlockHash& hash,
                                    rai::RewardableInfo& info) const
{
    auto data = it.store_it_->first.Data();
    auto size = it.store_it_->first.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream_key(data, size);
    bool error = rai::Read(stream_key, destination.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream_key, hash.bytes);
    IF_ERROR_RETURN(error, true);

    data = it.store_it_->second.Data();
    size = it.store_it_->second.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream_value(data, size);
    error = info.Deserialize(stream_value);
    IF_ERROR_RETURN(error, true);

    return false;
}

bool rai::Ledger::RewardableInfoDel(rai::Transaction& transaction,
                                    const rai::Account& representative,
                                    const rai::BlockHash& hash)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, representative.bytes);
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.rewardables_, key,
                      nullptr);
}


rai::Iterator rai::Ledger::RewardableInfoLowerBound(
    rai::Transaction& transaction, const rai::Account& account)
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::BlockHash hash(0);
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.rewardables_, key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::RewardableInfoUpperBound(
    rai::Transaction& transaction, const rai::Account& account)
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::BlockHash hash(std::numeric_limits<rai::uint256_t>::max());
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.rewardables_, key);
    return rai::Iterator(std::move(store_it));
}

bool rai::Ledger::RollbackBlockPut(rai::Transaction& transaction,
                                   const rai::BlockHash& hash,
                                   const rai::Block& block)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
    }
    rai::MdbVal key(hash);
    rai::MdbVal value(bytes.size(), bytes.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.rollbacks_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::RollbackBlockGet(rai::Transaction& transaction,
                                   const rai::BlockHash& hash,
                                   std::shared_ptr<rai::Block>& block) const
{
    rai::MdbVal key(hash);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.rollbacks_, key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockUnverify(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    return false;
}

void rai::Ledger::RepWeightAdd(rai::Transaction& transaction,
                               const rai::Account& representative,
                               const rai::Amount& weight)
{
    rai::RepWeightOpration op(true, representative, weight);
    transaction.rep_weight_operations_.push_back(op);
}

void rai::Ledger::RepWeightSub(rai::Transaction& transaction,
                               const rai::Account& representative,
                               const rai::Amount& weight)
{
    rai::RepWeightOpration op(false, representative, weight);
    transaction.rep_weight_operations_.push_back(op);
}

bool rai::Ledger::RepWeightGet(const rai::Account& representative,
                               rai::Amount& weight) const
{
    std::lock_guard<std::mutex> lock(rep_weights_mutex_);
    auto it = rep_weights_.find(representative);
    if (it == rep_weights_.end())
    {
        return true;
    }
    weight = it->second;
    return false;
}

void rai::Ledger::RepWeightTotalGet(rai::Amount& total) const
{
    std::lock_guard<std::mutex> lock(rep_weights_mutex_);
    total = total_rep_weight_;
}

void rai::Ledger::RepWeightsGet(
    rai::Amount& total,
    std::unordered_map<rai::Account, rai::Amount>& weights) const
{
    std::lock_guard<std::mutex> lock(rep_weights_mutex_);
    total = total_rep_weight_;
    weights = rep_weights_;
}

bool rai::Ledger::SourcePut(rai::Transaction& transaction,
                            const rai::BlockHash& source)
{
    if (!transaction.write_)
    {
        return true;
    }


    rai::MdbVal key(source);
    uint8_t junk = 0;
    rai::MdbVal value(sizeof(junk), &junk);
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.sources_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::SourcePut(rai::Transaction& transaction,
                            const rai::BlockHash& source,
                            const rai::Block& block)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
    }
    rai::MdbVal key(source);
    rai::MdbVal value(bytes.size(), bytes.data());

    bool error =
        store_.Put(transaction.mdb_transaction_, store_.sources_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::SourceGet(rai::Transaction& transaction,
                            const rai::BlockHash& source,
                            std::shared_ptr<rai::Block>& block) const
{
    block = nullptr;
    rai::MdbVal key(source);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.sources_, key, value);
    IF_ERROR_RETURN(error, error);

    uint8_t junk;
    if (value.Size() > sizeof(junk))
    {
        rai::BufferStream stream(value.Data(), value.Size());
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        block = rai::DeserializeBlockUnverify(error_code, stream);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            return true;
        }
    }

    return false;
}

bool rai::Ledger::SourceDel(rai::Transaction& transaction,
                            const rai::BlockHash& source)
{
    if (!transaction.write_)
    {
        return true;
    }

    rai::MdbVal key(source);
    return store_.Del(transaction.mdb_transaction_, store_.sources_, key,
                      nullptr);
}

bool rai::Ledger::SourceExists(rai::Transaction& transaction,
                               const rai::BlockHash& source) const
{
    rai::MdbVal key(source);
    rai::MdbVal junk;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.sources_, key, junk);
    if (error)
    {
        return false;
    }
    return true;
}

bool rai::Ledger::WalletInfoPut(rai::Transaction& transaction, uint32_t id,
                                const rai::WalletInfo& info)
{
    if (!transaction.write_)
    {
        return true;
    }

    if (id == 0)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, id);
        uint32_t account_id = 0;
        rai::Write(stream, account_id);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());

    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        info.Serialize(stream);
    }
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.wallets_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::WalletInfoGet(rai::Transaction& transaction, uint32_t id,
                                rai::WalletInfo& info) const
{
    if (id == 0)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, id);
        uint32_t account_id = 0;
        rai::Write(stream, account_id);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());

    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.wallets_, key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    error = info.Deserialize(stream);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::WalletInfoGetAll(
    rai::Transaction& transaction,
    std::vector<std::pair<uint32_t, rai::WalletInfo>>& results) const
{
    rai::StoreIterator it(transaction.mdb_transaction_, store_.wallets_);
    rai::StoreIterator end(nullptr);
    for (; it != end; ++it)
    {
        auto data = it->first.Data();
        auto size = it->first.Size();
        if (data == nullptr || size == 0)
        {
            assert(0);
            return true;
        }

        uint32_t wallet_id = 0;
        uint32_t account_id = 0;
        rai::BufferStream stream_key(data, size);
        bool error = rai::Read(stream_key, wallet_id);
        IF_ERROR_RETURN(error, true);
        error = rai::Read(stream_key, account_id);
        IF_ERROR_RETURN(error, true);
        if (account_id != 0)
        {
            continue;
        }
        
        data = it->second.Data();
        size = it->second.Size();
        if (data == nullptr || size == 0)
        {
            assert(0);
            return true;
        }

        rai::WalletInfo info;
        rai::BufferStream stream_value(data, size);
        error = info.Deserialize(stream_value);
        IF_ERROR_RETURN(error, true);

        results.emplace_back(wallet_id, info);
    }

    return false;
}

bool rai::Ledger::WalletAccountInfoPut(rai::Transaction& transaction,
                                       uint32_t wallet_id, uint32_t account_id,
                                       const rai::WalletAccountInfo& info)
{
    if (!transaction.write_)
    {
        return true;
    }

    if (wallet_id == 0 || account_id == 0)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, wallet_id);
        rai::Write(stream, account_id);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());

    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        info.Serialize(stream);
    }
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.wallets_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::WalletAccountInfoGet(rai::Transaction& transaction,
                                       uint32_t wallet_id, uint32_t account_id,
                                       rai::WalletAccountInfo& info) const
{
    if (wallet_id == 0 || account_id == 0)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, wallet_id);
        rai::Write(stream, account_id);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());

    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.wallets_, key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    error = info.Deserialize(stream);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::WalletAccountInfoGetAll(
    rai::Transaction& transaction, uint32_t wallet_id,
    std::vector<std::pair<uint32_t, rai::WalletAccountInfo>>& results) const
{
    std::vector<uint8_t> bytes_key_begin;
    {
        rai::VectorStream stream(bytes_key_begin);
        rai::Write(stream, wallet_id);
        uint32_t account_id = 1;
        rai::Write(stream, account_id);
    }
    rai::MdbVal key_begin(bytes_key_begin.size(), bytes_key_begin.data());
    rai::StoreIterator it(transaction.mdb_transaction_, store_.wallets_,
                          key_begin);

    std::vector<uint8_t> bytes_key_end;
    {
        rai::VectorStream stream(bytes_key_end);
        rai::Write(stream, wallet_id);
        uint32_t account_id = std::numeric_limits<uint32_t>::max();
        rai::Write(stream, account_id);
    }
    rai::MdbVal key_end(bytes_key_end.size(), bytes_key_end.data());
    rai::StoreIterator end(transaction.mdb_transaction_, store_.wallets_,
                          key_end);

    for (; it != end; ++it)
    {
        auto data = it->first.Data();
        auto size = it->first.Size();
        if (data == nullptr || size == 0)
        {
            assert(0);
            return true;
        }

        uint32_t wallet_id_l = 0;
        uint32_t account_id = 0;
        rai::BufferStream stream_key(data, size);
        bool error = rai::Read(stream_key, wallet_id_l);
        IF_ERROR_RETURN(error, true);
        error = rai::Read(stream_key, account_id);
        IF_ERROR_RETURN(error, true);
        if (wallet_id_l != wallet_id || account_id == 0)
        {
            assert(0);
            return true;
        }
        
        data = it->second.Data();
        size = it->second.Size();
        if (data == nullptr || size == 0)
        {
            assert(0);
            return true;
        }

        rai::WalletAccountInfo info;
        rai::BufferStream stream_value(data, size);
        error = info.Deserialize(stream_value);
        IF_ERROR_RETURN(error, true);

        results.emplace_back(account_id, info);
    }

    return false;
}

bool rai::Ledger::SelectedWalletIdPut(rai::Transaction& transaction,
                                      uint32_t wallet_id)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, rai::MetaKey::SELECTED_WALLET_ID);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());

    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        rai::Write(stream, wallet_id);
    }
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.meta_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::SelectedWalletIdGet(rai::Transaction& transaction,
                                      uint32_t& wallet_id) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, rai::MetaKey::SELECTED_WALLET_ID);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());

    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.meta_, key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    error = rai::Read(stream, wallet_id);
    IF_ERROR_RETURN(error, error);

    return false;
}


bool rai::Ledger::VersionPut(rai::Transaction& transaction,
                                      uint32_t version)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, rai::MetaKey::VERSION);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());

    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        rai::Write(stream, version);
    }
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.meta_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::VersionGet(rai::Transaction& transaction,
                                      uint32_t& version) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, rai::MetaKey::VERSION);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());

    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.meta_, key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    error = rai::Read(stream, version);
    IF_ERROR_RETURN(error, error);

    return false;
}

void rai::Ledger::UpdateRichList(const rai::Block& block)
{
    if (!enable_rich_list_ || block.Type() != rai::BlockType::TX_BLOCK)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(rich_list_mutex_);
    UpdateRichList_(block.Account(), block.Balance());
}

std::vector<rai::RichListEntry> rai::Ledger::GetRichList(uint64_t max)
{
    size_t size = max;
    std::vector<rai::RichListEntry> result;
    std::lock_guard<std::mutex> lock(rich_list_mutex_);

    if (rich_list_.size() < size)
    {
        size = rich_list_.size();
    }
    result.reserve(size);

    size_t count = 0;
    for (auto i = rich_list_.begin(), n = rich_list_.end(); i != n; ++i)
    {
        if (count >= max)
        {
            break;
        }

        result.push_back(*i);
        ++count;
    }

    return result;
}

void rai::Ledger::UpdateDelegatorList(const rai::Block& block)
{
    if (!enable_delegator_list_ || !block.HasRepresentative())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(delegator_list_mutex_);
    UpdateDelegatorList_(block.Account(), block.Representative(),
                         block.Balance(), block.Type());
}

std::vector<rai::DelegatorListEntry> rai::Ledger::GetDelegatorList(
    const rai::Account& rep, uint64_t max)
{
    std::multimap<rai::Amount, rai::DelegatorListEntry,
                  std::greater<rai::Amount>>
        delegators;
    std::vector<rai::DelegatorListEntry> result;
    std::lock_guard<std::mutex> lock(delegator_list_mutex_);

    auto i = delegator_list_.get<1>().lower_bound(rep);
    auto n = delegator_list_.get<1>().upper_bound(rep);
    for (; i != n; ++i)
    {
        delegators.insert(std::make_pair(i->weight_, *i));
        if (delegators.size() > max)
        {
            delegators.erase(std::prev(delegators.end()));
        }
    }

    result.reserve(delegators.size());

    for (const auto& i : delegators)
    {
        result.push_back(i.second);
    }
    return result;
}

rai::ErrorCode rai::Ledger::UpgradeWallet(rai::Transaction& transaction)
{
    uint32_t version = 0;
    bool error = VersionGet(transaction, version);
    if (error)
    {
        version = 1;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    switch (version)
    {
        case 1:
        {
            error_code = UpgradeWalletV1V2(transaction);
            IF_NOT_SUCCESS_RETURN(error_code);
        }
        case 2:
        {
            break;
        }
        default:
        {
            return rai::ErrorCode::LEDGER_UNKNOWN_VERSION;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Ledger::UpgradeWalletV1V2(rai::Transaction& transaction)
{
    uint64_t count = 0;
    for (auto i = AccountInfoBegin(transaction),
              n = AccountInfoEnd(transaction);
         i != n; ++i)
    {
        rai::Account account;
        rai::AccountInfo info;
        bool error = AccountInfoGet(i, account, info);
        if (error)
        {
            return rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET;
        }

        if (info.head_height_ == rai::Block::INVALID_HEIGHT)
        {
            continue;
        }

        rai::BlockHash hash(info.head_);
        while (!hash.IsZero())
        {
            std::shared_ptr<rai::Block> block(nullptr);
            error = BlockGet(transaction, hash, block);
            if (error)
            {
                return rai::ErrorCode::LEDGER_BLOCK_GET;
            }

            if (block->Opcode() == rai::BlockOpcode::RECEIVE)
            {
                error = SourcePut(transaction, block->Link());
                if (error)
                {
                    return rai::ErrorCode::LEDGER_SOURCE_PUT;
                }
                ++count;
            }
            hash = block->Previous();
        }
    }

    bool error = VersionPut(transaction, 2);
    if (error)
    {
        return rai::ErrorCode::LEDGER_VERSION_PUT;
    }

    std::cout << "Upgrade wallet ledger from V1 to V2, put " << count
              << " receivable sources to the ledger" << std::endl;

    return rai::ErrorCode::SUCCESS;
}

bool rai::Ledger::BlockIndexPut_(rai::Transaction& transaction,
                                 const rai::Account& account, uint64_t height,
                                 const rai::BlockHash& hash)
{
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes.size(), bytes.data());
    rai::MdbVal value(hash);
    bool error = store_.Put(transaction.mdb_transaction_, store_.blocks_index_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::BlockIndexGet_(rai::Transaction& transaction,
                                 const rai::Account& account, uint64_t height,
                                 rai::BlockHash& hash) const
{
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes.size(), bytes.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_, store_.blocks_index_,
                            key, value);
    IF_ERROR_RETURN(error, error);
    assert(value.Size() == sizeof(hash));

    hash = value.uint256_union();
    return false;
}

bool rai::Ledger::BlockIndexDel_(rai::Transaction& transaction,
                                 const rai::Account& account, uint64_t height)
{
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes.size(), bytes.data());
    return store_.Del(transaction.mdb_transaction_, store_.blocks_index_, key,
                      nullptr);
}

void rai::Ledger::RepWeightsCommit_(
    const std::vector<rai::RepWeightOpration>& ops)
{
    std::lock_guard<std::mutex> lock(rep_weights_mutex_);
    for (const auto& op : ops)
    {
        if (op.weight_.IsZero())
        {
            continue;
        }

        if (op.add_)
        {
            if (rep_weights_.find(op.representative_) == rep_weights_.end())
            {
                rep_weights_[op.representative_] = op.weight_;
            }
            else
            {
                rep_weights_[op.representative_] += op.weight_;
                assert(rep_weights_[op.representative_] > op.weight_);
            }
            total_rep_weight_ += op.weight_;
        }
        else
        {
            if (rep_weights_.find(op.representative_) == rep_weights_.end())
            {
                assert(0);
                continue;
            }
            if (op.weight_ > rep_weights_[op.representative_])
            {
                assert(0);
                rep_weights_.erase(op.representative_);
                total_rep_weight_ -= rep_weights_[op.representative_];
            }
            else if (op.weight_ == rep_weights_[op.representative_])
            {
                rep_weights_.erase(op.representative_);
                total_rep_weight_ -= op.weight_;
            }
            else
            {
                rep_weights_[op.representative_] -= op.weight_;
                total_rep_weight_ -= op.weight_;
            }
        }
    }
}

rai::ErrorCode rai::Ledger::InitMemoryTables_(rai::Transaction& transaction)
{
    std::lock_guard<std::mutex> lock_rep_weights(rep_weights_mutex_);
    std::lock_guard<std::mutex> lock_rich_list(rich_list_mutex_);
    std::lock_guard<std::mutex> lock_delegator_list(delegator_list_mutex_);

    for (auto i = AccountInfoBegin(transaction),
              n = AccountInfoEnd(transaction);
         i != n; ++i)
    {
        rai::Account account;
        rai::AccountInfo info;
        bool error = AccountInfoGet(i, account, info);
        IF_ERROR_RETURN(error, rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET);

        if (info.head_height_ == rai::Block::INVALID_HEIGHT)
        {
            assert(0);
            continue;
        }
        std::shared_ptr<rai::Block> block(nullptr);
        error = BlockGet(transaction, info.head_, block);
        IF_ERROR_RETURN(error, rai::ErrorCode::LEDGER_BLOCK_GET);

        if (block->HasRepresentative() && !block->Balance().IsZero())
        {
            rep_weights_[block->Representative()] += block->Balance();
            total_rep_weight_ += block->Balance();
        }

        if (enable_delegator_list_ && block->HasRepresentative())
        {
            UpdateDelegatorList_(account, block->Representative(),
                                 block->Balance(), block->Type());
        }

        if (enable_rich_list_ && block->Type() == rai::BlockType::TX_BLOCK)
        {
            UpdateRichList_(block->Account(), block->Balance());
        }
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::Ledger::UpdateRichList_(const rai::Account& account,
                                  const rai::Amount& balance)
{
    auto it = rich_list_.get<1>().find(account);
    if (it == rich_list_.get<1>().end())
    {
        if (balance < rai::Ledger::RICH_LIST_MINIMUM)
        {
            return;
        }
        rich_list_.insert({account, balance});
    }
    else
    {
        if (balance < rai::Ledger::RICH_LIST_MINIMUM)
        {
            rich_list_.get<1>().erase(it);
            return;
        }
        rich_list_.get<1>().modify(
            it, [&](rai::RichListEntry& data) { data.balance_ = balance; });
    }
}

void rai::Ledger::UpdateDelegatorList_(const rai::Account& account,
                                       const rai::Account& rep,
                                       const rai::Amount& weight,
                                       rai::BlockType type)
{
    auto it = delegator_list_.find(account);
    if (it == delegator_list_.end())
    {
        delegator_list_.insert({account, rep, weight, type});
    }
    else
    {
        delegator_list_.modify(it, [&](rai::DelegatorListEntry& data) {
            data.rep_    = rep;
            data.weight_ = weight;
            data.type_ = type;
        });
    }
}

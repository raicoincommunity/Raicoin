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

rai::Iterator::Iterator() : store_it_(nullptr)
{
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

rai::Iterator& rai::Iterator::operator=(rai::Iterator&& other)
{
    store_it_ = std::move(other.store_it_);
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

bool rai::TokenKey::Valid() const
{
    return chain_ != rai::Chain::INVALID && !address_.IsZero();
}

rai::TokenTransfer::TokenTransfer()
    : token_(), ts_comp_(0), account_(0), height_(0)
{
}

rai::TokenTransfer::TokenTransfer(const rai::TokenKey& token,
                                  uint64_t timestamp,
                                  const rai::Account& account, uint64_t height)
    : token_(token), ts_comp_(~timestamp), account_(account), height_(height)
{
}

void rai::TokenTransfer::Serialize(rai::Stream& stream) const
{
    token_.Serialize(stream);
    rai::Write(stream, ts_comp_);
    rai::Write(stream, account_.bytes);
    rai::Write(stream, height_);
}

bool rai::TokenTransfer::Deserialize(rai::Stream& stream)
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

rai::TokenIdTransferKey::TokenIdTransferKey() : token_(), id_(0), index_comp_(0)
{
}

rai::TokenIdTransferKey::TokenIdTransferKey(const rai::TokenKey& token,
                                            const rai::TokenValue& id,
                                            uint64_t index)
    : token_(token), id_(id), index_comp_(~index)
{
}

void rai::TokenIdTransferKey::Serialize(rai::Stream& stream) const
{
    token_.Serialize(stream);
    rai::Write(stream, id_.bytes);
    rai::Write(stream, index_comp_);
}

bool rai::TokenIdTransferKey::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = token_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, id_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, index_comp_);
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
      created_at_(rai::Block::INVALID_HEIGHT),
      total_supply_(0),
      cap_supply_(0),
      local_supply_(0)
{
}

rai::TokenInfo::TokenInfo(rai::TokenType type, const std::string& symbol,
                          const std::string& name, uint8_t decimals,
                          bool burnable, bool mintable, bool circulable,
                          const rai::TokenValue& cap_supply,
                          uint64_t created_at)
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
      created_at_(created_at),
      total_supply_(0),
      cap_supply_(cap_supply),
      local_supply_(0)
{
}

rai::TokenInfo::TokenInfo(rai::TokenType type, const std::string& symbol,
                          const std::string& name, bool burnable,
                          bool circulable, const rai::TokenValue& cap_supply,
                          const std::string base_uri, uint64_t created_at)
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
      created_at_(created_at),
      total_supply_(0),
      cap_supply_(cap_supply),
      local_supply_(0),
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
    rai::Write(stream, created_at_);
    rai::Write(stream, total_supply_.bytes);
    rai::Write(stream, cap_supply_.bytes);
    rai::Write(stream, local_supply_.bytes);
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
    error = rai::Read(stream, created_at_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, total_supply_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, cap_supply_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, local_supply_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, base_uri_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::TokenBlock::TokenBlock()
    : previous_(rai::Block::INVALID_HEIGHT),
      hash_(0),
      status_(rai::ErrorCode::SUCCESS),
      value_(0),
      value_op_(rai::TokenBlock::ValueOp::NONE),
      token_()
{
}

rai::TokenBlock::TokenBlock(uint64_t previous, const rai::BlockHash& hash,
                            rai::ErrorCode status, const rai::TokenValue& value,
                            rai::TokenBlock::ValueOp value_op,
                            const rai::TokenKey& token)
    : previous_(previous),
      hash_(hash),
      status_(status),
      value_(value),
      value_op_(value_op),
      token_(token)
{
}

void rai::TokenBlock::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, previous_);
    rai::Write(stream, hash_.bytes);
    rai::Write(stream, status_);
    rai::Write(stream, value_.bytes);
    rai::Write(stream, value_op_);
    token_.Serialize(stream);
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
    error = rai::Read(stream, value_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, value_op_);
    IF_ERROR_RETURN(error, true);
    error = token_.Deserialize(stream);
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

rai::TokenReceivableKey::TokenReceivableKey()
    : to_(0), token_(), chain_(rai::Chain::INVALID), tx_hash_(0)
{
}

rai::TokenReceivableKey::TokenReceivableKey(const rai::Account& to,
                                            const rai::TokenKey& token,
                                            rai::Chain chain,
                                            const rai::BlockHash& tx_hash)
    : to_(to), token_(token), chain_(chain), tx_hash_(tx_hash)
{
}

void rai::TokenReceivableKey::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, to_.bytes);
    token_.Serialize(stream);
    rai::Write(stream, chain_);
    rai::Write(stream, tx_hash_.bytes);
}

bool rai::TokenReceivableKey::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, to_.bytes);
    IF_ERROR_RETURN(error, true);
    error = token_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, chain_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, tx_hash_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::TokenReceivable::TokenReceivable()
    : from_(0),
      value_(0),
      block_height_(rai::Block::INVALID_HEIGHT),
      token_type_(rai::TokenType::INVALID),
      source_(rai::TokenSource::INVALID)
{
}

rai::TokenReceivable::TokenReceivable(const rai::Account& from,
                                      const rai::TokenValue& value,
                                      uint64_t block_height,
                                      rai::TokenType token_type,
                                      rai::TokenSource source)
    : from_(from),
      value_(value),
      block_height_(block_height),
      token_type_(token_type),
      source_(source)
{
}

void rai::TokenReceivable::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, from_.bytes);
    rai::Write(stream, value_.bytes);
    rai::Write(stream, block_height_);
    rai::Write(stream, token_type_);
    rai::Write(stream, source_);
}

bool rai::TokenReceivable::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, from_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, value_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, block_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, token_type_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, source_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::TokenIdInfo::TokenIdInfo()
    : burned_(false), unmapped_(false), wrapped_(false), transfers_(0)
{
}

rai::TokenIdInfo::TokenIdInfo(const std::string& uri)
    : burned_(false),
      unmapped_(false),
      wrapped_(false),
      transfers_(0),
      uri_(uri)
{
}

void rai::TokenIdInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, burned_);
    rai::Write(stream, unmapped_);
    rai::Write(stream, wrapped_);
    rai::Write(stream, transfers_);
    rai::Write(stream, uri_);
}

bool rai::TokenIdInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, burned_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, unmapped_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, wrapped_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, transfers_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, uri_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::AccountTokensInfo::AccountTokensInfo()
    : head_(rai::Block::INVALID_HEIGHT), blocks_(0)
{
}

rai::AccountTokensInfo::AccountTokensInfo(uint64_t head, uint64_t blocks)
    : head_(head), blocks_(blocks)
{
}

void rai::AccountTokensInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, head_);
    rai::Write(stream, blocks_);
}

bool rai::AccountTokensInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, head_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, blocks_);
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
      type_offer_(rai::TokenType::INVALID),
      id_offer_(0),
      token_want_(),
      type_want_(rai::TokenType::INVALID),
      id_want_(0),
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
      type_offer_(rai::TokenType::_20),
      id_offer_(0),
      token_want_(chain_want, address_want),
      type_want_(rai::TokenType::_20),
      id_want_(0),
      rate_(rate),
      maker_(maker),
      height_(height)
{
}

rai::OrderIndex::OrderIndex(rai::Chain chain_offer,
                            const rai::TokenAddress& address_offer,
                            rai::Chain chain_want,
                            const rai::TokenAddress& address_want,
                            const rai::TokenValue& id_want,
                            const rai::SwapRate& rate,
                            const rai::Account& maker, uint64_t height)
    : token_offer_(chain_offer, address_offer),
      type_offer_(rai::TokenType::_20),
      id_offer_(0),
      token_want_(chain_want, address_want),
      type_want_(rai::TokenType::_721),
      id_want_(id_want),
      rate_(rate),
      maker_(maker),
      height_(height)
{
}

rai::OrderIndex::OrderIndex(rai::Chain chain_offer,
                            const rai::TokenAddress& address_offer,
                            const rai::TokenValue& id_offer,
                            rai::Chain chain_want,
                            const rai::TokenAddress& address_want,
                            const rai::SwapRate& rate,
                            const rai::Account& maker, uint64_t height)
    : token_offer_(chain_offer, address_offer),
      type_offer_(rai::TokenType::_721),
      id_offer_(id_offer),
      token_want_(chain_want, address_want),
      type_want_(rai::TokenType::_20),
      id_want_(0),
      rate_(rate),
      maker_(maker),
      height_(height)
{
}

rai::OrderIndex::OrderIndex(rai::Chain chain_offer,
                            const rai::TokenAddress& address_offer,
                            const rai::TokenValue& id_offer,
                            rai::Chain chain_want,
                            const rai::TokenAddress& address_want,
                            const rai::TokenValue& id_want,
                            const rai::SwapRate& rate,
                            const rai::Account& maker, uint64_t height)
    : token_offer_(chain_offer, address_offer),
      type_offer_(rai::TokenType::_721),
      token_want_(chain_want, address_want),
      type_want_(rai::TokenType::_721),
      id_offer_(id_offer),
      id_want_(id_want),
      rate_(rate),
      maker_(maker),
      height_(height)
{
}

rai::OrderIndex::OrderIndex(const rai::TokenKey& token_offer,
                            rai::TokenType type_offer,
                            const rai::TokenKey& token_want,
                            rai::TokenType type_want)
    : OrderIndex(token_offer, type_offer, token_want, type_want, 0)
{
}

rai::OrderIndex::OrderIndex(const rai::TokenKey& token_offer,
                            rai::TokenType type_offer,
                            const rai::TokenKey& token_want,
                            rai::TokenType type_want,
                            const rai::TokenValue& id_offer)
    : token_offer_(token_offer),
      type_offer_(type_offer),
      token_want_(token_want),
      type_want_(type_want),
      id_offer_(id_offer),
      id_want_(0),
      rate_(0),
      maker_(0),
      height_(0)
{
}

void rai::OrderIndex::Serialize(rai::Stream& stream) const
{
    token_offer_.Serialize(stream);
    rai::Write(stream, type_offer_);
    token_want_.Serialize(stream);
    rai::Write(stream, type_want_);
    rai::Write(stream, id_offer_.bytes);
    rai::Write(stream, id_want_.bytes);
    rai::Write(stream, rate_.bytes);
    rai::Write(stream, maker_.bytes);
    rai::Write(stream, height_);
}

bool rai::OrderIndex::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = token_offer_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, type_offer_);
    IF_ERROR_RETURN(error, true);
    error = token_want_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, type_want_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, id_offer_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, id_want_.bytes);
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
      type_offer_(rai::TokenType::INVALID),
      token_want_(),
      type_want_(rai::TokenType::INVALID),
      value_offer_(0),
      value_want_(0),
      min_offer_(0),
      max_offer_(0),
      left_(0),
      finished_height_(rai::Block::INVALID_HEIGHT),
      finished_by_(rai::OrderInfo::FinishedBy::INVALID),
      hash_(0)
{
}

rai::OrderInfo::OrderInfo(const rai::Account& main_account,
                          rai::Chain chain_offer,
                          const rai::TokenAddress& address_offer,
                          rai::TokenType type_offer, rai::Chain chain_want,
                          const rai::TokenAddress& address_want,
                          rai::TokenType type_want,
                          const rai::TokenValue& value_offer,
                          const rai::TokenValue& value_want, uint64_t timeout,
                          const rai::BlockHash& hash)
    : main_(main_account),
      token_offer_(chain_offer, address_offer),
      type_offer_(type_offer),
      token_want_(chain_want, address_want),
      type_want_(type_want),
      value_offer_(value_offer),
      value_want_(value_want),
      min_offer_(0),
      max_offer_(0),
      left_(0),
      timeout_(timeout),
      finished_height_(rai::Block::INVALID_HEIGHT),
      finished_by_(rai::OrderInfo::FinishedBy::INVALID),
      hash_(hash)
{
}

rai::OrderInfo::OrderInfo(
    const rai::Account& main_account, rai::Chain chain_offer,
    const rai::TokenAddress& address_offer, rai::TokenType type_offer,
    rai::Chain chain_want, const rai::TokenAddress& address_want,
    rai::TokenType type_want, const rai::TokenValue& value_offer,
    const rai::TokenValue& value_want, const rai::TokenValue& min_offer,
    const rai::TokenValue& max_offer, uint64_t timeout,
    const rai::BlockHash& hash)
    : main_(main_account),
      token_offer_(chain_offer, address_offer),
      type_offer_(type_offer),
      token_want_(chain_want, address_want),
      type_want_(type_want),
      value_offer_(value_offer),
      value_want_(value_want),
      min_offer_(min_offer),
      max_offer_(max_offer),
      left_(max_offer),
      timeout_(timeout),
      finished_height_(rai::Block::INVALID_HEIGHT),
      finished_by_(rai::OrderInfo::FinishedBy::INVALID),
      hash_(hash)
{
}

void rai::OrderInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, main_.bytes);
    token_offer_.Serialize(stream);
    rai::Write(stream, type_offer_);
    token_want_.Serialize(stream);
    rai::Write(stream, type_want_);
    rai::Write(stream, value_offer_.bytes);
    rai::Write(stream, value_want_.bytes);
    rai::Write(stream, min_offer_.bytes);
    rai::Write(stream, max_offer_.bytes);
    rai::Write(stream, left_.bytes);
    rai::Write(stream, timeout_);
    rai::Write(stream, finished_height_);
    rai::Write(stream, finished_by_);
    rai::Write(stream, hash_.bytes);
}

bool rai::OrderInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, main_.bytes);
    IF_ERROR_RETURN(error, true);
    error = token_offer_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, type_offer_);
    IF_ERROR_RETURN(error, true);
    error = token_want_.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, type_want_);
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
    error = rai::Read(stream, finished_by_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, hash_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

bool rai::OrderInfo::Finished() const
{
    return finished_by_ != rai::OrderInfo::FinishedBy::INVALID
           || finished_height_ != rai::Block::INVALID_HEIGHT;
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

rai::TokenSwapIndex::TokenSwapIndex(const rai::TokenKey& token,
                                    const rai::Account& taker,
                                    uint64_t inquiry_height, uint64_t timestamp)
    : token_(token),
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
    rai::Write(stream, trade_previous_.bytes);
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
    error = rai::Read(stream, trade_previous_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

std::string rai::SwapInfo::StatusToString(rai::SwapInfo::Status status)
{
    using S = rai::SwapInfo::Status;
    switch (status)
    {
        case S::INQUIRY:
        {
            return "inquiry";
        }
        case S::INQUIRY_ACK:
        {
            return "inquiry_ack";
        }
        case S::INQUIRY_NACK:
        {
            return "inquiry_nack";
        }
        case S::TAKE:
        {
            return "take";
        }
        case S::TAKE_ACK:
        {
            return "take_ack";
        }
        case S::TAKE_NACK:
        {
            return "take_nack";
        }
        default:
        {
            return "invalid";
        }
    }
}

rai::AccountSwapInfo::AccountSwapInfo()
    : active_orders_(0),
      total_orders_(0),
      active_swaps_(0),
      total_swaps_(0),
      trusted_(0),
      blocked_(0)
{
}

void rai::AccountSwapInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, active_orders_);
    rai::Write(stream, total_orders_);
    rai::Write(stream, active_swaps_);
    rai::Write(stream, total_swaps_);
    rai::Write(stream, trusted_);
    rai::Write(stream, blocked_);
}

bool rai::AccountSwapInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, active_orders_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, total_orders_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, active_swaps_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, total_swaps_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, trusted_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, blocked_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::InquiryWaiting::InquiryWaiting()
    : main_(0),
      ack_height_(rai::Block::INVALID_HEIGHT),
      taker_(0),
      inquiry_height_(rai::Block::INVALID_HEIGHT)
{
}

rai::InquiryWaiting::InquiryWaiting(const rai::Account& main_account,
                                    uint64_t ack_height,
                                    const rai::Account& taker,
                                    uint64_t inquiry_height)
    : main_(main_account),
      ack_height_(ack_height),
      taker_(taker),
      inquiry_height_(inquiry_height)
{
}

void rai::InquiryWaiting::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, main_.bytes);
    rai::Write(stream, ack_height_);
    rai::Write(stream, taker_.bytes);
    rai::Write(stream, inquiry_height_);
}

bool rai::InquiryWaiting::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, main_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, ack_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, taker_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, inquiry_height_);
    IF_ERROR_RETURN(error, true);
    return false;
}


rai::TakeWaiting::TakeWaiting()
    : maker_(0),
      trade_height_(rai::Block::INVALID_HEIGHT),
      taker_(0),
      inquiry_height_(rai::Block::INVALID_HEIGHT)
{
}

rai::TakeWaiting::TakeWaiting(const rai::Account& maker,
                                    uint64_t trade_height,
                                    const rai::Account& taker,
                                    uint64_t inquiry_height)
    : maker_(maker),
      trade_height_(trade_height),
      taker_(taker),
      inquiry_height_(inquiry_height)
{
}

void rai::TakeWaiting::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, maker_.bytes);
    rai::Write(stream, trade_height_);
    rai::Write(stream, taker_.bytes);
    rai::Write(stream, inquiry_height_);
}

bool rai::TakeWaiting::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error = rai::Read(stream, maker_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, trade_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, taker_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, inquiry_height_);
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

bool rai::Ledger::AccountTokenInfoGet(const rai::Iterator& it,
                                      rai::Account& account, rai::Chain& chain,
                                      rai::TokenAddress& address,
                                      rai::AccountTokenInfo& info) const
{
    auto data = it.store_it_->first.Data();
    auto size = it.store_it_->first.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream_key(data, size);
    bool error = rai::Read(stream_key, account.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream_key, chain);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream_key, address.bytes);
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

rai::Iterator rai::Ledger::AccountTokenInfoLowerBound(
    rai::Transaction& transaction, const rai::Account& account) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        uint32_t chain = 0;
        rai::Write(stream, chain);
        rai::TokenAddress address(0);
        rai::Write(stream, address.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.account_token_info_, key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::AccountTokenInfoUpperBound(
    rai::Transaction& transaction, const rai::Account& account) const
{
    rai::Account next = account + 1;
    if (next.IsZero())
    {
        rai::StoreIterator store_it(nullptr);
        return rai::Iterator(std::move(store_it));
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, next.bytes);
        uint32_t chain = 0;
        rai::Write(stream, chain);
        rai::TokenAddress address(0);
        rai::Write(stream, address.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.account_token_info_, key);
    return rai::Iterator(std::move(store_it));
}

bool rai::Ledger::AccountTokenLinkPut(rai::Transaction& transaction,
                                      const rai::AccountTokenLink& link,
                                      uint64_t previous)
{
    return AccountTokenLinkPut(transaction, link, previous,
                               rai::Block::INVALID_HEIGHT);
}

bool rai::Ledger::AccountTokenLinkPut(rai::Transaction& transaction,
                                      const rai::AccountTokenLink& link,
                                      uint64_t previous, uint64_t successor)
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
        rai::Write(stream, successor);
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
    uint64_t ignore;
    return AccountTokenLinkGet(transaction, link, previous, ignore);
}

bool rai::Ledger::AccountTokenLinkGet(rai::Transaction& transaction,
                                      const rai::AccountTokenLink& link,
                                      uint64_t& previous,
                                      uint64_t& successor) const
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
    error = rai::Read(stream, previous);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, successor);
    IF_ERROR_RETURN(error, true);
    return false;
}

bool rai::Ledger::AccountTokenLinkSuccessorSet(
    rai::Transaction& transaction, const rai::AccountTokenLink& link,
    uint64_t successor)
{
    uint64_t previous;
    bool error = AccountTokenLinkGet(transaction, link, previous);
    IF_ERROR_RETURN(error, true);

    error = AccountTokenLinkPut(transaction, link, previous, successor);
    IF_ERROR_RETURN(error, true);
    return false;
}

bool rai::Ledger::AccountTokenLinkSuccessorGet(
    rai::Transaction& transaction, const rai::AccountTokenLink& link,
    uint64_t& successor) const
{
    uint64_t ignore;
    return AccountTokenLinkGet(transaction, link, ignore, successor);
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

bool rai::Ledger::AccountTokenIdPut(rai::Transaction& transaction,
                                    const rai::AccountTokenId& token_id,
                                    uint64_t receive_at)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        token_id.Serialize(stream);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        rai::Write(stream, receive_at);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.account_token_id_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::AccountTokenIdGet(rai::Transaction& transaction,
                                    const rai::AccountTokenId& token_id,
                                    uint64_t& receive_at) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        token_id.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.account_token_id_, key, value);
    IF_ERROR_RETURN(error, true);
    rai::BufferStream stream(value.Data(), value.Size());
    return rai::Read(stream, receive_at);
}

bool rai::Ledger::AccountTokenIdGet(rai::Iterator& it,
                                    rai::AccountTokenId& id,
                                    uint64_t& receive_at) const
{
    auto data = it.store_it_->first.Data();
    auto size = it.store_it_->first.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream_key(data, size);
    bool error = id.Deserialize(stream_key);
    IF_ERROR_RETURN(error, true);

    data = it.store_it_->second.Data();
    size = it.store_it_->second.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream_value(data, size);
    error = rai::Read(stream_value, receive_at);
    IF_ERROR_RETURN(error, true);

    return false;
}

bool rai::Ledger::AccountTokenIdDel(rai::Transaction& transaction,
                                    const rai::AccountTokenId& token_id)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        token_id.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.account_token_id_,
                      key, nullptr);
}

bool rai::Ledger::AccountTokenIdExist(rai::Transaction& transaction,
                                      const rai::AccountTokenId& token_id) const
{
    uint64_t receive_at;
    return !AccountTokenIdGet(transaction, token_id, receive_at);
}

rai::Iterator rai::Ledger::AccountTokenIdLowerBound(
    rai::Transaction& transaction, const rai::Account& account,
    const rai::TokenKey& key) const
{
    return AccountTokenIdLowerBound(transaction,
                                    rai::AccountTokenId(account, key, 0));
}

rai::Iterator rai::Ledger::AccountTokenIdLowerBound(
    rai::Transaction& transaction, const rai::AccountTokenId& id) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        id.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.account_token_id_, key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::AccountTokenIdUpperBound(
    rai::Transaction& transaction, const rai::Account& account,
    const rai::TokenKey& key) const
{
    rai::Account account_l(account);
    rai::TokenKey key_l(key);
    uint32_t next_chain = static_cast<uint32_t>(key_l.chain_) + 1;
    key_l.chain_ = static_cast<rai::Chain>(next_chain);
    if (next_chain == 0)
    {
        key_l.address_ += 1;
        if (key_l.address_.IsZero())
        {
            account_l += 1;
            if (account_l.IsZero())
            {
                return rai::Iterator(rai::StoreIterator(nullptr));
            }
        }
    }
    return AccountTokenIdLowerBound(transaction, account_l, key_l);
}

bool rai::Ledger::TokenBlockPut(rai::Transaction& transaction,
                                const rai::Account& account, uint64_t height,
                                const rai::TokenBlock& block)
{
    return TokenBlockPut(transaction, account, height, block,
                         rai::Block::INVALID_HEIGHT);
}

bool rai::Ledger::TokenBlockPut(rai::Transaction& transaction,
                                const rai::Account& account, uint64_t height,
                                const rai::TokenBlock& block,
                                uint64_t successor)
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
        rai::Write(stream, successor);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_, store_.token_block_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::TokenBlockGet(rai::Transaction& transaction,
                                const rai::Account& account, uint64_t height,
                                rai::TokenBlock& block) const
{
    uint64_t ignore;
    return TokenBlockGet(transaction, account, height, block, ignore);
}

bool rai::Ledger::TokenBlockGet(rai::Transaction& transaction,
                                const rai::Account& account, uint64_t height,
                                rai::TokenBlock& block,
                                uint64_t& successor) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.token_block_, key, value);
    IF_ERROR_RETURN(error, true);
    rai::BufferStream stream(value.Data(), value.Size());
    error = block.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, successor);
    IF_ERROR_RETURN(error, true);

    return false;
}

bool rai::Ledger::TokenBlockSuccessorSet(rai::Transaction& transaction,
                                         const rai::Account& account,
                                         uint64_t height,
                                         uint64_t successor)
{
    rai::TokenBlock block;
    bool error = TokenBlockGet(transaction, account, height, block);
    IF_ERROR_RETURN(error, true);
    return TokenBlockPut(transaction, account, height, block, successor);
}

bool rai::Ledger::TokenBlockSuccessorGet(rai::Transaction& transaction,
                                         const rai::Account& account,
                                         uint64_t height,
                                         uint64_t& successor) const
{
    rai::TokenBlock block;
    return TokenBlockGet(transaction, account, height, block, successor);
}

bool rai::Ledger::TokenInfoPut(rai::Transaction& transaction,
                               const rai::TokenKey& token,
                               const rai::TokenInfo& info)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        token.Serialize(stream);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        info.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_, store_.token_info_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::TokenInfoGet(rai::Transaction& transaction,
                               const rai::TokenKey& token,
                               rai::TokenInfo& info) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        token.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.token_info_, key, value);
    IF_ERROR_RETURN(error, true);
    rai::BufferStream stream(value.Data(), value.Size());
    return info.Deserialize(stream);
}

bool rai::Ledger::TokenReceivablePut(
    rai::Transaction& transaction,
    const rai::TokenReceivableKey& receivable_key,
    const rai::TokenReceivable& receivable)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        receivable_key.Serialize(stream);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        receivable.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.token_receivable_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::TokenReceivableGet(
    rai::Transaction& transaction,
    const rai::TokenReceivableKey& receivable_key,
    rai::TokenReceivable& receivable) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        receivable_key.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.token_receivable_, key, value);
    IF_ERROR_RETURN(error, true);
    rai::BufferStream stream(value.Data(), value.Size());
    return receivable.Deserialize(stream);
}

bool rai::Ledger::TokenReceivableGet(const rai::Iterator& it,
                        rai::TokenReceivableKey& receivable_key,
                        rai::TokenReceivable& receivable) const
{
    auto data = it.store_it_->first.Data();
    auto size = it.store_it_->first.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream_key(data, size);
    bool error = receivable_key.Deserialize(stream_key);
    IF_ERROR_RETURN(error, true);

    data = it.store_it_->second.Data();
    size = it.store_it_->second.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream_value(data, size);
    error = receivable.Deserialize(stream_value);
    IF_ERROR_RETURN(error, true);

    return false;
}

bool rai::Ledger::TokenReceivableDel(
    rai::Transaction& transaction,
    const rai::TokenReceivableKey& receivable_key)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        receivable_key.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.token_receivable_,
                      key, nullptr);
}

rai::Iterator rai::Ledger::TokenReceivableLowerBound(
    rai::Transaction& transaction, const rai::Account& account) const
{
    rai::TokenReceivableKey receivable_key(
        account, rai::TokenKey(rai::Chain::INVALID, rai::TokenAddress(0)),
        rai::Chain::INVALID, rai::BlockHash(0));
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        receivable_key.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.token_receivable_, key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::TokenReceivableUpperBound(
    rai::Transaction& transaction, const rai::Account& account) const
{
    rai::Account account_next = account + 1;
    if (account_next.IsZero())
    {
        return rai::Iterator(rai::StoreIterator(nullptr));
    }

    return TokenReceivableLowerBound(transaction, account_next);
}

rai::Iterator rai::Ledger::TokenReceivableLowerBound(
    rai::Transaction& transaction, const rai::Account& account,
    const rai::TokenKey& token) const
{
    rai::TokenReceivableKey receivable_key(account, token, rai::Chain::INVALID,
                                           rai::BlockHash(0));
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        receivable_key.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.token_receivable_, key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::TokenReceivableUpperBound(
    rai::Transaction& transaction, const rai::Account& account,
    const rai::TokenKey& token) const
{
    rai::Account account_l(account);
    rai::TokenKey token_l(token);

    do
    {
        uint32_t next_chain = static_cast<uint32_t>(token_l.chain_) + 1;
        token_l.chain_ = static_cast<rai::Chain>(next_chain);
        if (next_chain != 0) break;

        token_l.address_ += 1;
        if (!token_l.address_.IsZero()) break;

        account_l += 1;
        if (!account_l.IsZero()) break;

        return rai::Iterator(rai::StoreIterator(nullptr));
    } while (0);

    return TokenReceivableLowerBound(transaction, account_l, token_l);
}

bool rai::Ledger::TokenIdInfoPut(rai::Transaction& transaction,
                                 const rai::TokenKey& token,
                                 const rai::TokenValue& id,
                                 const rai::TokenIdInfo& info)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        token.Serialize(stream);
        rai::Write(stream, (~id).bytes);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        info.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.token_id_info_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::TokenIdInfoGet(rai::Transaction& transaction,
                                 const rai::TokenKey& token,
                                 const rai::TokenValue& id,
                                 rai::TokenIdInfo& info)
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        token.Serialize(stream);
        rai::Write(stream, (~id).bytes);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_, store_.token_id_info_,
                            key, value);
    IF_ERROR_RETURN(error, true);
    rai::BufferStream stream(value.Data(), value.Size());
    return info.Deserialize(stream);
}

bool rai::Ledger::MaxTokenIdGet(rai::Transaction& transaction,
                                const rai::TokenKey& token,
                                rai::TokenValue& id) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        token.Serialize(stream);
        rai::Write(stream, rai::TokenValue(0).bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.token_id_info_, key);
    auto data = store_it->first.Data();
    auto size = store_it->first.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }

    rai::TokenKey token_found;
    rai::BufferStream stream(data, size);
    bool error = token_found.Deserialize(stream);
    IF_ERROR_RETURN(error, true);
    if (token_found.chain_ != token.chain_
        || token_found.address_ != token.address_)
    {
        return true;
    }
    error = rai::Read(stream, id.bytes);
    IF_ERROR_RETURN(error, true);
    id = ~id;
    return false;
}

bool rai::Ledger::TokenHolderPut(rai::Transaction& transaction,
                                 const rai::TokenKey& token,
                                 const rai::Account& account,
                                 const rai::TokenValue& balance)
{
    if (!transaction.write_)
    {
        return true;
    }

    rai::TokenHolder holder(token.chain_, token.address_, balance, account);
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        holder.Serialize(stream);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        rai::Write(stream, balance.bytes);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.token_holders_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::TokenHolderExist(rai::Transaction& transaction,
                                   const rai::TokenKey& token,
                                   const rai::Account& account,
                                   const rai::TokenValue& balance) const
{
    rai::TokenHolder holder(token.chain_, token.address_, balance, account);
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        holder.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_, store_.token_holders_,
                            key, value);
    IF_ERROR_RETURN(error, false);
    rai::BufferStream stream(value.Data(), value.Size());
    rai::TokenValue balance_l;
    error = rai::Read(stream, balance_l.bytes);
    return !error;
}

bool rai::Ledger::TokenHolderDel(rai::Transaction& transaction,
                                 const rai::TokenKey& token,
                                 const rai::Account& account,
                                 const rai::TokenValue& balance)
{
    if (!transaction.write_)
    {
        return true;
    }

    rai::TokenHolder holder(token.chain_, token.address_, balance, account);
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        holder.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.token_holders_, key,
                      nullptr);
}

bool rai::Ledger::TokenTransferPut(rai::Transaction& transaction,
                                   const rai::TokenTransfer& transfer)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        transfer.Serialize(stream);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        rai::Write(stream, ~transfer.ts_comp_);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.token_transfer_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::TokenIdTransferPut(
    rai::Transaction& transaction, const rai::TokenIdTransferKey& transfer_key,
    const rai::Account& account, uint64_t height)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        transfer_key.Serialize(stream);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.token_id_transfer_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::SwapMainAccountPut(rai::Transaction& transaction,
                                     const rai::Account& account,
                                     const rai::Account& main_account)
{
    if (!transaction.write_)
    {
        return true;
    }
    rai::MdbVal key(account);
    rai::MdbVal value(main_account);
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.swap_main_account_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::SwapMainAccountGet(rai::Transaction& transaction,
                                     const rai::Account& account,
                                     rai::Account& main_account) const
{
    rai::MdbVal key(account);
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.swap_main_account_, key, value);
    IF_ERROR_RETURN(error, true);
    rai::BufferStream stream(value.Data(), value.Size());
    return rai::Read(stream, main_account.bytes);
}

bool rai::Ledger::AccountSwapInfoPut(rai::Transaction& transaction,
                                     const rai::Account& account,
                                     const rai::AccountSwapInfo& info)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        info.Serialize(stream);
    }

    rai::MdbVal key(account);
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.account_swap_info_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::AccountSwapInfoGet(rai::Transaction& transaction,
                                     const rai::Account& account,
                                     rai::AccountSwapInfo& info) const
{
    rai::MdbVal key(account);
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.account_swap_info_, key, value);
    IF_ERROR_RETURN(error, true);
    rai::BufferStream stream(value.Data(), value.Size());
    return info.Deserialize(stream);
}

bool rai::Ledger::OrderInfoPut(rai::Transaction& transaction,
                               const rai::Account& account, uint64_t height,
                               const rai::OrderInfo& info)
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
        info.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.order_info_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::OrderInfoGet(rai::Transaction& transaction,
                               const rai::Account& account, uint64_t height,
                               rai::OrderInfo& info) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.order_info_, key, value);
    IF_ERROR_RETURN(error, true);
    rai::BufferStream stream(value.Data(), value.Size());
    return info.Deserialize(stream);
}

bool rai::Ledger::OrderIndexPut(rai::Transaction& transaction,
                                const rai::OrderIndex& index)
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
    uint8_t junk = 0;
    rai::MdbVal value(sizeof(junk), &junk);
    bool error = store_.Put(transaction.mdb_transaction_, store_.order_index_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::OrderIndexDel(rai::Transaction& transaction,
                                const rai::OrderIndex& index)
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
    return store_.Del(transaction.mdb_transaction_, store_.order_index_, key,
                      nullptr);
}

bool rai::Ledger::OrderIndexGet(const rai::Iterator& it,
                                rai::OrderIndex& index) const
{
    auto data = it.store_it_->first.Data();
    auto size = it.store_it_->first.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream_key(data, size);
    bool error = index.Deserialize(stream_key);
    IF_ERROR_RETURN(error, true);

    return false;
}

rai::Iterator rai::Ledger::OrderIndexLowerBound(
    rai::Transaction& transaction, const rai::TokenKey& token_offer,
    rai::TokenType type_offer, const rai::TokenKey& token_want,
    rai::TokenType type_want) const
{
    return OrderIndexLowerBound(transaction, token_offer, type_offer,
                                token_want, type_want, 0);
}

rai::Iterator rai::Ledger::OrderIndexUpperBound(
    rai::Transaction& transaction, const rai::TokenKey& token_offer,
    rai::TokenType type_offer, const rai::TokenKey& token_want,
    rai::TokenType type_want) const
{
    uint8_t type = static_cast<uint8_t>(type_want) + 1;
    return OrderIndexLowerBound(transaction, token_offer, type_offer,
                                token_want, static_cast<rai::TokenType>(type));
}

rai::Iterator rai::Ledger::OrderIndexLowerBound(
    rai::Transaction& transaction, const rai::TokenKey& token_offer,
    rai::TokenType type_offer, const rai::TokenKey& token_want,
    rai::TokenType type_want, const rai::TokenValue& id_offer) const
{
    rai::OrderIndex index(token_offer, type_offer, token_want, type_want,
                          id_offer);
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        index.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.order_index_, key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::OrderIndexUpperBound(
    rai::Transaction& transaction, const rai::TokenKey& token_offer,
    rai::TokenType type_offer, const rai::TokenKey& token_want,
    rai::TokenType type_want, const rai::TokenValue& id_offer) const
{
    rai::TokenValue id(id_offer);
    id += 1;
    if (!id.IsZero())
    {
        return OrderIndexLowerBound(transaction, token_offer, type_offer,
                                    token_want, type_want, id);
    }

    return OrderIndexUpperBound(transaction, token_offer, type_offer,
                                token_want, type_want);
}

bool rai::Ledger::OrderCount(rai::Transaction& transaction, size_t& count) const
{
    MDB_stat stat;
    auto ret =
        mdb_stat(transaction.mdb_transaction_, store_.order_info_, &stat);
    if (ret != MDB_SUCCESS)
    {
        return true;
    }

    count = stat.ms_entries;
    return false;
}

bool rai::Ledger::SwapInfoPut(rai::Transaction& transaction,
                              const rai::Account& account, uint64_t height,
                              const rai::SwapInfo& info)
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
        info.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.swap_info_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::SwapInfoGet(rai::Transaction& transaction,
                              const rai::Account& account, uint64_t height,
                              rai::SwapInfo& info) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.swap_info_, key, value);
    IF_ERROR_RETURN(error, true);
    rai::BufferStream stream(value.Data(), value.Size());
    return info.Deserialize(stream);
}

bool rai::Ledger::InquiryWaitingPut(rai::Transaction& transaction,
                                    const rai::InquiryWaiting& waiting)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        waiting.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    uint8_t junk = 0;
    rai::MdbVal value(sizeof(junk), &junk);
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.inquiry_waiting_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::InquiryWaitingGet(const rai::Iterator& it,
                                    rai::InquiryWaiting& waiting) const
{
    auto data = it.store_it_->first.Data();
    auto size = it.store_it_->first.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }

    rai::BufferStream stream(data, size);
    return waiting.Deserialize(stream);
}

bool rai::Ledger::InquiryWaitingDel(rai::Transaction& transaction,
                                    const rai::InquiryWaiting& waiting)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        waiting.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.inquiry_waiting_,
                      key, nullptr);
}

bool rai::Ledger::InquiryWaitingExist(rai::Transaction& transaction,
                                      const rai::InquiryWaiting& waiting) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        waiting.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_,
                            store_.inquiry_waiting_, key, value);
    IF_ERROR_RETURN(error, false);
    rai::BufferStream stream(value.Data(), value.Size());
    uint8_t junk;
    error = rai::Read(stream, junk);
    return !error;
}

bool rai::Ledger::InquiryWaitingExist(rai::Transaction& transaction,
                                      const rai::Account& main_account,
                                      uint64_t height) const
{
    rai::Iterator i =
        InquiryWaitingLowerBound(transaction, main_account, height);
    rai::Iterator n =
        InquiryWaitingUpperBound(transaction, main_account, height);
    return i != n;
}

rai::Iterator rai::Ledger::InquiryWaitingLowerBound(
    rai::Transaction& transaction, const rai::Account& main_account,
    uint64_t ack_height) const
{
    rai::InquiryWaiting waiting(main_account, ack_height, rai::Account(0), 0);
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        waiting.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.inquiry_waiting_, key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::InquiryWaitingUpperBound(
    rai::Transaction& transaction, const rai::Account& main_account,
    uint64_t ack_height) const
{
    rai::InquiryWaiting waiting(main_account, ack_height, rai::Account::Max(),
                                std::numeric_limits<uint64_t>::max());
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        waiting.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.inquiry_waiting_, key);
    return rai::Iterator(std::move(store_it));
}

bool rai::Ledger::TakeWaitingPut(rai::Transaction& transaction,
                                 const rai::TakeWaiting& waiting)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        waiting.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    uint8_t junk = 0;
    rai::MdbVal value(sizeof(junk), &junk);
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.take_waiting_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::TakeWaitingGet(const rai::Iterator& it,
                                 rai::TakeWaiting& waiting) const
{
    auto data = it.store_it_->first.Data();
    auto size = it.store_it_->first.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }

    rai::BufferStream stream(data, size);
    return waiting.Deserialize(stream);
}

bool rai::Ledger::TakeWaitingDel(rai::Transaction& transaction,
                                 const rai::TakeWaiting& waiting)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        waiting.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.take_waiting_, key,
                      nullptr);
}

bool rai::Ledger::TakeWaitingExist(rai::Transaction& transaction,
                                   const rai::TakeWaiting& waiting) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        waiting.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_, store_.take_waiting_,
                            key, value);
    IF_ERROR_RETURN(error, false);
    rai::BufferStream stream(value.Data(), value.Size());
    uint8_t junk;
    error = rai::Read(stream, junk);
    return !error;
}

bool rai::Ledger::TakeWaitingExist(rai::Transaction& transaction,
                                   const rai::Account& maker,
                                   uint64_t height) const
{
    rai::Iterator i = TakeWaitingLowerBound(transaction, maker, height);
    rai::Iterator n = TakeWaitingUpperBound(transaction, maker, height);
    return i != n;
}

rai::Iterator rai::Ledger::TakeWaitingLowerBound(rai::Transaction& transaction,
                                                 const rai::Account& maker,
                                                 uint64_t trade_height) const
{
    rai::TakeWaiting waiting(maker, trade_height, rai::Account(0), 0);
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        waiting.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.take_waiting_, key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::TakeWaitingUpperBound(rai::Transaction& transaction,
                                                 const rai::Account& maker,
                                                 uint64_t trade_height) const
{
    rai::TakeWaiting waiting(maker, trade_height, rai::Account::Max(),
                             std::numeric_limits<uint64_t>::max());
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        waiting.Serialize(stream);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_,
                                store_.take_waiting_, key);
    return rai::Iterator(std::move(store_it));
}

bool rai::Ledger::OrderSwapIndexPut(rai::Transaction& transaction,
                                    const rai::OrderSwapIndex& index)
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
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        rai::Write(stream, ~index.ts_comp_);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.order_swap_index_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::TokenSwapIndexPut(rai::Transaction& transaction,
                                    const rai::TokenSwapIndex& index)
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
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        rai::Write(stream, ~index.ts_comp_);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_,
                            store_.token_swap_index_, key, value);
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

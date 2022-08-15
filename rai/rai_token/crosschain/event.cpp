#include <rai/rai_token/crosschain/event.hpp>

namespace
{
template <typename T>
bool CrossChainEventEqual(const T& first, const rai::CrossChainEvent& second)
{
    static_assert(std::is_base_of<rai::CrossChainEvent, T>::value,
                  "Input parameter is not a cross chain event type");
    return (first.Type() == second.Type())
           && ((static_cast<const T&>(second)) == first);
}
}  // namespace

std::string rai::CrossChainEventTypeToString(rai::CrossChainEventType type)
{
    switch (type)
    {
    case rai::CrossChainEventType::INVALID:
    {
        return "invalid";
    }
    case rai::CrossChainEventType::TOKEN:
    {
        return "token";
    }
    case rai::CrossChainEventType::CREATE:
    {
        return "create";
    }
    case rai::CrossChainEventType::MAP:
    {
        return "map";
    }
    case rai::CrossChainEventType::UNMAP:
    {
        return "unmap";
    }
    case rai::CrossChainEventType::WRAP:
    {
        return "wrap";
    }
    case rai::CrossChainEventType::UNWRAP:
    {
        return "unwrap";
    }
    default:
        return "unknown";
    }
}

bool rai::CrossChainEvent::operator!=(const rai::CrossChainEvent& other) const
{
    return !(*this == other);
}

rai::CrossChainTokenEvent::CrossChainTokenEvent(
    uint64_t height, uint64_t index, rai::Chain chain,
    const rai::TokenAddress& address, rai::TokenType token_type,
    uint8_t decimals, bool wrapped, const rai::BlockHash& hash)
    : type_(rai::CrossChainEventType::TOKEN),
      block_height_(height),
      index_(index),
      chain_(chain),
      address_(address),
      token_type_(token_type),
      decimals_(decimals),
      wrapped_(wrapped),
      tx_hash_(hash)
{
}

rai::CrossChainEventType rai::CrossChainTokenEvent::Type() const
{
    return type_;
}

bool rai::CrossChainTokenEvent::operator==(
    const rai::CrossChainEvent& other) const
{
    return CrossChainEventEqual(*this, other);
}

bool rai::CrossChainTokenEvent::operator==(
    const rai::CrossChainTokenEvent& other) const
{
    return (block_height_ == other.block_height_) && (index_ == other.index_)
           && (chain_ == other.chain_) && (address_ == other.address_)
           && (token_type_ == other.token_type_)
           && (decimals_ == other.decimals_) && (wrapped_ == other.wrapped_)
           && (tx_hash_ == other.tx_hash_);
}

uint64_t rai::CrossChainTokenEvent::BlockHeight() const
{
    return block_height_;
}

uint64_t rai::CrossChainTokenEvent::Index() const
{
    return index_;
}

rai::Chain rai::CrossChainTokenEvent::Chain() const
{
    return chain_;
}

void rai::CrossChainTokenEvent::Ptree(rai::Ptree& ptree) const
{
    ptree.put("type", rai::CrossChainEventTypeToString(type_));
    ptree.put("block_height", std::to_string(block_height_));
    ptree.put("index", std::to_string(index_));
    ptree.put("chain", rai::ChainToString(chain_));
    ptree.put("chain_id", std::to_string(static_cast<uint32_t>(chain_)));
    ptree.put("address", address_.StringHex());
    ptree.put("token_type", rai::TokenTypeToString(token_type_));
    ptree.put("decimals", std::to_string(decimals_));
    ptree.put("wrapped", rai::BoolToString(wrapped_));
    ptree.put("txn_hash", tx_hash_.StringHex());
}

rai::CrossChainCreateEvent::CrossChainCreateEvent(
    uint64_t height, uint64_t index, rai::Chain chain,
    const rai::TokenAddress& address, rai::TokenType token_type,
    rai::Chain original_chain, const rai::TokenAddress& original_address,
    const rai::BlockHash& hash)
    : type_(rai::CrossChainEventType::CREATE),
      block_height_(height),
      index_(index),
      chain_(chain),
      address_(address),
      token_type_(token_type),
      original_chain_(original_chain),
      original_address_(original_address),
      tx_hash_(hash)
{
}

rai::CrossChainEventType rai::CrossChainCreateEvent::Type() const
{
    return type_;
}

bool rai::CrossChainCreateEvent::operator==(
    const rai::CrossChainEvent& other) const
{
    return CrossChainEventEqual(*this, other);
}

bool rai::CrossChainCreateEvent::operator==(
    const rai::CrossChainCreateEvent& other) const
{
    return (block_height_ == other.block_height_) && (index_ == other.index_)
           && (chain_ == other.chain_) && (address_ == other.address_)
           && (token_type_ == other.token_type_)
           && (original_chain_ == other.original_chain_)
           && (original_address_ == other.original_address_)
           && (tx_hash_ == other.tx_hash_);
}

uint64_t rai::CrossChainCreateEvent::BlockHeight() const
{
    return block_height_;
}

uint64_t rai::CrossChainCreateEvent::Index() const
{
    return index_;
}

rai::Chain rai::CrossChainCreateEvent::Chain() const
{
    return chain_;
}

void rai::CrossChainCreateEvent::Ptree(rai::Ptree& ptree) const
{
    ptree.put("type", rai::CrossChainEventTypeToString(type_));
    ptree.put("block_height", std::to_string(block_height_));
    ptree.put("index", std::to_string(index_));
    ptree.put("chain", rai::ChainToString(chain_));
    ptree.put("chain_id", std::to_string(static_cast<uint32_t>(chain_)));
    ptree.put("address", address_.StringHex());
    ptree.put("token_type", rai::TokenTypeToString(token_type_));
    ptree.put("original_chain", rai::ChainToString(original_chain_));
    ptree.put("original_chain_id",
              std::to_string(static_cast<uint32_t>(original_chain_)));
    ptree.put("original_address", original_address_.StringHex());
    ptree.put("txn_hash", tx_hash_.StringHex());
}

rai::CrossChainMapEvent::CrossChainMapEvent(
    uint64_t height, uint64_t index, rai::Chain chain,
    const rai::TokenAddress& address, rai::TokenType token_type,
    const rai::Account& from, const rai::Account& to,
    const rai::TokenValue& value, const rai::BlockHash& hash)
    : type_(rai::CrossChainEventType::MAP),
      block_height_(height),
      index_(index),
      chain_(chain),
      address_(address),
      token_type_(token_type),
      from_(from),
      to_(to),
      value_(value),
      tx_hash_(hash)
{
}

rai::CrossChainEventType rai::CrossChainMapEvent::Type() const
{
    return type_;
}

bool rai::CrossChainMapEvent::operator==(
    const rai::CrossChainEvent& other) const
{
    return CrossChainEventEqual(*this, other);
}

bool rai::CrossChainMapEvent::operator==(
    const rai::CrossChainMapEvent& other) const
{
    return (block_height_ == other.block_height_) && (index_ == other.index_)
           && (chain_ == other.chain_) && (address_ == other.address_)
           && (token_type_ == other.token_type_) && (from_ == other.from_)
           && (to_ == other.to_) && (value_ == other.value_)
           && (tx_hash_ == other.tx_hash_);
}

uint64_t rai::CrossChainMapEvent::BlockHeight() const
{
    return block_height_;
}

uint64_t rai::CrossChainMapEvent::Index() const
{
    return index_;
}

rai::Chain rai::CrossChainMapEvent::Chain() const
{
    return chain_;
}

void rai::CrossChainMapEvent::Ptree(rai::Ptree& ptree) const
{
    ptree.put("type", rai::CrossChainEventTypeToString(type_));
    ptree.put("block_height", std::to_string(block_height_));
    ptree.put("index", std::to_string(index_));
    ptree.put("chain", rai::ChainToString(chain_));
    ptree.put("chain_id", std::to_string(static_cast<uint32_t>(chain_)));
    ptree.put("address", address_.StringHex());
    ptree.put("token_type", rai::TokenTypeToString(token_type_));
    ptree.put("from", from_.StringHex());
    ptree.put("to", to_.StringAccount());
    ptree.put("value", value_.StringDec());
    ptree.put("txn_hash", tx_hash_.StringHex());
}

rai::CrossChainUnmapEvent::CrossChainUnmapEvent(
    uint64_t height, uint64_t index, rai::Chain chain,
    const rai::TokenAddress& address, rai::TokenType token_type,
    const rai::Account& from, uint64_t from_height,
    const rai::BlockHash& from_tx_hash, const rai::Account& to,
    const rai::TokenValue& value, const rai::BlockHash& hash)
    : type_(rai::CrossChainEventType::UNMAP),
      block_height_(height),
      index_(index),
      chain_(chain),
      address_(address),
      token_type_(token_type),
      from_(from),
      from_height_(from_height),
      from_tx_hash_(from_tx_hash),
      to_(to),
      value_(value),
      tx_hash_(hash)
{
}

rai::CrossChainEventType rai::CrossChainUnmapEvent::Type() const
{
    return type_;
}

bool rai::CrossChainUnmapEvent::operator==(
    const rai::CrossChainEvent& other) const
{
    return CrossChainEventEqual(*this, other);
}

bool rai::CrossChainUnmapEvent::operator==(
    const rai::CrossChainUnmapEvent& other) const
{
    return (block_height_ == other.block_height_) && (index_ == other.index_)
           && (chain_ == other.chain_) && (address_ == other.address_)
           && (token_type_ == other.token_type_) && (from_ == other.from_)
           && (from_height_ == other.from_height_)
           && (from_tx_hash_ == other.from_tx_hash_) && (to_ == other.to_)
           && (value_ == other.value_) && (tx_hash_ == other.tx_hash_);
}

uint64_t rai::CrossChainUnmapEvent::BlockHeight() const
{
    return block_height_;
}

uint64_t rai::CrossChainUnmapEvent::Index() const
{
    return index_;
}

rai::Chain rai::CrossChainUnmapEvent::Chain() const
{
    return chain_;
}

void rai::CrossChainUnmapEvent::Ptree(rai::Ptree& ptree) const
{
    ptree.put("type", rai::CrossChainEventTypeToString(type_));
    ptree.put("block_height", std::to_string(block_height_));
    ptree.put("index", std::to_string(index_));
    ptree.put("chain", rai::ChainToString(chain_));
    ptree.put("chain_id", std::to_string(static_cast<uint32_t>(chain_)));
    ptree.put("address", address_.StringHex());
    ptree.put("token_type", rai::TokenTypeToString(token_type_));
    ptree.put("from", from_.StringAccount());
    ptree.put("from_height", std::to_string(from_height_));
    ptree.put("from_txn_hash", from_tx_hash_.StringHex());
    ptree.put("to", to_.StringAccount());
    ptree.put("value", value_.StringDec());
    ptree.put("txn_hash", tx_hash_.StringHex());
}

rai::CrossChainWrapEvent::CrossChainWrapEvent(
    uint64_t height, uint64_t index, rai::Chain chain,
    const rai::TokenAddress& address, rai::TokenType token_type,
    rai::Chain original_chain, const rai::TokenAddress& original_address,
    const rai::Account& from, uint64_t from_height,
    const rai::BlockHash& from_tx_hash, const rai::Account& to,
    const rai::TokenValue& value, const rai::BlockHash& hash)
    : type_(rai::CrossChainEventType::WRAP),
      block_height_(height),
      index_(index),
      chain_(chain),
      address_(address),
      token_type_(token_type),
      original_chain_(original_chain),
      original_address_(original_address),
      from_(from),
      from_height_(from_height),
      from_tx_hash_(from_tx_hash),
      to_(to),
      value_(value),
      tx_hash_(hash)
{
}

rai::CrossChainEventType rai::CrossChainWrapEvent::Type() const
{
    return type_;
}

bool rai::CrossChainWrapEvent::operator==(
    const rai::CrossChainEvent& other) const
{
    return CrossChainEventEqual(*this, other);
}

bool rai::CrossChainWrapEvent::operator==(
    const rai::CrossChainWrapEvent& other) const
{
    return (block_height_ == other.block_height_) && (index_ == other.index_)
           && (chain_ == other.chain_) && (address_ == other.address_)
           && (token_type_ == other.token_type_)
           && (original_chain_ == other.original_chain_)
           && (original_address_ == other.original_address_)
           && (from_ == other.from_) && (from_height_ == other.from_height_)
           && (from_tx_hash_ == other.from_tx_hash_) && (to_ == other.to_)
           && (value_ == other.value_) && (tx_hash_ == other.tx_hash_);
}

uint64_t rai::CrossChainWrapEvent::BlockHeight() const
{
    return block_height_;
}

uint64_t rai::CrossChainWrapEvent::Index() const
{
    return index_;
}

rai::Chain rai::CrossChainWrapEvent::Chain() const
{
    return chain_;
}

void rai::CrossChainWrapEvent::Ptree(rai::Ptree& ptree) const
{
    ptree.put("type", rai::CrossChainEventTypeToString(type_));
    ptree.put("block_height", std::to_string(block_height_));
    ptree.put("index", std::to_string(index_));
    ptree.put("chain", rai::ChainToString(chain_));
    ptree.put("chain_id", std::to_string(static_cast<uint32_t>(chain_)));
    ptree.put("address", address_.StringHex());
    ptree.put("token_type", rai::TokenTypeToString(token_type_));
    ptree.put("original_chain", rai::ChainToString(original_chain_));
    ptree.put("original_chain_id",
              std::to_string(static_cast<uint32_t>(original_chain_)));
    ptree.put("original_address", original_address_.StringHex());
    ptree.put("from", from_.StringAccount());
    ptree.put("from_height", std::to_string(from_height_));
    ptree.put("from_txn_hash", from_tx_hash_.StringHex());
    ptree.put("to", to_.StringHex());
    ptree.put("value", value_.StringDec());
    ptree.put("txn_hash", tx_hash_.StringHex());
}

rai::CrossChainUnwrapEvent::CrossChainUnwrapEvent(
    uint64_t height, uint64_t index, rai::Chain chain,
    const rai::TokenAddress& address, rai::TokenType token_type,
    rai::Chain original_chain, const rai::TokenAddress& original_address,
    const rai::Account& from, const rai::Account& to,
    const rai::TokenValue& value, const rai::BlockHash& hash)
    : type_(rai::CrossChainEventType::UNWRAP),
      block_height_(height),
      index_(index),
      chain_(chain),
      address_(address),
      token_type_(token_type),
      original_chain_(original_chain),
      original_address_(original_address),
      from_(from),
      to_(to),
      value_(value),
      tx_hash_(hash)
{
}

rai::CrossChainEventType rai::CrossChainUnwrapEvent::Type() const
{
    return type_;
}

bool rai::CrossChainUnwrapEvent::operator==(
    const rai::CrossChainEvent& other) const
{
    return CrossChainEventEqual(*this, other);
}

bool rai::CrossChainUnwrapEvent::operator==(
    const rai::CrossChainUnwrapEvent& other) const
{
    return (block_height_ == other.block_height_) && (index_ == other.index_)
           && (chain_ == other.chain_) && (address_ == other.address_)
           && (token_type_ == other.token_type_)
           && (original_chain_ == other.original_chain_)
           && (original_address_ == other.original_address_)
           && (from_ == other.from_) && (to_ == other.to_)
           && (value_ == other.value_) && (tx_hash_ == other.tx_hash_);
}

uint64_t rai::CrossChainUnwrapEvent::BlockHeight() const
{
    return block_height_;
}

uint64_t rai::CrossChainUnwrapEvent::Index() const
{
    return index_;
}

rai::Chain rai::CrossChainUnwrapEvent::Chain() const
{
    return chain_;
}

void rai::CrossChainUnwrapEvent::Ptree(rai::Ptree& ptree) const
{
    ptree.put("type", rai::CrossChainEventTypeToString(type_));
    ptree.put("block_height", std::to_string(block_height_));
    ptree.put("index", std::to_string(index_));
    ptree.put("chain", rai::ChainToString(chain_));
    ptree.put("chain_id", std::to_string(static_cast<uint32_t>(chain_)));
    ptree.put("address", address_.StringHex());
    ptree.put("token_type", rai::TokenTypeToString(token_type_));
    ptree.put("original_chain", rai::ChainToString(original_chain_));
    ptree.put("original_chain_id",
              std::to_string(static_cast<uint32_t>(original_chain_)));
    ptree.put("original_address", original_address_.StringHex());
    ptree.put("from", from_.StringHex());
    ptree.put("to", to_.StringAccount());
    ptree.put("value", value_.StringDec());
    ptree.put("txn_hash", tx_hash_.StringHex());
}

rai::CrossChainBlockEvents::CrossChainBlockEvents(
    rai::Chain chain, uint64_t height,
    const std::vector<std::shared_ptr<rai::CrossChainEvent>>& events)
    : chain_(chain), block_height_(height), events_(events)
{
}

rai::CrossChainBlockEvents::CrossChainBlockEvents(
    rai::Chain chain, uint64_t height,
    std::vector<std::shared_ptr<rai::CrossChainEvent>>&& events)
    : chain_(chain), block_height_(height), events_(std::move(events))
{
}

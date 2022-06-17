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


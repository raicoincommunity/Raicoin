#pragma once

#include <rai/common/numbers.hpp>
#include <rai/common/chain.hpp>
#include <rai/common/token.hpp>

namespace rai
{

enum class CrossChainEventType
{
    INVALID  = 0,
    TOKEN    = 1, // sync token info
    CREATE   = 2, // create wrapped token
    MAP      = 3,
    UNMAP    = 4,
    WRAP     = 5,
    UNWRAP   = 6,
};

class CrossChainEvent
{
public:
    bool operator!=(const rai::CrossChainEvent&) const;

    virtual ~CrossChainEvent()                                  = default;
    virtual rai::CrossChainEventType Type() const               = 0;
    virtual bool operator==(const rai::CrossChainEvent&) const  = 0;
    virtual uint64_t BlockHeight() const              = 0;
    virtual uint64_t Index() const                              = 0;
};

class CrossChainTokenEvent : public CrossChainEvent
{
public:
    CrossChainTokenEvent(uint64_t, uint64_t, rai::Chain,
                         const rai::TokenAddress&, rai::TokenType, uint8_t,
                         bool, const rai::BlockHash&);
    virtual ~CrossChainTokenEvent() = default;
    rai::CrossChainEventType Type() const override;
    bool operator==(const rai::CrossChainEvent&) const override;
    bool operator==(const rai::CrossChainTokenEvent&) const;
    uint64_t BlockHeight() const override;
    uint64_t Index() const override;

private:
    rai::CrossChainEventType type_;
    uint64_t block_height_;
    uint64_t index_;
    rai::Chain chain_;
    rai::TokenAddress address_;
    rai::TokenType token_type_;
    uint8_t decimals_;
    bool wrapped_;
    rai::BlockHash tx_hash_;
};

class CrossChainCreateEvent : public CrossChainEvent
{
public:
    CrossChainCreateEvent(uint64_t, uint64_t, rai::Chain,
                          const rai::TokenAddress&, rai::TokenType, rai::Chain,
                          const rai::TokenAddress&, const rai::BlockHash&);
    virtual ~CrossChainCreateEvent() = default;
    rai::CrossChainEventType Type() const override;
    bool operator==(const rai::CrossChainEvent&) const override;
    bool operator==(const rai::CrossChainCreateEvent&) const;
    uint64_t BlockHeight() const override;
    uint64_t Index() const override;

private:
    rai::CrossChainEventType type_;
    uint64_t block_height_;
    uint64_t index_;
    rai::Chain chain_;
    rai::TokenAddress address_;
    rai::TokenType token_type_;
    rai::Chain original_chain_;
    rai::TokenAddress original_address_;
    rai::BlockHash tx_hash_;
};

class CrossChainMapEvent : public CrossChainEvent
{
public:
    CrossChainMapEvent(uint64_t, uint64_t, rai::Chain,
                        const rai::TokenAddress&, rai::TokenType, rai::Chain,
                        const rai::TokenAddress&);
    virtual ~CrossChainMapEvent() = default;
    rai::CrossChainEventType Type() const override;
    bool operator==(const rai::CrossChainEvent&) const override;
    bool operator==(const rai::CrossChainMapEvent&) const;
    uint64_t BlockHeight() const override;
    uint64_t Index() const override;

    rai::Chain OriginalChain() const;
    rai::TokenAddress OriginalAddress() const;

    rai::Chain TargetChain() const;
    rai::TokenAddress TargetAddress() const;

private:
    rai::CrossChainEventType type_;
    uint64_t index_;
    rai::Chain chain_;
    rai::TokenAddress address_;
    rai::TokenType token_type_;
    rai::Account from_;
    rai::Account to_;
    rai::TokenValue value_;
    rai::BlockHash tx_hash_;
    // todo:
};

class CrossChainUnmapEvent : public CrossChainEvent
{
public:
    CrossChainUnmapEvent(uint64_t, uint64_t, rai::Chain,
                          const rai::TokenAddress&, rai::TokenType, rai::Chain,
                          const rai::TokenAddress&);
    virtual ~CrossChainUnmapEvent() = default;
    rai::CrossChainEventType Type() const override;
    bool operator==(const rai::CrossChainEvent&) const override;
    bool operator==(const rai::CrossChainUnmapEvent&) const;
    uint64_t BlockHeight() const override;
    uint64_t Index() const override;

private:
    rai::CrossChainEventType type_;
    uint64_t index_;
    rai::Chain chain_;
    rai::TokenAddress address_;
    rai::TokenType token_type_;
    rai::Account from_;
    rai::Account to_;
    rai::TokenValue value_;
    rai::BlockHash tx_hash_;
};

} // namespace rai
#pragma once

#include <rai/common/numbers.hpp>
#include <rai/common/chain.hpp>
#include <rai/common/token.hpp>
#include <rai/common/util.hpp>

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
std::string CrossChainEventTypeToString(CrossChainEventType);

class CrossChainEvent
{
public:
    bool operator!=(const rai::CrossChainEvent&) const;

    virtual ~CrossChainEvent()                                  = default;
    virtual rai::CrossChainEventType Type() const               = 0;
    virtual bool operator==(const rai::CrossChainEvent&) const  = 0;
    virtual uint64_t BlockHeight() const                        = 0;
    virtual uint64_t Index() const                              = 0;
    virtual rai::Chain Chain() const                            = 0;
    virtual void Ptree(rai::Ptree&) const                       = 0;
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
    rai::Chain Chain() const override;
    void Ptree(rai::Ptree&) const override;

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
    rai::Chain Chain() const override;
    void Ptree(rai::Ptree&) const override;

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
    CrossChainMapEvent(uint64_t, uint64_t, rai::Chain, const rai::TokenAddress&,
                       rai::TokenType, const rai::Account&, const rai::Account&,
                       const rai::TokenValue&, const rai::BlockHash&);
    virtual ~CrossChainMapEvent() = default;
    rai::CrossChainEventType Type() const override;
    bool operator==(const rai::CrossChainEvent&) const override;
    bool operator==(const rai::CrossChainMapEvent&) const;
    uint64_t BlockHeight() const override;
    uint64_t Index() const override;
    rai::Chain Chain() const override;
    void Ptree(rai::Ptree&) const override;

    rai::CrossChainEventType type_;
    uint64_t block_height_;
    uint64_t index_;
    rai::Chain chain_;
    rai::TokenAddress address_;
    rai::TokenType token_type_;
    rai::Account from_;
    rai::Account to_;
    rai::TokenValue value_;
    rai::BlockHash tx_hash_;
};

class CrossChainUnmapEvent : public CrossChainEvent
{
public:
    CrossChainUnmapEvent(uint64_t, uint64_t, rai::Chain,
                         const rai::TokenAddress&, rai::TokenType,
                         const rai::Account&, uint64_t, const rai::BlockHash&,
                         const rai::Account&, const rai::TokenValue&,
                         const rai::BlockHash&);
    virtual ~CrossChainUnmapEvent() = default;
    rai::CrossChainEventType Type() const override;
    bool operator==(const rai::CrossChainEvent&) const override;
    bool operator==(const rai::CrossChainUnmapEvent&) const;
    uint64_t BlockHeight() const override;
    uint64_t Index() const override;
    rai::Chain Chain() const override;
    void Ptree(rai::Ptree&) const override;

    rai::CrossChainEventType type_;
    uint64_t block_height_;
    uint64_t index_;
    rai::Chain chain_;
    rai::TokenAddress address_;
    rai::TokenType token_type_;
    rai::Account from_;
    uint64_t from_height_;
    rai::BlockHash from_tx_hash_;
    rai::Account to_;
    rai::TokenValue value_;
    rai::BlockHash tx_hash_;
};

class CrossChainWrapEvent : public CrossChainEvent
{
public:
    CrossChainWrapEvent(uint64_t, uint64_t, rai::Chain,
                        const rai::TokenAddress&, rai::TokenType, rai::Chain,
                        const rai::TokenAddress&, const rai::Account&, uint64_t,
                        const rai::BlockHash&, const rai::Account&,
                        const rai::TokenValue&, const rai::BlockHash&);
    virtual ~CrossChainWrapEvent() = default;
    rai::CrossChainEventType Type() const override;
    bool operator==(const rai::CrossChainEvent&) const override;
    bool operator==(const rai::CrossChainWrapEvent&) const;
    uint64_t BlockHeight() const override;
    uint64_t Index() const override;
    rai::Chain Chain() const override;
    void Ptree(rai::Ptree&) const override;

    rai::CrossChainEventType type_;
    uint64_t block_height_;
    uint64_t index_;
    rai::Chain chain_;
    rai::TokenAddress address_;
    rai::TokenType token_type_;
    rai::Chain original_chain_;
    rai::TokenAddress original_address_;
    rai::Account from_;
    uint64_t from_height_;
    rai::BlockHash from_tx_hash_;
    rai::Account to_;
    rai::TokenValue value_;
    rai::BlockHash tx_hash_;

};

class CrossChainUnwrapEvent : public CrossChainEvent
{
public:
    CrossChainUnwrapEvent(uint64_t, uint64_t, rai::Chain,
                          const rai::TokenAddress&, rai::TokenType, rai::Chain,
                          const rai::TokenAddress&, const rai::Account&,
                          const rai::Account&, const rai::TokenValue&,
                          const rai::BlockHash&);
    virtual ~CrossChainUnwrapEvent() = default;
    rai::CrossChainEventType Type() const override;
    bool operator==(const rai::CrossChainEvent&) const override;
    bool operator==(const rai::CrossChainUnwrapEvent&) const;
    uint64_t BlockHeight() const override;
    uint64_t Index() const override;
    rai::Chain Chain() const override;
    void Ptree(rai::Ptree&) const override;

    rai::CrossChainEventType type_;
    uint64_t block_height_;
    uint64_t index_;
    rai::Chain chain_;
    rai::TokenAddress address_;
    rai::TokenType token_type_;
    rai::Chain original_chain_;
    rai::TokenAddress original_address_;
    rai::Account from_;
    rai::Account to_;
    rai::TokenValue value_;
    rai::BlockHash tx_hash_;
};

class CrossChainBlockEvents
{
public:
    CrossChainBlockEvents(
        rai::Chain, uint64_t,
        const std::vector<std::shared_ptr<rai::CrossChainEvent>>&);
    CrossChainBlockEvents(rai::Chain, uint64_t,
                          std::vector<std::shared_ptr<rai::CrossChainEvent>>&&);

    rai::Chain chain_;
    uint64_t block_height_;
    std::vector<std::shared_ptr<rai::CrossChainEvent>> events_;
};

} // namespace rai
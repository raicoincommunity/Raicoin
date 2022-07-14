#pragma once

#include <array>
#include <bitset>
#include <rai/common/parameters.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/blocks.hpp>
#include <rai/node/network.hpp>

namespace rai
{
uint8_t constexpr PROTOCOL_VERSION_MIN   = 1;
uint8_t constexpr PROTOCOL_VERSION_USING = 2;

// version 1
enum class MessageType : uint8_t
{
    INVALID     = 0,
    HANDSHAKE   = 1,
    KEEPLIVE    = 2,
    PUBLISH     = 3,
    CONFIRM     = 4,
    QUERY       = 5,
    FORK        = 6,
    CONFLICT    = 7,
    BOOTSTRAP   = 8,
    WEIGHT      = 9,
    CROSSCHAIN  = 10,

    MAX
};

enum class MessageFlags
{
    PROXY = 0,
    RELAY = 1,
    ACK   = 2,

    INVALID = 8
};

class MessageHeader
{
public:
    MessageHeader(rai::MessageType);
    MessageHeader(rai::MessageType, uint16_t);
    MessageHeader(rai::ErrorCode&, rai::Stream&);

    void Serialize(rai::Stream&) const;
    rai::ErrorCode Deserialize(rai::Stream&);
    void SetFlag(rai::MessageFlags flag);
    void ClearFlag(rai::MessageFlags flag);
    bool GetFlag(rai::MessageFlags flag) const;

    static std::array<uint8_t, 2> MagicNumber();
    static uint32_t FlagToIndex(rai::MessageFlags flag);

    uint8_t version_using_;
    uint8_t version_min_;
    rai::MessageType type_;
    std::bitset<8> flags_;
    uint16_t extension_;

    // for proxy message
    rai::Endpoint peer_endpoint_;
    uint16_t payload_length_;
};

class MessageVisitor;
class Message
{
public:
    Message(rai::MessageType);
    Message(rai::MessageType, uint16_t);
    Message(const rai::MessageHeader&);
    virtual ~Message()                                = default;
    virtual void Serialize(rai::Stream&) const        = 0;
    virtual rai::ErrorCode Deserialize(rai::Stream&)  = 0;
    virtual void Visit(rai::MessageVisitor&)          = 0;

    void SetFlag(rai::MessageFlags flag);
    void ClearFlag(rai::MessageFlags flag);
    bool GetFlag(rai::MessageFlags flag) const;
    void EnableProxy(const rai::Endpoint&);
    void DisableProxy();
    void ToBytes(std::vector<uint8_t>&) const;
    rai::Endpoint PeerEndpoint() const;
    void SetPeerEndpoint(const rai::Endpoint&);
    uint8_t Version() const;
    uint8_t VersionMin() const;

    rai::MessageHeader header_;
};

class HandshakeMessage : public Message
{
public:
    HandshakeMessage(rai::ErrorCode&, rai::Stream&, const rai::MessageHeader&);
    HandshakeMessage(const rai::Account&, const rai::uint256_union&);
    HandshakeMessage(const rai::Account&, const rai::Signature&);
    virtual ~HandshakeMessage() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void Visit(rai::MessageVisitor&) override;
    bool IsRequest() const;
    bool IsResponse() const;

    static uint16_t constexpr REQUEST  = 1;
    static uint16_t constexpr RESPONSE = 2;

    uint64_t timestamp_;
    rai::uint256_union cookie_;
    rai::Account account_;
    rai::Signature signature_;
};

class KeepliveMessage : public Message
{
public:
    KeepliveMessage(rai::ErrorCode&, rai::Stream&, const rai::MessageHeader&);
    KeepliveMessage(const std::vector<std::pair<rai::Account, rai::Endpoint>>&,
                    const rai::Account&, uint8_t);
    KeepliveMessage(const rai::BlockHash&, const rai::Account&);
    virtual ~KeepliveMessage() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void Visit(rai::MessageVisitor&) override;
    rai::BlockHash Hash() const;
    void SetSignature(const rai::Signature&);
    bool CheckSignature() const;
    uint8_t ReachablePeers() const;

    static size_t constexpr MAX_PEERS = 8;
    std::vector<std::pair<rai::Account, rai::Endpoint>> peers_;
    uint64_t timestamp_;
    rai::Account account_;
    rai::Signature signature_;

    // ack
    rai::BlockHash hash_;

private:
    void Serialize_(rai::Stream&) const; 
};

class RelayMessage : public Message
{
public:
    RelayMessage(rai::ErrorCode&, rai::Stream&, const rai::MessageHeader&);
    virtual ~RelayMessage() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void Visit(rai::MessageVisitor&) override;

    std::vector<uint8_t> payload_;
};

class PublishMessage : public Message
{
public:
    PublishMessage(rai::ErrorCode&, rai::Stream&, const rai::MessageHeader&);
    PublishMessage(const std::shared_ptr<rai::Block>&);
    PublishMessage(const std::shared_ptr<rai::Block>&, const rai::Account&);
    virtual ~PublishMessage() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void Visit(rai::MessageVisitor&) override;
    bool NeedConfirm() const;

    rai::Account account_;
    std::shared_ptr<rai::Block> block_;
};

class ConfirmMessage : public Message
{
public:
    ConfirmMessage(rai::ErrorCode&, rai::Stream&, const rai::MessageHeader&);
    ConfirmMessage(uint64_t, const rai::Account&,
                   const std::shared_ptr<rai::Block>&);
    ConfirmMessage(uint64_t, const rai::Account&, const rai::Signature&,
                   const std::shared_ptr<rai::Block>&);
    virtual ~ConfirmMessage() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void Visit(rai::MessageVisitor&) override;
    rai::BlockHash Hash() const;
    void SetSignature(const rai::Signature&);

    uint64_t timestamp_;
    rai::Account representative_;
    rai::Signature signature_;
    std::shared_ptr<rai::Block> block_;
};

enum class QueryBy : uint8_t
{
    INVALID  = 0,
    HASH     = 1,
    HEIGHT   = 2,
    PREVIOUS = 3,

    MAX
};

enum class QueryStatus : uint8_t
{
    INVALID = 0,
    SUCCESS = 1,
    MISS    = 2,
    PRUNED  = 3,
    FORK    = 4,
    MAX,
    
    PENDING = 254,
    TIMEOUT = 255
};

class QueryMessage : public Message
{
public:
    QueryMessage(rai::ErrorCode&, rai::Stream&, const rai::MessageHeader&);
    QueryMessage(uint64_t, rai::QueryBy, const rai::Account&, uint64_t,
                 const rai::BlockHash&);
    virtual ~QueryMessage() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void Visit(rai::MessageVisitor&) override;
    rai::QueryBy QueryBy() const;
    rai::QueryStatus QueryStatus() const;
    void SetStatus(rai::QueryStatus);

    static uint16_t ToExtention(rai::QueryBy, rai::QueryStatus);

    uint64_t sequence_;
    rai::Account account_;
    uint64_t height_;
    rai::BlockHash hash_;
    std::shared_ptr<rai::Block> block_;

private:
    rai::ErrorCode Check_() const;
};

class ForkMessage : public Message
{
public:
    ForkMessage(rai::ErrorCode&, rai::Stream&, const rai::MessageHeader&);
    ForkMessage(const std::shared_ptr<rai::Block>&,
                const std::shared_ptr<rai::Block>&);
    virtual ~ForkMessage() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void Visit(rai::MessageVisitor&) override;

    std::shared_ptr<rai::Block> first_;
    std::shared_ptr<rai::Block> second_;
};

class ConflictMessage : public Message
{
public:
    ConflictMessage(rai::ErrorCode&, rai::Stream&, const rai::MessageHeader&);
    ConflictMessage(const rai::Account&, uint64_t, uint64_t,
                    const rai::Signature&, const rai::Signature&,
                    const std::shared_ptr<rai::Block>&,
                    const std::shared_ptr<rai::Block>&);
    virtual ~ConflictMessage() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void Visit(rai::MessageVisitor&) override;

    rai::Account representative_;
    uint64_t timestamp_first_;
    uint64_t timestamp_second_;
    rai::Signature signature_first_;
    rai::Signature signature_second_;
    std::shared_ptr<rai::Block> block_first_;
    std::shared_ptr<rai::Block> block_second_;

private:
    rai::ErrorCode Check_() const;
    rai::BlockHash Hash_(uint64_t, const rai::Account&,
                         const rai::Block&) const;
};

enum class BootstrapType : uint8_t
{
    INVALID = 0,
    FULL    = 1,
    LIGHT   = 2,
    FORK    = 3,

    MAX
};

class BootstrapMessage : public Message
{
public:
    BootstrapMessage(rai::ErrorCode&, rai::Stream&, const rai::MessageHeader&);
    BootstrapMessage(rai::BootstrapType, const rai::Account&, uint64_t,
                     uint16_t);
    virtual ~BootstrapMessage() = default;

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void Visit(rai::MessageVisitor&) override;

    uint16_t MaxSize() const;

    rai::BootstrapType type_;
    rai::Account start_;
    uint64_t height_;

private:
    rai::ErrorCode Check_() const;
};

class WeightMessage : public Message
{
public:
    WeightMessage(rai::ErrorCode&, rai::Stream&, const rai::MessageHeader&);
    WeightMessage(const rai::uint256_union&, const rai::Account&);
    virtual ~WeightMessage() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void Visit(rai::MessageVisitor&) override;
    rai::uint256_union request_id_;
    rai::Account rep_;

    // only in response
    uint32_t epoch_;
    rai::Amount weight_;
    rai::Account replier_;
};

// Cross-chain messages between validators

class CrosschainMessage : public Message
{
public:
    CrosschainMessage(rai::ErrorCode&, rai::Stream&, const rai::MessageHeader&);
    CrosschainMessage(const rai::Account&, const rai::Account&,
                      std::vector<uint8_t>&&);
    virtual ~CrosschainMessage() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void Visit(rai::MessageVisitor&) override;

    rai::Account source_;
    rai::Account destination_;
    std::vector<uint8_t> payload_;
};

class MessageVisitor
{
public:
    virtual void Handshake(const rai::HandshakeMessage&)    = 0;
    virtual void Keeplive(const rai::KeepliveMessage&)      = 0;
    virtual void Publish(const rai::PublishMessage&)        = 0;
    virtual void Relay(rai::RelayMessage&)                  = 0;
    virtual void Confirm(const rai::ConfirmMessage&)        = 0;
    virtual void Query(const rai::QueryMessage&)            = 0;
    virtual void Fork(const rai::ForkMessage&)              = 0;
    virtual void Conflict(const rai::ConflictMessage&)      = 0;
    virtual void Weight(const rai::WeightMessage&)          = 0;
    virtual void Crosschain(const rai::CrosschainMessage&)  = 0;
};

class MessageParser
{
public:
    MessageParser(rai::MessageVisitor&);
    rai::ErrorCode Parse(rai::Stream&);

private:
    template<typename T>
    rai::ErrorCode Parse(rai::Stream& stream, const rai::MessageHeader& header)
    {
        rai::ErrorCode error_code;
        T msg(error_code, stream, header);
        IF_NOT_SUCCESS_RETURN(error_code);
        if (!rai::StreamEnd(stream))
        {
            return rai::ErrorCode::STREAM;
        }
        msg.Visit(visitor_);
        return rai::ErrorCode::SUCCESS;
    }

    rai::MessageVisitor& visitor_;
};

}  // namespace rai
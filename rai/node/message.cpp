#include <rai/node/message.hpp>

#include <blake2/blake2.h>
#include <boost/endian/conversion.hpp>
#include <rai/common/parameters.hpp>


size_t constexpr rai::KeepliveMessage::MAX_PEERS;

rai::MessageHeader::MessageHeader(rai::MessageType type)
    : MessageHeader(type, 0)
{
}

rai::MessageHeader::MessageHeader(rai::MessageType type, uint16_t extension)
    : version_using_(rai::PROTOCOL_VERSION_USING),
      version_min_(rai::PROTOCOL_VERSION_MIN),
      type_(type),
      flags_(),
      extension_(extension),
      peer_endpoint_(),
      payload_length_()
{
}

rai::MessageHeader::MessageHeader(rai::ErrorCode& error_code,
                                  rai::Stream& stream)
{
    error_code = Deserialize(stream);
}

void rai::MessageHeader::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, rai::MessageHeader::MagicNumber());
    rai::Write(stream, version_using_);
    rai::Write(stream, version_min_);
    rai::Write(stream, type_);
    uint8_t flags = static_cast<uint8_t>(flags_.to_ullong());
    rai::Write(stream, flags);
    rai::Write(stream, extension_);
    if (GetFlag(rai::MessageFlags::PROXY))
    {
        uint32_t ip = peer_endpoint_.address().to_v4().to_ulong();
        rai::Write(stream, ip);
        uint16_t port = peer_endpoint_.port();
        rai::Write(stream, port);
        rai::Write(stream, payload_length_);
    }
}

rai::ErrorCode rai::MessageHeader::Deserialize(rai::Stream& stream)
{
    bool error = false;
    std::array<uint8_t, 2> magic_number;

    error = rai::Read(stream, magic_number);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    if (magic_number != rai::MessageHeader::MagicNumber())
    {
        return rai::ErrorCode::MAGIC_NUMBER;
    }

    error = rai::Read(stream, version_using_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    if (version_using_ < rai::PROTOCOL_VERSION_MIN)
    {
        return rai::ErrorCode::OUTDATED_VERSION;
    }

    error = rai::Read(stream, version_min_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    if (version_min_ > rai::PROTOCOL_VERSION_USING)
    {
        return rai::ErrorCode::UNKNOWN_VERSION;
    }

    error = rai::Read(stream, type_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    if (type_ == rai::MessageType::INVALID)
    {
        return rai::ErrorCode::INVALID_MESSAGE;
    }
    if (type_ >= rai::MessageType::MAX)
    {
        return rai::ErrorCode::UNKNOWN_MESSAGE;
    }

    uint8_t flags;
    error = rai::Read(stream, flags);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    flags_ = flags;

    error = rai::Read(stream, extension_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    if (GetFlag(rai::MessageFlags::PROXY))
    {
        uint32_t ip;
        error = rai::Read(stream, ip);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

        uint16_t port;
        error = rai::Read(stream, port);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

        peer_endpoint_ = rai::Endpoint(rai::IP(ip), port);

        error = rai::Read(stream, payload_length_);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::MessageHeader::SetFlag(rai::MessageFlags flag)
{
    flags_[FlagToIndex(flag)] = true;
}

void rai::MessageHeader::ClearFlag(rai::MessageFlags flag)
{
    flags_[FlagToIndex(flag)] = false;
}

bool rai::MessageHeader::GetFlag(rai::MessageFlags flag) const
{
    return flags_[FlagToIndex(flag)];
}

std::array<uint8_t, 2> rai::MessageHeader::MagicNumber()
{
    if (rai::RAI_NETWORK == rai::RaiNetworks::LIVE)
    {
        return std::array<uint8_t, 2>({'R', 'Z'});
    }
    else if (rai::RAI_NETWORK == rai::RaiNetworks::BETA)
    {
        return std::array<uint8_t, 2>({'R', 'Y'});
    }
    else
    {
        return std::array<uint8_t, 2>({'R', 'X'});
    }
}

uint32_t rai::MessageHeader::FlagToIndex(rai::MessageFlags flag)
{
    if (flag >= rai::MessageFlags::INVALID)
    {
        throw std::invalid_argument("invalid message flag");
    }
    return static_cast<uint32_t>(flag);
}

rai::Message::Message(rai::MessageType type) : header_(type)
{
}

rai::Message::Message(rai::MessageType type, uint16_t extension)
    : header_(type, extension)
{
}

rai::Message::Message(const rai::MessageHeader& header) : header_(header)
{
}

void rai::Message::SetFlag(rai::MessageFlags flag)
{
    header_.SetFlag(flag);
}

void rai::Message::ClearFlag(rai::MessageFlags flag)
{
    header_.ClearFlag(flag);
}

bool rai::Message::GetFlag(rai::MessageFlags flag) const
{
    return header_.GetFlag(flag);
}

void rai::Message::EnableProxy(const rai::Endpoint& peer_endpoint)
{
    header_.SetFlag(rai::MessageFlags::PROXY);
    header_.peer_endpoint_ = peer_endpoint;

    std::vector<uint8_t> header_bytes;
    {
        rai::VectorStream stream(header_bytes);
        header_.Serialize(stream);
    }
    std::vector<uint8_t> all_bytes;
    ToBytes(all_bytes);
    header_.payload_length_ =
        static_cast<uint16_t>(all_bytes.size() - header_bytes.size());
}

void rai::Message::DisableProxy()
{
    header_.ClearFlag(rai::MessageFlags::PROXY);
}

void rai::Message::ToBytes(std::vector<uint8_t>& bytes) const
{
    rai::VectorStream stream(bytes);
    Serialize(stream);
}

rai::Endpoint rai::Message::PeerEndpoint() const
{
    return header_.peer_endpoint_;
}

void rai::Message::SetPeerEndpoint(const rai::Endpoint& endpoint)
{
    header_.peer_endpoint_ = endpoint;
}

uint8_t rai::Message::Version() const
{
    return header_.version_using_;
}
uint8_t rai::Message::VersionMin() const
{
    return header_.version_min_;
}

rai::HandshakeMessage::HandshakeMessage(rai::ErrorCode& error_code,
                                        rai::Stream& stream,
                                        const rai::MessageHeader& header)
    : Message(header)
{
    if ((header_.extension_ != rai::HandshakeMessage::REQUEST)
        && (header_.extension_ != rai::HandshakeMessage::RESPONSE))
    {
        error_code = rai::ErrorCode::HANDSHAKE_TYPE;
        return;
    }

    error_code = Deserialize(stream);
}

rai::HandshakeMessage::HandshakeMessage(const rai::Account& account,
                                        const rai::uint256_union& cookie)
    : Message(rai::MessageType::HANDSHAKE, rai::HandshakeMessage::REQUEST),
      timestamp_(rai::CurrentTimestamp()),
      cookie_(cookie),
      account_(account)
{
}

rai::HandshakeMessage::HandshakeMessage(const rai::Account& account,
                                        const rai::Signature& signature)
    : Message(rai::MessageType::HANDSHAKE, rai::HandshakeMessage::RESPONSE),
      account_(account),
      signature_(signature)
{
}

void rai::HandshakeMessage::Serialize(rai::Stream& stream) const
{
    header_.Serialize(stream);
    if (header_.extension_ == rai::HandshakeMessage::REQUEST)
    {
        rai::Write(stream, timestamp_);
        rai::Write(stream, cookie_.bytes);
        rai::Write(stream, account_.bytes);
    }
    else
    {
        rai::Write(stream, account_.bytes);
        rai::Write(stream, signature_.bytes);
    }
}

rai::ErrorCode rai::HandshakeMessage::Deserialize(rai::Stream& stream)
{
    bool error = false;

    if (header_.extension_ == rai::HandshakeMessage::REQUEST)
    {
        error = rai::Read(stream, timestamp_);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
        error = rai::Read(stream, cookie_.bytes);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
        error = rai::Read(stream, account_.bytes);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    }
    else
    {
        error = rai::Read(stream, account_.bytes);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
        error = rai::Read(stream, signature_.bytes);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::HandshakeMessage::Visit(rai::MessageVisitor& visitor)
{
    visitor.Handshake(*this);
}

bool rai::HandshakeMessage::IsRequest() const
{
    return rai::HandshakeMessage::REQUEST == header_.extension_;
}

bool rai::HandshakeMessage::IsResponse() const
{
    return rai::HandshakeMessage::RESPONSE == header_.extension_;
}

rai::KeepliveMessage::KeepliveMessage(rai::ErrorCode& error_code,
                                      rai::Stream& stream,
                                      const rai::MessageHeader& header)
    : Message(header)
{
    uint8_t total = header_.extension_ & 0xFF;
    if (total > rai::KeepliveMessage::MAX_PEERS)
    {
        error_code = rai::ErrorCode::KEEPLIVE_PEERS;
        return;
    }

    uint8_t reachable = static_cast<uint8_t>(header_.extension_ >> 8);
    if (reachable > total)
    {
        error_code = rai::ErrorCode::KEEPLIVE_REACHABLE_PEERS;
        return;
    }

    error_code = Deserialize(stream);
}

rai::KeepliveMessage::KeepliveMessage(
    const std::vector<std::pair<rai::Account, rai::Endpoint>>& peers,
    const rai::Account& account, uint8_t reachable_peers)
    : Message(rai::MessageType::KEEPLIVE,
              (static_cast<uint16_t>(reachable_peers) << 8) + peers.size()),
      peers_(peers),
      timestamp_(rai::CurrentTimestamp()),
      account_(account),
      signature_()
{
    if (peers.size() > rai::KeepliveMessage::MAX_PEERS)
    {
        throw std::invalid_argument("Too many peers for keeplive message");
    }
}

rai::KeepliveMessage::KeepliveMessage(const rai::BlockHash& hash,
                                      const rai::Account& account)
    : Message(rai::MessageType::KEEPLIVE), account_(account), hash_(hash)
{
    SetFlag(rai::MessageFlags::ACK);
}

void rai::KeepliveMessage::Serialize(rai::Stream& stream) const
{
    header_.Serialize(stream);
    Serialize_(stream);
}

rai::ErrorCode rai::KeepliveMessage::Deserialize(rai::Stream& stream)
{
    bool error = false;

    if (GetFlag(rai::MessageFlags::ACK))
    {
        error = rai::Read(stream, hash_.bytes);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
        error = rai::Read(stream, account_.bytes);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
        return rai::ErrorCode::SUCCESS;
    }

    for (uint16_t i = 0; i < (header_.extension_ & 0x00FF); ++i)
    {
        rai::Account account;
        error = rai::Read(stream, account.bytes);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

        uint32_t ip;
        error = rai::Read(stream, ip);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

        uint16_t port;
        error = rai::Read(stream, port);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

        rai::Endpoint endpoint(rai::IP(ip), port);
        peers_.push_back(
            std::pair<rai::Account, rai::Endpoint>(account, endpoint));
    }

    error = rai::Read(stream, timestamp_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, account_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, signature_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return rai::ErrorCode::SUCCESS;
}

void rai::KeepliveMessage::Visit(rai::MessageVisitor& visitor)
{
    visitor.Keeplive(*this);
}

rai::BlockHash rai::KeepliveMessage::Hash() const
{
    int ret;
    rai::BlockHash result;
    blake2b_state state;

    ret = blake2b_init(&state, sizeof(result.bytes));
    assert(0 == ret);

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        Serialize_(stream);
    }
    ret = blake2b_update(&state, bytes.data(),
                         bytes.size() - signature_.bytes.size());
    assert(0 == ret);

    ret = blake2b_final(&state, result.bytes.data(), sizeof(result.bytes));
    assert(0 == ret);
    return result;
}

void rai::KeepliveMessage::SetSignature(const rai::Signature& signature)
{
    signature_ = signature;
}

bool rai::KeepliveMessage::CheckSignature() const
{
    return rai::ValidateMessage(account_, Hash(), signature_);
}

uint8_t rai::KeepliveMessage::ReachablePeers() const
{
    return static_cast<uint8_t>(header_.extension_ >> 8);
}

void rai::KeepliveMessage::Serialize_(rai::Stream& stream) const
{
    if (GetFlag(rai::MessageFlags::ACK))
    {
        rai::Write(stream, hash_.bytes);
        rai::Write(stream, account_.bytes);
    }
    else
    {
        for (const auto& peer : peers_)
        {
            rai::Write(stream, peer.first.bytes);
            uint32_t ip = peer.second.address().to_v4().to_ulong();
            rai::Write(stream, ip);
            uint16_t port = peer.second.port();
            rai::Write(stream, port);
        }

        rai::Write(stream, timestamp_);
        rai::Write(stream, account_.bytes);
        rai::Write(stream, signature_.bytes);
    }
}

rai::RelayMessage::RelayMessage(rai::ErrorCode& error_code, rai::Stream& stream,
                                const rai::MessageHeader& header)
    : Message(header)
{
    SetFlag(rai::MessageFlags::RELAY);
    error_code = Deserialize(stream);
}

void rai::RelayMessage::Serialize(rai::Stream& stream) const
{
    header_.Serialize(stream);
    rai::Write(stream, payload_);
}

rai::ErrorCode rai::RelayMessage::Deserialize(rai::Stream& stream)
{
    bool error = rai::Read(stream, header_.payload_length_, payload_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return rai::ErrorCode::SUCCESS;
}

void rai::RelayMessage::Visit(rai::MessageVisitor& visitor)
{
    visitor.Relay(*this);
}

rai::PublishMessage::PublishMessage(rai::ErrorCode& error_code,
                                    rai::Stream& stream,
                                    const rai::MessageHeader& header)
    : Message(header)
{
    error_code = Deserialize(stream);
}

rai::PublishMessage::PublishMessage(const std::shared_ptr<rai::Block>& block)
    : Message(rai::MessageType::PUBLISH, 0), block_(block)
{
}

rai::PublishMessage::PublishMessage(const std::shared_ptr<rai::Block>& block,
                                    const rai::Account& account)
    : Message(rai::MessageType::PUBLISH, 1), account_(account), block_(block)
{
}

void rai::PublishMessage::Serialize(rai::Stream& stream) const
{
    header_.Serialize(stream);
    if (NeedConfirm())
    {
        rai::Write(stream, account_.bytes);
    }
    block_->Serialize(stream);
}

rai::ErrorCode rai::PublishMessage::Deserialize(rai::Stream& stream)
{
    if ((header_.extension_ != 0) && (header_.extension_ != 1))
    {
        return rai::ErrorCode::PUBLISH_NEED_CONFIRM;
    }

    if (header_.extension_ == 1)
    {
        bool error = rai::Read(stream, account_.bytes);
        if (error)
        {
            return rai::ErrorCode::STREAM;
        }
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    // verify in rai::Node::ReceiveBlock
    block_ = DeserializeBlockUnverify(error_code, stream);
    return error_code;
}

void rai::PublishMessage::Visit(rai::MessageVisitor& visitor)
{
    visitor.Publish(*this);
}

bool rai::PublishMessage::NeedConfirm() const
{
    return header_.extension_ == 1 ? true : false;
}

rai::ConfirmMessage::ConfirmMessage(rai::ErrorCode& error_code,
                                    rai::Stream& stream,
                                    const rai::MessageHeader& header)
    : Message(header)
{
    error_code = Deserialize(stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    bool error = rai::ValidateMessage(representative_, Hash(), signature_);
    if (error)
    {
        error_code = rai::ErrorCode::MESSAGE_CONFIRM_SIGNATURE;
    }
}

rai::ConfirmMessage::ConfirmMessage(uint64_t timestamp,
                                    const rai::Account& representative,
                                    const std::shared_ptr<rai::Block>& block)
    : Message(rai::MessageType::CONFIRM),
      timestamp_(timestamp),
      representative_(representative),
      block_(block)
{
}

rai::ConfirmMessage::ConfirmMessage(uint64_t timestamp,
                                    const rai::Account& representative,
                                    const rai::Signature& signature,
                                    const std::shared_ptr<rai::Block>& block)
    : Message(rai::MessageType::CONFIRM),
      timestamp_(timestamp),
      representative_(representative),
      signature_(signature),
      block_(block)
{
}

void rai::ConfirmMessage::Serialize(rai::Stream& stream) const
{
    header_.Serialize(stream);
    rai::Write(stream, timestamp_);
    rai::Write(stream, representative_.bytes);
    rai::Write(stream, signature_.bytes);
    block_->Serialize(stream);
}

rai::ErrorCode rai::ConfirmMessage::Deserialize(rai::Stream& stream)
{
    bool error = false;

    error = rai::Read(stream, timestamp_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, representative_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, signature_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block_ = DeserializeBlock(error_code, stream);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

void rai::ConfirmMessage::Visit(rai::MessageVisitor& visitor)
{
    visitor.Confirm(*this);
}

rai::BlockHash rai::ConfirmMessage::Hash() const
{
    rai::BlockHash result;
    blake2b_state state;

    auto ret = blake2b_init(&state, sizeof(result.bytes));
    assert(0 == ret);

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, timestamp_);
        rai::Write(stream, representative_.bytes);
        rai::Write(stream, block_->Hash().bytes);
    }
    ret = blake2b_update(&state, bytes.data(), bytes.size());
    assert(0 == ret);

    ret = blake2b_final(&state, result.bytes.data(), result.bytes.size());
    assert(0 == ret);
    return result;
}

void rai::ConfirmMessage::SetSignature(const rai::Signature& signature)
{
    signature_ = signature;
}

rai::QueryMessage::QueryMessage(rai::ErrorCode& error_code, rai::Stream& stream,
                                const rai::MessageHeader& header)
    : Message(header)
{
    if (QueryBy() == rai::QueryBy::INVALID)
    {
        error_code = rai::ErrorCode::MESSAGE_QUERY_BY;
        return;
    }

    if (GetFlag(rai::MessageFlags::ACK))
    {
        if (QueryStatus() == rai::QueryStatus::INVALID)
        {
            error_code = rai::ErrorCode::MESSAGE_QUERY_STATUS;
            return;
        }
    }

    error_code = Deserialize(stream);
}

rai::QueryMessage::QueryMessage(uint64_t sequence, rai::QueryBy by,
                                const rai::Account& account, uint64_t height,
                                const rai::BlockHash& hash)
    : Message(rai::MessageType::QUERY,
              ToExtention(by, rai::QueryStatus::INVALID)),
      sequence_(sequence),
      account_(account),
      height_(height),
      hash_(hash),
      block_(nullptr)
{
}

void rai::QueryMessage::Serialize(rai::Stream& stream) const
{
    header_.Serialize(stream);
    rai::Write(stream, sequence_);
    rai::Write(stream, account_.bytes);
    rai::Write(stream, height_);
    if (QueryBy() == rai::QueryBy::HASH || QueryBy() == rai::QueryBy::PREVIOUS)
    {
        rai::Write(stream, hash_.bytes);
    }

    if (GetFlag(rai::MessageFlags::ACK) && block_ != nullptr)
    {
        if (QueryStatus() == rai::QueryStatus::SUCCESS
            || QueryStatus() == rai::QueryStatus::FORK)
        {
            block_->Serialize(stream);
        }
    }
}

rai::ErrorCode rai::QueryMessage::Deserialize(rai::Stream& stream)
{
    bool error = rai::Read(stream, sequence_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, account_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    if (QueryBy() == rai::QueryBy::HASH || QueryBy() == rai::QueryBy::PREVIOUS)
    {
        error = rai::Read(stream, hash_.bytes);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    }

    if (GetFlag(rai::MessageFlags::ACK))
    {
        if (QueryStatus() == rai::QueryStatus::SUCCESS
            || QueryStatus() == rai::QueryStatus::FORK)
        {
            rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
            block_ = DeserializeBlock(error_code, stream);
            IF_NOT_SUCCESS_RETURN(error_code);
        }
    }

    return Check_();
}

void rai::QueryMessage::Visit(rai::MessageVisitor& visitor)
{
    visitor.Query(*this);
}

rai::QueryBy rai::QueryMessage::QueryBy() const
{
    uint8_t by = static_cast<uint8_t>(header_.extension_ >> 8);
    if (by >= static_cast<uint8_t>(rai::QueryBy::MAX))
    {
        return rai::QueryBy::INVALID;
    }
    return static_cast<rai::QueryBy>(by);
}

rai::QueryStatus rai::QueryMessage::QueryStatus() const
{
    uint8_t status = static_cast<uint8_t>(header_.extension_ & 0xff);
    if (status >= static_cast<uint8_t>(rai::QueryStatus::MAX))
    {
        return rai::QueryStatus::INVALID;
    }
    return static_cast<rai::QueryStatus>(status);
}

void rai::QueryMessage::SetStatus(rai::QueryStatus status)
{
    header_.extension_ =
        (header_.extension_ & 0xff00) | static_cast<uint8_t>(status);
}

uint16_t rai::QueryMessage::ToExtention(rai::QueryBy by,
                                        rai::QueryStatus status)
{
    return (static_cast<uint16_t>(by) << 8) | static_cast<uint16_t>(status);
}

rai::ErrorCode rai::QueryMessage::Check_() const
{
    if (block_ == nullptr)
    {
        return rai::ErrorCode::SUCCESS;
    }
    
    rai::QueryBy by = QueryBy();
    rai::QueryStatus status = QueryStatus();
    
    if (by == rai::QueryBy::HASH)
    {
        if (status != rai::QueryStatus::SUCCESS)
        {
            return rai::ErrorCode::MESSAGE_QUERY_STATUS;
        }
        if (block_->Account() != account_ && !account_.IsZero())
        {
            return rai::ErrorCode::MESSAGE_QUERY_BLOCK;
        }
        if (block_->Height() != height_
            && height_ != rai::Block::INVALID_HEIGHT)
        {
            return rai::ErrorCode::MESSAGE_QUERY_BLOCK;
        }
        if (block_->Hash() != hash_)
        {
            return rai::ErrorCode::MESSAGE_QUERY_BLOCK;
        }
    }
    else if (by == rai::QueryBy::HEIGHT)
    {
        if (status != rai::QueryStatus::SUCCESS)
        {
            return rai::ErrorCode::MESSAGE_QUERY_STATUS;
        }
        if (block_->Account() != account_)
        {
            return rai::ErrorCode::MESSAGE_QUERY_BLOCK;
        }
        if (block_->Height() != height_)
        {
            return rai::ErrorCode::MESSAGE_QUERY_BLOCK;
        }
    }
    else if (by == rai::QueryBy::PREVIOUS)
    {
        if (status == rai::QueryStatus::SUCCESS)
        {
            if (block_->Account() != account_)
            {
                return rai::ErrorCode::MESSAGE_QUERY_BLOCK;
            }
            if (block_->Height() != height_)
            {
                return rai::ErrorCode::MESSAGE_QUERY_BLOCK;
            }
            if (block_->Previous() != hash_)
            {
                return rai::ErrorCode::MESSAGE_QUERY_BLOCK;
            }
        }
        else if (status == rai::QueryStatus::FORK)
        {
            if (block_->Account() != account_)
            {
                return rai::ErrorCode::MESSAGE_QUERY_BLOCK;
            }
            if (block_->Height() + 1 != height_)
            {
                return rai::ErrorCode::MESSAGE_QUERY_BLOCK;
            }
            if (block_->Hash() == hash_)
            {
                return rai::ErrorCode::MESSAGE_QUERY_BLOCK;
            }
        }
        else
        {
            return rai::ErrorCode::MESSAGE_QUERY_STATUS;
        }
    }
    else
    {
        return rai::ErrorCode::MESSAGE_QUERY_BY;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ForkMessage::ForkMessage(rai::ErrorCode& error_code, rai::Stream& stream,
                              const rai::MessageHeader& header)
    : Message(header)
{
    error_code = Deserialize(stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    if (first_->Account() != second_->Account())
    {
        error_code = rai::ErrorCode::MESSAGE_FORK_ACCOUNT;
        return;
    }

    if (first_->Height() != second_->Height())
    {
        error_code = rai::ErrorCode::MESSAGE_FORK_HEIGHT;
        return;
    }

    if (*first_ == *second_)
    {
        error_code = rai::ErrorCode::MESSAGE_FORK_BLOCK_EQUAL;
        return;
    }
}

rai::ForkMessage::ForkMessage(const std::shared_ptr<rai::Block>& first,
                              const std::shared_ptr<rai::Block>& second)
    : Message(rai::MessageType::FORK), first_(first), second_(second)
{
}

void rai::ForkMessage::Serialize(rai::Stream& stream) const
{
    header_.Serialize(stream);
    first_->Serialize(stream);
    second_->Serialize(stream);
}

rai::ErrorCode rai::ForkMessage::Deserialize(rai::Stream& stream)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    first_ = DeserializeBlock(error_code, stream);
    IF_NOT_SUCCESS_RETURN(error_code);

    second_ = DeserializeBlock(error_code, stream);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

void rai::ForkMessage::Visit(rai::MessageVisitor& visitor)
{
    visitor.Fork(*this);
}

rai::ConflictMessage::ConflictMessage(rai::ErrorCode& error_code,
                                      rai::Stream& stream,
                                      const rai::MessageHeader& header)
    : Message(header)
{
    error_code = Deserialize(stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    error_code = Check_();
}

rai::ConflictMessage::ConflictMessage(
    const rai::Account& representative, uint64_t timestamp_first,
    uint64_t timestamp_second, const rai::Signature& signature_first,
    const rai::Signature& signature_second,
    const std::shared_ptr<rai::Block>& block_first,
    const std::shared_ptr<rai::Block>& block_second)
    : Message(rai::MessageType::CONFLICT),
      representative_(representative),
      timestamp_first_(timestamp_first),
      timestamp_second_(timestamp_second),
      signature_first_(signature_first),
      signature_second_(signature_second),
      block_first_(block_first),
      block_second_(block_second)
{
}

void rai::ConflictMessage::Serialize(rai::Stream& stream) const
{
    header_.Serialize(stream);
    rai::Write(stream, representative_.bytes);
    rai::Write(stream, timestamp_first_);
    rai::Write(stream, timestamp_second_);
    rai::Write(stream, signature_first_.bytes);
    rai::Write(stream, signature_second_.bytes);
    block_first_->Serialize(stream);
    block_second_->Serialize(stream);
}

rai::ErrorCode rai::ConflictMessage::Deserialize(rai::Stream& stream)
{
    bool error = false;

    error = rai::Read(stream, representative_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, timestamp_first_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, timestamp_second_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, signature_first_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, signature_second_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block_first_ = DeserializeBlock(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return rai::ErrorCode::MESSAGE_CONFLICT_BLOCK;
    }
    block_second_ = DeserializeBlock(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return rai::ErrorCode::MESSAGE_CONFLICT_BLOCK;
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::ConflictMessage::Visit(rai::MessageVisitor& visitor)
{
    visitor.Conflict(*this);
}

rai::ErrorCode rai::ConflictMessage::Check_() const
{
    if (block_first_->Account() != block_second_->Account())
    {
        return rai::ErrorCode::MESSAGE_CONFLICT_BLOCK;
    }
    if (block_first_->Height() != block_second_->Height())
    {
        return rai::ErrorCode::MESSAGE_CONFLICT_BLOCK;
    }

    uint64_t diff = timestamp_first_ > timestamp_second_
                        ? timestamp_first_ - timestamp_second_
                        : timestamp_second_ - timestamp_first_;

    if (diff == 0 && *block_first_ == *block_second_)
    {
        return rai::ErrorCode::MESSAGE_CONFLICT_BLOCK;
    }

    if (diff >= rai::MIN_CONFIRM_INTERVAL)
    {
        return rai::ErrorCode::MESSAGE_CONFLICT_TIMESTAMP;
    }

    rai::BlockHash hash_first =
        Hash_(timestamp_first_, representative_, *block_first_);
    bool error =
        rai::ValidateMessage(representative_, hash_first, signature_first_);
    IF_ERROR_RETURN(error, rai::ErrorCode::MESSAGE_CONFLICT_SIGNATURE);

    rai::BlockHash hash_second =
        Hash_(timestamp_second_, representative_, *block_second_);
    error =
        rai::ValidateMessage(representative_, hash_second, signature_second_);
    IF_ERROR_RETURN(error, rai::ErrorCode::MESSAGE_CONFLICT_SIGNATURE);

    return rai::ErrorCode::SUCCESS;
}

rai::BlockHash rai::ConflictMessage::Hash_(uint64_t timestamp,
                                           const rai::Account& representative,
                                           const rai::Block& block) const
{
    rai::BlockHash result;
    blake2b_state state;

    auto ret = blake2b_init(&state, sizeof(result.bytes));
    assert(0 == ret);

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, timestamp);
        rai::Write(stream, representative.bytes);
        rai::Write(stream, block.Hash().bytes);
    }
    ret = blake2b_update(&state, bytes.data(), bytes.size());
    assert(0 == ret);

    ret = blake2b_final(&state, result.bytes.data(), result.bytes.size());
    assert(0 == ret);
    return result;
}

rai::BootstrapMessage::BootstrapMessage(rai::ErrorCode& error_code,
                                        rai::Stream& stream,
                                        const rai::MessageHeader& header)
    : Message(header)
{
    error_code = Deserialize(stream);
}

rai::BootstrapMessage::BootstrapMessage(rai::BootstrapType type,
                                        const rai::Account& start,
                                        uint64_t height, uint16_t size)
    : Message(rai::MessageType::BOOTSTRAP, size),
      type_(type),
      start_(start),
      height_(height)
{
}

void rai::BootstrapMessage::Serialize(rai::Stream& stream) const
{
    header_.Serialize(stream);
    rai::Write(stream, type_);
    rai::Write(stream, start_.bytes);
    rai::Write(stream, height_);
}

rai::ErrorCode rai::BootstrapMessage::Deserialize(rai::Stream& stream)
{
    bool error = rai::Read(stream, type_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, start_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    
    return Check_();
}

void rai::BootstrapMessage::Visit(rai::MessageVisitor& visitor)
{
}

uint16_t rai::BootstrapMessage::MaxSize() const
{
    return header_.extension_;
}

rai::ErrorCode rai::BootstrapMessage::Check_() const
{
    if (type_ == rai::BootstrapType::INVALID || type_ >= rai::BootstrapType::MAX)
    {
        return rai::ErrorCode::BOOTSTRAP_TYPE;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::WeightMessage::WeightMessage(rai::ErrorCode& error_code, rai::Stream& stream,
                                const rai::MessageHeader& header)
    : Message(header)
{
    error_code = Deserialize(stream);
}

void rai::WeightMessage::Serialize(rai::Stream& stream) const
{
    header_.Serialize(stream);
    rai::Write(stream, request_id_.bytes);
    rai::Write(stream, rep_.bytes);

    if (GetFlag(rai::MessageFlags::ACK))
    {
        rai::Write(stream, epoch_);
        rai::Write(stream, weight_.bytes);
        rai::Write(stream, replier_.bytes);
    }
}

rai::ErrorCode rai::WeightMessage::Deserialize(rai::Stream& stream)
{
    bool error = rai::Read(stream, request_id_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, rep_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    if (GetFlag(rai::MessageFlags::ACK))
    {
        error = rai::Read(stream, epoch_);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

        error = rai::Read(stream, weight_.bytes);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

        error = rai::Read(stream, replier_.bytes);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::WeightMessage::Visit(rai::MessageVisitor& visitor)
{
    visitor.Weight(*this);
}

rai::MessageParser::MessageParser(rai::MessageVisitor& visitor)
    : visitor_(visitor)
{
}

rai::ErrorCode rai::MessageParser::Parse(rai::Stream& stream)
{
    rai::ErrorCode error_code;
    rai::MessageHeader header(error_code, stream);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (header.GetFlag(rai::MessageFlags::PROXY)
        && !header.GetFlag(rai::MessageFlags::RELAY))
    {
        return Parse<rai::RelayMessage>(stream, header);
    }

    switch (header.type_)
    {
        case rai::MessageType::HANDSHAKE:
        {
            return Parse<rai::HandshakeMessage>(stream, header);
        }
        case rai::MessageType::KEEPLIVE:
        {
            return Parse<rai::KeepliveMessage>(stream, header);
        }
        case rai::MessageType::PUBLISH:
        {
            return Parse<rai::PublishMessage>(stream, header);
        }
        case rai::MessageType::CONFIRM:
        {
            return Parse<rai::ConfirmMessage>(stream, header);
        }
        case rai::MessageType::QUERY:
        {
            return Parse<rai::QueryMessage>(stream, header);
        }
        case rai::MessageType::FORK:
        {
            return Parse<rai::ForkMessage>(stream, header);
        }
        case rai::MessageType::CONFLICT:
        {
            return Parse<rai::ConflictMessage>(stream, header);
        }
        default:
        {
            return rai::ErrorCode::UNKNOWN_MESSAGE;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

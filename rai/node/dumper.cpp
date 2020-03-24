#include <rai/node/dumper.hpp>
#include <rai/node/node.hpp>

rai::Ptree rai::MessageDumpEntry::Get() const
{
    rai::Ptree result;
    result.put("direction", send_ ? "send" : "receive");
    result.put("transport", transport_);
    result.put("endpoint", endpoint_);
    result.put("timestamp", std::to_string(timestamp_));
    result.put("raw", rai::BytesToHex(bytes_.data(), bytes_.size()));
    if (parser_)
    {
        result.put_child("parse", parser_(bytes_));
    }
    return result;
}

rai::MessageDumper::MessageDumper() : on_(false), index_(0)
{
}

void rai::MessageDumper::Dump(bool send, const rai::Endpoint& remote,
                              const std::vector<uint8_t>& bytes)
{
    std::vector<uint8_t> bytes_l(bytes);
    Dump(send, remote, std::move(bytes_l));
}

void rai::MessageDumper::Dump(bool send, const rai::Endpoint& remote,
                              const uint8_t* data, size_t size)
{
    std::vector<uint8_t> bytes(data, data + size);
    Dump(send, remote, std::move(bytes));
}

void rai::MessageDumper::Dump(bool send, const rai::Endpoint& remote,
                              std::vector<uint8_t>&& bytes)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!on_ || bytes.size() < 5)
    {
        return;
    }
    rai::MessageType type = static_cast<rai::MessageType>(bytes[4]);
    if (!type_.empty() && type_ != rai::MessageDumper::ToString(type))
    {
        return;
    }

    if (!ip_.empty() && ip_ != remote.address().to_v4().to_string())
    {
        return;
    }

    rai::MessageDumpEntry entry;
    entry.send_ = send;
    entry.timestamp_ = rai::CurrentTimestampMilliseconds();
    entry.transport_ = "udp";
    entry.endpoint_ = rai::ToString(remote);
    entry.bytes_ = std::move(bytes);
    entry.parser_ = rai::MessageDumper::ParseMessageNormal;
    if (index_ < rai::MessageDumper::MAX_SIZE)
    {
        messages_.push_back(entry);
    }
    else
    {
        messages_[index_ % rai::MessageDumper::MAX_SIZE] = entry;
    }
    ++index_;
}

rai::Ptree rai::MessageDumper::Get() const
{
    rai::Ptree result;
    std::lock_guard<std::mutex> lock(mutex_);
    if (index_ <= rai::MessageDumper::MAX_SIZE)
    {
        for (auto i = messages_.begin(), n = messages_.end(); i != n; ++i)
        {
            result.push_back(std::make_pair("", i->Get()));
        }
    }
    else
    {
        uint64_t index = index_ % rai::MessageDumper::MAX_SIZE;
        for (auto i = messages_.begin() + index, n = messages_.end(); i != n;
             ++i)
        {
            result.push_back(std::make_pair("", i->Get()));
        }
        for (auto i = messages_.begin(), n = messages_.begin() + index; i != n; ++i)
        {
            result.push_back(std::make_pair("", i->Get()));
        }
    }

    return result;
}


void rai::MessageDumper::On(const std::string& type, const std::string& ip)
{
    std::lock_guard<std::mutex> lock(mutex_);
    on_ = true;
    type_ = type;
    ip_ = ip;
    index_ = 0;
    messages_.clear();
}

void rai::MessageDumper::Off()
{
    std::lock_guard<std::mutex> lock(mutex_);
    on_ = false;
    type_ = "";
    ip_ = "";
    index_ = 0;
    messages_.clear();
}

std::string rai::MessageDumper::ToString(rai::MessageType type)
{
    switch (type)
    {
        case rai::MessageType::INVALID:
        {
            return "invalid";
        }
        case rai::MessageType::HANDSHAKE:
        {
            return "handshake";
        }
        case rai::MessageType::KEEPLIVE:
        {
            return "keeplive";
        }
        case rai::MessageType::PUBLISH:
        {
            return "publish";
        }
        case rai::MessageType::CONFIRM:
        {
            return "confirm";
        }
        case rai::MessageType::QUERY:
        {
            return "query";
        }
        case rai::MessageType::FORK:
        {
            return "fork";
        }
        case rai::MessageType::CONFLICT:
        {
            return "conflict";
        }
        case rai::MessageType::BOOTSTRAP:
        {
            return "bootstrap";
        }
        default:
        {
            return "unknown(" + std::to_string(static_cast<uint32_t>(type))
                   + ")";
        }
    }
}

rai::Ptree rai::MessageDumper::ParseMessageNormal(
    const std::vector<uint8_t>& bytes)
{
    rai::Ptree result;

    rai::ErrorCode error_code;
    rai::BufferStream stream(bytes.data(), bytes.size());
    rai::MessageHeader header(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        result.put("error", rai::ErrorString(error_code));
        result.put("error_code",
                   std::to_string(static_cast<uint32_t>(error_code)));
        return result;
    }

    rai::Ptree header_ptree;
    header_ptree.put("version", std::to_string(header.version_using_));
    header_ptree.put("version_min", std::to_string(header.version_min_));
    header_ptree.put("type", ToString(header.type_));
    std::string flags;
    if (header.GetFlag(rai::MessageFlags::PROXY))
    {
        flags += "proxy";
    }
    if (header.GetFlag(rai::MessageFlags::RELAY))
    {
        if (!flags.empty()) flags += ", ";
        flags += "relay";
    }
    if (header.GetFlag(rai::MessageFlags::ACK))
    {
        if (!flags.empty()) flags += ", ";
        flags += "ack";
    }
    header_ptree.put("flags", flags);
    header_ptree.put("extension", std::to_string(header.extension_));
    if (header.GetFlag(rai::MessageFlags::PROXY))
    {
        header_ptree.put("peer_endpoint", rai::ToString(header.peer_endpoint_));
        header_ptree.put("payload_length",
                         std::to_string(header.payload_length_));
    }
    
    std::vector<uint8_t> body;
    uint8_t byte;
    while (!rai::Read(stream, byte))
    {
        body.push_back(byte);
    }

    result.put_child("header", header_ptree);
    result.put("body", rai::BytesToHex(body.data(), body.size()));
    return result;
}

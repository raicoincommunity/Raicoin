#pragma once

#include <rai/common/util.hpp>
#include <rai/node/message.hpp>

namespace rai
{

class MessageDumpEntry
{
public:
    rai::Ptree Get() const;

    bool send_;
    uint64_t timestamp_; // in ms
    std::string transport_; // udp or tcp
    std::string endpoint_;
    std::vector<uint8_t> bytes_;
    std::function<rai::Ptree(const std::vector<uint8_t>&)> parser_;
};

class MessageDumper
{
public:
    MessageDumper();
    void Dump(bool, const rai::Endpoint&, const std::vector<uint8_t>&);
    void Dump(bool, const rai::Endpoint&, const uint8_t*, size_t);
    void Dump(bool, const rai::Endpoint&, std::vector<uint8_t>&&);
    rai::Ptree Get() const;
    void On(const std::string&, const std::string&);
    void Off();

    static std::string ToString(rai::MessageType);
    static rai::Ptree ParseMessageNormal(const std::vector<uint8_t>&);

    static size_t constexpr MAX_SIZE = 16;
private:
    mutable std::mutex mutex_;
    bool on_;
    std::string type_;
    std::string ip_;
    uint32_t index_;
    std::vector<rai::MessageDumpEntry> messages_;
};

class Dumpers
{
public:
    MessageDumper message_;
};
}  // namespace rai
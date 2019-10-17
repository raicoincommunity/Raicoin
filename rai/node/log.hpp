#pragma once

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>

namespace rai
{
class Node;
class LogConfig
{
public:
    LogConfig();
    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;

    bool Network() const;
    bool NetworkSend() const;
    bool NetworkReceive() const;
    bool Message() const;
    bool MessageHandshake() const;
    bool Rpc() const;
    bool Log() const;
    bool LogToCerr() const;
    bool Flush() const;
    uintmax_t MaxSize() const;
    uintmax_t RotationSize() const;

private:
    bool network_;
    bool network_send_;
    bool network_receive_;
    bool message_;
    bool message_handshake_;
    bool rpc_;
    bool log_to_cerr_;
    bool flush_;
    uintmax_t max_size_;
    uintmax_t rotation_size_;
};

class Log
{
public:
    static void Init(const boost::filesystem::path&, const rai::LogConfig&);
    static void Error(rai::Node&, const std::string&);
    static void Network(rai::Node&, const std::string&);
    static void NetworkSend(rai::Node&, const std::string&);
    static void NetworkReceive(rai::Node&, const std::string&);
    static void Message(rai::Node&, const std::string&);
    static void MessageHandshake(rai::Node&, const std::string&);
    static void Rpc(rai::Node&, const std::string&);
};
} // namespace rai
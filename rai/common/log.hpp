#pragma once

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sources/logger.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>

namespace rai
{
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
    Log() = delete;

    static void Init(const boost::filesystem::path&, const rai::LogConfig&);
    static void Error(const std::string&);
    static void Network(const std::string&);
    static void NetworkSend(const std::string&);
    static void NetworkReceive(const std::string&);
    static void Message(const std::string&);
    static void MessageHandshake(const std::string&);
    static void Rpc(const std::string&);

    static rai::LogConfig config_;
    static boost::log::sources::logger_mt logger_;
};
} // namespace rai
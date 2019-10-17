#include <rai/node/log.hpp>

#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <rai/node/node.hpp>

rai::LogConfig::LogConfig()
    : network_(true),
      network_send_(false),
      network_receive_(false),
      message_(true),
      message_handshake_(false),
      rpc_(true),
      log_to_cerr_(false),
      max_size_(16 * 1024 * 1024),
      rotation_size_(4 * 1024 * 1024),
      flush_(true)
{
}

rai::ErrorCode rai::LogConfig::DeserializeJson(bool& upgraded, rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    try
    {
        error_code = rai::ErrorCode::JSON_CONFIG_LOG_VERSION;
        std::string version_str = ptree.get<std::string>("version");
        uint32_t version = 0;
        bool error = rai::StringToUint(version_str, version);
        IF_ERROR_RETURN(error, error_code);

        error_code = UpgradeJson(upgraded, version, ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_LOG_NETWORK;
        network_ = ptree.get<bool>("network");

        error_code = rai::ErrorCode::JSON_CONFIG_LOG_NETWORK_SEND;
        network_send_ = ptree.get<bool>("network_send");

        error_code = rai::ErrorCode::JSON_CONFIG_LOG_NETWORK_RECEIVE;
        network_receive_ = ptree.get<bool>("network_receive");

        error_code = rai::ErrorCode::JSON_CONFIG_LOG_MESSAGE;
        message_ = ptree.get<bool>("message");

        error_code = rai::ErrorCode::JSON_CONFIG_LOG_MESSAGE_HANDSHAKE;
        message_handshake_ = ptree.get<bool>("message_handshake");

        error_code = rai::ErrorCode::JSON_CONFIG_LOG_RPC;
        rpc_ = ptree.get<bool>("rpc");

        error_code = rai::ErrorCode::JSON_CONFIG_LOG_TO_CERR;
        log_to_cerr_ = ptree.get<bool>("log_to_cerr");

        error_code = rai::ErrorCode::JSON_CONFIG_LOG_MAX_SIZE;
        max_size_ = ptree.get<uintmax_t>("max_size");

        error_code = rai::ErrorCode::JSON_CONFIG_LOG_ROTATION_SIZE;
        rotation_size_ = ptree.get<uintmax_t>("rotation_size");

        error_code = rai::ErrorCode::JSON_CONFIG_LOG_FLUSH;
        flush_ = ptree.get<bool>("flush");
    }
    catch (const std::exception&)
    {
        return error_code;
    }
    return rai::ErrorCode::SUCCESS;
}

void rai::LogConfig::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("version", "1");
    ptree.put("network", network_);
    ptree.put("network_send", network_send_);
    ptree.put("network_receive", network_receive_);
    ptree.put("message", message_);
    ptree.put("message_handshake", message_handshake_);
    ptree.put("rpc", rpc_);
    ptree.put("log_to_cerr", log_to_cerr_);
    ptree.put("max_size", max_size_);
    ptree.put("rotation_size", rotation_size_);
    ptree.put("flush", flush_);
}

rai::ErrorCode rai::LogConfig::UpgradeJson(bool& upgraded, uint32_t version,
                                           rai::Ptree& ptree) const
{
    switch (version)
    {
        case 1:
        {
            break;
        }
        default:
        {
            return rai::ErrorCode::CONFIG_LOG_VERSION;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

bool rai::LogConfig::Network() const
{
    return network_;
}

bool rai::LogConfig::NetworkSend() const
{
    return Network() && network_send_;
}

bool rai::LogConfig::NetworkReceive() const
{
    return Network() && network_receive_;
}

bool rai::LogConfig::Message() const
{
    return message_;
}

bool rai::LogConfig::MessageHandshake() const
{
    return Message() && message_handshake_;
}

bool rai::LogConfig::Rpc() const
{
    return rpc_;
}

bool rai::LogConfig::LogToCerr() const
{
    return log_to_cerr_;
}

bool rai::LogConfig::Flush() const
{
    return flush_;
}

uintmax_t rai::LogConfig::MaxSize() const
{
    return max_size_;
}

uintmax_t rai::LogConfig::RotationSize() const
{
    return rotation_size_;
}

void rai::Log::Init(const boost::filesystem::path& path,
                    const rai::LogConfig& config)
{
    static std::atomic_flag flag = ATOMIC_FLAG_INIT;
    if (flag.test_and_set())
    {
        return;
    }
    boost::log::add_common_attributes();
    if (config.LogToCerr())
    {
        boost::log::add_console_log(
            std::cerr,
            boost::log::keywords::format = "[%TimeStamp%]: %Message%");
    }
    boost::log::add_file_log(
        boost::log::keywords::target = path / "log",
        boost::log::keywords::file_name =
            path / "log" / "log_%Y-%m-%d_%H-%M-%S.%N.log",
        boost::log::keywords::rotation_size = config.RotationSize(),
        boost::log::keywords::auto_flush    = config.Flush(),
        boost::log::keywords::scan_method =
            boost::log::sinks::file::scan_method::scan_matching,
        boost::log::keywords::max_size = config.MaxSize(),
        boost::log::keywords::format   = "[%TimeStamp%]: %Message%");
}

void rai::Log::Error(rai::Node& node, const std::string& str)
{
    BOOST_LOG(node.log_) << "[Error]" << str;
}

void rai::Log::Network(rai::Node& node, const std::string& str)
{
    if (node.config_.log_.Network())
    {
        BOOST_LOG(node.log_) << str;
    }
}

void rai::Log::NetworkSend(rai::Node& node, const std::string& str)
{
    if (node.config_.log_.NetworkSend())
    {
        BOOST_LOG(node.log_) << str;
    }
}

void rai::Log::NetworkReceive(rai::Node& node, const std::string& str)
{
    if (node.config_.log_.NetworkReceive())
    {
        BOOST_LOG(node.log_) << str;
    }
}

void rai::Log::Message(rai::Node& node, const std::string& str)
{
    if (node.config_.log_.Message())
    {
        BOOST_LOG(node.log_) << str;
    }
}

void rai::Log::MessageHandshake(rai::Node& node, const std::string& str)
{
    if (node.config_.log_.MessageHandshake())
    {
        BOOST_LOG(node.log_) << str;
    }
}

void rai::Log::Rpc(rai::Node& node, const std::string& str)
{
    if (node.config_.log_.Rpc())
    {
        BOOST_LOG(node.log_) << str;
    }
}
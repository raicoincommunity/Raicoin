#include <rai/node/config.hpp>

#include <rai/node/node.hpp>

rai::NodeConfig::NodeConfig()
    : address_(rai::IP::any()),
      port_(rai::Network::DEFAULT_PORT),
      io_threads_(std::max<uint32_t>(4, std::thread::hardware_concurrency())),
      daily_forward_times_(rai::NodeConfig::DEFAULT_DAILY_FORWARD_TIMES),
      election_concurrency_(rai::Elections::ELECTION_CONCURRENCY),
      enable_rich_list_(false),
      enable_delegator_list_(false)
{
    switch (rai::RAI_NETWORK)
    {
        case rai::RaiNetworks::TEST:
        {
            preconfigured_peers_.push_back("peers-test.raicoin.org");
            return;
        }
        case rai::RaiNetworks::BETA:
        {
            preconfigured_peers_.push_back("peers-beta.raicoin.org");
            break;
        }
        case rai::RaiNetworks::LIVE:
        {
            preconfigured_peers_.push_back("peers.raicoin.org");
            break;
        }
        default:
        {
            throw std::runtime_error("Unknown rai::RAI_NETWORK");
        }
    }
}


rai::ErrorCode rai::NodeConfig::DeserializeJson(bool& upgraded,
                                                rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    try
    {
        error_code = rai::ErrorCode::JSON_CONFIG_NODE_VERSION;
        std::string version_str = ptree.get<std::string>("version");
        uint32_t version = 0;
        bool error = rai::StringToUint(version_str, version);
        IF_ERROR_RETURN(error, error_code);

        error_code = UpgradeJson(upgraded, version, ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_NODE_ADDRESS;
        std::string address = ptree.get<std::string>("address");
        boost::system::error_code ec;
        address_ = boost::asio::ip::make_address_v4(address, ec);
        if (ec)
        {
            return error_code;
        }

        error_code = rai::ErrorCode::JSON_CONFIG_NODE_PORT;
        port_ = ptree.get<uint16_t>("port");

        error_code = rai::ErrorCode::JSON_CONFIG_NODE_IO_THREADS;
        io_threads_ = ptree.get<uint32_t>("io_threads");
        io_threads_ = (0 == io_threads_) ? 1 : io_threads_;

        error_code = rai::ErrorCode::JSON_CONFIG_LOG;
        rai::Ptree& log_ptree = ptree.get_child("log");
        error_code = log_.DeserializeJson(upgraded, log_ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_PRECONFIGURED_PEERS;
        preconfigured_peers_.clear();
        auto preconfigured_peers = ptree.get_child("preconfigured_peers");
        for (const auto& i : preconfigured_peers)
        {
            preconfigured_peers_.push_back(i.second.get<std::string>(""));
        }

        error_code = rai::ErrorCode::JSON_CONFIG_CALLBACK_URL;
        auto callback_url = ptree.get_optional<std::string>("callback_url");
        if (callback_url && !callback_url->empty())
        {
           bool error = callback_url_.Parse(*callback_url);
           IF_ERROR_RETURN(error, error_code);
        }

        error_code = rai::ErrorCode::JSON_CONFIG_REWARD_TO;
        std::string forward_to = ptree.get<std::string>("forward_reward_to");
        error = forward_reward_to_.DecodeAccount(forward_to);
        IF_ERROR_RETURN(error, error_code);
        if (forward_reward_to_.IsZero())
        {
            return rai::ErrorCode::REWARD_TO_ACCOUNT;
        }

        error_code = rai::ErrorCode::JSON_CONFIG_DAILY_REWARD_TIMES;
        auto forward_times = ptree.get_optional<uint32_t>("daily_forward_times");
        daily_forward_times_ =
            forward_times ? *forward_times
                          : rai::NodeConfig::DEFAULT_DAILY_FORWARD_TIMES;

        error_code = rai::ErrorCode::JSON_CONFIG_ELECTION_CONCURRENCY;
        auto election_concurrency =
            ptree.get_optional<uint32_t>("election_concurrency");
        election_concurrency_ = election_concurrency
                                    ? *election_concurrency
                                    : rai::Elections::ELECTION_CONCURRENCY;

        error_code = rai::ErrorCode::JSON_CONFIG_ENABLE_RICH_LIST;
        auto enable_rich_list_o = ptree.get_optional<bool>("enable_rich_list");
        if (enable_rich_list_o)
        {
            enable_rich_list_ = *enable_rich_list_o;
        }

        error_code = rai::ErrorCode::JSON_CONFIG_ENABLE_DELEGATOR_LIST;
        auto enable_delegator_list_o =
            ptree.get_optional<bool>("enable_delegator_list");
        if (enable_delegator_list_o)
        {
            enable_delegator_list_ = *enable_delegator_list_o;
        }
    }
    catch (const std::exception&)
    {
        return error_code;
    }
    return rai::ErrorCode::SUCCESS;
}

void rai::NodeConfig::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("version", "5");
    ptree.put("address", address_.to_string());
    ptree.put("port", port_);
    ptree.put("io_threads", io_threads_);
    rai::Ptree log_ptree;
    log_.SerializeJson(log_ptree);
    ptree.add_child("log", log_ptree);
    rai::Ptree preconfigured_peers;
    for (const auto& i : preconfigured_peers_)
    {
        boost::property_tree::ptree entry;
        entry.put("", i);
        preconfigured_peers.push_back(std::make_pair("", entry));
    }
    ptree.add_child("preconfigured_peers", preconfigured_peers);
    ptree.put("callback_url", callback_url_.String());
    ptree.put("forward_reward_to", forward_reward_to_.StringAccount());
    ptree.put("daily_forward_times", std::to_string(daily_forward_times_));
    ptree.put("election_concurrency", std::to_string(election_concurrency_));
    ptree.put("enable_rich_list", enable_rich_list_);
    ptree.put("enable_delegator_list", enable_delegator_list_);
}

rai::ErrorCode rai::NodeConfig::UpgradeJson(bool& upgraded, uint32_t version,
                                            rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    switch (version)
    {
        case 1:
        {
            upgraded = true;
            error_code = UpgradeV1V2(ptree);
            IF_NOT_SUCCESS_RETURN(error_code);
        }
        case 2:
        {
            upgraded = true;
            error_code = UpgradeV2V3(ptree);
            IF_NOT_SUCCESS_RETURN(error_code);
        }
        case 3:
        {
            upgraded = true;
            error_code = UpgradeV3V4(ptree);
            IF_NOT_SUCCESS_RETURN(error_code);
        }
        case 4:
        {
            upgraded = true;
            error_code = UpgradeV4V5(ptree);
            IF_NOT_SUCCESS_RETURN(error_code);
        }
        case 5:
        {
            break;
        }
        default:
        {
            return rai::ErrorCode::CONFIG_NODE_VERSION;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::NodeConfig::UpgradeV1V2(rai::Ptree& ptree) const
{
    ptree.put("version", 2);

    auto reward_to_o = ptree.get_optional<std::string>("reward_to");
    if (reward_to_o)
    {
        ptree.put("forward_reward_to", *reward_to_o);
        ptree.erase("reward_to");
    }

    auto daily_reward_times_o =
        ptree.get_optional<std::string>("daily_reward_times");
    if (daily_reward_times_o)
    {
        ptree.put("daily_forward_times", *daily_reward_times_o);
        ptree.erase("daily_reward_times");
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::NodeConfig::UpgradeV2V3(rai::Ptree& ptree) const
{
    ptree.put("version", 3);

    ptree.put("enable_rich_list", enable_rich_list_);
    ptree.put("enable_delegator_list", enable_delegator_list_);

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::NodeConfig::UpgradeV3V4(rai::Ptree& ptree) const
{
    ptree.put("version", 4);

    ptree.put("address", address_.to_string());

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::NodeConfig::UpgradeV4V5(rai::Ptree& ptree) const
{
    ptree.put("version", 5);

    ptree.put("election_concurrency", std::to_string(election_concurrency_));

    return rai::ErrorCode::SUCCESS;
}
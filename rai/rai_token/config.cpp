#include <rai/rai_token/config.hpp>

rai::TokenConfig::TokenConfig()
    : rpc_enable_(true),
      rpc_{boost::asio::ip::address_v4::any(),
           rai::TokenConfig::DEFAULT_RPC_PORT,
           false,
           {boost::asio::ip::address_v4::loopback()}},
      app_(rai::TokenConfig::DEFAULT_WS_PORT)
{
}

rai::ErrorCode rai::TokenConfig::DeserializeJson(bool& upgraded,
                                                 rai::Ptree& ptree)
{
    if (ptree.empty())
    {
        upgraded = true;
        SerializeJson(ptree);
        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    try
    {
        error_code = rai::ErrorCode::JSON_CONFIG_VERSION;
        std::string version_str = ptree.get<std::string>("version");
        uint32_t version = 0;
        bool error = rai::StringToUint(version_str, version);
        IF_ERROR_RETURN(error, error_code);

        error_code = UpgradeJson(upgraded, version, ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_RPC;
        const rai::Ptree& rpc_ptree = ptree.get_child("rpc");

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_ENABLE;
        rpc_enable_ = rpc_ptree.get<bool>("enable");

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_ADDRESS;
        std::string address = rpc_ptree.get<std::string>("address");
        boost::system::error_code ec;
        rpc_.address_ = boost::asio::ip::make_address_v4(address, ec);
        if (ec)
        {
            return error_code;
        }

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_PORT;
        std::string port = rpc_ptree.get<std::string>("port");
        if (port.empty() || rai::StringToUint(port, rpc_.port_))
        {
            return error_code;
        }

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_ENABLE_CONTROL;
        rpc_.enable_control_ = rpc_ptree.get<bool>("enable_control");

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_WHITELIST;
        rpc_.whitelist_.clear();
        auto whitelist = rpc_ptree.get_child("whitelist");
        for (const auto& i : whitelist)
        {
            address = i.second.get<std::string>("");
            boost::asio::ip::address_v4 ip =
                boost::asio::ip::make_address_v4(address, ec);
            if (ec)
            {
                return error_code;
            }
            rpc_.whitelist_.push_back(ip);
        }

        error_code = rai::ErrorCode::JSON_CONFIG_APP;
        rai::Ptree& app_ptree = ptree.get_child("app");
        error_code = app_.DeserializeJson(upgraded, app_ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_LOG;
        rai::Ptree& log_ptree = ptree.get_child("log");
        error_code = log_.DeserializeJson(upgraded, log_ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_CROSS_CHAIN;
        rai::Ptree& cross_chain_ptree = ptree.get_child("cross_chain");
        error_code = cross_chain_.DeserializeJson(upgraded, cross_chain_ptree);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    catch (const std::exception&)
    {
        return error_code;
    }
    return rai::ErrorCode::SUCCESS;
}

void rai::TokenConfig::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("version", "2");

    rai::Ptree rpc;
    rpc.put("enable", rpc_enable_);
    rpc.put("address", rpc_.address_.to_string());
    rpc.put("port", std::to_string(rpc_.port_));
    rpc.put("enable_control", rpc_.enable_control_);
    boost::property_tree::ptree whitelist;
    for (const auto& i : rpc_.whitelist_)
    {
        boost::property_tree::ptree entry;
        entry.put("", i.to_string());
        whitelist.push_back(std::make_pair("", entry));
    }
    rpc.add_child("whitelist", whitelist);
    ptree.add_child("rpc", rpc);

    rai::Ptree app;
    app_.SerializeJson(app);
    ptree.add_child("app", app);

    rai::Ptree log;
    log_.SerializeJson(log);
    ptree.add_child("log", log);

    rai::Ptree cross_chain;
    cross_chain_.SerializeJson(cross_chain);
    ptree.add_child("cross_chain", cross_chain);
}

rai::ErrorCode rai::TokenConfig::UpgradeJson(bool& upgraded, uint32_t version,
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
            break;
        }
        default:
        {
            return rai::ErrorCode::CONFIG_VERSION;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::TokenConfig::UpgradeV1V2(rai::Ptree& ptree) const
{
    ptree.put("version", 2);

    rai::Ptree cross_chain;
    cross_chain_.SerializeJson(cross_chain);
    ptree.add_child("cross_chain", cross_chain);

    return rai::ErrorCode::SUCCESS;
}
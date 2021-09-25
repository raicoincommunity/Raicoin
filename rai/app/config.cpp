#include <rai/app/config.hpp>

#include <rai/common/parameters.hpp>

namespace
{
std::string DefaultNodeGateway()
{
    switch (rai::RAI_NETWORK)
    {
        case rai::RaiNetworks::TEST:
        {
            return "wss://node-gateway-test.raicoin.org";
        }
        case rai::RaiNetworks::BETA:
        {
            return "wss://node-gateway-beta.raicoin.org";
        }
        case rai::RaiNetworks::LIVE:
        {
            return "wss://node-gateway.raicoin.org";
        }
        default:
        {
            throw std::runtime_error("Unknown rai::RAI_NETWORK");
        }
    }
}
}

rai::AppConfig::AppConfig(uint16_t ws_port)
    : node_gateway_("wss"),
      ws_ip_(boost::asio::ip::address_v4::loopback()),
      ws_port_(ws_port)
{
    bool error = node_gateway_.Parse(DefaultNodeGateway());
    if (error)
    {
        throw std::runtime_error("Invalid default node gateway");
    }
}

rai::ErrorCode rai::AppConfig::DeserializeJson(bool& update,
                                                  rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_CONFIG_APP_VERSION;
        std::string version_str = ptree.get<std::string>("version");
        uint32_t version = 0;
        bool error = rai::StringToUint(version_str, version);
        IF_ERROR_RETURN(error, error_code);

        error_code = UpgradeJson(update, version, ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_NODE_GATEWAY;
        auto node_gateway = ptree.get_optional<std::string>("node_gateway");
        if (!node_gateway || node_gateway_.Parse(*node_gateway))
        {
            return error_code;
        }

        error_code = rai::ErrorCode::JSON_CONFIG_WS_ADDRESS;
        std::string address = ptree.get<std::string>("websocket_address");
        boost::system::error_code ec;
        ws_ip_ = boost::asio::ip::make_address_v4(address, ec);
        if (ec)
        {
            return error_code;
        }

        error_code = rai::ErrorCode::JSON_CONFIG_WS_PORT;
        std::string ws_port_str = ptree.get<std::string>("websocket_port");
        if (ws_port_str.empty() || rai::StringToUint(ws_port_str, ws_port_))
        {
            return error_code;
        }
    }
    catch (...)
    {
        return error_code;
    }
    return rai::ErrorCode::SUCCESS;
}

void rai::AppConfig::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("version", "1");
    ptree.put("node_gateway", node_gateway_.String());
    ptree.put("websocket_address", ws_ip_.to_string());
    ptree.put("websocket_port", std::to_string(ws_port_));
}

rai::ErrorCode rai::AppConfig::UpgradeJson(bool& update, uint32_t version,
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
            return rai::ErrorCode::CONFIG_APP_VERSION;
        }
    }

    return rai::ErrorCode::SUCCESS;
}
#pragma once

#include <rai/common/util.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/log.hpp>
#include <rai/secure/rpc.hpp>
#include <rai/app/config.hpp>

namespace rai
{
class AliasConfig
{
public:
    AliasConfig();
    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;

    static uint16_t constexpr DEFAULT_WS_PORT =
        rai::RAI_NETWORK == rai::RaiNetworks::LIVE ? 7183 : 54308;
    static uint16_t constexpr DEFAULT_RPC_PORT =
        rai::RAI_NETWORK == rai::RaiNetworks::LIVE ? 7184 : 54309;

    bool rpc_enable_;
    rai::RpcConfig rpc_;
    rai::AppConfig app_;
    rai::LogConfig log_;
};

}
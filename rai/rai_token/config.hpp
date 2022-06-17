#pragma once

#include <rai/common/util.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/log.hpp>
#include <rai/secure/rpc.hpp>
#include <rai/app/config.hpp>
#include <rai/rai_token/crosschain/config.hpp>

namespace rai
{
class TokenConfig
{
public:
    TokenConfig();
    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;
    rai::ErrorCode UpgradeV1V2(rai::Ptree&) const;

    static uint16_t constexpr DEFAULT_WS_PORT =
        rai::RAI_NETWORK == rai::RaiNetworks::LIVE ? 7185 : 54310;
    static uint16_t constexpr DEFAULT_RPC_PORT =
        rai::RAI_NETWORK == rai::RaiNetworks::LIVE ? 7186 : 54311;

    bool rpc_enable_;
    rai::RpcConfig rpc_;
    rai::AppConfig app_;
    rai::LogConfig log_;
    rai::CrossChainConfig cross_chain_;
};

}
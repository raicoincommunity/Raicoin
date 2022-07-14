#pragma once

#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/common/log.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/chain.hpp>

namespace rai
{
class NodeConfig
{
public:
    NodeConfig();

    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;
    rai::ErrorCode UpgradeV1V2(rai::Ptree&) const;
    rai::ErrorCode UpgradeV2V3(rai::Ptree&) const;
    rai::ErrorCode UpgradeV3V4(rai::Ptree&) const;
    rai::ErrorCode UpgradeV4V5(rai::Ptree&) const;
    rai::ErrorCode UpgradeV5V6(rai::Ptree&) const;

    static uint32_t constexpr DEFAULT_DAILY_FORWARD_TIMES = 12;

    rai::IP address_;
    uint16_t port_;
    rai::LogConfig log_;
    uint32_t io_threads_;
    std::vector<std::string> preconfigured_peers_;
    rai::Url callback_url_;
    rai::Account forward_reward_to_;
    uint32_t daily_forward_times_;
    uint32_t election_concurrency_;
    bool enable_rich_list_;
    bool enable_delegator_list_;
    rai::Url validator_url_;
};

}
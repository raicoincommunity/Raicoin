#pragma once

#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/common/numbers.hpp>

namespace rai
{
class AppConfig
{
public:
    AppConfig();

    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;

    rai::Url node_gateway_;
    rai::Url callback_;
};
}
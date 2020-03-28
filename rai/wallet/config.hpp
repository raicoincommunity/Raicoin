#pragma once
#include <string>

#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/common/numbers.hpp>

namespace rai
{
class WalletConfig
{
public:
    WalletConfig();

    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;

    rai::Url server_;
    std::vector<rai::Account> preconfigured_reps_;
};
};
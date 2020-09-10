#pragma once

#include <rai/wallet/wallet.hpp>

namespace rai
{
class QtWalletConfig
{
public:
    QtWalletConfig();
    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;
    rai::ErrorCode UpgradeV1V2(rai::Ptree&) const;

    bool auto_receive_;
    rai::WalletConfig wallet_;
};
}
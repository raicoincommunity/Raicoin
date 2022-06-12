#pragma once

#include <rai/common/util.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/chain.hpp>

namespace rai
{
class CrossChainConfig
{
public:
    CrossChainConfig();

    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;
    

    std::vector<rai::Url> eth_endpoints_;
    std::vector<rai::Url> bsc_endpoints_;
    std::vector<rai::Url> eth_test_goerli_endpoints_;
    std::vector<rai::Url> bsc_test_endpoints_;

private:
    bool DeserializeEndpoints_(bool&, rai::Ptree&, std::vector<rai::Url>&);
};
}
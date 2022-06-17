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
    

    std::vector<rai::Url> eth_urls_;
    std::vector<rai::Url> bsc_urls_;
    std::vector<rai::Url> eth_test_goerli_urls_;
    std::vector<rai::Url> bsc_test_urls_;

private:
    rai::ErrorCode DeserializeEndpoints_(const rai::Ptree&,
                                         std::vector<rai::Url>&) const;
    void SerializeEndpoints_(const std::vector<rai::Url>&, rai::Ptree&) const;
    rai::ErrorCode MakeUrls_(const std::vector<std::string>&,
                             std::vector<rai::Url>&) const;
};
}
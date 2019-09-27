#pragma once

#include <boost/filesystem.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/node/node.hpp>
#include <rai/node/rpc.hpp>

namespace rai
{
class Daemon
{
public:
    rai::ErrorCode Run(const boost::filesystem::path&,
                       const boost::filesystem::path&);
};

class DaemonConfig
{
public:
    DaemonConfig(const boost::filesystem::path&);
    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;

    rai::NodeConfig node_;
    rai::RpcConfig rpc_;
};
}
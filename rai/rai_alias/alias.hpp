#pragma once

#include <rai/common/errors.hpp>
#include <rai/app/app.hpp>
#include <rai/rai_alias/config.hpp>
#include <rai/rai_alias/subscribe.hpp>
#include <rai/app/provider.hpp>

namespace rai
{
class Alias : public rai::App
{
public:
    Alias(rai::ErrorCode&, boost::asio::io_service&,
          const boost::filesystem::path&, rai::Alarm&, const rai::AliasConfig&);
    virtual ~Alias() = default;

    std::shared_ptr<rai::Alias> Shared();

    static std::vector<rai::BlockType> BlockTypes();
    static rai::Provider::Info Provide();

    boost::asio::io_service& service_;
    rai::Alarm& alarm_;
    const rai::AliasConfig& config_;
    std::shared_ptr<rai::Rpc> rpc_;
    rai::AliasSubscriptions subscribe_;
};
}
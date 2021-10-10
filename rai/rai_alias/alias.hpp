#pragma once

#include <rai/common/errors.hpp>
#include <rai/common/extensions.hpp>
#include <rai/app/app.hpp>
#include <rai/app/provider.hpp>
#include <rai/rai_alias/config.hpp>
#include <rai/rai_alias/subscribe.hpp>
#include <rai/rai_alias/rpc.hpp>

namespace rai
{
class Alias : public rai::App
{
public:
    Alias(rai::ErrorCode&, boost::asio::io_service&,
          const boost::filesystem::path&, rai::Alarm&, const rai::AliasConfig&);
    virtual ~Alias() = default;

    rai::ErrorCode PreBlockAppend(rai::Transaction&,
                                  const std::shared_ptr<rai::Block>&,
                                  bool) override;
    rai::ErrorCode AfterBlockAppend(rai::Transaction&,
                                    const std::shared_ptr<rai::Block>&,
                                    bool) override;
    rai::ErrorCode PreBlockRollback(
        rai::Transaction&, const std::shared_ptr<rai::Block>&) override;
    rai::ErrorCode AfterBlockRollback(
        rai::Transaction&, const std::shared_ptr<rai::Block>&) override;
    std::shared_ptr<rai::AppRpcHandler> MakeRpcHandler(
        const rai::UniqueId&, bool, const std::string&,
        const std::function<void(const rai::Ptree&)>&) override;

    std::shared_ptr<rai::Alias> Shared();
    rai::RpcHandlerMaker RpcHandlerMaker();

    void Start() override;
    void Stop() override;
    rai::ErrorCode Process(rai::Transaction&,
                           const std::shared_ptr<rai::Block>&,
                           const rai::Extensions&);

    static std::vector<rai::BlockType> BlockTypes();
    static rai::Provider::Info Provide();

    boost::asio::io_service& service_;
    rai::Alarm& alarm_;
    const rai::AliasConfig& config_;
    std::shared_ptr<rai::Rpc> rpc_;
    std::shared_ptr<rai::OngoingServiceRunner> runner_;
    rai::AliasSubscriptions subscribe_;
};
}
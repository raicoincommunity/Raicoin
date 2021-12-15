#pragma once

#include <rai/common/errors.hpp>
#include <rai/common/extensions.hpp>
#include <rai/app/app.hpp>
#include <rai/app/provider.hpp>
#include <rai/rai_token/config.hpp>
#include <rai/rai_token/subscribe.hpp>
#include <rai/rai_token/rpc.hpp>

namespace rai
{
class TokenObservers
{
public:
};


class Token : public rai::App
{
public:
    Token(rai::ErrorCode&, boost::asio::io_service&,
          const boost::filesystem::path&, rai::Alarm&, const rai::TokenConfig&);
    virtual ~Token() = default;

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

    std::shared_ptr<rai::Token> Shared();
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
    const rai::TokenConfig& config_;
    rai::TokenObservers observers_;
    std::shared_ptr<rai::Rpc> rpc_;
    std::shared_ptr<rai::OngoingServiceRunner> runner_;
    rai::TokenSubscriptions subscribe_;

private:
    static uint32_t constexpr CURRENT_LEDGER_VERSION = 1;
    rai::ErrorCode InitLedger_();
};

};
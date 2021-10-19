#pragma once

#include <rai/common/numbers.hpp>
#include <rai/secure/rpc.hpp>

namespace rai
{
class App;

class AppRpcHandler : public RpcHandler
{
public:
    AppRpcHandler(rai::App&, const rai::UniqueId&, bool, rai::Rpc&,
                  const std::string&, const boost::asio::ip::address_v4&,
                  const std::function<void(const rai::Ptree&)>&);
    virtual ~AppRpcHandler() = default;

    virtual void Stop() = 0;

    void ProcessImpl() override;
    void ExtraCheck(const std::string&) override;

    void AccountCount();
    void AccountInfo();
    void AccountSync();
    void AppActionCount();
    void AppTraceOn();
    void AppTraceOff();
    void AppTraceStatus();
    void BlockCacheCount();
    void BlockConfirmCount();
    void BlockCount();
    void BlockQuery();
    void BlockQueryByPrevious();
    void BlockQueryByHash();
    void BlockQueryByHeight();
    void BootstrapStatus();
    void Clients();
    void ServiceSubscribe();
    void Stats();
    void StatsVerbose();
    void StatsClear();
    void Subscription();
    void Subscriptions();
    void SubscriptionCount();
    void SubscriptionAccountCount();

    rai::App& app_;
    rai::UniqueId uid_;
    bool check_;

protected:
    bool GetService_(std::string&);
    bool GetFilters_(std::vector<std::pair<std::string, std::string>>&);

};

}  // namespace rai
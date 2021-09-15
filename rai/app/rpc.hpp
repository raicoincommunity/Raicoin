#pragma once

#include <rai/secure/rpc.hpp>

namespace rai
{
class App;

class AppRpcHandler : public RpcHandler
{
public:
    AppRpcHandler(rai::App&, rai::Rpc&, const std::string&,
                  const boost::asio::ip::address_v4&,
                  const std::function<void(const rai::Ptree&)>&);
    virtual ~AppRpcHandler() = default;

    virtual void Stop() = 0;

    void ProcessImpl() override;

    void AccountCount();
    void AccountInfo();
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

    rai::App& app_;
};

}  // namespace rai
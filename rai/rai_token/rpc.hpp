#pragma once

#include <rai/secure/ledger.hpp>
#include <rai/app/rpc.hpp>

namespace rai
{
class Token;
class TokenRpcHandler : public AppRpcHandler
{
public:
    TokenRpcHandler(rai::Token&, const rai::UniqueId&, bool, rai::Rpc&,
                    const std::string&, const boost::asio::ip::address_v4&,
                    const std::function<void(const rai::Ptree&)>&);
    virtual ~TokenRpcHandler() = default;

    void ProcessImpl() override;

    void Stop() override;

    void LedgerVersion();

private:
    rai::Token& token_;
};

}  // namespace rai
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

    void AccountTokenLink();
    void AccountTokenInfo();
    void AccountTokensInfo();
    void LedgerVersion();
    void NextAccountTokenLinks();
    void NextTokenBlocks();
    void PreviousAccountTokenLinks();
    void PreviousTokenBlocks();
    void TokenBlock();
    void TokenIdInfo();
    void TokenInfo();
    void TokenMaxId();
    void TokenReceivables();

private:
    bool GetChain_(rai::Chain&);
    bool GetTokenAddress_(rai::Chain, rai::TokenAddress&);
    bool GetTokenId_(rai::TokenValue&);
    bool PutTokenBlock_(rai::Transaction&, uint64_t, const rai::TokenBlock&,
                        rai::Ptree&);

    rai::Token& token_;
};

}  // namespace rai
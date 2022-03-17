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

    void AccountSwapInfo();
    void AccountTokenLink();
    void AccountTokenInfo();
    void AccountTokenIds();
    void AccountTokensInfo();
    void LedgerVersion();
    void NextAccountTokenLinks();
    void NextTokenBlocks();
    void OrderCount();
    void PreviousAccountTokenLinks();
    void PreviousTokenBlocks();
    void ReceivableTokens();
    void SearchOrders();
    void SearchOrdersById();
    void SearchOrdersByPair();
    void TokenBlock();
    void TokenIdInfo();
    void TokenInfo();
    void TokenMaxId();
    void TokenReceivables();
    void TokenReceivablesAll();
    void TokenReceivablesSummary();

private:
    bool GetChain_(rai::Chain&);
    bool GetTokenAddress_(rai::Chain, rai::TokenAddress&);
    bool GetTokenId_(rai::TokenValue&, const std::string& = "token_id");
    bool GetTokens_(std::vector<rai::TokenKey>&);
    bool GetToken_(const std::string&, rai::ErrorCode, rai::ErrorCode,
                   rai::TokenKey&, rai::TokenType&);
    bool PutTokenBlock_(rai::Transaction&, uint64_t, const rai::TokenBlock&,
                        rai::Ptree&);

    rai::Token& token_;
};

}  // namespace rai
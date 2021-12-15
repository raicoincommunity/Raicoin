#include <rai/rai_token/rpc.hpp>

#include <rai/rai_token/token.hpp>

rai::TokenRpcHandler::TokenRpcHandler(
    rai::Token& token, const rai::UniqueId& uid, bool check, rai::Rpc& rpc,
    const std::string& body, const boost::asio::ip::address_v4& ip,
    const std::function<void(const rai::Ptree&)>& send_response)
    : rai::AppRpcHandler(token, uid, check, rpc, body, ip, send_response),
      token_(token)
{
}

void rai::TokenRpcHandler::ProcessImpl()
{
    std::string action = request_.get<std::string>("action");

    if (action == "ledger_version")
    {
        LedgerVersion();
    }
    else if (action == "stop")
    {
        if (!CheckLocal_())
        {
            Stop();
        }
    }
    else
    {
        AppRpcHandler::ProcessImpl();
    }
}

void rai::TokenRpcHandler::Stop()
{
    token_.Stop();
    response_.put("success", "");
}

void rai::TokenRpcHandler::LedgerVersion()
{
    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    uint32_t version = 0;
    bool error = token_.ledger_.VersionGet(transaction, version);
    if (error)
    {
        error_code_ = rai::ErrorCode::TOKEN_LEDGER_GET;
        return;
    }

    response_.put("verison", std::to_string(version));
}
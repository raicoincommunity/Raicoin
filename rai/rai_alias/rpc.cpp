#include <rai/rai_alias/rpc.hpp>

#include <rai/common/stat.hpp>
#include <rai/rai_alias/alias.hpp>

rai::AliasRpcHandler::AliasRpcHandler(
    rai::Alias& alias, const rai::UniqueId& uid, bool check, rai::Rpc& rpc,
    const std::string& body, const boost::asio::ip::address_v4& ip,
    const std::function<void(const rai::Ptree&)>& send_response)
    : rai::AppRpcHandler(alias, uid, check, rpc, body, ip, send_response),
      alias_(alias)
{
}

void rai::AliasRpcHandler::ProcessImpl()
{
    std::string action = request_.get<std::string>("action");

    // todo:
    if (action == "alias_query")
    {
        AliasQuery();
    }
    else if (action == "alias_search")
    {
        AliasSearch();
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

void rai::AliasRpcHandler::Stop()
{
    alias_.Stop();
}

void rai::AliasRpcHandler::AliasQuery()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    rai::Transaction transaction(error_code_, alias_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::AliasInfo info;
    error = alias_.ledger_.AliasInfoGet(transaction, account, info);
    if (error)
    {
        response_.put("name", "");
        response_.put("dns", "");
        return;
    }

    response_.put("name", info.name_);
    response_.put("dns", info.dns_);
}

void rai::AliasRpcHandler::AliasSearch()
{
    std::string dns;
    std::string name;
    auto dns_o = request_.get_optional<std::string>("dns_");
    auto name_o = request_.get_optional<std::string>("name_");
    if (!dns_o && !name_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_NAME;
        return;
    }
    else if (dns_o && name_o)
    {
        dns = *dns_o;
        name = *name_o;
        if (dns.empty() && name.empty())
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_NAME;
            return;
        }
    }
    else if (dns_o && !name_o)
    {
        dns = *dns_o;
        if (dns.empty())
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_DNS;
            return;
        }
    }
    else
    {
        name = *name_o;
        if (name.empty())
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_NAME;
            return;
        }
    }
    
    uint64_t count = 0;
    bool error = GetCount_(count);
    IF_ERROR_RETURN_VOID(error);
    if (count == 0 || count > 100)
    {
        count = 100;
    }

    if (dns.empty())
    {
        AliasSearchByName_(name, count);
    }
    else
    {
        AliasSearchByDns_(dns, name, count);
    }
}

rai::AliasRpcHandler::SearchEntry::SearchEntry(const rai::Account& account,
                                               const std::vector<uint8_t>& name,
                                               const std::vector<uint8_t>& dns)
    : account_(account),
      name_(name.begin(), name.end()),
      dns_(dns.begin(), dns.end())
{
}

void rai::AliasRpcHandler::AliasSearchByDns_(const std::string& dns,
                                             const std::string& name,
                                             uint64_t count)
{
    rai::AliasRpcHandler::SearchResult exact_result;
    rai::AliasRpcHandler::SearchResult prefix_result;

    do
    {
        rai::Transaction transaction(error_code_, alias_.ledger_, false);
        IF_NOT_SUCCESS_RETURN_VOID(error_code_);

        if (dns.size() < sizeof(rai::Prefix))
        {
            rai::Prefix prefix(dns);
            rai::Iterator i =
                alias_.ledger_.AliasDnsIndexLowerBound(transaction, prefix);
            rai::Iterator n =
                alias_.ledger_.AliasDnsIndexUpperBound(transaction, prefix);
            for (; i != n; ++i)
            {
                rai::AliasIndex index;
                bool error = alias_.ledger_.AliasDnsIndexGet(i, index);
                if (error)
                {
                    std::string error_info = rai::ToString(
                        "AliasRpcHandler::AliasSearchByDns_: AliasDnsIndexGet "
                        "failed, dns=",
                        dns);
                    rai::Log::Error(error_info);
                    error_code_ = rai::ErrorCode::ALIAS_LEDGER_GET;
                    rai::Stats::Add(error_code_, error_info);
                    return;
                }

                rai::AliasInfo info;
                error = alias_.ledger_.AliasInfoGet(transaction, index.account_,
                                                    info);
                if (error)
                {
                    std::string error_info = rai::ToString(
                        "AliasRpcHandler::AliasSearchByDns_: AliasInfoGet "
                        "failed, account=",
                        index.account_.StringAccount());
                    rai::Log::Error(error_info);
                    error_code_ = rai::ErrorCode::ALIAS_LEDGER_GET;
                    rai::Stats::Add(error_code_, error_info);
                    return;
                }

                rai::AccountInfo account_info;
                error = alias_.ledger_.AccountInfoGet(
                    transaction, index.account_, account_info);
                // todo:
                rai::AliasRpcHandler::SearchEntry entry(index.account_,
                                                        info.name_, info.dns_);
            }
        }

    } while (0);


}

void rai::AliasRpcHandler::AliasSearchByName_(const std::string& name,
                                              uint64_t count)
{
}

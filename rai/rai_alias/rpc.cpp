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

    if (action == "alias_query")
    {
        AliasQuery();
    }
    else if (action == "alias_search")
    {
        AliasSearch();
    }
    else if (action == "ledger_version")
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

void rai::AliasRpcHandler::Stop()
{
    alias_.Stop();
    response_.put("success", "");
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

    response_.put("name", std::string(info.name_.begin(), info.name_.end()));
    response_.put("dns", std::string(info.dns_.begin(), info.dns_.end()));
}

void rai::AliasRpcHandler::AliasSearch()
{
    std::string dns;
    std::string name;
    auto dns_o = request_.get_optional<std::string>("dns");
    auto name_o = request_.get_optional<std::string>("name");
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

    if (error_code_ == rai::ErrorCode::SUCCESS)
    {
        response_.put("name", name);
        response_.put("dns", dns);
    }
}

void rai::AliasRpcHandler::LedgerVersion()
{
    rai::Transaction transaction(error_code_, alias_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    uint32_t version = 0;
    bool error = alias_.ledger_.VersionGet(transaction, version);
    if (error)
    {
        error_code_ = rai::ErrorCode::ALIAS_LEDGER_GET;
        return;
    }

    response_.put("verison", std::to_string(version));
}


rai::AliasRpcHandler::SearchEntry::SearchEntry(const rai::Account& account,
                                               const std::vector<uint8_t>& name,
                                               const std::vector<uint8_t>& dns)
    : account_(account),
      name_(name.begin(), name.end()),
      dns_(dns.begin(), dns.end())
{
}

rai::Ptree rai::AliasRpcHandler::SearchEntry::Json() const
{
    rai::Ptree ptree;
    ptree.put("account", account_.StringAccount());
    ptree.put("name", name_);
    ptree.put("dns", dns_);
    return ptree;
}

void rai::AliasRpcHandler::AliasSearchByDns_(const std::string& dns,
                                             const std::string& name,
                                             uint64_t count)
{
    rai::AliasRpcHandler::SearchResult exact_result;
    rai::AliasRpcHandler::SearchResult prefix_result;
    std::unordered_map<rai::Account, int32_t> account_count;
    rai::Prefix prefix(dns);

    rai::Transaction transaction(error_code_, alias_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    if (dns.size() < sizeof(rai::Prefix))
    {
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
                error_code_ = rai::ErrorCode::ALIAS_LEDGER_GET;
                rai::Log::Error(error_info);
                rai::Stats::Add(error_code_, error_info);
                return;
            }

            uint16_t credit;
            SearchEntry entry;
            error = GetSearchEntry_(transaction, index.account_, credit, entry);
            IF_ERROR_RETURN_VOID(error);
            if (!rai::StringStartsWith(entry.name_, name, true)) continue;

            exact_result.emplace(credit, entry);
            if (account_count.find(index.account_) == account_count.end())
            {
                account_count[index.account_] = 0;
            }
            account_count[index.account_] += 1;

            if (exact_result.size() > count)
            {
                auto it = exact_result.erase(std::prev(exact_result.end()));
                account_count[it->second.account_] -= 1;
            }
        }
    }

    if (exact_result.size() < count)
    {
        uint64_t left = count - exact_result.size();
        size_t searchs = 0;
        rai::Iterator i =
            alias_.ledger_.AliasDnsIndexLowerBound(transaction, prefix);
        rai::Iterator n = alias_.ledger_.AliasDnsIndexUpperBound(
            transaction, prefix, dns.size());
        for (; i != n; ++i)
        {
            ++searchs;
            if (searchs > rai::AliasRpcHandler::MAX_SEARCHS_PER_QUERY) break;

            rai::AliasIndex index;
            bool error = alias_.ledger_.AliasDnsIndexGet(i, index);
            if (error)
            {
                std::string error_info = rai::ToString(
                    "AliasRpcHandler::AliasSearchByDns_: AliasDnsIndexGet "
                    "failed, dns=",
                    dns);
                error_code_ = rai::ErrorCode::ALIAS_LEDGER_GET;
                rai::Log::Error(error_info);
                rai::Stats::Add(error_code_, error_info);
                return;
            }

            if (account_count[index.account_] > 0) continue;

            uint16_t credit;
            SearchEntry entry;
            error = GetSearchEntry_(transaction, index.account_, credit, entry);
            IF_ERROR_RETURN_VOID(error);
            if (!rai::StringStartsWith(entry.name_, name, true)) continue;

            prefix_result.emplace(credit, entry);
            if (prefix_result.size() > left)
            {
                prefix_result.erase(std::prev(prefix_result.end()));
            }
        }
    }

    rai::Ptree alias;
    for (const auto& i : exact_result)
    {
        alias.push_back(std::make_pair("", i.second.Json()));
    }
    for (const auto& i : prefix_result)
    {
        alias.push_back(std::make_pair("", i.second.Json()));
    }
    response_.put_child("alias", alias);
}

void rai::AliasRpcHandler::AliasSearchByName_(const std::string& name,
                                              uint64_t count)
{
    rai::AliasRpcHandler::SearchResult exact_result;
    rai::AliasRpcHandler::SearchResult prefix_result;
    std::unordered_map<rai::Account, int32_t> account_count;
    rai::Prefix prefix(name);

    rai::Transaction transaction(error_code_, alias_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    if (name.size() < sizeof(rai::Prefix))
    {
        rai::Iterator i =
            alias_.ledger_.AliasIndexLowerBound(transaction, prefix);
        rai::Iterator n =
            alias_.ledger_.AliasIndexUpperBound(transaction, prefix);
        for (; i != n; ++i)
        {
            rai::AliasIndex index;
            bool error = alias_.ledger_.AliasIndexGet(i, index);
            if (error)
            {
                std::string error_info = rai::ToString(
                    "AliasRpcHandler::AliasSearchByName_: AliasIndexGet "
                    "failed, name=",
                    name);
                error_code_ = rai::ErrorCode::ALIAS_LEDGER_GET;
                rai::Log::Error(error_info);
                rai::Stats::Add(error_code_, error_info);
                return;
            }

            uint16_t credit;
            SearchEntry entry;
            error = GetSearchEntry_(transaction, index.account_, credit, entry);
            IF_ERROR_RETURN_VOID(error);

            exact_result.emplace(credit, entry);
            if (account_count.find(index.account_) == account_count.end())
            {
                account_count[index.account_] = 0;
            }
            account_count[index.account_] += 1;

            if (exact_result.size() > count)
            {
                auto it = exact_result.erase(std::prev(exact_result.end()));
                account_count[it->second.account_] -= 1;
            }
        }
    }

    if (exact_result.size() < count)
    {
        uint64_t left = count - exact_result.size();
        size_t searchs = 0;
        rai::Iterator i =
            alias_.ledger_.AliasIndexLowerBound(transaction, prefix);
        rai::Iterator n = alias_.ledger_.AliasIndexUpperBound(
            transaction, prefix, name.size());
        for (; i != n; ++i)
        {
            ++searchs;
            if (searchs > rai::AliasRpcHandler::MAX_SEARCHS_PER_QUERY) break;

            rai::AliasIndex index;
            bool error = alias_.ledger_.AliasDnsIndexGet(i, index);
            if (error)
            {
                std::string error_info = rai::ToString(
                    "AliasRpcHandler::AliasSearchByName_: AliasIndexGet "
                    "failed, name=",
                    name);
                error_code_ = rai::ErrorCode::ALIAS_LEDGER_GET;
                rai::Log::Error(error_info);
                rai::Stats::Add(error_code_, error_info);
                return;
            }

            if (account_count[index.account_] > 0) continue;

            uint16_t credit;
            SearchEntry entry;
            error = GetSearchEntry_(transaction, index.account_, credit, entry);
            IF_ERROR_RETURN_VOID(error);

            prefix_result.emplace(credit, entry);
            if (prefix_result.size() > left)
            {
                prefix_result.erase(std::prev(prefix_result.end()));
            }
        }
    }

    rai::Ptree alias;
    for (const auto& i : exact_result)
    {
        alias.push_back(std::make_pair("", i.second.Json()));
    }
    for (const auto& i : prefix_result)
    {
        alias.push_back(std::make_pair("", i.second.Json()));
    }
    response_.put_child("alias", alias);
}

bool rai::AliasRpcHandler::GetSearchEntry_(rai::Transaction& transaction,
                                           const rai::Account& account,
                                           uint16_t& credit, SearchEntry& entry)
{
    std::string error_info;
    do
    {
        rai::AliasInfo info;
        bool error = alias_.ledger_.AliasInfoGet(transaction, account, info);
        if (error)
        {
            error_info = rai::ToString("AliasInfoGet failed, account=",
                                       account.StringAccount());
            error_code_ = rai::ErrorCode::ALIAS_LEDGER_GET;
            break;
        }

        rai::AccountInfo account_info;
        error =
            alias_.ledger_.AccountInfoGet(transaction, account, account_info);
        if (error || !account_info.Valid())
        {
            error_info = rai::ToString("AccountInfoGet failed, account=",
                                       account.StringAccount());
            error_code_ = rai::ErrorCode::ALIAS_LEDGER_GET;
            break;
        }

        std::shared_ptr<rai::Block> block(nullptr);
        error = alias_.ledger_.BlockGet(transaction, account_info.head_,
                                        block);
        if (error || block == nullptr)
        {
            error_info = rai::ToString("BlockGet failed, hash=",
                                        account_info.head_.StringHex());
            error_code_ = rai::ErrorCode::ALIAS_LEDGER_GET;
            break;
        }

        credit = block->Credit();
        entry = SearchEntry(account, info.name_, info.dns_);
    } while (0);

    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        if (!error_info.empty())
        {
            error_info = "AliasRpcHandler::GetSearchEntry_: " + error_info;
            rai::Log::Error(error_info);
            rai::Stats::Add(error_code_, error_info);
        }
        return true;
    }

    return false;
}

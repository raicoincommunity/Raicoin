#include <rai/app/rpc.hpp>

#include <rai/app/app.hpp>

rai::AppRpcHandler::AppRpcHandler(
    rai::App& app, const rai::UniqueId& uid, bool check, rai::Rpc& rpc,
    const std::string& body, const boost::asio::ip::address_v4& ip,
    const std::function<void(const rai::Ptree&)>& send_response)
    : RpcHandler(rpc, body, ip, send_response),
      app_(app),
      uid_(uid),
      check_(check)
{
}

void rai::AppRpcHandler::ProcessImpl()
{
    std::string action = request_.get<std::string>("action");

    if (action == "account_count")
    {
        AccountCount();
    }
    else if (action == "account_info")
    {
        AccountInfo();
    }
    else if (action == "app_trace_on")
    {
        if (!CheckLocal_())
        {
            AppTraceOn();
        }
    }
    else if (action == "app_trace_off")
    {
        if (!CheckLocal_())
        {
            AppTraceOff();
        }
    }
    else if (action == "app_trace_status")
    {
        AppTraceStatus();
    }
    else if (action == "block_cache_count")
    {
        BlockCacheCount();
    }
    else if (action == "block_confirm_count")
    {
        BlockConfirmCount();
    }
    else if (action == "block_count")
    {
        BlockCount();
    }
    else if (action == "clients")
    {
        Clients();
    }
    else if (action == "service_subscribe")
    {
        ServiceSubscribe();
    }
    else if (action == "subscription")
    {
        Subscription();
    }
    else if (action == "subscription_count")
    {
        SubscriptionCount();
    }
    else if (action == "subscription_account_count")
    {
        SubscriptionAccountCount();
    }
    else if (action == "subscriptions")
    {
        Subscriptions();
    }
    else
    {
        error_code_ = rai::ErrorCode::RPC_UNKNOWN_ACTION;
    }
}

void rai::AppRpcHandler::ExtraCheck(const std::string& action)
{
    if (!check_)
    {
        return;
    }

    if (rai::Contain(app_.provider_actions_, action))
    {
        return;
    }

    error_code_ = rai::ErrorCode::RPC_ACTION_NOT_ALLOWED;
}

void rai::AppRpcHandler::AccountCount()
{
    rai::Transaction transaction(error_code_, app_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    size_t count = 0;
    bool error = app_.ledger_.AccountCount(transaction, count);
    if (error)
    {
        error_code_ = rai::ErrorCode::LEDGER_ACCOUNT_COUNT;
        return;
    }

    response_.put("count", count);
}

void rai::AppRpcHandler::AccountInfo()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    rai::Transaction transaction(error_code_, app_.ledger_, false);
    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    rai::AccountInfo info;
    error = app_.ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        response_.put("error", "The account does not exist");
        return;
    }

    response_.put("type", rai::BlockTypeToString(info.type_));
    response_.put("head", info.head_.StringHex());
    response_.put("head_height", info.head_height_);
    response_.put("tail", info.tail_.StringHex());
    response_.put("tail_height", info.tail_height_);
    if (info.confirmed_height_ == rai::Block::INVALID_HEIGHT)
    {
        response_.put("confirmed_height", "");
    }
    else
    {
        response_.put("confirmed_height", info.confirmed_height_);
    }

    std::shared_ptr<rai::Block> head_block(nullptr);
    error = app_.ledger_.BlockGet(transaction, info.head_, head_block);
    if (error || head_block == nullptr)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
        return;
    }
    rai::Ptree head_ptree;
    head_block->SerializeJson(head_ptree);
    response_.add_child("head_block", head_ptree);

    std::shared_ptr<rai::Block> tail_block(nullptr);
    error = app_.ledger_.BlockGet(transaction, info.tail_, tail_block);
    if (error || tail_block == nullptr)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
        return;
    }
    rai::Ptree tail_ptree;
    tail_block->SerializeJson(tail_ptree);
    response_.add_child("tail_block", tail_ptree);

    if (info.confirmed_height_ != rai::Block::INVALID_HEIGHT)
    {
        std::shared_ptr<rai::Block> confirmed_block(nullptr);
        if (info.confirmed_height_ == info.head_height_)
        {
            confirmed_block = head_block;
        }
        else
        {
            error = app_.ledger_.BlockGet(transaction, account, info.confirmed_height_, confirmed_block);
            if (error || confirmed_block == nullptr)
            {
                error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
                return;
            }
        }
        rai::Ptree confirmed_ptree;
        confirmed_block->SerializeJson(confirmed_ptree);
        response_.add_child("confirmed_block", confirmed_ptree);
    }
}

void rai::AppRpcHandler::AppActionCount()
{
    response_.put("count", app_.ActionSize());
}

void rai::AppRpcHandler::AppTraceOn()
{
    auto trace_o = request_.get_optional<std::string>("trace");
    if (!trace_o)
    {
        error_code_ = rai::ErrorCode::APP_RPC_MISS_FIELD_TRACE;
        return;
    }

    if (*trace_o == "message_from_gateway")
    {
        app_.trace_.message_from_gateway_ = true;
    }
    else if (*trace_o == "message_to_gateway")
    {
        app_.trace_.message_to_gateway_ = true;
    }
    else
    {
        error_code_ = rai::ErrorCode::APP_RPC_INVALID_FIELD_TRACE;
        return;
    }
    
    response_.put("succes", "");
}

void rai::AppRpcHandler::AppTraceOff()
{
    auto trace_o = request_.get_optional<std::string>("trace");
    if (!trace_o)
    {
        error_code_ = rai::ErrorCode::APP_RPC_MISS_FIELD_TRACE;
        return;
    }

    if (*trace_o == "message_from_gateway")
    {
        app_.trace_.message_from_gateway_ = false;
    }
    else if (*trace_o == "message_to_gateway")
    {
        app_.trace_.message_to_gateway_ = false;
    }
    else
    {
        error_code_ = rai::ErrorCode::APP_RPC_INVALID_FIELD_TRACE;
        return;
    }
    
    response_.put("succes", "");
}

void rai::AppRpcHandler::AppTraceStatus()
{
    rai::Ptree trace;
    trace.put("message_from_gateway",
              app_.trace_.message_from_gateway_ ? "true" : "false");
    trace.put("message_to_gateway",
              app_.trace_.message_to_gateway_ ? "true" : "false");
    response_.put_child("trace", trace);
}

void rai::AppRpcHandler::BlockCacheCount()
{
    response_.put("count", std::to_string(app_.block_cache_.Size()));
}

void rai::AppRpcHandler::BlockConfirmCount()
{
    response_.put("count", std::to_string(app_.block_confirm_.Size()));
} 

void rai::AppRpcHandler::BlockCount()
{
    rai::Transaction transaction(error_code_, app_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    size_t count = 0;
    bool error   = app_.ledger_.BlockCount(transaction, count);
    if (error)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_COUNT;
        return;
    }

    response_.put("count", count);
}


void rai::AppRpcHandler::BlockQuery()
{
    auto hash_o = request_.get_optional<std::string>("hash");
    if (hash_o)
    {
        BlockQueryByHash();
        return;
    }

    auto previous_o = request_.get_optional<std::string>("previous");
    if (!previous_o)
    {
        BlockQueryByHeight();
        return;
    }

    BlockQueryByPrevious();
}

void rai::AppRpcHandler::BlockQueryByPrevious()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    uint64_t height;
    error = GetHeight_(height);
    IF_ERROR_RETURN_VOID(error);

    rai::BlockHash previous;
    error = GetPrevious_(previous);
    IF_ERROR_RETURN_VOID(error);

    rai::Transaction transaction(error_code_, app_.ledger_, false);
    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }
    rai::AccountInfo info;
    error = app_.ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        response_.put("status", "miss");
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    if (height == 0)
    {
        error = app_.ledger_.BlockGet(transaction, account, height, block);
        if (error)
        {
            error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
            return;
        }
        response_.put("status", "success");
        response_.put("confirmed", rai::BoolToString(info.Confirmed(height)));
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        response_.add_child("block", block_ptree);
        return;
    }

    if (height < info.tail_height_ + 1)
    {
        response_.put("status", "pruned");
        return;
    }
    if (height > info.head_height_ + 1)
    {
        response_.put("status", "miss");
        return;
    }

    rai::BlockHash successor;
    error = app_.ledger_.BlockSuccessorGet(transaction, previous, successor);
    if (error)
    {
        error = app_.ledger_.BlockGet(transaction, account, height - 1, block);
        if (error)
        {
            error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
            return;
        }
        response_.put("status", "fork");
        response_.put("confirmed",
                      rai::BoolToString(info.Confirmed(height - 1)));
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        response_.add_child("block", block_ptree);
    }
    else
    {
        if (successor.IsZero())
        {
            response_.put("status", "miss");
            return;
        }
        error = app_.ledger_.BlockGet(transaction, successor, block);
        if (error)
        {
            error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
            return;
        }
        if (block->Height() != height)
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_PREVIOUS;
            return;
        }
        response_.put("status", "success");
        response_.put("confirmed", rai::BoolToString(info.Confirmed(height)));
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        response_.add_child("block", block_ptree);
    }
}

void rai::AppRpcHandler::BlockQueryByHash()
{
    rai::BlockHash hash;
    bool error = GetHash_(hash);
    IF_ERROR_RETURN_VOID(error);

    rai::Transaction transaction(error_code_, app_.ledger_, false);
    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::BlockHash successor;
    error = app_.ledger_.BlockGet(transaction, hash, block, successor);
    if (!error && block != nullptr)
    {
        response_.put("status", "success");
        response_.put("successor", successor.StringHex());
    }
    else
    {
        error = app_.ledger_.RollbackBlockGet(transaction, hash, block);
        if (!error && block != nullptr)
        {
            response_.put("status", "rollback");
        }
    }
    
    if (block == nullptr)
    {
        response_.put("status", "miss");
        return;
    }

    rai::Ptree block_ptree;
    block->SerializeJson(block_ptree);
    response_.add_child("block", block_ptree);
}

void rai::AppRpcHandler::BlockQueryByHeight()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    uint64_t height;
    error = GetHeight_(height);
    IF_ERROR_RETURN_VOID(error);

    rai::Transaction transaction(error_code_, app_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    
    rai::AccountInfo info;
    error = app_.ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        response_.put("status", "miss");
        return;
    }

    if (height < info.tail_height_)
    {
        response_.put("status", "pruned");
        return;
    }

    if (height > info.head_height_)
    {
        response_.put("status", "miss");
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    error = app_.ledger_.BlockGet(transaction, account, height, block);
    if (error)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
        return;
    }
    response_.put("status", "success");
    response_.put("confirmed", rai::BoolToString(info.Confirmed(height)));
    rai::Ptree block_ptree;
    block->SerializeJson(block_ptree);
    response_.add_child("block", block_ptree);
}

void rai::AppRpcHandler::BootstrapStatus()
{
    app_.bootstrap_.Status(response_);
}

void rai::AppRpcHandler::Clients()
{
    rai::Ptree clients;
    if (app_.ws_server_)
    {
        std::vector<rai::UniqueId> uids = app_.ws_server_->Clients();
        for (const auto& uid : uids)
        {
            rai::Ptree entry;
            entry.put("", uid.StringHex());
            clients.push_back(std::make_pair("", entry));
        }
    }
    response_.put_child("clients", clients);
}

void rai::AppRpcHandler::ServiceSubscribe()
{
    std::string service;
    bool error = GetService_(service);
    IF_ERROR_RETURN_VOID(error);

    std::vector<std::pair<std::string, std::string>> filters;
    error = GetFilters_(filters);
    IF_ERROR_RETURN_VOID(error);

    if (filters.size() != 1)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_FILTERS;
        return;
    }
    
    using P = rai::Provider;
    if (filters[0].first != P::ToString(P::Filter::APP_ACCOUNT))
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_FILTERS;
        return;
    }

    rai::Account account;
    error = account.DecodeAccount(filters[0].second);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_FILTERS;
        return;
    }

    error_code_ = app_.subscribe_.Subscribe(account, uid_);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    response_.put("success", "");
}

void rai::AppRpcHandler::Subscription()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    app_.subscribe_.JsonByAccount(account, response_);
}

void rai::AppRpcHandler::Subscriptions()
{
    auto client_id_o = request_.get_optional<std::string>("client_id");
    if (client_id_o)
    {
        rai::UniqueId client_id;
        bool error = client_id.DecodeHex(*client_id_o);
        if (error)
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_CLIENT_ID;
            return;
        }

        app_.subscribe_.JsonByUid(client_id, response_);
    }
    else
    {
        app_.subscribe_.Json(response_);
    }
}

void rai::AppRpcHandler::SubscriptionCount()
{
    auto client_id_o = request_.get_optional<std::string>("client_id");
    if (client_id_o)
    {
        rai::UniqueId client_id;
        bool error = client_id.DecodeHex(*client_id_o);
        if (error)
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_CLIENT_ID;
            return;
        }

        response_.put("count", std::to_string(app_.subscribe_.Size(client_id)));
    }
    else
    {
        response_.put("count", std::to_string(app_.subscribe_.Size()));
    }
}

void rai::AppRpcHandler::SubscriptionAccountCount()
{
    response_.put("count", std::to_string(app_.subscribe_.AccountSize()));
}

bool rai::AppRpcHandler::GetService_(std::string& service)
{
    auto service_o = request_.get_optional<std::string>("service");
    if (!service_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_SERVICE;
        return true;
    }

    if (service_o->empty())
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_SERVICE;
        return true;
    }

    service = *service_o;
    return false;
}

bool rai::AppRpcHandler::GetFilters_(
    std::vector<std::pair<std::string, std::string>>& filters)
{
    auto filters_o = request_.get_child_optional("filters");
    if (!filters_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_FILTERS;
        return true;
    }

    for (const auto& i : *filters_o)
    {
        auto key_o = i.second.get_optional<std::string>("key");
        if (!key_o)
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_FILTERS;
            return true;
        }

        auto value_o = i.second.get_optional<std::string>("value");
        if (!value_o)
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_FILTERS;
            return true;
        }
        filters.emplace_back(*key_o, *value_o);
    }

    if (filters.size() == 0)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_FILTERS;
        return true;
    }

    return false;
}

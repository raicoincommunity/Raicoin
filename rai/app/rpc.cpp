#include <rai/app/rpc.hpp>

#include <rai/app/app.hpp>

rai::AppRpcHandler::AppRpcHandler(
    rai::App& app, rai::Rpc& rpc, const std::string& body,
    const boost::asio::ip::address_v4& ip,
    const std::function<void(const rai::Ptree&)>& send_response)
    : RpcHandler(rpc, body, ip, send_response), app_(app)
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
    else
    {
        error_code_ = rai::ErrorCode::RPC_UNKNOWN_ACTION;
    }
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

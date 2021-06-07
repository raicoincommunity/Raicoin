#include <rai/rai_wallet_rpc/wallet_rpc.hpp>

#include <rai/common/blocks.hpp>
#include <rai/common/log.hpp>
#include <rai/common/parameters.hpp>

rai::WalletRpcConfig::WalletRpcConfig()
    : auto_credit_(true), auto_receive_(true), receive_mininum_(rai::RAI)
{
    rpc_.address_ = boost::asio::ip::address_v4::any();
    rpc_.port_ = rai::WalletRpcConfig::DEFAULT_PORT;
    rpc_.enable_control_ = true;
    rpc_.whitelist_ = {
        boost::asio::ip::address_v4::loopback(),
    };
    rai::uint128_union api_key;
    rai::random_pool.GenerateBlock(api_key.bytes.data(), api_key.bytes.size());
    rai_api_key_ = api_key.StringHex();
}

rai::ErrorCode rai::WalletRpcConfig::DeserializeJson(bool& upgraded,
                                                     rai::Ptree& ptree)
{
    if (ptree.empty())
    {
        upgraded = true;
        SerializeJson(ptree);
        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    try
    {
        error_code = rai::ErrorCode::JSON_CONFIG_VERSION;
        std::string version_str = ptree.get<std::string>("version");
        uint32_t version = 0;
        bool error = rai::StringToUint(version_str, version);
        IF_ERROR_RETURN(error, error_code);

        error_code = UpgradeJson(upgraded, version, ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_RPC;
        rai::Ptree& rpc_ptree = ptree.get_child("rpc");

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_ADDRESS;
        std::string address = rpc_ptree.get<std::string>("address");
        boost::system::error_code ec;
        rpc_.address_ = boost::asio::ip::make_address_v4(address, ec);
        if (ec)
        {
            return error_code;
        }

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_PORT;
        rpc_.port_ = rpc_ptree.get<uint16_t>("port");

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_ENABLE_CONTROL;
        rpc_.enable_control_ = rpc_ptree.get<bool>("enable_control");

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_WHITELIST;
        rpc_.whitelist_.clear();
        auto whitelist = rpc_ptree.get_child("whitelist");
        for (const auto& i : whitelist)
        {
            address = i.second.get<std::string>("");
            boost::asio::ip::address_v4 ip =
                boost::asio::ip::make_address_v4(address, ec);
            if (ec)
            {
                return error_code;
            }
            rpc_.whitelist_.push_back(ip);
        }

        error_code = rai::ErrorCode::JSON_CONFIG_WALLET;
        rai::Ptree& wallet_ptree = ptree.get_child("wallet");
        error_code = wallet_.DeserializeJson(upgraded, wallet_ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_CALLBACK_URL;
        std::string callback_url = ptree.get<std::string>("callback_url");
        error = callback_url_.Parse(callback_url);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_AUTO_CREDIT;
        auto_credit_ = ptree.get<bool>("auto_credit");

        error_code = rai::ErrorCode::JSON_CONFIG_AUTO_RECEIVE;
        auto_receive_ = ptree.get<bool>("auto_receive");

        error_code = rai::ErrorCode::JSON_CONFIG_RECEIVE_MINIMUM;
        std::string receive_mininum = ptree.get<std::string>("receive_minimum");
        auto it_rai = receive_mininum.find("RAI");
        if (it_rai == std::string::npos)
        {
            return error_code;
        }
        receive_mininum = receive_mininum.substr(0, it_rai);
        rai::StringTrim(receive_mininum, " \r\n\t");
        error = receive_mininum_.DecodeBalance(rai::RAI, receive_mininum);
        IF_ERROR_RETURN(error, error_code);

        auto api_key_o = ptree.get_optional<std::string>("rai_api_key");
        if (api_key_o)
        {
            rai_api_key_ = *api_key_o;
        }
        else
        {
            upgraded = true;
            ptree.put("rai_api_key", rai_api_key_);
        }

        error_code = rai::ErrorCode::JSON_CONFIG_LOG;
        rai::Ptree& log_ptree = ptree.get_child("log");
        error_code = log_.DeserializeJson(upgraded, log_ptree);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    catch (const std::exception&)
    {
        return error_code;
    }
    return rai::ErrorCode::SUCCESS;
}

void rai::WalletRpcConfig::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("version", "1");

    rai::Ptree rpc_ptree;
    rpc_ptree.put("address", rpc_.address_.to_string());
    rpc_ptree.put("port", rpc_.port_);
    rpc_ptree.put("enable_control", rpc_.enable_control_);
    rai::Ptree whitelist;
    for (const auto& i : rpc_.whitelist_)
    {
        rai::Ptree entry;
        entry.put("", i.to_string());
        whitelist.push_back(std::make_pair("", entry));
    }
    rpc_ptree.add_child("whitelist", whitelist);
    ptree.put_child("rpc", rpc_ptree);

    rai::Ptree wallet_ptree;
    wallet_.SerializeJson(wallet_ptree);
    ptree.add_child("wallet", wallet_ptree);

    ptree.put("callback_url", callback_url_.String());
    ptree.put("auto_credit", auto_credit_);
    ptree.put("auto_receive", auto_receive_);
    ptree.put("receive_minimum",
              receive_mininum_.StringBalance(rai::RAI) + "RAI");
    ptree.put("rai_api_key", rai_api_key_);
}

rai::ErrorCode rai::WalletRpcConfig::UpgradeJson(bool& upgraded,
                                                 uint32_t version,
                                                 rai::Ptree& ptree) const
{
    rai::ErrorCode error_code;
    switch (version)
    {
        case 1:
        {
            upgraded = true;
            error_code = UpgradeV1V2(ptree);
            IF_NOT_SUCCESS_RETURN(error_code);
        }
        case 2:
        {
            break;
        }
        default:
        {
            return rai::ErrorCode::CONFIG_VERSION;
        }
    }

    return rai::ErrorCode::SUCCESS;
}


rai::ErrorCode rai::WalletRpcConfig::UpgradeV1V2(rai::Ptree& ptree) const
{
    ptree.put("version", 2);

    auto log_o = ptree.get_child_optional("log");
    if (log_o)
    {
        return rai::ErrorCode::SUCCESS;
    }

    rai::Ptree log_ptree;
    log_.SerializeJson(log_ptree);
    ptree.add_child("log", log_ptree);

    return rai::ErrorCode::SUCCESS;
}

rai::WalletRpcHandler::WalletRpcHandler(
    rai::WalletRpc& wallet_rpc, rai::Rpc& rpc, const std::string& body,
    const boost::asio::ip::address_v4& ip,
    const std::function<void(const rai::Ptree&)>& send_response)
    : RpcHandler(rpc, body, ip, send_response), main_(wallet_rpc)
{
}

void rai::WalletRpcHandler::ProcessImpl()
{
    CheckApiKey();
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    std::string action = request_.get<std::string>("action");

    if (action == "account_info")
    {
        AccountInfo();
    }
    else if (action == "account_send")
    {
        AccountSend();
    }
    else if (action == "account_change")
    {
        AccountChange();
    }
    else if (action == "block_query")
    {
        BlockQuery();
    }
    else if (action == "current_account")
    {
        CurrentAccount();
    }
    else if (action == "status")
    {
        Status();
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
        error_code_ = rai::ErrorCode::RPC_UNKNOWN_ACTION;
    }
}

void rai::WalletRpcHandler::CheckApiKey()
{
    std::string api_key;
    auto api_key_o = request_.get_optional<std::string>("rai_api_key");
    if (api_key_o)
    {
        api_key = *api_key_o;
    }
    else
    {
        api_key = header_api_key_;
    }

    if (api_key != main_.config_.rai_api_key_)
    {
        error_code_ = rai::ErrorCode::API_KEY;
    }
}

void rai::WalletRpcHandler::AccountInfo()
{
    rai::Account account;
    auto account_o = request_.get_optional<std::string>("account");
    if (account_o)
    {
        if (account.DecodeAccount(*account_o))
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_ACCOUNT;
            return;
        }
    }
    else
    {
        account = main_.wallets_->SelectedAccount();
    }
    response_.put("account", account.StringAccount());

    rai::Transaction transaction(error_code_, main_.ledger_, false);
    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    rai::AccountInfo info;
    bool error = main_.ledger_.AccountInfoGet(transaction, account, info);
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
    response_.put("forks", info.forks_);
    response_.put("restricted", info.Restricted());

    std::shared_ptr<rai::Block> head_block(nullptr);
    error = main_.ledger_.BlockGet(transaction, info.head_, head_block);
    if (error || head_block == nullptr)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
        return;
    }
    rai::Ptree head_ptree;
    head_block->SerializeJson(head_ptree);
    response_.add_child("head_block", head_ptree);

    std::shared_ptr<rai::Block> tail_block(nullptr);
    error = main_.ledger_.BlockGet(transaction, info.tail_, tail_block);
    if (error || tail_block == nullptr)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
        return;
    }
    rai::Ptree tail_ptree;
    tail_block->SerializeJson(tail_ptree);
    response_.add_child("tail_block", tail_ptree);
}

void rai::WalletRpcHandler::AccountSend()
{
    rai::Account to;
    rai::Amount amount;
    std::vector<uint8_t> extensions;
    error_code_ = ParseAccountSend(request_, to, amount, extensions);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    main_.AddRequest(request_);
    response_.put("success", "");
}

void rai::WalletRpcHandler::AccountChange()
{
    rai::Account rep;
    std::vector<uint8_t> extensions;
    error_code_ = ParseAccountChange(request_, rep, extensions);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    main_.AddRequest(request_);
    response_.put("success", "");
}

void rai::WalletRpcHandler::BlockQuery()
{
    std::shared_ptr<rai::Block> block(nullptr);

    auto hash_o = request_.get_optional<std::string>("hash");
    auto previous_o = request_.get_optional<std::string>("previous");
    auto height_o = request_.get_optional<std::string>("height");
    if (hash_o)
    {
        BlockQueryByHash(block);
    }
    else if (previous_o)
    {
        BlockQueryByPrevious(block);
    }
    else if (height_o)
    {
        BlockQueryByHeight(block);
    }
    else
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_HASH;
    }

    if (block != nullptr)
    {
        response_.put("hash", block->Hash().StringHex());
    }

    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    auto status = response_.get<std::string>("status");
    if (status != "success")
    {
        return;
    }

    rai::Transaction transaction(error_code_, main_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    bool error = false;
    rai::Amount amount(0);
    if (block->Height() == 0)
    {
        error = block->Amount(amount);
    }
    else
    {
        std::shared_ptr<rai::Block> previous(nullptr);
        error =
            main_.ledger_.BlockGet(transaction, block->Previous(), previous);
        if (!error)
        {
            error = block->Amount(*previous, amount);
        }
    }
    if (error)
    {
        error_code_ = rai::ErrorCode::BLOCK_AMOUNT_GET;
        return;
    }
    response_.put("amount", amount.StringDec());
    response_.put("amount_in_rai", amount.StringBalance(rai::RAI) + " RAI");

    rai::AccountInfo info;
    error = main_.ledger_.AccountInfoGet(transaction, block->Account(), info);
    if (error || !info.Valid())
    {
        error_code_ = rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET;
        return;
    }
    response_.put("confirmed",
                  info.Confirmed(block->Height()) ? "true" : "false");

    if (block->Opcode() == rai::BlockOpcode::RECEIVE)
    {
        std::shared_ptr<rai::Block> source(nullptr);
        error = main_.ledger_.SourceGet(transaction, block->Link(), source);
        if (!error && source != nullptr)
        {
            rai::Ptree block_ptree;
            source->SerializeJson(block_ptree);
            response_.add_child("source_block", block_ptree);
        }
    }
}

void rai::WalletRpcHandler::BlockQueryByHash(std::shared_ptr<rai::Block>& block)
{
    rai::BlockHash hash;
    bool error = GetHash_(hash);
    IF_ERROR_RETURN_VOID(error);

    rai::Transaction transaction(error_code_, main_.ledger_, false);
    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    rai::BlockHash successor;
    error = main_.ledger_.BlockGet(transaction, hash, block, successor);
    if (!error && block != nullptr)
    {
        response_.put("status", "success");
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        response_.add_child("block", block_ptree);
        response_.put("successor", successor.StringHex());
        return;
    }

    error = main_.ledger_.RollbackBlockGet(transaction, hash, block);
    if (!error && block != nullptr)
    {
        response_.put("status", "rollback");
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        response_.add_child("block", block_ptree);
        return;
    }

    response_.put("status", "miss");
}

void rai::WalletRpcHandler::BlockQueryByHeight(
    std::shared_ptr<rai::Block>& block)
{
    rai::Account account;
    auto account_o = request_.get_optional<std::string>("account");
    if (!account_o)
    {
        account = main_.wallets_->SelectedAccount();
    }
    else
    {
        if (account.DecodeAccount(*account_o))
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_ACCOUNT;
            return;
        }
    }

    uint64_t height;
    bool error = GetHeight_(height);
    IF_ERROR_RETURN_VOID(error);

    rai::Transaction transaction(error_code_, main_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::AccountInfo info;
    error = main_.ledger_.AccountInfoGet(transaction, account, info);
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

    rai::BlockHash successor;
    error =
        main_.ledger_.BlockGet(transaction, account, height, block, successor);
    if (error)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
        return;
    }
    response_.put("status", "success");
    rai::Ptree block_ptree;
    block->SerializeJson(block_ptree);
    response_.add_child("block", block_ptree);
    response_.put("successor", successor.StringHex());
}

void rai::WalletRpcHandler::BlockQueryByPrevious(
    std::shared_ptr<rai::Block>& block)
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

    rai::Transaction transaction(error_code_, main_.ledger_, false);
    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }
    rai::AccountInfo info;
    error = main_.ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        response_.put("status", "miss");
        return;
    }

    if (height == 0)
    {
        rai::BlockHash successor;
        error = main_.ledger_.BlockGet(transaction, account, height, block,
                                       successor);
        if (error)
        {
            error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
            return;
        }
        response_.put("status", "success");
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        response_.add_child("block", block_ptree);
        response_.put("successor", successor.StringHex());
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
    error = main_.ledger_.BlockSuccessorGet(transaction, previous, successor);
    if (error)
    {
        error = main_.ledger_.BlockGet(transaction, account, height - 1, block);
        if (error)
        {
            error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
            return;
        }
        response_.put("status", "fork");
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
        error =
            main_.ledger_.BlockGet(transaction, successor, block, successor);
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
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        response_.add_child("block", block_ptree);
        response_.put("successor", successor.StringHex());
    }
}

void rai::WalletRpcHandler::CurrentAccount()
{
    response_.put("account", main_.wallets_->SelectedAccount().StringAccount());
}

rai::ErrorCode rai::WalletRpcHandler::ParseNote(
    const rai::Ptree& request, std::vector<uint8_t>& extensions)
{
    auto note_o = request.get_optional<std::string>("note");
    if (note_o && !note_o->empty())
    {
        bool error =
            rai::ExtensionAppend(rai::ExtensionType::NOTE, *note_o, extensions);
        IF_ERROR_RETURN(error, rai::ErrorCode::EXTENSION_APPEND);
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::WalletRpcHandler::ParseExtensions(
    const rai::Ptree& request, std::vector<uint8_t>& extensions)
{
    auto custom_o = request.get_child_optional("custom_extensions");
    if (custom_o && !custom_o->empty())
    {
        try
        {
            for (const auto& i : *custom_o)
            {
                auto type_str = i.second.get<std::string>("type");
                uint16_t type;
                bool error = rai::StringToUint(type_str, type);
                IF_ERROR_RETURN(error, rai::ErrorCode::EXTENSION_APPEND);
                auto value = i.second.get<std::string>("value");
                error = rai::ExtensionAppend(
                    static_cast<rai::ExtensionType>(type), value, extensions);
                IF_ERROR_RETURN(error, rai::ErrorCode::EXTENSION_APPEND);
            }
        }
        catch (...)
        {
            return rai::ErrorCode::EXTENSION_APPEND;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::WalletRpcHandler::ParseAccountSend(
    const rai::Ptree& request, rai::Account& destination, rai::Amount& amount,
    std::vector<uint8_t>& extensions)
{
    auto destination_o = request.get_optional<std::string>("to");
    if (!destination_o)
    {
        return rai::ErrorCode::RPC_MISS_FIELD_TO;
    }

    rai::AccountParser parser(*destination_o);
    if (parser.Error())
    {
        return rai::ErrorCode::RPC_INVALID_FIELD_TO;
    }
    destination = parser.Account();
    std::string sub_account = parser.SubAccount();
    if (!sub_account.empty())
    {
        bool error = rai::ExtensionAppend(rai::ExtensionType::SUB_ACCOUNT,
                                          sub_account, extensions);
        IF_ERROR_RETURN(error, rai::ErrorCode::EXTENSION_APPEND);
    }

    auto amount_o = request.get_optional<std::string>("amount");
    if (!amount_o)
    {
        return rai::ErrorCode::RPC_MISS_FIELD_AMOUNT;
    }
    uint64_t u;
    bool error = rai::StringToUint(*amount_o, u);
    IF_ERROR_RETURN(error, rai::ErrorCode::RPC_INVALID_FIELD_AMOUNT);
    amount = u;

    rai::ErrorCode error_code =
        rai::WalletRpcHandler::ParseNote(request, extensions);
    IF_NOT_SUCCESS_RETURN(error_code);

    // other extensions here

    error_code = rai::WalletRpcHandler::ParseExtensions(request, extensions);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (extensions.size() > rai::TxBlock::MaxExtensionsLength())
    {
        return rai::ErrorCode::EXTENSIONS_LENGTH;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::WalletRpcHandler::ParseAccountChange(
    const rai::Ptree& request, rai::Account& rep,
    std::vector<uint8_t>& extensions)
{
    rep = 0;
    auto rep_o = request.get_optional<std::string>("representative");
    if (rep_o)
    {
        bool error = rep.DecodeAccount(*rep_o);
        IF_ERROR_RETURN(error, rai::ErrorCode::RPC_INVALID_FIELD_REP);
    }

    rai::ErrorCode error_code =
        rai::WalletRpcHandler::ParseNote(request, extensions);
    IF_NOT_SUCCESS_RETURN(error_code);

    // other extensions here

    error_code = rai::WalletRpcHandler::ParseExtensions(request, extensions);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (extensions.size() > rai::TxBlock::MaxExtensionsLength())
    {
        return rai::ErrorCode::EXTENSIONS_LENGTH;
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::WalletRpcHandler::Status()
{
    main_.Status(response_);
}

void rai::WalletRpcHandler::Stop()
{
    main_.Stop();
    response_.put("success", "");
}

rai::WalletRpcAction::WalletRpcAction()
    : processed_(false),
      callback_error_(false),
      last_callback_(0),
      error_code_(rai::ErrorCode::SUCCESS),
      block_(nullptr)
{
}

rai::WalletRpc::WalletRpc(const std::shared_ptr<rai::Wallets>& wallets,
                          const rai::WalletRpcConfig& config)
    : wallets_(wallets),
      config_(config),
      ledger_(wallets->ledger_),
      sequence_(0),
      stopped_(false),
      waiting_callback_(false),
      waiting_wallet_(false),
      callback_height_(rai::Block::INVALID_HEIGHT),
      thread_([this]() { Run(); })

{
}

void rai::WalletRpc::AddRequest(const rai::Ptree& request)
{
    std::lock_guard<std::mutex> lock(mutex_);
    requests_.push_back(request);
}

void rai::WalletRpc::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        lock.unlock();
        if (!wallets_->Synced(wallets_->SelectedAccount()))
        {
            lock.lock();
            condition_.wait_for(lock, std::chrono::seconds(5));
            continue;
        }
        lock.lock();

        bool loop = false;
        ProcessPendings_(lock, loop);
        if (loop)
        {
            continue;
        }

        if (waiting_wallet_)
        {
            condition_.wait_for(lock, std::chrono::seconds(1));
            continue;
        }

        ProcessAutoCredit_(lock, loop);
        if (loop)
        {
            condition_.wait_for(lock, std::chrono::seconds(1));
            continue;
        }

        ProcessAutoReceive_(lock, loop);
        if (loop)
        {
            condition_.wait_for(lock, std::chrono::seconds(1));
            continue;
        }

        ProcessRequests_(lock, loop);
        if (loop)
        {
            // no wait_for here
            continue;
        }

        condition_.wait_for(lock, std::chrono::seconds(5));
    }
}

void rai::WalletRpc::Stop()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
    }

    if (thread_.joinable())
    {
        thread_.join();
    }

    wallets_->Stop();
}

void rai::WalletRpc::SendCallback(const rai::Ptree& message)
{
    rai::HttpCallback handler = [this](rai::ErrorCode error_code,
                                       const std::string&) {
        ProcessCallbackResult(error_code);
    };

    auto http = std::make_shared<rai::HttpClient>(wallets_->service_);
    rai::ErrorCode error_code =
        http->Post(config_.callback_url_, message, handler);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        handler(error_code, "");
    }
}

void rai::WalletRpc::ProcessCallbackResult(rai::ErrorCode error_code)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        waiting_callback_ = false;
        auto it = actions_.begin();
        if (it == actions_.end())
        {
            rai::Log::Error("WalletRpc::ProcessCallbackResult: pendings empty");
            return;
        }

        if (error_code == rai::ErrorCode::SUCCESS)
        {
            actions_.erase(it);
            return;
        }

        actions_.modify(it, [](rai::WalletRpcAction& data) {
            data.callback_error_ = true;
        });
    }
    condition_.notify_all();
}

void rai::WalletRpc::ProcessAccountActionResult(
    rai::ErrorCode error_code, const std::shared_ptr<rai::Block>& block,
    uint64_t sequence)
{
    do
    {
        std::lock_guard<std::mutex> lock(mutex_);
        waiting_wallet_ = false;
        auto it = actions_.get<1>().find(sequence);
        if (it == actions_.get<1>().end())
        {
            rai::Log::Error(
                "WalletRpc::ProcessAccountActionResult: failed to find data, "
                "sequence="
                + std::to_string(sequence));
            break;
        }

        actions_.get<1>().modify(it, [&](rai::WalletRpcAction& data) {
            data.processed_ = true;
            data.error_code_ = error_code;
            data.block_ = block;
        });
    } while (0);
    condition_.notify_all();
}

void rai::WalletRpc::Status(rai::Ptree& ptree) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    ptree.put("actions", actions_.size());
    ptree.put("requests", requests_.size());
    ptree.put("synced", wallets_->Synced(wallets_->SelectedAccount()));
}

rai::AccountActionCallback rai::WalletRpc::AccountActionCallback(
    uint64_t sequence)
{
    rai::AccountActionCallback callback =
        [this, sequence](rai::ErrorCode error_code,
                         const std::shared_ptr<rai::Block>& block) {
            this->ProcessAccountActionResult(error_code, block, sequence);
        };
    return callback;
}

rai::RpcHandlerMaker rai::WalletRpc::RpcHandlerMaker()
{
    return [this](rai::Rpc& rpc, const std::string& body,
                  const boost::asio::ip::address_v4& ip,
                  const std::function<void(const rai::Ptree&)>& send_response)
               -> std::unique_ptr<rai::RpcHandler> {
        return std::make_unique<rai::WalletRpcHandler>(*this, rpc, body, ip,
                                                       send_response);
    };
}

uint64_t rai::WalletRpc::CurrentTimestamp() const
{
    uint64_t now = 0;
    bool error = wallets_->CurrentTimestamp(now);  // server time
    if (!error)
    {
        return now;
    }

    return rai::CurrentTimestamp();  // local time
}

rai::Ptree rai::WalletRpc::MakeResponse_(
    const rai::WalletRpcAction& result, const rai::Amount& amount,
    const std::shared_ptr<rai::Block>& source) const
{
    rai::Ptree response;

    response.put("notify", result.desc_);

    if (result.error_code_ != rai::ErrorCode::SUCCESS)
    {
        response.put("error", rai::ErrorString(result.error_code_));
    }
    else
    {
        response.put("success", "");
        rai::Ptree block_ptree;
        result.block_->SerializeJson(block_ptree);
        response.put_child("block", block_ptree);
        response.put("hash", result.block_->Hash().StringHex());
        response.put("amount", amount.StringDec());
        response.put("amount_in_rai", amount.StringBalance(rai::RAI) + " RAI");
        if (source)
        {
            rai::Ptree ptree_source;
            source->SerializeJson(ptree_source);
            response.put_child("source_block", ptree_source);
        }
    }

    if (!result.request_.empty())
    {
        response.put_child("request", result.request_);
    }

    return response;
}

void rai::WalletRpc::ProcessPendings_(std::unique_lock<std::mutex>& lock,
                                      bool& loop)
{
    loop = false;

    if (actions_.empty() || waiting_callback_)
    {
        return;
    }

    auto it = actions_.begin();
    if (!it->processed_)
    {
        return;
    }

    if (it->callback_error_)
    {
        if (it->last_callback_ + 5 > CurrentTimestamp())
        {
            return;
        }
    }

    rai::Amount amount(0);
    std::shared_ptr<rai::Block> source(nullptr);
    if (it->error_code_ == rai::ErrorCode::SUCCESS)
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, false);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            rai::Log::Error(
                "WalletRpc::ProcessCallback_: failed to construct transaction");
            return;
        }

        if (!ledger_.BlockExists(transaction, it->block_->Hash()))
        {
            rai::Log::Error(
                "WalletRpc::ProcessCallback_: block doesn't exist, hash="
                + it->block_->Hash().StringHex());
            actions_.modify(it, [](rai::WalletRpcAction& data) {
                data.error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
            });
            loop = true;
            return;
        }

        rai::AccountInfo info;
        bool error =
            ledger_.AccountInfoGet(transaction, it->block_->Account(), info);
        if (error || !info.Confirmed(it->block_->Height()))
        {
            return;
        }

        if (it->block_->Height() == 0)
        {
            error = it->block_->Amount(amount);
            if (error)
            {
                rai::Log::Error(
                    "WalletRpc::ProcessCallback_: invalid balance, block hash="
                    + it->block_->Hash().StringHex());
            }
        }
        else
        {
            std::shared_ptr<rai::Block> previous(nullptr);
            error =
                ledger_.BlockGet(transaction, it->block_->Previous(), previous);
            if (error)
            {
                rai::Log::Error(
                    "WalletRpc::ProcessCallback_: failed to get previous "
                    "block, hash="
                    + it->block_->Previous().StringHex());
            }
            else
            {
                it->block_->Amount(*previous, amount);
            }
        }

        if (it->block_->Opcode() == rai::BlockOpcode::RECEIVE)
        {
            error = ledger_.SourceGet(transaction, it->block_->Link(), source);
            if (error || source == nullptr)
            {
                rai::Log::Error(
                    "WalletRpc::ProcessCallback_: failed to get source "
                    "block, hash="
                    + it->block_->Link().StringHex());
            }
        }
    }

    uint64_t timestamp = CurrentTimestamp();
    rai::Ptree response = MakeResponse_(*it, amount, source);
    actions_.modify(it, [timestamp](rai::WalletRpcAction& data) {
        data.callback_error_ = false;
        data.last_callback_ = timestamp;
    });
    waiting_callback_ = true;

    lock.unlock();

    SendCallback(response);

    lock.lock();
}

void rai::WalletRpc::ProcessAutoCredit_(std::unique_lock<std::mutex>& lock,
                                        bool& loop)
{
    loop = false;
    if (!config_.auto_credit_)
    {
        return;
    }

    bool credit = false;

    lock.unlock();
    do
    {
        rai::Account account = wallets_->SelectedAccount();
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, false);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            rai::Log::Error(
                "WalletRpc::ProcessCredit_: failed to construct transaction");
            break;
        }

        rai::AccountInfo info;
        bool error = ledger_.AccountInfoGet(transaction, account, info);
        if (error || !info.Valid() || info.Restricted())
        {
            break;
        }

        std::shared_ptr<rai::Block> block(nullptr);
        error = ledger_.BlockGet(transaction, info.head_, block);
        if (error)
        {
            rai::Log::Error(
                "WalletRpc::ProcessCredit_: failed to get block, hash="
                + info.head_.StringHex());
            break;
        }

        if (block->Counter() < block->Credit() * rai::TRANSACTIONS_PER_CREDIT)
        {
            break;
        }

        if (block->Balance() < rai::CreditPrice(CurrentTimestamp()))
        {
            break;
        }

        credit = true;
    } while (0);
    lock.lock();

    if (!credit)
    {
        return;
    }

    rai::WalletRpcAction action;
    action.sequence_ = sequence_++;
    action.desc_ = "account_credit";
    actions_.push_back(action);
    waiting_wallet_ = true;
    loop = true;

    lock.unlock();
    rai::AccountActionCallback callback =
        AccountActionCallback(action.sequence_);
    rai::ErrorCode error_code = wallets_->AccountCredit(1, callback);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, nullptr);
    }

    lock.lock();
}

void rai::WalletRpc::ProcessAutoReceive_(std::unique_lock<std::mutex>& lock,
                                         bool& loop)
{
    loop = false;
    if (!config_.auto_receive_)
    {
        return;
    }

    bool receive = false;

    lock.unlock();
    rai::BlockHash link;
    rai::Account account = wallets_->SelectedAccount();
    do
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, false);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            rai::Log::Error(
                "WalletRpc::ProcessReceive_: failed to construct transaction");
            break;
        }

        rai::ReceivableInfos receivables;
        bool error = ledger_.ReceivableInfosGet(transaction, account,
                                                rai::ReceivableInfosType::ALL,
                                                receivables, 1);
        if (error)
        {
            rai::Log::Error(
                "WalletRpc::ProcessReceive_: failed to get receivables");
            break;
        }

        auto it = receivables.begin();
        if (it == receivables.end())
        {
            break;
        }

        uint64_t now = CurrentTimestamp();
        if (it->first.timestamp_ > now + 30
            || it->first.amount_ < config_.receive_mininum_)
        {
            break;
        }

        rai::AccountInfo info;
        error = ledger_.AccountInfoGet(transaction, account, info);
        if (error || !info.Valid())
        {
            if (it->first.amount_ < rai::CreditPrice(now))
            {
                break;
            }
        }
        else
        {
            if (info.Restricted())
            {
                break;
            }

            std::shared_ptr<rai::Block> block(nullptr);
            error = ledger_.BlockGet(transaction, info.head_, block);
            if (error)
            {
                rai::Log::Error(
                    "WalletRpc::ProcessReceive_: failed to get block, hash="
                    + info.head_.StringHex());
                break;
            }

            if (block->Counter()
                >= block->Credit() * rai::TRANSACTIONS_PER_CREDIT)
            {
                break;
            }
        }

        receive = true;
        link = it->second;
    } while (0);
    lock.lock();

    if (!receive)
    {
        return;
    }

    rai::WalletRpcAction action;
    action.sequence_ = sequence_++;
    action.desc_ = "account_receive";
    actions_.push_back(action);
    waiting_wallet_ = true;
    loop = true;

    lock.unlock();

    rai::AccountActionCallback callback =
        AccountActionCallback(action.sequence_);
    rai::ErrorCode error_code =
        wallets_->AccountReceive(account, link, callback);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, nullptr);
    }

    lock.lock();
}

void rai::WalletRpc::ProcessRequests_(std::unique_lock<std::mutex>& lock,
                                      bool& loop)
{
    loop = false;

    if (requests_.empty())
    {
        return;
    }

    rai::Ptree request = requests_.front();
    requests_.pop_front();

    std::string account_action = request.get<std::string>("action");

    rai::WalletRpcAction action;
    action.sequence_ = sequence_++;
    action.desc_ = account_action;
    action.request_ = request;
    actions_.push_back(action);
    waiting_wallet_ = true;
    loop = true;
    lock.unlock();

    rai::AccountActionCallback callback =
        AccountActionCallback(action.sequence_);
    rai::ErrorCode error_code;

    do
    {
        if (account_action == "account_send")
        {
            rai::Account to;
            rai::Amount amount;
            std::vector<uint8_t> extensions;
            error_code = rai::WalletRpcHandler::ParseAccountSend(
                request, to, amount, extensions);
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                callback(error_code, nullptr);
                break;
            }
            error_code =
                wallets_->AccountSend(to, amount, extensions, callback);
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                callback(error_code, nullptr);
                break;
            }
        }
        else if (account_action == "account_change")
        {
            rai::Account rep;
            std::vector<uint8_t> extensions;
            error_code = rai::WalletRpcHandler::ParseAccountChange(request, rep,
                                                                   extensions);
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                callback(error_code, nullptr);
                break;
            }
            error_code = wallets_->AccountChange(rep, extensions, callback);
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                callback(error_code, nullptr);
                break;
            }
        }
        else
        {
            callback(rai::ErrorCode::RPC_UNKNOWN_ACTION, nullptr);
        }
    } while (0);

    lock.lock();
}

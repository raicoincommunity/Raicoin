#include <rai/node/rpc.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <rai/common/log.hpp>
#include <rai/common/stat.hpp>
#include <rai/node/node.hpp>


rai::NodeRpcConfig::NodeRpcConfig()
    : enable_(false),
      address_(boost::asio::ip::address_v4::any()),
      port_(rai::NodeRpcConfig::DEFAULT_PORT),
      enable_control_(false),
      whitelist_({
          boost::asio::ip::address_v4::loopback(),
      })
{
}

rai::ErrorCode rai::NodeRpcConfig::DeserializeJson(bool& upgraded,
                                                   rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    try
    {
        error_code = rai::ErrorCode::JSON_CONFIG_RPC_VERSION;
        std::string version_str = ptree.get<std::string>("version");
        uint32_t version = 0;
        bool error = rai::StringToUint(version_str, version);
        IF_ERROR_RETURN(error, error_code);

        error_code = UpgradeJson(upgraded, version, ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_ENABLE;
        enable_ = ptree.get<bool>("enable");

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_ADDRESS;
        std::string address = ptree.get<std::string>("address");
        boost::system::error_code ec;
        address_ = boost::asio::ip::make_address_v4(address, ec);
        if (ec)
        {
            return error_code;
        }

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_PORT;
        port_ = ptree.get<uint16_t>("port");

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_ENABLE_CONTROL;
        enable_control_ = ptree.get<bool>("enable_control");

        error_code = rai::ErrorCode::JSON_CONFIG_RPC_WHITELIST;
        whitelist_.clear();
        auto whitelist = ptree.get_child("whitelist");
        for (const auto& i : whitelist)
        {
            address = i.second.get<std::string>("");
            boost::asio::ip::address_v4 ip =
                boost::asio::ip::make_address_v4(address, ec);
            if (ec)
            {
                return error_code;
            }
            whitelist_.push_back(ip);
        }
    }
    catch (const std::exception&)
    {
        return error_code;
    }
    return rai::ErrorCode::SUCCESS;
}

void rai::NodeRpcConfig::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("version", "1");
    ptree.put("enable", enable_);
    ptree.put("address", address_.to_string());
    ptree.put("port", port_);
    ptree.put("enable_control", enable_control_);
    boost::property_tree::ptree whitelist;
    for (const auto& i : whitelist_)
    {
        boost::property_tree::ptree entry;
        entry.put("", i.to_string());
        whitelist.push_back(std::make_pair("", entry));
    }
    ptree.add_child("whitelist", whitelist);
}

rai::ErrorCode rai::NodeRpcConfig::UpgradeJson(bool& upgraded, uint32_t version,
                                               rai::Ptree& ptree) const
{
    switch (version)
    {
        case 1:
        {
            break;
        }
        default:
        {
            return rai::ErrorCode::CONFIG_RPC_VERSION;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

rai::RpcConfig rai::NodeRpcConfig::RpcConfig() const
{
    return rai::RpcConfig{address_, port_, enable_control_, whitelist_};
}

rai::NodeRpcHandler::NodeRpcHandler(
    rai::Node& node, rai::Rpc& rpc, const std::string& body,
    const boost::asio::ip::address_v4& ip,
    const std::function<void(const rai::Ptree&)>& send_response)
    : RpcHandler(rpc, body, ip, send_response), node_(node)
{
}

void rai::NodeRpcHandler::ProcessImpl()
{
    std::string action = request_.get<std::string>("action");

    if (action == "account_count")
    {
        AccountCount();
    }
    else if (action == "account_forks")
    {
        AccountForks();
    }
    else if (action == "account_heads")
    {
        AccountHeads();
    }
    else if (action == "account_info")
    {
        AccountInfo();
    }
    else if (action == "account_subscribe")
    {
        AccountSubscribe();
    }
    else if (action == "account_unsubscribe")
    {
        AccountUnsubscribe();
    }
    else if (action == "active_account_heads")
    {
        ActiveAccountHeads();
    }
    else if (action == "block_confirm")
    {
        BlockConfirm();
    }
    else if (action == "block_count")
    {
        BlockCount();
    }
    else if (action == "block_dump")
    {
        BlockDump();
    }
    else if (action == "block_dump_off")
    {
        if (!CheckControl_())
        {
            BlockDumpOff();
        }
    }
    else if (action == "block_dump_on")
    {
        if (!CheckControl_())
        {
            BlockDumpOn();
        }
    }
    else if (action == "block_processor_status")
    {
        BlockProcessorStatus();
    }
    else if (action == "block_publish")
    {
        BlockPublish();
    }
    else if (action == "block_query")
    {
        BlockQuery();
    }
    else if (action == "blocks_query")
    {
        BlocksQuery();
    }
    else if (action == "bootstrap_status")
    {
        BootstrapStatus();
    }
    else if (action == "confirm_manager_status")
    {
        ConfirmManagerStatus();
    }
    else if (action == "delegator_list")
    {
        DelegatorList();
    }
    else if (action == "election_count")
    {
        ElectionCount();
    }
    else if (action == "election_info")
    {
        ElectionInfo();
    }
    else if (action == "elections")
    {
        Elections();
    }
    else if (action == "event_subscribe")
    {
        EventSubscribe();
    }
    else if (action == "event_unsubscribe")
    {
        EventUnsubscribe();
    }
    else if (action == "forks")
    {
        Forks();
    }
    else if (action == "full_peer_count")
    {
        FullPeerCount();
    }
    else if (action == "message_dump")
    {
        MessageDump();
    }
    else if (action == "message_dump_off")
    {
        if (!CheckControl_())
        {
            MessageDumpOff();
        }
    }
    else if (action == "message_dump_on")
    {
        if (!CheckControl_())
        {
            MessageDumpOn();
        }
    }
    else if (action == "node_account")
    {
        NodeAccount();
    }
    else if (action == "peer_count")
    {
        PeerCount();
    }
    else if (action == "peers")
    {
        Peers();
    }
    else if (action == "peers_verbose")
    {
        PeersVerbose();
    }
    else if (action == "querier_status")
    {
        QuerierStatus();
    }
    else if (action == "receivable_count")
    {
        ReceivableCount();
    }
    else if (action == "receivables")
    {
        Receivables();
    }
    else if (action == "rewardable")
    {
        Rewardable();
    }
    else if (action == "rewardables")
    {
        Rewardables();
    }
    else if (action == "rewarder_status")
    {
        RewarderStatus();
    }
    else if (action == "richlist")
    {
        RichList();
    }
    else if (action == "stats")
    {
        Stats();
    }
    else if (action == "stats_verbose")
    {
        StatsVerbose();
    }
    else if (action == "stats_clear")
    {
        if (!CheckControl_())
        {
            StatsClear();
        }
    }
    else if (action == "stop")
    {
        if (!CheckLocal_())
        {
            Stop();
        }
    }
    else if (action == "subscriber_count")
    {
        SubscriberCount();
    }
    else if (action == "subscribers")
    {
        Subscribers();
    }
    else if (action == "supply")
    {
        Supply();
    }
    else if (action == "syncer_status")
    {
        SyncerStatus();
    }
    else
    {
        error_code_ = rai::ErrorCode::RPC_UNKNOWN_ACTION;
    }
}

void rai::NodeRpcHandler::AccountCount()
{
    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    size_t count = 0;
    bool error = node_.ledger_.AccountCount(transaction, count);
    if (error)
    {
        error_code_ = rai::ErrorCode::LEDGER_ACCOUNT_COUNT;
        return;
    }

    response_.put("count", count);
}

void rai::NodeRpcHandler::AccountForks()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    rai::Ptree forks;
    rai::Iterator i = node_.ledger_.ForkLowerBound(transaction, account);
    rai::Iterator n = node_.ledger_.ForkUpperBound(transaction, account);
    for (; i != n; ++i)
    {
        std::shared_ptr<rai::Block> first(nullptr);
        std::shared_ptr<rai::Block> second(nullptr);
        error = node_.ledger_.ForkGet(i, first, second);
        if (error || first->Account() != account)
        {
            transaction.Abort();
            return;
        }

        rai::Ptree fork;
        rai::Ptree block_first;
        rai::Ptree block_second;
        first->SerializeJson(block_first);
        second->SerializeJson(block_second);
        fork.put_child("block_first", block_first);
        fork.put_child("block_second", block_second);
        forks.push_back(std::make_pair("", fork));
    }
    response_.put_child("forks", forks);
}

void rai::NodeRpcHandler::AccountHeads()
{
    rai::Account next;
    bool error = GetNext_(next);
    IF_ERROR_RETURN_VOID(error);
    
    uint64_t count;
    error = GetCount_(count);
    IF_ERROR_RETURN_VOID(error);
    if (count == 0 || count > 1000)
    {
        count = 1000;
    }

    std::vector<rai::BlockType> types;
    error = GetAccountTypes_(types);
    IF_ERROR_RETURN_VOID(error);

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    bool more = true;
    rai::AccountInfo info;
    rai::Ptree heads;
    while (count--)
    {
        error = node_.ledger_.NextAccountInfo(transaction, next, info);
        if (error)
        {
            next = 0;
            more = false;
            break;
        }

        if (info.Valid() && rai::Contain(types, info.type_))
        {
            rai::Ptree entry;
            entry.put("account", next.StringAccount());
            entry.put("height", std::to_string(info.head_height_));
            entry.put("head", info.head_.StringHex());
            heads.push_back(std::make_pair("", entry));
        }

        next += 1;
    }

    response_.put("status", "success");
    response_.put("more", more ? "true" : "false");
    response_.put("next", next.StringAccount());
    response_.put_child("heads", heads);
}

void rai::NodeRpcHandler::AccountInfo()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    rai::AccountInfo info;
    error = node_.ledger_.AccountInfoGet(transaction, account, info);
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
    error = node_.ledger_.BlockGet(transaction, info.head_, head_block);
    if (error || head_block == nullptr)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
        return;
    }
    rai::Ptree head_ptree;
    head_block->SerializeJson(head_ptree);
    response_.add_child("head_block", head_ptree);
    AppendBlockAmount_(transaction, *head_block, "head_block_");

    std::shared_ptr<rai::Block> tail_block(nullptr);
    error = node_.ledger_.BlockGet(transaction, info.tail_, tail_block);
    if (error || tail_block == nullptr)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
        return;
    }
    rai::Ptree tail_ptree;
    tail_block->SerializeJson(tail_ptree);
    response_.add_child("tail_block", tail_ptree);
    AppendBlockAmount_(transaction, *tail_block, "tail_block_");

    if (info.confirmed_height_ != rai::Block::INVALID_HEIGHT)
    {
        std::shared_ptr<rai::Block> confirmed_block(nullptr);
        if (info.confirmed_height_ == info.head_height_)
        {
            confirmed_block = head_block;
        }
        else
        {
            error = node_.ledger_.BlockGet(transaction, account, info.confirmed_height_, confirmed_block);
            if (error || confirmed_block == nullptr)
            {
                error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
                return;
            }
        }
        rai::Ptree confirmed_ptree;
        confirmed_block->SerializeJson(confirmed_ptree);
        response_.add_child("confirmed_block", confirmed_ptree);
        AppendBlockAmount_(transaction, *confirmed_block, "confirmed_block_");
    }
}

void rai::NodeRpcHandler::AccountSubscribe()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    uint64_t timestamp;
    error = GetTimestamp_(timestamp);
    IF_ERROR_RETURN_VOID(error);

    rai::Signature signature;
    bool has_signature = true;
    error = GetSignature_(signature);
    if (error)
    {
        if (error_code_ != rai::ErrorCode::RPC_MISS_FIELD_SIGNATURE)
        {
            return;
        }
        error_code_   = rai::ErrorCode::SUCCESS;
        has_signature = false;
    }

    if (has_signature)
    {
        error_code_ =
            node_.subscriptions_.Subscribe(account, timestamp, signature);
    }
    else
    {
        error_code_ = node_.subscriptions_.Subscribe(account, timestamp);
    }
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    response_.put("success", "");
    response_.put("verified", has_signature ? "true" : "false");
}

void rai::NodeRpcHandler::AccountUnsubscribe()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    node_.subscriptions_.Unsubscribe(account);
    response_.put("success", "");
}

void rai::NodeRpcHandler::ActiveAccountHeads()
{
    rai::Account next;
    bool error = GetNext_(next);
    IF_ERROR_RETURN_VOID(error);
    
    uint64_t count;
    error = GetCount_(count);
    IF_ERROR_RETURN_VOID(error);
    if (count == 0 || count > 1000)
    {
        count = 1000;
    }

    std::vector<rai::BlockType> types;
    error = GetAccountTypes_(types);
    IF_ERROR_RETURN_VOID(error);

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    bool more = true;
    rai::AccountInfo info;
    rai::Ptree heads;
    while (count--)
    {
        error = node_.active_accounts_.Next(next);
        if (error)
        {
            next = 0;
            more = false;
            break;
        }

        error = node_.ledger_.AccountInfoGet(transaction, next, info);
        if (error || !info.Valid())
        {
            next += 1;
            continue;
        }

        if (rai::Contain(types, info.type_))
        {
            rai::Ptree entry;
            entry.put("account", next.StringAccount());
            entry.put("height", std::to_string(info.head_height_));
            entry.put("head", info.head_.StringHex());
            heads.push_back(std::make_pair("", entry));
        }

        next += 1;
    }

    response_.put("status", "success");
    response_.put("more", more ? "true" : "false");
    response_.put("next", next.StringAccount());
    response_.put_child("heads", heads);
}

void rai::NodeRpcHandler::BlockConfirm()
{
    rai::Account account;
    uint64_t height = 0;
    rai::BlockHash hash;
    boost::optional<rai::Ptree&> block_ptree_o =
        request_.get_child_optional("block");
    if (block_ptree_o)
    {
        std::shared_ptr<rai::Block> block =
            rai::DeserializeBlockJson(error_code_, *block_ptree_o);
        IF_NOT_SUCCESS_RETURN_VOID(error_code_);
        if (block == nullptr)
        {
            error_code_ = rai::ErrorCode::UNEXPECTED;
            return;
        }
        account = block->Account();
        height = block->Height();
        hash = block->Hash();
    }
    else
    {
        bool error = GetAccount_(account);
        IF_ERROR_RETURN_VOID(error);
        error = GetHeight_(height);
        IF_ERROR_RETURN_VOID(error);
        error = GetHash_(hash);
        IF_ERROR_RETURN_VOID(error);
    }

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::AccountInfo info;
    bool error = node_.ledger_.AccountInfoGet(transaction, account, info);
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

    std::shared_ptr<rai::Block> block(nullptr);
    if (height > info.head_height_)
    {
        error = node_.ledger_.RollbackBlockGet(transaction, hash, block);
        if (error)
        {
            response_.put("status", "miss");
            return;
        }

        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        response_.add_child("block", block_ptree);
        response_.put("status", "rollback");
        return;
    }

    bool fork = false;
    error = node_.ledger_.BlockGet(transaction, hash, block);
    if (error)
    {
        fork = true;
        error = node_.ledger_.BlockGet(transaction, account, height, block);
        if (error)
        {
            error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
            return;
        }
    }
    response_.put("status", fork ? "fork" : "success");
    rai::Ptree block_ptree;
    block->SerializeJson(block_ptree);
    response_.add_child("block", block_ptree);
    AppendBlockAmount_(transaction, *block);

    bool confirmed = info.Confirmed(height);
    response_.put("confirmed", confirmed ? "true" : "false");
    if (confirmed)
    {
        return;
    }

    auto notify_o = request_.get_optional<std::string>("notify");
    if (notify_o && *notify_o == "true")
    {
        error_code_ = node_.subscriptions_.Subscribe(*block);
        if (error_code_ != rai::ErrorCode::SUCCESS)
        {
            response_.clear();
            return;
        }
    }

    node_.elections_.Add(block);
}

void rai::NodeRpcHandler::BlockCount()
{
    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    size_t count = 0;
    bool error   = node_.ledger_.BlockCount(transaction, count);
    if (error)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_COUNT;
        return;
    }

    response_.put("count", count);
}

void rai::NodeRpcHandler::BlockDump()
{
    response_.put_child("blocks", node_.dumpers_.block_.Get());
}

void rai::NodeRpcHandler::BlockDumpOff()
{
    node_.dumpers_.block_.Off();
    response_.put("success", "");
}

void rai::NodeRpcHandler::BlockDumpOn()
{
    rai::Account account;
    bool error = GetAccount_(account);
    if (error)
    {
        if (error_code_ != rai::ErrorCode::RPC_MISS_FIELD_ACCOUNT)
        {
            return;
        }
        error_code_ = rai::ErrorCode::SUCCESS;
    }

    rai::Account root;
    auto root_o = request_.get_optional<std::string>("root");
    if (root_o && root.DecodeAccount(*root_o))
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_ROOT;
        return;
    }

    node_.dumpers_.block_.On(account, root);
    response_.put("success", "");
}

void rai::NodeRpcHandler::BlockProcessorStatus()
{
    node_.block_processor_.Status(response_);
}

void rai::NodeRpcHandler::BlockPublish()
{
    boost::optional<rai::Ptree&> block_ptree =
        request_.get_child_optional("block");
    if (!block_ptree)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_BLOCK;
        return;
    }

    std::shared_ptr<rai::Block> block =
        rai::DeserializeBlockJson(error_code_, *block_ptree);
    if (error_code_ != rai::ErrorCode::SUCCESS || block == nullptr)
    {
        return;
    }

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }
    rai::AccountInfo info;
    bool error =
        node_.ledger_.AccountInfoGet(transaction, block->Account(), info);
    if (!error && info.Restricted())
    {
        error_code_ = rai::ErrorCode::ACCOUNT_RESTRICTED;
        return;
    }

    if (info.Valid() && block->Height() <= info.head_height_)
    {
        error_code_ = rai::ErrorCode::BLOCK_HEIGHT_DUPLICATED;
        return;
    }

    node_.ReceiveBlock(block, boost::none);
    response_.put("success", "");
}

void rai::NodeRpcHandler::BlockQuery()
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

void rai::NodeRpcHandler::BlockQueryByPrevious()
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

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }
    rai::AccountInfo info;
    error = node_.ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        response_.put("status", "miss");
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    if (height == 0)
    {
        error = node_.ledger_.BlockGet(transaction, account, height, block);
        if (error)
        {
            error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
            return;
        }
        response_.put("status", "success");
        response_.put("confirmed", info.Confirmed(height) ? "true" : "false");
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        response_.add_child("block", block_ptree);
        AppendBlockAmount_(transaction, *block);
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
    error = node_.ledger_.BlockSuccessorGet(transaction, previous, successor);
    if (error)
    {
        error = node_.ledger_.BlockGet(transaction, account, height - 1, block);
        if (error)
        {
            error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
            return;
        }
        response_.put("status", "fork");
        response_.put("confirmed",
                      info.Confirmed(height - 1) ? "true" : "false");
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        response_.add_child("block", block_ptree);
        AppendBlockAmount_(transaction, *block);
    }
    else
    {
        if (successor.IsZero())
        {
            response_.put("status", "miss");
            return;
        }
        error = node_.ledger_.BlockGet(transaction, successor, block);
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
        response_.put("confirmed", info.Confirmed(height) ? "true" : "false");
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        response_.add_child("block", block_ptree);
        AppendBlockAmount_(transaction, *block);
    }
}

void rai::NodeRpcHandler::BlockQueryByHash()
{
    rai::BlockHash hash;
    bool error = GetHash_(hash);
    IF_ERROR_RETURN_VOID(error);

    bool raw = false;
    auto raw_o = request_.get_optional<std::string>("raw");
    if (raw_o && *raw_o != "false")
    {
        raw = true;
    }

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::BlockHash successor;
    error = node_.ledger_.BlockGet(transaction, hash, block, successor);
    if (!error && block != nullptr)
    {
        response_.put("status", "success");
        response_.put("successor", successor.StringHex());
    }
    else
    {
        error = node_.ledger_.RollbackBlockGet(transaction, hash, block);
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

    AppendBlockAmount_(transaction, *block);

    rai::Ptree block_ptree;
    block->SerializeJson(block_ptree);
    response_.add_child("block", block_ptree);
    if (raw)
    {
        std::vector<uint8_t> bytes;
        {
            rai::VectorStream stream(bytes);
            block->Serialize(stream);
        }
        response_.put("block_raw", rai::BytesToHex(bytes.data(), bytes.size()));
    }
}

void rai::NodeRpcHandler::BlockQueryByHeight()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    uint64_t height;
    error = GetHeight_(height);
    IF_ERROR_RETURN_VOID(error);

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    
    rai::AccountInfo info;
    error = node_.ledger_.AccountInfoGet(transaction, account, info);
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
    error = node_.ledger_.BlockGet(transaction, account, height, block);
    if (error)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
        return;
    }
    response_.put("status", "success");
    response_.put("confirmed", info.Confirmed(height) ? "true" : "false");
    rai::Ptree block_ptree;
    block->SerializeJson(block_ptree);
    response_.add_child("block", block_ptree);
    AppendBlockAmount_(transaction, *block);
}

void rai::NodeRpcHandler::BlocksQuery()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    uint64_t height;
    error = GetHeight_(height);
    IF_ERROR_RETURN_VOID(error);

    uint64_t count = 0;
    error = GetCount_(count);
    IF_ERROR_RETURN_VOID(error);
    if (count == 0 || count > 100)
    {
        count = 100;
    }

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::AccountInfo info;
    error = node_.ledger_.AccountInfoGet(transaction, account, info);
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
    rai::BlockHash successor;
    error =
        node_.ledger_.BlockGet(transaction, account, height, block, successor);
    if (error)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
        return;
    }

    bool more = true;
    std::vector<std::shared_ptr<rai::Block>> blocks;
    blocks.push_back(block);
    while (blocks.size() < count)
    {
        if (successor.IsZero())
        {
            more = false;
            break;
        }

        error =
            node_.ledger_.BlockGet(transaction, successor, block, successor);
        if (error)
        {
            error_code_ = rai::ErrorCode::LEDGER_BLOCK_GET;
            return;
        }
        blocks.push_back(block);
    }

    rai::Ptree blocks_ptree;
    for (auto &i : blocks)
    {
        rai::Ptree entry;
        i->SerializeJson(entry);
        blocks_ptree.push_back(std::make_pair("", entry));
    }

    response_.put("status", "success");
    response_.put("more", more ? "true" : "false");
    response_.put_child("blocks", blocks_ptree);
}

void rai::NodeRpcHandler::BootstrapStatus()
{
    response_.put("count", node_.bootstrap_.Count());
    response_.put("waiting_syncer", node_.bootstrap_.WaitingSyncer());
}

void rai::NodeRpcHandler::ConfirmManagerStatus()
{
    response_ = node_.confirm_manager_.Status();
}

void rai::NodeRpcHandler::DelegatorList()
{
    uint64_t count = 1000;
    bool error = GetCount_(count);
    if (error && error_code_ != rai::ErrorCode::RPC_MISS_FIELD_COUNT)
    {
        return;
    }

    rai::Account rep(node_.account_);
    auto rep_o = request_.get_optional<std::string>("representative");
    if (rep_o)
    {
        if (rep.DecodeAccount(*rep_o))
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_REP;
            return;
        }
    }
    error_code_ = rai::ErrorCode::SUCCESS;

    auto list = node_.ledger_.GetDelegatorList(rep, count);

    response_.put("representative", rep.StringAccount());
    response_.put("count", list.size());

    rai::Amount total_weight(0);
    rai::Ptree list_ptree;
    for (const auto& i : list)
    {
        rai::Ptree entry;
        total_weight += i.weight_;
        entry.put("account", i.account_.StringAccount());
        entry.put("type", rai::BlockTypeToString(i.type_));
        entry.put("weight", i.weight_.StringDec());
        entry.put("weight_in_rai", i.weight_.StringBalance(rai::RAI) + " RAI");
        list_ptree.push_back(std::make_pair("", entry));
    }
    response_.put_child("list", list_ptree);

    response_.put("total_weight", total_weight.StringDec());
    response_.put("total_weight_in_rai",
                  total_weight.StringBalance(rai::RAI) + " RAI");
}

void rai::NodeRpcHandler::ElectionCount()
{
    response_.put("count", std::to_string(node_.elections_.Size()));
}

void rai::NodeRpcHandler::ElectionInfo()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    rai::Ptree election;
    error = node_.elections_.Get(account, election);
    if (error)
    {
        response_.put("miss", "");
        return;
    }

    response_.put_child("election", election);
}

void rai::NodeRpcHandler::Elections()
{
    auto elections = node_.elections_.GetAll();
    response_.put("count", std::to_string(elections.size()));
    rai::Ptree elections_ptree;
    for (const auto& i : elections)
    {
        rai::Ptree election;
        election.put("account", i.first.StringAccount());
        election.put("height", std::to_string(i.second));
        elections_ptree.push_back(std::make_pair("", election));
    }
    response_.put_child("elections", elections_ptree);
}

void rai::NodeRpcHandler::EventSubscribe()
{
    auto event_o = request_.get_optional<std::string>("event");
    if (!event_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_EVENT;
        return;
    }

    error_code_ = node_.subscriptions_.Subscribe(*event_o);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    response_.put("success", "");
}

void rai::NodeRpcHandler::EventUnsubscribe()
{
    auto event_o = request_.get_optional<std::string>("event");
    if (!event_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_EVENT;
        return;
    }

    rai::SubscriptionEvent event = rai::StringToSubscriptionEvent(*event_o);
    if (event == rai::SubscriptionEvent::INVALID)
    {
        error_code_ = rai::ErrorCode::SUBSCRIPTION_EVENT;
        return;
    }
    node_.subscriptions_.Unsubscribe(event);
    response_.put("success", "");
}

void rai::NodeRpcHandler::Forks()
{
    rai::Account account(0);
    bool error = GetAccount_(account);
    if (error && error_code_ != rai::ErrorCode::RPC_MISS_FIELD_ACCOUNT)
    {
        return;
    }

    uint64_t height = 0;
    error = GetHeight_(height);
    if (error && error_code_ != rai::ErrorCode::RPC_MISS_FIELD_HEIGHT)
    {
        return;
    }

    uint64_t count = 1000;
    error = GetCount_(count);
    if (error && error_code_ != rai::ErrorCode::RPC_MISS_FIELD_COUNT)
    {
        return;
    }

    if (count == 0)
    {
        response_.put("count", std::to_string(0));
        response_.put("forks", "");
        return;
    }

    error_code_ = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::Ptree forks;
    uint64_t i = 0;
    for (; i < count; ++i)
    {
        std::shared_ptr<rai::Block> first(nullptr);
        std::shared_ptr<rai::Block> second(nullptr);
        bool error =
            node_.ledger_.NextFork(transaction, account, height, first, second);
        if (error)
        {
            break;
        }

        if (height == rai::Block::INVALID_HEIGHT)
        {
            account += 1;
            height = 0;
        }
        else
        {
            height += 1;
        }

        rai::Ptree fork;
        rai::Ptree block_first;
        rai::Ptree block_second;
        first->SerializeJson(block_first);
        second->SerializeJson(block_second);
        fork.put_child("block_first", block_first);
        fork.put_child("block_second", block_second);
        forks.push_back(std::make_pair("", fork));
    }
    response_.put("count", std::to_string(i));
    response_.put_child("forks", forks);
}

void rai::NodeRpcHandler::FullPeerCount()
{
    response_.put("count", node_.peers_.FullPeerSize());
}

void rai::NodeRpcHandler::MessageDump()
{
    response_.put_child("messages", node_.dumpers_.message_.Get());
}

void rai::NodeRpcHandler::MessageDumpOff()
{
    node_.dumpers_.message_.Off();
    response_.put("success", "");
}

void rai::NodeRpcHandler::MessageDumpOn()
{
    std::string type;
    auto type_o = request_.get_optional<std::string>("type");
    if (type_o)
    {
        type = *type_o;
    }
    rai::StringTrim(type, " \r\n\t");

    std::string ip;
    auto ip_o = request_.get_optional<std::string>("ip");
    if (ip_o)
    {
        ip = *ip_o;
    }
    rai::StringTrim(ip, " \r\n\t");

    node_.dumpers_.message_.On(type, ip);
    response_.put("success", "");
}

void rai::NodeRpcHandler::NodeAccount()
{
    response_.put("account", node_.account_.StringAccount());
}

void rai::NodeRpcHandler::PeerCount()
{
    response_.put("count", node_.peers_.Size());
}

void rai::NodeRpcHandler::Peers()
{
    rai::Ptree ptree;
    std::vector<rai::Peer> peers = node_.peers_.List();
    for (auto i = peers.begin(), n = peers.end(); i != n; ++i)
    {
        std::stringstream endpoint;
        endpoint << i->Endpoint();
        rai::Ptree entry;
        entry.put("account", i->account_.StringAccount());
        entry.put("endpoint", endpoint.str());
        ptree.push_back(std::make_pair("", entry));
    }
    response_.add_child("peers", ptree);
}

void rai::NodeRpcHandler::PeersVerbose()
{
    rai::Ptree ptree;
    std::vector<rai::Peer> peers = node_.peers_.List();
    for (auto i = peers.begin(), n = peers.end(); i != n; ++i)
    {
        ptree.push_back(std::make_pair("", i->Ptree()));
    }
    response_.add_child("peers", ptree);
}

void rai::NodeRpcHandler::QuerierStatus()
{
    response_.put("entries", node_.block_queries_.Size());
}

void rai::NodeRpcHandler::ReceivableCount()
{
    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    size_t count = 0;
    bool error   = node_.ledger_.ReceivableInfoCount(transaction, count);
    if (error)
    {
        error_code_ = rai::ErrorCode::LEDGER_RECEIVABLE_INFO_COUNT;
        return;
    }

    response_.put("count", count);
}

void rai::NodeRpcHandler::Receivables()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    uint64_t count = 0;
    error = GetCount_(count);
    IF_ERROR_RETURN_VOID(error);

    std::string type_str("all");
    auto type_o = request_.get_optional<std::string>("type");
    if (type_o)
    {
        type_str = *type_o;
    }

    rai::ReceivableInfosType type;
    if (type_str == "confirmed")
    {
        type = rai::ReceivableInfosType::CONFIRMED;
    }
    else if (type_str == "not_confirmed")
    {
        type = rai::ReceivableInfosType::NOT_CONFIRMED;
    }
    else if (type_str == "all")
    {
        type = rai::ReceivableInfosType::ALL;
    }
    else
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_TYPE;
        return;
    }

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    rai::ReceivableInfos receivables;
    error = node_.ledger_.ReceivableInfosGet(transaction, account, type,
                                             receivables, count);
    if (error)
    {
        error_code_ = rai::ErrorCode::LEDGER_RECEIVABLES_GET;
        return;
    }

    response_.put("account", account.StringAccount());
    rai::Ptree receivables_ptree;
    for (const auto& i : receivables)
    {
        rai::Ptree entry;
        entry.put("source", i.first.source_.StringAccount());
        entry.put("amount", i.first.amount_.StringDec());
        entry.put("hash", i.second.StringHex());
        entry.put("timestamp", std::to_string(i.first.timestamp_));

        std::shared_ptr<rai::Block> block(nullptr);
        error = node_.ledger_.BlockGet(transaction, i.second, block);
        if (!error)
        {
            rai::Ptree ptree_block;
            block->SerializeJson(ptree_block);
            entry.put_child("source_block", ptree_block);
        }

        receivables_ptree.push_back(std::make_pair("", entry));
    }
    response_.put_child("receivables", receivables_ptree);
}

void rai::NodeRpcHandler::Rewardable()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    rai::BlockHash hash;
    error = GetHash_(hash);
    IF_ERROR_RETURN_VOID(error);

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    rai::RewardableInfo rewardable;
    error =
        node_.ledger_.RewardableInfoGet(transaction, account, hash, rewardable);
    if (error)
    {
        response_.put("miss", "");
        return;
    }

    response_.put("source", rewardable.source_.StringAccount());
    response_.put("amount", rewardable.amount_.StringDec());
    response_.put("amount_in_rai",
                  rewardable.amount_.StringBalance(rai::RAI) + " RAI");
    response_.put("valid_timestamp",
                  std::to_string(rewardable.valid_timestamp_));
}

void rai::NodeRpcHandler::Rewardables()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    uint64_t count = 0;
    error          = GetCount_(count);
    IF_ERROR_RETURN_VOID(error);
    if (count == 0 || count > 10000)
    {
        count = 10000;
    }

    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    std::multimap<rai::Amount, std::pair<rai::BlockHash, rai::RewardableInfo>,
                  std::greater<rai::Amount>>
        rewardables;
    rai::Iterator i =
        node_.ledger_.RewardableInfoLowerBound(transaction, account);
    rai::Iterator n =
        node_.ledger_.RewardableInfoUpperBound(transaction, account);
    for (; i != n; ++i)
    {
        rai::BlockHash hash;
        rai::RewardableInfo info;
        bool error = node_.ledger_.RewardableInfoGet(i, account, hash, info);
        if (error)
        {
            error_code_ = rai::ErrorCode::LEDGER_REWARDABLE_INFO_GET;
            rai::Stats::Add(error_code_, "NodeRpcHandler::Rewardables");
            return;
        }
        rewardables.emplace(info.amount_, std::make_pair(hash, info));
        if (rewardables.size() > count)
        {
            rewardables.erase(std::prev(rewardables.end()));
        }
    }

    response_.put("account", account.StringAccount());
    rai::Ptree rewardables_ptree;
    for (const auto& i : rewardables)
    {
        rai::Ptree entry;
        entry.put("source", i.second.second.source_.StringAccount());
        entry.put("amount", i.first.StringDec());
        entry.put("amount_in_rai", i.first.StringBalance(rai::RAI) + " RAI");
        entry.put("hash", i.second.first.StringHex());
        entry.put("valid_timestamp",
                  std::to_string(i.second.second.valid_timestamp_));
        rewardables_ptree.push_back(std::make_pair("", entry));
    }
    response_.put_child("rewardables", rewardables_ptree);
}

void rai::NodeRpcHandler::RewarderStatus()
{
    node_.rewarder_.Status(response_);
}

void rai::NodeRpcHandler::RichList()
{
    uint64_t count = 1000;
    bool error = GetCount_(count);
    if (error && error_code_ != rai::ErrorCode::RPC_MISS_FIELD_COUNT)
    {
        return;
    }
    error_code_ = rai::ErrorCode::SUCCESS;

    auto list = node_.ledger_.GetRichList(count);

    response_.put("count", list.size());
    rai::Ptree list_ptree;
    for (const auto& i : list)
    {
        rai::Ptree entry;
        entry.put("account", i.account_.StringAccount());
        entry.put("amount", i.balance_.StringDec());
        entry.put("amount_in_rai", i.balance_.StringBalance(rai::RAI) + " RAI");
        list_ptree.push_back(std::make_pair("", entry));
    }
    response_.put_child("list", list_ptree);

    rai::Amount supply = node_.Supply();
    response_.put("supply", supply.StringDec());
    response_.put("supply_in_rai", supply.StringBalance(rai::RAI) + " RAI");
}

void rai::NodeRpcHandler::Stats()
{
    boost::optional<std::string> type_o =
        request_.get_optional<std::string>("type");
    if (!type_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_TYPE;
        return;
    }

    rai::Ptree stats_ptree;
    if (*type_o == "error")
    {
        auto stats = rai::Stats::GetAll<rai::ErrorCode>();
        for (const auto& stat : stats)
        {
            rai::Ptree stat_ptree;
            stat_ptree.put("code", static_cast<uint32_t>(stat.index_));
            stat_ptree.put("description", rai::ErrorString(stat.index_));
            stat_ptree.put("count", stat.count_);
            stats_ptree.push_back(std::make_pair("", stat_ptree));
        }
    }
    else
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_TYPE;
        return;
    }

    response_.put("type", "error");
    response_.put_child("stats", stats_ptree);
}

void rai::NodeRpcHandler::StatsVerbose()
{
    boost::optional<std::string> type_o =
        request_.get_optional<std::string>("type");
    if (!type_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_TYPE;
        return;
    }

    rai::Ptree stats_ptree;
    if (*type_o == "error")
    {
        auto stats = rai::Stats::GetAll<rai::ErrorCode>();
        for (const auto& stat : stats)
        {
            rai::Ptree stat_ptree;
            stat_ptree.put("code", static_cast<uint32_t>(stat.index_));
            stat_ptree.put("description", rai::ErrorString(stat.index_));
            stat_ptree.put("count", stat.count_);
            if (!stat.details_.empty())
            {
                rai::Ptree desc_ptree;
                for (const auto& desc : stat.details_)
                {
                    rai::Ptree entry;
                    entry.put("", desc);
                    desc_ptree.push_back(std::make_pair("", entry));
                }
                stat_ptree.put_child("details", desc_ptree);
            }
            else
            {
                stat_ptree.put("details", "");
            }

            stats_ptree.push_back(std::make_pair("", stat_ptree));
        }
    }
    else
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_TYPE;
        return;
    }

    response_.put("type", "error");
    response_.put_child("stats", stats_ptree);
}

void rai::NodeRpcHandler::StatsClear()
{
    boost::optional<std::string> type_o =
        request_.get_optional<std::string>("type");
    if (!type_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_TYPE;
        return;
    }

    if (*type_o == "error")
    {
        rai::Stats::ResetAll<rai::ErrorCode>();
    }
    else
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_TYPE;
    }
}

void rai::NodeRpcHandler::Stop()
{
    node_.Stop();
    response_.put("success", "");
}

void rai::NodeRpcHandler::Subscribers()
{
    rai::Ptree ptree;
    std::vector<rai::Account> accounts = node_.subscriptions_.List();
    for (auto i = accounts.begin(), n = accounts.end(); i != n; ++i)
    {
        rai::Ptree entry;
        entry.put("", i->StringAccount());
        ptree.push_back(std::make_pair("", entry));
    }
    response_.put_child("subscribers", ptree);
}

void rai::NodeRpcHandler::SubscriberCount()
{
    response_.put("count", node_.subscriptions_.Size());
}

void rai::NodeRpcHandler::Supply()
{
    rai::Amount supply = node_.Supply();
    response_.put("amount", supply.StringDec());
    response_.put("amount_in_rai", supply.StringBalance(rai::RAI) + " RAI");
}

void rai::NodeRpcHandler::SyncerStatus()
{
    rai::SyncStat stat = node_.syncer_.Stat();
    response_.put("total", stat.total_);
    response_.put("miss", stat.miss_);
    response_.put("size", node_.syncer_.Size());
    response_.put("queries", node_.syncer_.Queries());
}

void rai::NodeRpcHandler::AppendBlockAmount_(rai::Transaction& transaction,
                                             const rai::Block& block,
                                             const std::string& prefix)
{
    bool error = false;
    rai::Amount amount(0);
    if (block.Height() == 0)
    {
        error = block.Amount(amount);
    }
    else
    {
        std::shared_ptr<rai::Block> previous(nullptr);
        error =
            node_.ledger_.BlockGet(transaction, block.Previous(), previous);
        if (!error)
        {
            if (previous != nullptr)
            {
                error = block.Amount(*previous, amount);
            }
            else
            {
                std::string log = rai::ToString(
                    "NodeRpcHandler::AppendBlockAmount_: BlockGet failed, "
                    "hash=",
                    block.Previous().StringHex());
                rai::Log::Error(log);
                error = true;
            }
        }
    }
    if (!error)
    {
        response_.put(prefix + "amount", amount.StringDec());
        response_.put(prefix + "amount_in_rai",
                      amount.StringBalance(rai::RAI) + " RAI");
    }
}
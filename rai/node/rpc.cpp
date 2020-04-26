#include <rai/node/rpc.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <rai/common/stat.hpp>
#include <rai/node/log.hpp>
#include <rai/node/node.hpp>

rai::RpcConfig::RpcConfig()
    : enable_(false),
      address_(boost::asio::ip::address_v4::any()),
      port_(rai::Rpc::DEFAULT_PORT),
      enable_control_(false),
      whitelist_({
          boost::asio::ip::address_v4::loopback(),
      })
{
}

rai::ErrorCode rai::RpcConfig::DeserializeJson(bool& upgraded,
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

void rai::RpcConfig::SerializeJson(rai::Ptree& ptree) const
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

rai::ErrorCode rai::RpcConfig::UpgradeJson(bool& upgraded, uint32_t version,
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

rai::Rpc::Rpc(boost::asio::io_service& service, rai::Node& node,
              const rai::RpcConfig& config)
    : acceptor_(service),
      config_(config),
      node_(node),
      stopped_(ATOMIC_FLAG_INIT)
{
}

void rai::Rpc::Start()
{
    boost::asio::ip::tcp::endpoint endpoint(config_.address_, config_.port_);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

    boost::system::error_code ec;
    acceptor_.bind(endpoint, ec);
    if (ec)
    {
        rai::Log::Network(
            node_,
            boost::str(
                boost::format("Error while binding for RPC on port %1%: %2%")
                % endpoint.port() % ec.message()));
        throw std::runtime_error(ec.message());
    }

    acceptor_.listen();
    // TODO: add observer

    Accept();
}

void rai::Rpc::Stop()
{
    if (stopped_.test_and_set())
    {
        return;
    }
    acceptor_.close();
}

void rai::Rpc::Accept()
{
    auto connection = std::make_shared<rai::RpcConnection>(node_, *this);
    acceptor_.async_accept(
        connection->socket_,
        [this, connection](const boost::system::error_code& ec) {
            if (boost::asio::error::operation_aborted != ec
                && acceptor_.is_open())
            {
                Accept();
            }

            if (!ec)
            {
                connection->Parse();
            }
            else
            {
                rai::Log::Network(
                    this->node_,
                    boost::str(
                        boost::format("Error accepting RPC connections: %1%")
                        % ec.message()));
            }
        });
}

bool rai::Rpc::CheckWhitelist(const boost::asio::ip::address_v4& ip) const
{
    bool error = true;
    for (const auto& i : config_.whitelist_)
    {
        if (ip == i)
        {
            error = false;
            break;
        }
    }
    return error;
}

rai::RpcConnection::RpcConnection(rai::Node& node, rai::Rpc& rpc)
    : node_(node), rpc_(rpc), socket_(node.service_)
{
    responded_.clear();
}

void rai::RpcConnection::Parse()
{
    boost::system::error_code ec;
    auto remote = socket_.remote_endpoint(ec);
    if (ec)
    {
        return;
    }
    if (rpc_.CheckWhitelist(remote.address().to_v4()))
    {
        rai::Log::Rpc(node_,
                      boost::str(boost::format("RPC request from %1% denied")
                                 % remote));
        return;
    }

    Read();
}

void rai::RpcConnection::Read()
{
    auto connection = shared_from_this();
    boost::beast::http::async_read(
        socket_, buffer_, request_,
        [connection](const boost::system::error_code& ec, size_t size) {
            if (ec)
            {
                rai::Log::Rpc(connection->node_,
                              boost::str(boost::format("RPC read error:%1%")
                                         % ec.message()));
                return;
            }

            connection->node_.Background([connection]() {
                auto start = std::chrono::steady_clock::now();
                auto version = connection->request_.version();
                std::string request_id = boost::str(
                    boost::format("%1%")
                    % boost::io::group(
                          std::hex, std::showbase,
                          reinterpret_cast<uintptr_t>(connection.get())));

                auto response_handler =
                    [connection, version, start,
                     request_id](const boost::property_tree::ptree& ptree) {
                        std::stringstream ostream;
                        boost::property_tree::write_json(ostream, ptree);
                        ostream.flush();
                        std::string body = ostream.str();
                        connection->Write(body, version);
                        boost::beast::http::async_write(
                            connection->socket_, connection->response_,
                            [connection](const boost::system::error_code& ec,
                                         size_t size) {
                                if (ec)
                                {
                                    rai::Log::Rpc(
                                        connection->node_,
                                        boost::str(
                                            boost::format("RPC write error:%1%")
                                            % ec.message()));
                                    return;
                                }
                            });
                        rai::Log::Rpc(
                            connection->node_,
                            boost::str(
                                boost::format("RPC request %1% completed in: "
                                              "%2% microseconds")
                                % request_id
                                % std::chrono::duration_cast<
                                      std::chrono::microseconds>(
                                      std::chrono::steady_clock::now() - start)
                                      .count()));
                    };

                if (connection->request_.method()
                    == boost::beast::http::verb::post)
                {
                    rai::RpcHandler rpc_handler(
                        connection->node_, connection->rpc_,
                        connection->request_.body(), request_id,
                        connection->socket_.remote_endpoint().address().to_v4(),
                        response_handler);
                    rpc_handler.Process();
                }
                else
                {
                    rai::Ptree response;
                    response.put("error", "Only POST requests are allowed");
                    response_handler(response);
                }
            });
        });
}

void rai::RpcConnection::Write(const std::string& body, unsigned version)
{
    if (responded_.test_and_set())
    {
        rai::Log::Error(node_, "RPC already responded");
        return;
    }

    response_.set("Content-Type", "application/json");
    response_.set("Access-Control-Allow-Origin", "*");
    response_.set("Access-Control-Allow-Headers",
                  "Accept, Accept-Language, Content-Language, Content-Type");
    response_.set("Connection", "close");
    response_.result(boost::beast::http::status::ok);
    response_.body() = body;
    response_.version(version);
    response_.prepare_payload();
}

rai::RpcHandler::RpcHandler(
    rai::Node& node, rai::Rpc& rpc, const std::string& body,
    const std::string& request_id, const boost::asio::ip::address_v4& ip,
    const std::function<void(const rai::Ptree&)>& send_response)
    : node_(node),
      rpc_(rpc),
      body_(body),
      request_id_(request_id),
      ip_(ip),
      send_response_(send_response),
      error_code_(rai::ErrorCode::SUCCESS)
{
}

void rai::RpcHandler::Check()
{
    if (body_.size() > rai::RpcHandler::MAX_BODY_SIZE)
    {
        error_code_ = rai::ErrorCode::RPC_HTTP_BODY_SIZE;
        return;
    }

    int depth = 0;
    for (auto ch : body_)
    {
        if (ch == '[' || ch == '{')
        {
            ++depth;
        }
        else if (ch == ']' || ch == '}')
        {
            --depth;
        }
        else
        {
            continue;
        }

        if (depth < 0)
        {
            error_code_ = rai::ErrorCode::RPC_JSON;
            return;
        }

        if (depth > rai::RpcHandler::MAX_JSON_DEPTH)
        {
            error_code_ = rai::ErrorCode::RPC_JSON_DEPTH;
            return;
        }
    }
}

void rai::RpcHandler::Process()
{
    try
    {
        Check();
        if (error_code_ != rai::ErrorCode::SUCCESS)
        {
            Response();
            return;
        }

        std::stringstream stream(body_);
        boost::property_tree::read_json(stream, request_);
        std::string action = request_.get<std::string>("action");

        if (action == "account_count")
        {
            AccountCount();
        }
        else if (action == "account_forks")
        {
            AccountForks();
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
        else if (action == "block_count")
        {
            BlockCount();
        }
        else if (action == "block_publish")
        {
            BlockPublish();
        }
        else if (action == "block_query")
        {
            BlockQuery();
        }
        else if (action == "bootstrap_status")
        {
            BootstrapStatus();
        }
        else if (action == "confirm_manager_status")
        {
            ConfirmManagerStatus();
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
        else if (action == "forks")
        {
            Forks();
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
        else if (action == "rewardables")
        {
            Rewardables();
        }
        else if (action == "rewarder_status")
        {
            RewarderStatus();
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
            StatsClear();
        }
        else if (action == "stop")
        {
            Stop();
        }
        else if (action == "subscriber_count")
        {
            SubscriberCount();
        }
        else if (action == "syncer_status")
        {
            SyncerStatus();
        }
        else
        {
            error_code_ = rai::ErrorCode::RPC_UNKNOWN_ACTION;
        }
        response_.put("ack", action);
        auto request_id = request_.get_optional<std::string>("request_id");
        if (request_id)
        {
            response_.put("request_id", *request_id);
        }
    }
    catch (const std::exception& e)
    {
        response_.put("exception", e.what());
    }
    catch (...)
    {
        error_code_ = rai::ErrorCode::RPC_GENERIC;
    }

    Response();
}

void rai::RpcHandler::Response()
{
    if (error_code_ != rai::ErrorCode::SUCCESS || response_.empty())
    {
        rai::Ptree ptree;
        std::string error = "Empty response";
        if (error_code_ != rai::ErrorCode::SUCCESS)
        {
            error = rai::ErrorString(error_code_);
        }
        ptree.put("error", error);
        ptree.put("error_code", static_cast<uint32_t>(error_code_));
        send_response_(ptree);
    }
    else
    {
        send_response_(response_);
    }

    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        rai::Stats::Add(error_code_);
    }
}

void rai::RpcHandler::AccountCount()
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

void rai::RpcHandler::AccountForks()
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

void rai::RpcHandler::AccountInfo()
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
    response_.put("limited",
                  info.forks_ > rai::MaxAllowedForks(rai::CurrentTimestamp()));

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
}

void rai::RpcHandler::AccountSubscribe()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

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
        error_code_ = rai::ErrorCode::SUCCESS;
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

void rai::RpcHandler::AccountUnsubscribe()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    node_.subscriptions_.Unsubscribe(account);
    response_.put("success", "");
}

void rai::RpcHandler::BlockCount()
{
    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    size_t count = 0;
    bool error = node_.ledger_.BlockCount(transaction, count);
    if (error)
    {
        error_code_ = rai::ErrorCode::LEDGER_BLOCK_COUNT;
        return;
    }

    response_.put("count", count);
}

void rai::RpcHandler::BlockPublish()
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
    if (!error && info.forks_ > rai::MaxAllowedForks(rai::CurrentTimestamp()))
    {
        error_code_ = rai::ErrorCode::ACCOUNT_LIMITED;
        return;
    }

    node_.ReceiveBlock(block, boost::none);
    response_.put("success", "");
}

void rai::RpcHandler::BlockQuery()
{
    auto hash_o = request_.get_optional<std::string>("hash");
    if (hash_o)
    {
        BlockQueryByHash();
        return;
    }

    BlockQueryByPrevious();
}

void rai::RpcHandler::BlockQueryByPrevious()
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
    }
}

void rai::RpcHandler::BlockQueryByHash()
{
    rai::BlockHash hash;
    bool error = GetHash_(hash);
    IF_ERROR_RETURN_VOID(error);

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
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        response_.add_child("block", block_ptree);
        response_.put("successor", successor.StringHex());
        return;
    }

    error = node_.ledger_.RollbackBlockGet(transaction, hash, block);
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

void rai::RpcHandler::BootstrapStatus()
{
    response_.put("count", node_.bootstrap_.Count());
    response_.put("waiting_syncer", node_.bootstrap_.WaitingSyncer());
}

void rai::RpcHandler::ConfirmManagerStatus()
{
    response_ = node_.confirm_manager_.Status();
}

void rai::RpcHandler::ElectionCount()
{
    response_.put("count", std::to_string(node_.elections_.Size()));
}

void rai::RpcHandler::ElectionInfo()
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

void rai::RpcHandler::Elections()
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

void rai::RpcHandler::Forks()
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

void rai::RpcHandler::MessageDump()
{
    response_.put_child("messages", node_.dumpers_.message_.Get());
}

void rai::RpcHandler::MessageDumpOff()
{
    node_.dumpers_.message_.Off();
    response_.put("success", "");
}

void rai::RpcHandler::MessageDumpOn()
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

void rai::RpcHandler::Peers()
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

void rai::RpcHandler::PeersVerbose()
{
    rai::Ptree ptree;
    std::vector<rai::Peer> peers = node_.peers_.List();
    for (auto i = peers.begin(), n = peers.end(); i != n; ++i)
    {
        ptree.push_back(std::make_pair("", i->Ptree()));
    }
    response_.add_child("peers", ptree);
}

void rai::RpcHandler::QuerierStatus()
{
    response_.put("entries", node_.block_queries_.Size());
}

void rai::RpcHandler::ReceivableCount()
{
    rai::Transaction transaction(error_code_, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    size_t count = 0;
    bool error = node_.ledger_.ReceivableInfoCount(transaction, count);
    if (error)
    {
        error_code_ = rai::ErrorCode::LEDGER_RECEIVABLE_INFO_COUNT;
        return;
    }

    response_.put("count", count);
}

void rai::RpcHandler::Receivables()
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
        receivables_ptree.push_back(std::make_pair("", entry));
    }
    response_.put_child("receivables", receivables_ptree);
}

void rai::RpcHandler::Rewardables()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    uint64_t count = 0;
    error = GetCount_(count);
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
            rai::Stats::Add(error_code_, "RpcHandler::Rewardables");
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

void rai::RpcHandler::RewarderStatus()
{
    node_.rewarder_.Status(response_);
}

void rai::RpcHandler::Stats()
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


void rai::RpcHandler::StatsVerbose()
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
                for (const auto& desc: stat.details_)
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

void rai::RpcHandler::StatsClear()
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

void rai::RpcHandler::Stop()
{
    if (ip_ != boost::asio::ip::address_v4::loopback())
    {
        error_code_ = rai::ErrorCode::RPC_NOT_LOCALHOST;
        return;
    }
    rpc_.Stop();
    node_.Stop();
    response_.put("success", "");
}

void rai::RpcHandler::SubscriberCount()
{
    response_.put("acount", node_.subscriptions_.Size());
}

void rai::RpcHandler::SyncerStatus()
{
    rai::SyncStat stat = node_.syncer_.Stat();
    response_.put("total", stat.total_);
    response_.put("miss", stat.miss_);
    response_.put("size", node_.syncer_.Size());
    response_.put("queries", node_.syncer_.Queries());
}

bool rai::RpcHandler::CheckControl_()
{
    if (ip_ == boost::asio::ip::address_v4::loopback())
    {
        return false;
    }

    if (rpc_.config_.enable_control_)
    {
        return false;
    }

    error_code_ = rai::ErrorCode::RPC_ENABLE_CONTROL;
    return true;
}


bool rai::RpcHandler::GetAccount_(rai::Account& account)
{
    auto account_o = request_.get_optional<std::string>("account");
    if (!account_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_ACCOUNT;
        return true;
    }

    if (account.DecodeAccount(*account_o))
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_ACCOUNT;
        return true;
    }

    return false;
}

bool rai::RpcHandler::GetCount_(uint64_t& count)
{
    auto count_o = request_.get_optional<std::string>("count");
    if (!count_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_COUNT;
        return true;
    }

    bool error = rai::StringToUint(*count_o, count);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_COUNT;
        return true;
    }

    return false;
}

bool rai::RpcHandler::GetHash_(rai::BlockHash& hash)
{
    auto hash_o = request_.get_optional<std::string>("hash");
    if (!hash_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_HASH;
        return true;
    }

    bool error = hash.DecodeHex(*hash_o);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_HASH;
        return true;
    }

    return false;
}

bool rai::RpcHandler::GetHeight_(uint64_t& height)
{
    auto height_o = request_.get_optional<std::string>("height");
    if (!height_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_HEIGHT;
        return true;
    }

    bool error = rai::StringToUint(*height_o, height);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_HEIGHT;
        return  true;
    }

    return false;
}

bool rai::RpcHandler::GetPrevious_(rai::BlockHash& previous)
{
    auto previous_o = request_.get_optional<std::string>("previous");
    if (!previous_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_PREVIOUS;
        return true;
    }

    bool error = previous.DecodeHex(*previous_o);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_PREVIOUS;
        return true;
    }

    return false;
}

bool rai::RpcHandler::GetSignature_(rai::Signature& signature)
{
    auto signature_o = request_.get_optional<std::string>("signature");
    if (!signature_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_SIGNATURE;
        return true;
    }

    bool error = signature.DecodeHex(*signature_o);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_SIGNATURE;
        return true;
    }

    return false;
}

bool rai::RpcHandler::GetTimestamp_(uint64_t& timestamp)
{
    auto timestamp_o = request_.get_optional<std::string>("timestamp");
    if (!timestamp_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_TIMESTAMP;
        return true;
    }

    bool error = rai::StringToUint(*timestamp_o, timestamp);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_TIMESTAMP;
        return true;
    }

    return false;
}


std::unique_ptr<rai::Rpc> rai::MakeRpc(boost::asio::io_service& service,
                                       rai::Node& node,
                                       const rai::RpcConfig& config)
{
    std::unique_ptr<rai::Rpc> impl;

    impl.reset(new rai::Rpc(service, node, config));

    return impl;
}
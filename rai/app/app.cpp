#include <rai/app/app.hpp>

#include <rai/common/stat.hpp>
#include <rai/common/log.hpp>

rai::AppTrace::AppTrace()
    : message_from_gateway_(false),
      message_to_gateway_(false),
      message_from_client_(false),
      message_to_client_(false)
{
}

rai::App::App(rai::ErrorCode& error_code, boost::asio::io_service& service,
              const boost::filesystem::path& db_path, rai::Alarm& alarm,
              const rai::AppConfig& config, rai::AppSubscriptions& subscribe,
              const std::vector<rai::BlockType>& account_types,
              const rai::Provider::Info& provider_info)
    : service_(service),
      alarm_(alarm),
      config_(config),
      subscribe_(subscribe),
      store_(error_code, db_path),
      ledger_(error_code, store_, rai::LedgerType::APP),
      account_types_(account_types),
      service_runner_(service_gateway_),
      bootstrap_(*this),
      block_confirm_(*this),
      provider_info_(provider_info),
      stopped_(false),
      thread_([this]() { Run(); })
{
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    if (!config.node_gateway_
        || (config.node_gateway_.protocol_ != "ws"
            && config.node_gateway_.protocol_ != "wss"))
    {
        error_code = rai::ErrorCode::APP_INVALID_GATEWAY_URL;
        return;
    }

    const rai::Url& gateway = config_.node_gateway_;
    gateway_ws_ = std::make_shared<rai::WebsocketClient>(
        service_gateway_, gateway.host_, gateway.port_, gateway.path_,
        gateway.protocol_ == "wss");

    if (config_.ws_enable_)
    {
        ws_server_ = std::make_shared<rai::WebsocketServer>(
            service_, config_.ws_ip_, config_.ws_port_);
    }

    provider_actions_ = rai::Provider::ToString(provider_info_.actions_);
}

std::shared_ptr<rai::App> rai::App::Shared()
{
    return shared_from_this();
}

void rai::App::Start()
{
    RegisterObservers_();

    Ongoing(std::bind(&rai::App::ConnectToGateway, this),
            std::chrono::seconds(5));
    Ongoing(std::bind(&rai::AppBootstrap::Run, &bootstrap_),
            std::chrono::seconds(3));
    Ongoing(std::bind(&rai::App::Subscribe, this), std::chrono::seconds(300));
    Ongoing(std::bind(&rai::AppSubscriptions::Cutoff, &subscribe_),
            std::chrono::seconds(5));
    Ongoing(std::bind(&rai::BlockCache::Age, &block_cache_, 300),
            std::chrono::seconds(5));
    Ongoing(std::bind(&rai::BlockWaiting::Age, &block_waiting_, 3600),
            std::chrono::seconds(60));

    if (ws_server_)
    {
        ws_server_->Start();
    }
}

void rai::App::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
    }

    condition_.notify_all();
    if (thread_.joinable())
    {
        thread_.join();
    }

    if (ws_server_)
    {
        ws_server_->Stop();
    }

    alarm_.Stop();
    gateway_ws_->Close();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    service_runner_.Stop();
}

void rai::App::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        if (actions_.empty())
        {
            condition_.wait(lock);
            continue;
        }

        auto front = actions_.begin();
        auto action(std::move(front->second));
        actions_.erase(front);

        lock.unlock();
        action();
        lock.lock();
    }
}

bool rai::App::Busy() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return actions_.size() > 100000;
}


void rai::App::Ongoing(const std::function<void()>& process,
                       const std::chrono::seconds& delay)
{
    process();
    std::weak_ptr<rai::App> app(Shared());
    alarm_.Add(std::chrono::steady_clock::now() + delay,
               [app, process, delay]() {
                   auto app_l = app.lock();
                   if (app_l)
                   {
                       app_l->Ongoing(process, delay);
                   }
               });
}

rai::Ptree rai::App::AccountTypes() const
{
    rai::Ptree ptree;
    for (const auto i : account_types_)
    {
        boost::property_tree::ptree entry;
        entry.put("", rai::BlockTypeToString(i));
        ptree.push_back(std::make_pair("", entry));
    }
    return ptree;
}

size_t rai::App::ActionSize() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return actions_.size();
}

bool rai::App::BlockCacheFull() const
{
    return block_cache_.Size() >= rai::App::MAX_BLOCK_CACHE_SIZE;
}

void rai::App::ConnectToGateway()
{
    gateway_ws_->Run();
    service_runner_.Notify();
}

bool rai::App::GatewayConnected() const
{
    return gateway_ws_
           && gateway_ws_->Status() == rai::WebsocketStatus::CONNECTED;
}

void rai::App::ProcessBlock(const std::shared_ptr<rai::Block>& block,
                            bool confirmed)
{
    if (!rai::Contain(account_types_, block->Type()))
    {
        return;
    }

    if (!confirmed && block_waiting_.Exists(block->Account(), block->Height()))
    {
        block_confirm_.Add(block);
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    {
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            rai::Stats::Add(error_code, "App::ProcessBlock");
            return;
        }

        error_code = AppendBlock_(transaction, block, confirmed);
        switch (error_code)
        {
            case rai::ErrorCode::SUCCESS:
            {
                PullNextBlockAsync(block);
                if (confirmed)
                {
                    QueueWaitings(block);
                }
                break;
            }
            case rai::ErrorCode::APP_PROCESS_LEDGER_ACCOUNT_PUT:
            case rai::ErrorCode::APP_PROCESS_LEDGER_BLOCK_PUT:
            case rai::ErrorCode::APP_PROCESS_LEDGER_SUCCESSOR_SET:
            case rai::ErrorCode::APP_PROCESS_LEDGER_PUT:
            case rai::ErrorCode::APP_PROCESS_LEDGER_DEL:
            {
                std::string error_info = rai::ToString(
                    "App::ProcessBlock: ledger operation error code ",
                    static_cast<uint32_t>(error_code));
                rai::Log::Error(error_info);
                std::cout << error_info << std::endl;
                break;
            }
            case rai::ErrorCode::APP_PROCESS_GAP_PREVIOUS:
            {
                SyncAccountAsync(block->Account());
                break;
            }
            case rai::ErrorCode::APP_PROCESS_PRUNED:
            {
                rai::Stats::AddDetail(
                    error_code, "account=", block->Account().StringAccount(),
                    ", height=", block->Height(),
                    ", hash=", block->Hash().StringHex());
                break;
            }
            case rai::ErrorCode::APP_PROCESS_PREVIOUS_MISMATCH:
            {
                std::shared_ptr<rai::Block> head(nullptr);
                bool error = GetHeadBlock_(transaction, block->Account(), head);
                if (!error)
                {
                    block_confirm_.Add(head);
                }
                break;
            }
            case rai::ErrorCode::APP_PROCESS_FORK:
            {
                if (confirmed)
                {
                    std::shared_ptr<rai::Block> head(nullptr);
                    bool error = GetHeadBlock_(transaction, block->Account(), head);
                    if (error)
                    {
                        std::string error_info = rai::ToString(
                            "App::ProcessBlock: failed to get head block, "
                            "account=",
                            block->Account().StringAccount());
                        rai::Log::Error(error_info);
                        std::cout << error_info << std::endl;
                    }
                    else
                    {
                        QueueBlockRollback(head);
                        QueueBlock(block, confirmed, rai::AppActionPri::HIGH);
                    }
                }
                else
                {
                    block_confirm_.Add(block);
                }
                break;
            }
            case rai::ErrorCode::APP_PROCESS_CONFIRMED_FORK:
            case rai::ErrorCode::APP_PROCESS_EXIST:
            {
                break;
            }
            case rai::ErrorCode::APP_PROCESS_CONFIRMED:
            {
                QueueWaitings(block);
                break;
            }
            case rai::ErrorCode::APP_PROCESS_CONFIRM_REQUIRED:
            {
                block_confirm_.Add(block);
                break;
            }
            case rai::ErrorCode::APP_PROCESS_HALT:
            {
                std::string error_info = rai::ToString(
                    "App::ProcessBlock: fatal error, the app processor is "
                    "halted, block hash=",
                    block->Hash().StringHex());
                rai::Log::Error(error_info);
                std::cout << error_info << std::endl;
                Background([this](){Stop();});
            }
            case rai::ErrorCode::APP_PROCESS_WAITING:
            {
                break;
            }
            case rai::ErrorCode::APP_PROCESS_UNEXPECTED:
            {
                std::string error_info = rai::ToString(
                    "App::ProcessBlock: unexpected error, block hash=",
                    block->Hash().StringHex());
                rai::Log::Error(error_info);
                std::cout << error_info << std::endl;
                break;
            }
            default:
            {
                // log
                std::cout << "App::ProcessBlock: unknown error code "
                          << static_cast<uint32_t>(error_code) << std::endl;
            }
        }

        if (error_code != rai::ErrorCode::SUCCESS)
        {
            transaction.Abort();
        }
    }
    
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Stats::Add(error_code, "App::ProcessBlock");
    }
    else
    {
        if (block_observer_)
        {
            block_observer_(block, confirmed);
        }
    }
}

void rai::App::ProcessBlockRollback(const std::shared_ptr<rai::Block>& block)
{
    if (!rai::Contain(account_types_, block->Type()))
    {
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    {
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            rai::Log::Error("App::ProcessBlockRollback: construct transaction");
            rai::Stats::Add(error_code, "App::ProcessBlockRollback");
            return;
        }

        error_code = RollbackBlock_(transaction, block);
        switch (error_code)
        {
            case rai::ErrorCode::SUCCESS:
            {
                break;
            }
            case rai::ErrorCode::APP_PROCESS_ROLLBACK_ACCOUNT_MISS:
            case rai::ErrorCode::APP_PROCESS_ROLLBACK_NON_HEAD:
            case rai::ErrorCode::APP_PROCESS_ROLLBACK_CONFIRMED:
            case rai::ErrorCode::APP_PROCESS_ROLLBACK_BLOCK_PUT:
            {
                break;
            }
            case rai::ErrorCode::APP_PROCESS_LEDGER_BLOCK_DEL:
            case rai::ErrorCode::APP_PROCESS_LEDGER_SUCCESSOR_SET:
            case rai::ErrorCode::APP_PROCESS_LEDGER_ACCOUNT_DEL:
            case rai::ErrorCode::APP_PROCESS_LEDGER_ACCOUNT_PUT:
            {
                std::string error_info = rai::ToString(
                    "App::ProcessBlockRollback: ledger operation error code ",
                    static_cast<uint32_t>(error_code));
                rai::Log::Error(error_info);
                std::cout << error_info << std::endl;
                break;
            }
            default:
            {
                std::string error_info = rai::ToString(
                    "App::ProcessBlockRollback: unknown error code ",
                    static_cast<uint32_t>(error_code));
                rai::Log::Error(error_info);
                std::cout << error_info << std::endl;
            }
        }

        if (error_code != rai::ErrorCode::SUCCESS)
        {
            transaction.Abort();
        }
    }

    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Stats::Add(error_code, "App::ProcessBlockRollback");
    }
    else
    {
        if (block_rollback_observer_)
        {
            block_rollback_observer_(block);
        }
    }
}

void rai::App::PullNextBlock(const std::shared_ptr<rai::Block>& block)
{
    rai::BlockHash hash = block->Hash();
    std::shared_ptr<rai::Block> next = block_cache_.Query(hash);
    if (next)
    {
        QueueBlock(next, false);
        block_cache_.Remove(hash);
    }
    else
    {
        SyncAccount(block->Account(), block->Height() + 1);
    }
}

void rai::App::PullNextBlockAsync(const std::shared_ptr<rai::Block>& block)
{
    std::weak_ptr<rai::App> app_w(Shared());
    Background([app_w, block]() {
        if (auto app_s = app_w.lock())
        {
            app_s->PullNextBlock(block);
        }
    });
}

void rai::App::QueueAction(rai::AppActionPri pri,
                           const std::function<void()>& action)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        actions_.insert(std::make_pair(pri, action));
    }
    condition_.notify_all();
}

void rai::App::QueueBlock(const std::shared_ptr<rai::Block>& block,
                          bool confirmed, rai::AppActionPri pri)
{
    std::weak_ptr<rai::App> app_w(Shared());
    QueueAction(pri, [app_w, block, confirmed]() {
        if (auto app_s = app_w.lock())
        {
            app_s->ProcessBlock(block, confirmed);
        }
    });
}

void rai::App::QueueBlockRollback(const std::shared_ptr<rai::Block>& block)
{
    std::weak_ptr<rai::App> app_w(Shared());
    QueueAction(rai::AppActionPri::URGENT, [app_w, block]() {
        if (auto app_s = app_w.lock())
        {
            app_s->ProcessBlockRollback(block);
        }
    });
}

void rai::App::QueueWaitings(const std::shared_ptr<rai::Block>& block)
{
    auto waitings = block_waiting_.Remove(block->Account(), block->Height());
    for (const auto& i : waitings)
    {
        QueueBlock(i.block_, i.confirmed_);
    }
}

void rai::App::ReceiveGatewayMessage(const std::shared_ptr<rai::Ptree>& message)
{
    if (trace_.message_from_gateway_)
    {
        std::cout << "receive gateway message<<=====:\n";
        std::stringstream ostream;
        boost::property_tree::write_json(ostream, *message);
        ostream.flush();
        std::string body = ostream.str();
        std::cout << body << std::endl;
    }

    auto ack_o = message->get_optional<std::string>("ack");
    if (ack_o)
    {
        std::string ack(*ack_o);
        if (ack == "account_heads" || ack == "active_account_heads")
        {
            bootstrap_.ReceiveAccountHeadMessage(message);
        }
        else if (ack == "blocks_query")
        {
            ReceiveBlocksQueryAck(message);
        }
        else if (ack == "block_confirm")
        {
            ReceiveBlockConfirmAck(message);
        }
    }

    auto notify_o = message->get_optional<std::string>("notify");
    if (notify_o)
    {
        std::string notify(*notify_o);
        if (notify == "block_append")
        {
            ReceiveBlockAppendNotify(message);
        }
        else if (notify == "block_confirm")
        {
            ReceiveBlockConfirmNotify(message);
        }
        else if (notify == "block_rollback")
        {
            ReceiveBlockRollbackNotify(message);
        }
    }
}

void rai::App::ReceiveBlockAppendNotify(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto block_o = message->get_child_optional("block");
    if (!block_o)
    {
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockJson(error_code, *block_o);
    if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
    {
        rai::Stats::Add(error_code, "App::ReceiveBlockAppendNotify");
        return;
    }

    QueueBlock(block, false);
}

void rai::App::ReceiveBlockConfirmNotify(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto block_o = message->get_child_optional("block");
    if (!block_o)
    {
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockJson(error_code, *block_o);
    if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
    {
        rai::Stats::Add(error_code, "App::ReceiveBlockConfirmNotify");
        return;
    }

    QueueBlock(block, true);
    block_confirm_.Remove(block);
}

void rai::App::ReceiveBlockConfirmAck(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto status_o = message->get_optional<std::string>("status");
    if (!status_o)
    {
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    if (*status_o == "rollback")
    {
        auto block_o = message->get_child_optional("block");
        if (!block_o || block_o->empty())
        {
            return;
        }
        block = rai::DeserializeBlockJson(error_code, *block_o);
        if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
        {
            return;
        }

        QueueBlockRollback(block);
        block_confirm_.Remove(block);
    }
    else if (*status_o == "success" || *status_o == "fork")
    {
        auto confirmed_o = message->get_optional<std::string>("confirmed");
        if (!confirmed_o || *confirmed_o != "true")
        {
            return;
        }

        auto block_o = message->get_child_optional("block");
        if (!block_o || block_o->empty())
        {
            return;
        }
        block = rai::DeserializeBlockJson(error_code, *block_o);
        if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
        {
            return;
        }

        QueueBlock(block, true);
        block_confirm_.Remove(block);
    }
    else
    {
        // do nothing
    }
}

void rai::App::ReceiveBlocksQueryAck(const std::shared_ptr<rai::Ptree>& message)
{
    auto status_o = message->get_optional<std::string>("status");
    if (!status_o)
    {
        return;
    }

    rai::Account account;
    auto account_o = message->get_optional<std::string>("request_id");
    if (!account_o || account.DecodeAccount(*account_o))
    {
        return;
    }

    if (*status_o == "miss")
    {
        subscribe_.Synced(account);
        return;
    }
    else if (*status_o != "success")
    {
        return;
    }

    auto blocks_o = message->get_child_optional("blocks");
    if (!blocks_o || blocks_o->empty())
    {
        return;
    }
    
    bool enque = true;
    std::shared_ptr<rai::Block> block(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    for (const auto& i : *blocks_o)
    {
        block = rai::DeserializeBlockJson(error_code, i.second);
        if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
        {
            return;
        }

        if (enque)
        {
            // enque first block
            enque = false;
            QueueBlock(block, false);
        }
        else
        {
            block_cache_.Insert(block);
        }
    }
}

void rai::App::ReceiveBlockRollbackNotify(
    const std::shared_ptr<rai::Ptree>& message)
{

    auto block_o = message->get_child_optional("block");
    if (!block_o || block_o->empty())
    {
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockJson(error_code, *block_o);
    if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
    {
        return;
    }

    QueueBlockRollback(block);
}

void rai::App::SendToClient(const rai::Ptree& message, const rai::UniqueId& uid)
{
    if (trace_.message_to_client_)
    {
        std::cout << "send message to client " << uid.StringHex()
                  << "=====>>:\n";
        std::stringstream ostream;
        boost::property_tree::write_json(ostream, message);
        ostream.flush();
        std::string body = ostream.str();
        std::cout << body << std::endl;
    }

    ws_server_->Send(uid, message);
}

void rai::App::SendToGateway(const rai::Ptree& message)
{
    if (trace_.message_to_gateway_)
    {
        std::cout << "send message to gateway=====>>:\n";
        std::stringstream ostream;
        boost::property_tree::write_json(ostream, message);
        ostream.flush();
        std::string body = ostream.str();
        std::cout << body << std::endl;
    }

    gateway_ws_->Send(message);
}

void rai::App::Subscribe()
{
    SubscribeBlockAppend();
    SubscribeBlockRollbacK();
}

void rai::App::SubscribeBlockAppend()
{
    rai::Ptree ptree;

    ptree.put("action", "event_subscribe");
    ptree.put("event", "block_append");

    SendToGateway(ptree);
}

void rai::App::SubscribeBlockRollbacK()
{
    rai::Ptree ptree;

    ptree.put("action", "event_subscribe");
    ptree.put("event", "block_rollback");

    SendToGateway(ptree);
}

void rai::App::SyncAccount(const rai::Account& account)
{
    uint64_t height = 0;

    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, false);
        IF_NOT_SUCCESS_RETURN_VOID(error_code);

        rai::AccountInfo info;
        bool error = ledger_.AccountInfoGet(transaction, account, info);
        bool account_exist = !error && info.Valid();
        if (!account_exist)
        {
            height = 0;
        }
        else
        {
            height = info.head_height_ + 1;
        }
    }

    SyncAccount(account, height);
}

void rai::App::SyncAccount(const rai::Account& account, uint64_t height,
                           uint64_t count)
{
    rai::Ptree ptree;

    ptree.put("action", "blocks_query");
    ptree.put("account", account.StringAccount());
    ptree.put("height", std::to_string(height));

    if (BlockCacheFull())
    {
        count = 1;
    }
    ptree.put("count", std::to_string(count));
    ptree.put("request_id", account.StringAccount());

    SendToGateway(ptree);
}

void rai::App::SyncAccountAsync(const rai::Account& account)
{
    std::weak_ptr<rai::App> app_w(Shared());
    Background([app_w, account](){
        if (auto app_s = app_w.lock())
        {
            app_s->SyncAccount(account);
        }
    });
}

void rai::App::SyncAccountAsync(const rai::Account& account, uint64_t height,
                                uint64_t count)
{
    std::weak_ptr<rai::App> app_w(Shared());
    Background([app_w, account, height, count]() {
        if (auto app_s = app_w.lock())
        {
            app_s->SyncAccount(account, height, count);
        }
    });
}

void rai::App::ReceiveWsMessage(const std::string& message,
                                const rai::UniqueId& uid)
{
    if (trace_.message_from_client_)
    {
        std::cout << "receive message from client " << uid.StringHex()
                  << "<<=====:\n";
        std::cout << message << std::endl;
    }

    std::shared_ptr<rai::AppRpcHandler> handler = MakeRpcHandler(
        uid, true, message, [this, uid](const rai::Ptree& response) {
            ws_server_->Send(uid, response);
        });
    if (handler)
    {
        handler->Process();
    }
}

void rai::App::ProcessWsSession(const rai::UniqueId& uid, bool add)
{

    if (add)
    {
        NotifyProviderInfo(uid);
    }
    else
    {
        subscribe_.Unsubscribe(uid);
    }
}

void rai::App::NotifyProviderInfo(const rai::UniqueId& uid)
{
    using P = rai::Provider;
    rai::Ptree ptree;

    P::PutId(ptree, provider_info_.id_, "register");

    rai::Ptree filters;
    for (auto filter : provider_info_.filters_)
    {
        rai::Ptree entry;
        entry.put("", P::ToString(filter));
        filters.push_back(std::make_pair("", entry));
    }
    ptree.put_child("filters", filters);

    rai::Ptree actions;
    for (auto action : provider_info_.actions_)
    {
        rai::Ptree entry;
        entry.put("", P::ToString(action));
        actions.push_back(std::make_pair("", entry));
    }
    ptree.put_child("actions", actions);

    SendToClient(ptree, uid);
}

rai::ErrorCode rai::App::WaitBlock(rai::Transaction& transaction,
                                   const rai::Account& account, uint64_t height,
                                   const std::shared_ptr<rai::Block>& waiting,
                                   bool confirmed,
                                   std::shared_ptr<rai::Block>& out)
{
    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        block_waiting_.Insert(account, height, waiting, confirmed);
        SyncAccountAsync(account, 0);
        return rai::ErrorCode::APP_PROCESS_WAITING;
    }

    if (height > info.head_height_)
    {
        block_waiting_.Insert(account, height, waiting, confirmed);
        SyncAccountAsync(account, info.head_height_ + 1);
        return rai::ErrorCode::APP_PROCESS_WAITING;
    }
    else if (height < info.tail_height_)
    {
        return rai::ErrorCode::APP_PROCESS_PRUNED;
    }

    std::shared_ptr<rai::Block> block;
    error = ledger_.BlockGet(transaction, account, height, block);
    if (error || block == nullptr)
    {
        return rai::ErrorCode::APP_PROCESS_HALT;
    }

    if (!info.Confirmed(height))
    {
        block_waiting_.Insert(account, height, waiting, confirmed);
        block_confirm_.Add(block);
        return rai::ErrorCode::APP_PROCESS_WAITING;
    }

    out = block;
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::App::WaitBlockIfExist(
    rai::Transaction& transaction, const rai::Account& account, uint64_t height,
    const std::shared_ptr<rai::Block>& waiting, bool confirmed,
    std::shared_ptr<rai::Block>& out)
{
    out = nullptr;
    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        return rai::ErrorCode::SUCCESS;
    }

    if (height > info.head_height_)
    {
        return rai::ErrorCode::SUCCESS;
    }

    if (height < info.tail_height_)
    {
        return rai::ErrorCode::APP_PROCESS_PRUNED;
    }

    std::shared_ptr<rai::Block> block;
    error = ledger_.BlockGet(transaction, account, height, block);
    if (error || block == nullptr)
    {
        return rai::ErrorCode::APP_PROCESS_HALT;
    }

    if (!info.Confirmed(height))
    {
        block_waiting_.Insert(account, height, waiting, confirmed);
        block_confirm_.Add(block);
        return rai::ErrorCode::APP_PROCESS_WAITING;
    }

    out = block;
    return rai::ErrorCode::SUCCESS;
}

void rai::App::RegisterObservers_()
{
    std::weak_ptr<rai::App> app(Shared());
    gateway_ws_->message_processor_ =
        [app](const std::shared_ptr<rai::Ptree>& message) {
            auto app_s = app.lock();
            if (app_s == nullptr) return;
            app_s->ReceiveGatewayMessage(message);
        };

    gateway_ws_->status_observer_ = [app](rai::WebsocketStatus status) {
        auto app_s = app.lock();
        if (app_s == nullptr) return;

        app_s->Background([app, status]() {
            if (auto app_s = app.lock())
            {
                app_s->observers_.gateway_status_.Notify(status);
            }
        });
    };

    ws_server_->message_observer_ = [app](const std::string& message,
                                          const rai::UniqueId& uid) {
        auto app_s = app.lock();
        if (app_s == nullptr) return;
        app_s->ReceiveWsMessage(message, uid);
    };

    ws_server_->session_observer_ = [app](const rai::UniqueId& uid, bool add) {
        auto app_s = app.lock();
        if (app_s == nullptr) return;
        app_s->ProcessWsSession(uid, add);
    };

    block_observer_ = [app](const std::shared_ptr<rai::Block>& block,
                            bool confirmed) {
        auto app_s = app.lock();
        if (app_s == nullptr) return;

        app_s->Background([app, block, confirmed]() {
            if (auto app_s = app.lock())
            {
                app_s->observers_.block_.Notify(block, confirmed);
            }
        });
    };

    block_rollback_observer_ = [app](const std::shared_ptr<rai::Block>& block) {
        auto app_s = app.lock();
        if (app_s == nullptr) return;

        app_s->Background([app, block]() {
            if (auto app_s = app.lock())
            {
                app_s->observers_.block_rollback_.Notify(block);
            }
        });
    };

    observers_.gateway_status_.Add([this](rai::WebsocketStatus status) {
        if (status == rai::WebsocketStatus::CONNECTED)
        {
            std::cout << "node gateway connected\n";
            Subscribe();
        }
        else if (status == rai::WebsocketStatus::CONNECTING)
        {
            std::cout << "node gateway connecting\n";
        }
        else
        {
            std::cout << "node gateway disconnected\n";
        }
    });
}

rai::ErrorCode rai::App::AppendBlock_(rai::Transaction& transaction,
                                      const std::shared_ptr<rai::Block>& block,
                                      bool confirmed)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, block->Account(), info);
    bool account_exist = !error && info.Valid();
    if (!account_exist)
    {
        if (block->Height() != 0)
        {
            return rai::ErrorCode::APP_PROCESS_GAP_PREVIOUS;
        }

        error_code = PreBlockAppend(transaction, block, confirmed);
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::AccountInfo new_info(block->Type(), block->Hash());
        if (confirmed)
        {
            new_info.confirmed_height_ = 0;
        }

        error = ledger_.AccountInfoPut(transaction, block->Account(), new_info);
        IF_ERROR_RETURN(error, rai::ErrorCode::APP_PROCESS_LEDGER_ACCOUNT_PUT);
        error = ledger_.BlockPut(transaction, block->Hash(), *block);
        IF_ERROR_RETURN(error, rai::ErrorCode::APP_PROCESS_LEDGER_BLOCK_PUT);

        error_code = AfterBlockAppend(transaction, block, confirmed);
        return error_code;
    }

    // account exists
    uint64_t height = block->Height();
    if (height > info.head_height_ + 1)
    {
        return rai::ErrorCode::APP_PROCESS_GAP_PREVIOUS;
    }
    else if (height == info.head_height_ + 1)
    {
        if (block->Previous() != info.head_)
        {
            return rai::ErrorCode::APP_PROCESS_PREVIOUS_MISMATCH;
        }

        error_code = PreBlockAppend(transaction, block, confirmed);
        IF_NOT_SUCCESS_RETURN(error_code);

        info.head_height_ = block->Height();
        info.head_        = block->Hash();
        if (confirmed)
        {
            info.confirmed_height_ = block->Height();
        }
        error = ledger_.AccountInfoPut(transaction, block->Account(), info);
        IF_ERROR_RETURN(error, rai::ErrorCode::APP_PROCESS_LEDGER_ACCOUNT_PUT);
        error = ledger_.BlockPut(transaction, block->Hash(), *block);
        IF_ERROR_RETURN(error, rai::ErrorCode::APP_PROCESS_LEDGER_BLOCK_PUT);
        error = ledger_.BlockSuccessorSet(transaction, block->Previous(),
                                          block->Hash());
        IF_ERROR_RETURN(error,
                        rai::ErrorCode::APP_PROCESS_LEDGER_SUCCESSOR_SET);

        error_code = AfterBlockAppend(transaction, block, confirmed);
        return error_code;
    }
    else if (height >= info.tail_height_)
    {
        if (ledger_.BlockExists(transaction, block->Hash()))
        {
            if (info.Confirmed(height))
            {
                return rai::ErrorCode::APP_PROCESS_CONFIRMED;
            }

            if (!confirmed)
            {
                return rai::ErrorCode::APP_PROCESS_EXIST;
            }

            info.confirmed_height_ = block->Height();
            error =
                ledger_.AccountInfoPut(transaction, block->Account(), info);
            IF_ERROR_RETURN(error,
                            rai::ErrorCode::APP_PROCESS_LEDGER_ACCOUNT_PUT);
            return rai::ErrorCode::SUCCESS;
        }
        else
        {
            if (info.Confirmed(height))
            {
                return rai::ErrorCode::APP_PROCESS_CONFIRMED_FORK;
            }
            else
            {
                return rai::ErrorCode::APP_PROCESS_FORK;
            }
        }
    }
    else
    {
        return rai::ErrorCode::APP_PROCESS_PRUNED;
    }
}

rai::ErrorCode rai::App::RollbackBlock_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block)
{
    rai::AccountInfo info;
    rai::Account account = block->Account();
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        return rai::ErrorCode::APP_PROCESS_ROLLBACK_ACCOUNT_MISS;
    }

    if (info.head_ != block->Hash())
    {
        return rai::ErrorCode::APP_PROCESS_ROLLBACK_NON_HEAD;
    }

    if (info.Confirmed(block->Height()))
    {
        return rai::ErrorCode::APP_PROCESS_ROLLBACK_CONFIRMED;
    }

    rai::ErrorCode error_code = PreBlockRollback(transaction, block);
    IF_NOT_SUCCESS_RETURN(error_code);

    error = ledger_.RollbackBlockPut(transaction, info.head_, *block);
    IF_ERROR_RETURN(error, rai::ErrorCode::APP_PROCESS_ROLLBACK_BLOCK_PUT);

    error = ledger_.BlockDel(transaction, info.head_);
    IF_ERROR_RETURN(error, rai::ErrorCode::APP_PROCESS_LEDGER_BLOCK_DEL);

    if (block->Height() != 0)
    {
        error = ledger_.BlockSuccessorSet(transaction, block->Previous(),
                                          rai::BlockHash(0));
        IF_ERROR_RETURN(error, rai::ErrorCode::APP_PROCESS_LEDGER_SUCCESSOR_SET);
    }

    if (info.head_height_ == 0)
    {
        error = ledger_.AccountInfoDel(transaction, account);
        IF_ERROR_RETURN(error, rai::ErrorCode::APP_PROCESS_LEDGER_ACCOUNT_DEL);
    }
    else
    {
        info.head_ = block->Previous();
        info.head_height_ -= 1;
        if (info.confirmed_height_ != rai::Block::INVALID_HEIGHT
            && info.confirmed_height_ > info.head_height_)
        {
            info.confirmed_height_ = info.head_height_;
        }
        error = ledger_.AccountInfoPut(transaction, account, info);
        IF_ERROR_RETURN(error, rai::ErrorCode::APP_PROCESS_LEDGER_ACCOUNT_PUT);
    }

    return AfterBlockRollback(transaction, block);
}

bool rai::App::GetHeadBlock_(rai::Transaction& transaction,
                             const rai::Account& account,
                             std::shared_ptr<rai::Block>& block)
{
    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        return true;
    }

    error = ledger_.BlockGet(transaction, info.head_, block);
    if (error || block == nullptr)
    {
        return true;
    }

    return false;
}

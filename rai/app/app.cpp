#include <rai/app/app.hpp>

#include <rai/common/stat.hpp>

rai::AppTrace::AppTrace()
    : message_from_gateway_(false), message_to_gateway_(false)
{
}

rai::App::App(rai::ErrorCode& error_code, rai::Alarm& alarm,
              const boost::filesystem::path& data_path,
              const rai::Url& gateway_url,
              const std::vector<rai::BlockType>& account_types)
    : alarm_(alarm),
      store_(error_code, data_path / "app_data.ldb"),
      ledger_(error_code, store_, false),
      gateway_url_(gateway_url),
      account_types_(account_types),
      service_runner_(service_),
      bootstrap_(*this)
{
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    if (!gateway_url
        || (gateway_url.protocol_ != "ws" && gateway_url.protocol_ != "wss"))
    {
        error_code = rai::ErrorCode::APP_INVALID_GATEWAY_URL;
        return;
    }
    gateway_ws_ = std::make_shared<rai::WebsocketClient>(
        service_, gateway_url_.host_, gateway_url_.port_, gateway_url_.path_,
        gateway_url_.protocol_ == "wss");
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

    // todo:
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

    alarm_.Stop();
    gateway_ws_->Close();
    std::this_thread::sleep_for(std::chrono::seconds(1));
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
                break;
            }
            case rai::ErrorCode::APP_PROCESS_LEDGER_ACCOUNT_PUT:
            case rai::ErrorCode::APP_PROCESS_LEDGER_BLOCK_PUT:
            case rai::ErrorCode::APP_PROCESS_LEDGER_SUCCESSOR_SET:
            {
                // log
                break;
            }
            case rai::ErrorCode::APP_PROCESS_GAP_PREVIOUS:
            {
                SyncAccountAsync(block->Account());
                break;
            }
            case rai::ErrorCode::APP_PROCESS_PRUNED:
            {
                // log
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

                }
                else
                {

                }
                break;
            }

    APP_PROCESS_CONFIRMED_FORK          = 609,
    APP_PROCESS_EXIST                   = 610,
    APP_PROCESS_CONFIRMED               = 611,
        }

        if (error_code != rai::ErrorCode::SUCCESS)
        {
            transaction.Abort();
        }
    }
    
}

void rai::App::ProcessBlockRollback(const std::shared_ptr<rai::Block>& block)
{

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
        if (ack == "blocks_query")
        {
            //todo:
        }
        else if (ack == "block_confirm")
        {
            //todo:
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
    }
}

void rai::App::ReceiveBlockAppendNotify(
    const std::shared_ptr<rai::Ptree>& message)
{
    //todo:
}

void rai::App::ReceiveBlockConfirmNotify(
    const std::shared_ptr<rai::Ptree>& message)
{
    //todo:
}

void rai::App::SendToGateway(const rai::Ptree& message)
{
    gateway_ws_->Send(message);
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
        if (account_exist)
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
    Background([this, account](){
        SyncAccount(account);
    });
}

void rai::App::SyncAccountAsync(const rai::Account& account, uint64_t height,
                                uint64_t count)
{
    Background([this, account, height, count]() {
        SyncAccount(account, height, count);
    });
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
        error = ledger_.AccountInfoDel(transaction, block->Account());
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

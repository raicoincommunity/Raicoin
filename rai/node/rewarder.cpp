#include <rai/node/rewarder.hpp>
#include <functional>
#include <rai/node/node.hpp>

rai::Rewarder::Rewarder(rai::Node& node, const rai::Account& account,
                        uint32_t times)
    : node_(node),
      daily_send_times_(times),
      fan_(account, rai::Fan::FAN_OUT),
      stopped_(false),
      up_to_date_(false),
      block_(nullptr),
      thread_([this]() { Run(); })
{
    node_.observers_.block_.Add(
        [this](const rai::BlockProcessResult& result,
               const std::shared_ptr<rai::Block>& block) {
            // local block
            rai::ErrorCode error_code = result.error_code_;
            if (block->Account() == node_.account_)
            {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (block == nullptr || block != block_)
                    {
                        return;
                    }
                    block_.reset();
                }

                if (error_code == rai::ErrorCode::BLOCK_PROCESS_GAP_PREVIOUS)
                {
                    node_.previous_gap_cache_.Remove(block->Previous());
                }
                else if (error_code
                         == rai::ErrorCode::BLOCK_PROCESS_GAP_RECEIVE_SOURCE)
                {
                    node_.receive_source_gap_cache_.Remove(block->Link());
                }
                else if (error_code
                         == rai::ErrorCode::BLOCK_PROCESS_GAP_REWARD_SOURCE)
                {
                    node_.reward_source_gap_cache_.Remove(block->Link());
                }
                return;
            }

            // other block
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                return;
            }
            if (result.operation_ != rai::BlockOperation::APPEND
                && result.operation_ != rai::BlockOperation::CONFIRM)
            {
                return;
            }
            if (block == nullptr || block->Height() == 0
                || !block->HasRepresentative())
            {
                return;
            }
            if (block->Representative() != node_.account_
                && block->Opcode() != rai::BlockOpcode::CHANGE)
            {
                return;
            }

            rai::Transaction transaction(error_code, node_.ledger_, false);
            IF_NOT_SUCCESS_RETURN_VOID(error_code);

            std::shared_ptr<rai::Block> previous(nullptr);
            bool error = node_.ledger_.BlockGet(
                transaction, block->Previous(), previous);
            if (error)
            {
                rai::Stats::Add(rai::ErrorCode::LEDGER_BLOCK_GET,
                                "Rewarder::BlockProcessorCallback account=",
                                block->Account().StringAccount(),
                                ", height=", block->Height() - 1,
                                ", hash=", block->Previous().StringHex());
                return;
            }
            if (previous->Representative() != node_.account_)
            {
                return;
            }

            if (result.operation_ == rai::BlockOperation::APPEND)
            {
                node_.StartElection(block);
            }
            else if (result.operation_ == rai::BlockOperation::CONFIRM)
            {
                rai::Amount amount = rai::RewardAmount(previous->Balance(),
                                                       previous->Timestamp(),
                                                       block->Timestamp());
                if (amount.IsZero())
                {
                    return;
                }

                rai::RewardableInfo info;
                error = node_.ledger_.RewardableInfoGet(
                    transaction, node_.account_, block->Previous(), info);
                if (error)
                {
                    rai::Stats::Add(rai::ErrorCode::LEDGER_REWARDABLE_INFO_GET,
                                    "Rewarder::BlockProcessorCallback account=",
                                    block->Account().StringAccount(),
                                    ", height=", block->Height() - 1,
                                    ", hash=", block->Previous().StringHex());
                    return;
                }
                Add(block->Previous(), info.amount_, info.valid_timestamp_);
            }
            else
            {
                // do nothing
            }
            
        });
}

void rai::Rewarder::Add(const rai::BlockHash& hash, const rai::Amount& amount,
                        uint64_t timestamp)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rai::RewarderInfo info{timestamp, hash, amount};
        waitings_.insert(info);
    }
    condition_.notify_all();
}

void rai::Rewarder::Add(const rai::RewarderInfo& info)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        waitings_.insert(info);
    }
    condition_.notify_all();
}

void rai::Rewarder::UpToDate()
{
    std::lock_guard<std::mutex> lock(mutex_);
    up_to_date_ = true;
}

void rai::Rewarder::KeepUpToDate() const
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (up_to_date_)
        {
            return;
        }
    }

    if (node_.Status() != rai::NodeStatus::RUN)
    {
        std::weak_ptr<rai::Node> node_w(node_.Shared());
        node_.alarm_.Add(
            std::chrono::steady_clock::now() + std::chrono::seconds(10),
            [node_w]() {
                std::shared_ptr<rai::Node> node = node_w.lock();
                if (node)
                {
                    node->rewarder_.KeepUpToDate();
                }
            });

        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    rai::AccountInfo info;
    bool error =
        node_.ledger_.AccountInfoGet(transaction, node_.account_, info);
    if (error || !info.Valid())
    {
        node_.block_queries_.QueryByHeight(
            node_.account_, 0, false,
            QueryCallback_(node_.account_, 0, rai::BlockHash(0)));
    }
    else
    {
        node_.block_queries_.QueryByPrevious(
            node_.account_, info.head_height_ + 1, info.head_, false,
            QueryCallback_(node_.account_, info.head_height_ + 1, info.head_));
    }
}

void rai::Rewarder::QueueAction(const rai::RewarderAction& action,
                                const rai::RewarderCallback& callback)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        actions_.emplace_back(action, callback);
    }
    condition_.notify_all();
}

void rai::Rewarder::Run()
{ 
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        if (node_.Status() != rai::NodeStatus::RUN || !up_to_date_)
        {
            condition_.wait_for(lock, std::chrono::seconds(5));
            continue;
        }

        if (block_ != nullptr || Confirm_(lock))
        {
            condition_.wait_for(lock, std::chrono::seconds(1));
            continue;
        }

        if (!actions_.empty())
        {
            auto action = actions_.front();
            actions_.pop_front();
            lock.unlock();

            std::shared_ptr<rai::Block> block(nullptr);
            rai::ErrorCode error_code = action.first(block);
            if (error_code == rai::ErrorCode::SUCCESS && block != nullptr)
            {
                lock.lock();
                block_ = block;
                lock.unlock();
                node_.block_processor_.Add(block);
            }
            else
            {
                rai::Stats::Add(error_code);
            }
            
            action.second(error_code, block);
            lock.lock();
        }
        else if (!waitings_.empty() || !rewardables_.empty())
        {
            uint64_t sleep = 0;
            auto now = rai::CurrentTimestamp();
            while (!waitings_.empty())
            {
                auto it = waitings_.get<rai::RewarderInfoByTime>().begin();
                if (it->timestamp_ > now)
                {
                    sleep = it->timestamp_ - now;
                    break;
                }
                rewardables_.insert(*it);
                waitings_.get<rai::RewarderInfoByTime>().erase(it);
            }

            auto it = rewardables_.get<rai::RewarderInfoByAmount>().begin();
            if (it == rewardables_.get<rai::RewarderInfoByAmount>().end())
            {
                condition_.wait_for(lock, std::chrono::seconds(sleep));
                continue;
            }
            rai::RewarderInfo info(*it);
            lock.unlock();

            std::shared_ptr<rai::Block> block(nullptr);
            rai::ErrorCode error_code =
                ProcessReward_(info.amount_, info.hash_, block);
            if (error_code == rai::ErrorCode::SUCCESS && block != nullptr)
            {
                lock.lock();
                block_ = block;
                lock.unlock();
                node_.block_processor_.Add(block);
            }
            else
            {
                rai::Stats::Add(error_code);
            }

            lock.lock();
            uint32_t delay = 0;
            if (error_code == rai::ErrorCode::SUCCESS)
            {
                rewardables_.erase(info.hash_);
                delay = 1;
            }
            else if (error_code == rai::ErrorCode::LEDGER_REWARDABLE_INFO_GET
                     || error_code == rai::ErrorCode::REWARDER_AMOUNT
                     || error_code == rai::ErrorCode::LEDGER_BLOCK_GET)
            {
                rewardables_.erase(info.hash_);
            }
            else if (error_code == rai::ErrorCode::ACCOUNT_ACTION_TOO_QUICKLY)
            {
                delay = 1;
            }
            else if (error_code == rai::ErrorCode::REWARDER_TIMESTAMP
                     || error_code
                            == rai::ErrorCode::
                                   REWARDER_REWARDABLE_LESS_THAN_CREDIT)
            {
                delay = 5;
            }
            else if (error_code == rai::ErrorCode::ACCOUNT_RESTRICTED)
            {
                delay = 60;
            }
            else
            {
                rai::Stats::Add(rai::ErrorCode::GENERIC, "Rewarder::Run");
                rewardables_.erase(info.hash_);
            }
            
            if (delay > 0)
            {
                condition_.wait_for(lock, std::chrono::seconds(delay));
                continue;
            }
        }
        else
        {
            condition_.wait(lock);
            continue;
        }
    }
}

void rai::Rewarder::Send()
{
    if (node_.Status() != rai::NodeStatus::RUN)
    {
        return;
    }

    rai::RawKey destination;
    fan_.Get(destination);

    QueueAction(std::bind(&rai::Rewarder::ProcessSend_, this, destination.data_,
                          std::placeholders::_1),
                [](rai::ErrorCode, const std::shared_ptr<rai::Block>&) {});
}

rai::ErrorCode rai::Rewarder::Bind(rai::Chain chain,
                                   const rai::SignerAddress& signer)
{
    if (node_.Status() != rai::NodeStatus::RUN)
    {
        return rai::ErrorCode::NODE_STATUS;
    }

    boost::optional<rai::SignerAddress> signer_o =
        node_.BindingQuery(node_.account_, chain);
    if (signer_o && *signer_o == signer)
    {
        return rai::ErrorCode::BINDING_IGNORED;
    }

    // todo:
}

uint32_t rai::Rewarder::SendInterval() const
{
    if (daily_send_times_ == 0)
    {
        return 0;
    }

    uint32_t interval = 86400 / daily_send_times_;
    if (interval == 0)
    {
        interval = 1;
    }

    return interval;
}

void rai::Rewarder::Start()
{
    KeepUpToDate();
}

void rai::Rewarder::Status(rai::Ptree& ptree) const
{
    rai::RawKey destination;
    fan_.Get(destination);
    ptree.put("reward_to", destination.data_.StringAccount());
    ptree.put("daily_reward_times", std::to_string(daily_send_times_));

    std::lock_guard<std::mutex> lock(mutex_);
    ptree.put("up_to_date", up_to_date_ ? "true" : "false");
    if (block_)
    {
        rai::Ptree block;
        block_->SerializeJson(block);
        ptree.put_child("current_block", block);
    }
    ptree.put("rewardable_count", rewardables_.size());
    ptree.put("waiting_count", waitings_.size());
}

void rai::Rewarder::Stop()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
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
}

void rai::Rewarder::Sync()
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    std::vector<rai::RewarderInfo> confirms;
    std::unordered_map<rai::Account, std::shared_ptr<rai::Block>> elections;
    rai::Account account(node_.account_);
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
            rai::Stats::Add(rai::ErrorCode::LEDGER_REWARDABLE_INFO_GET,
                            "Rewarder::Sync");
            return;
        }
        assert(node_.account_ == account);

        std::shared_ptr<rai::Block> block;
        error = node_.ledger_.BlockGet(transaction, hash, block);
        if (error || block == nullptr)
        {
            rai::Stats::Add(rai::ErrorCode::LEDGER_BLOCK_GET,
                            "Rewarder::Sync hash=", hash.StringHex());
            return;
        }

        rai::AccountInfo account_info;
        error = node_.ledger_.AccountInfoGet(transaction, block->Account(),
                                             account_info);
        if (error || !account_info.Valid())
        {
            rai::Stats::Add(
                rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET,
                "Rewarder::Sync account=", block->Account().StringAccount());
            return;
        }

        if (account_info.Confirmed(block->Height()))
        {
            rai::RewarderInfo rewarder_info{info.valid_timestamp_, hash,
                                            info.amount_};
            confirms.push_back(rewarder_info);
        }
        else
        {
            auto it = elections.find(info.source_);
            if (it != elections.end() && it->second->Height() > block->Height())
            {
                continue;
            }
            elections[info.source_] = block;
        }
    }

    for (const auto& i : confirms)
    {
        Add(i);
    }
    for (const auto& i : elections)
    {
        node_.StartElection(i.second);
    }
}

bool rai::Rewarder::Confirm_(std::unique_lock<std::mutex>& lock)
{
    lock.unlock();

    bool ret = true;
    do
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, node_.ledger_, false);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            rai::Stats::Add(error_code, "Rewarder::Confirm");
            ret = true;
            break;
        }

        rai::AccountInfo info;
        bool error =
            node_.ledger_.AccountInfoGet(transaction, node_.account_, info);
        if (error || !info.Valid())
        {
            ret = false;
            break;
        }

        if (info.confirmed_height_ == info.head_height_)
        {
            ret = false;
            break;
        }

        std::shared_ptr<rai::Block> block(nullptr);
        error = node_.ledger_.BlockGet(transaction, info.head_, block);
        if (error)
        {
            rai::Stats::Add(rai::ErrorCode::LEDGER_BLOCK_GET,
                            "Rewarder::Confirm_");
            ret = true;
            break;
        }

        node_.StartElection(block);
        ret = true;
    } while (0);

    lock.lock();
    return ret;
}

rai::ErrorCode rai::Rewarder::ProcessReward_(const rai::Amount& amount,
                                             const rai::BlockHash& hash,
                                             std::shared_ptr<rai::Block>& block)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::RewardableInfo rewardable_info;
    bool error = node_.ledger_.RewardableInfoGet(transaction, node_.account_,
                                                 hash, rewardable_info);
    if (error)
    {
        error_code = rai::ErrorCode::LEDGER_REWARDABLE_INFO_GET;
        rai::Stats::AddDetail(
            error_code, "Rewarder::ProcessReward_: hash=", hash.StringHex());
        return error_code;
    }
    if (amount != rewardable_info.amount_)
    {
        return rai::ErrorCode::REWARDER_AMOUNT;
    }

    rai::AccountInfo info;
    error = node_.ledger_.AccountInfoGet(transaction, node_.account_, info);
    if (error || !info.Valid())
    {
        rai::BlockOpcode opcode = rai::BlockOpcode::REWARD;
        uint16_t credit = 1;
        uint64_t timestamp = rai::CurrentTimestamp();
        if (timestamp < rewardable_info.valid_timestamp_)
        {
            return rai::ErrorCode::REWARDER_TIMESTAMP;
        }
        uint32_t counter = 0;
        uint64_t height = 0;
        rai::BlockHash previous(0);
        if (amount < rai::CreditPrice(timestamp))
        {
            return rai::ErrorCode::REWARDER_REWARDABLE_LESS_THAN_CREDIT;
        }
        rai::Amount balance = amount - rai::CreditPrice(timestamp);
        rai::uint256_union link = hash;
        rai::RawKey private_key;
        node_.PrivateKey(private_key);
        block = std::make_shared<rai::RepBlock>(
            opcode, credit, counter, timestamp, height, node_.account_,
            previous, balance, link, private_key, node_.account_);
    }
    else
    {
        std::shared_ptr<rai::Block> head(nullptr);
        error = node_.ledger_.BlockGet(transaction, info.head_, head);
        if (error || head == nullptr)
        {
            rai::Stats::AddDetail(rai::ErrorCode::LEDGER_BLOCK_GET,
                                  "Rewarder::ProcessReward_");
            return rai::ErrorCode::LEDGER_BLOCK_GET;
        }

        rai::BlockOpcode opcode = rai::BlockOpcode::REWARD;
        uint16_t credit = head->Credit();
        uint64_t timestamp = rai::CurrentTimestamp();
        if (timestamp < rewardable_info.valid_timestamp_)
        {
            return rai::ErrorCode::REWARDER_TIMESTAMP;
        }
        if (timestamp < head->Timestamp())
        {
            return rai::ErrorCode::BLOCK_TIMESTAMP;
        }
        if (info.forks_ > rai::MaxAllowedForks(timestamp, credit))
        {
            return rai::ErrorCode::ACCOUNT_RESTRICTED;
        }

        uint32_t counter =
            rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() : 0;
        uint64_t height = head->Height() + 1;
        rai::BlockHash previous = info.head_;
        rai::Amount balance = head->Balance() + rewardable_info.amount_;
        rai::uint256_union link = hash;
        rai::RawKey private_key;
        node_.PrivateKey(private_key);
        block = std::make_shared<rai::RepBlock>(
            opcode, credit, counter, timestamp, height, node_.account_,
            previous, balance, link, private_key, node_.account_);
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Rewarder::ProcessSend_(const rai::Account& destination,
                                           std::shared_ptr<rai::Block>& block)
{
    if (destination.IsZero())
    {
        rai::Stats::AddDetail(rai::ErrorCode::REWARD_TO_ACCOUNT,
                              "Rewarder::ProcessSend_: zero account");
        return rai::ErrorCode::REWARD_TO_ACCOUNT;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::AccountInfo info;
    bool error = node_.ledger_.AccountInfoGet(transaction, destination, info);
    if (!error && info.Valid() && info.type_ != rai::BlockType::TX_BLOCK)
    {
        rai::Stats::AddDetail(
            rai::ErrorCode::REWARD_TO_ACCOUNT,
            "Rewarder::ProcessSend_: not transaction account");
        return rai::ErrorCode::REWARD_TO_ACCOUNT;
    }

    error = node_.ledger_.AccountInfoGet(transaction, node_.account_, info);
    if (error || !info.Valid())
    {
        error_code = rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET;
        rai::Stats::AddDetail(error_code, "Rewarder::ProcessSend_");
        return error_code;
    }

    std::shared_ptr<rai::Block> head(nullptr);
    error = node_.ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        error_code = rai::ErrorCode::LEDGER_BLOCK_GET;
        rai::Stats::AddDetail(error_code, "Rewarder::ProcessSend_");
        return error_code;
    }

    if (head->Balance().IsZero())
    {
        return rai::ErrorCode::ACCOUNT_ZERO_BALANCE;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::SEND;
    uint16_t credit = head->Credit();
    uint64_t now = rai::CurrentTimestamp();
    uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
    if (timestamp > now + 60)
    {
        return rai::ErrorCode::ACCOUNT_ACTION_TOO_QUICKLY;
    }
    if (info.forks_ > rai::MaxAllowedForks(timestamp, credit))
    {
        return rai::ErrorCode::ACCOUNT_RESTRICTED;
    }

    uint32_t counter =
        rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        return rai::ErrorCode::ACCOUNT_ACTION_CREDIT;
    }
    uint64_t height = head->Height() + 1;
    rai::BlockHash previous = info.head_;
    rai::Amount balance(0);
    rai::uint256_union link = destination;
    rai::RawKey private_key;
    node_.PrivateKey(private_key);
    block = std::make_shared<rai::RepBlock>(
        opcode, credit, counter, timestamp, height, node_.account_, previous,
        balance, link, private_key, node_.account_);

    return rai::ErrorCode::SUCCESS;
}

rai::QueryCallback rai::Rewarder::QueryCallback_(
    const rai::Account& account, uint64_t height,
    const rai::BlockHash& previous) const
{
    std::weak_ptr<rai::Node> node_w(node_.Shared());
    rai::QueryCallback callback =
        [node_w, account, height, previous, count = 0](
            const std::vector<rai::QueryAck>& acks,
            std::vector<rai::QueryCallbackStatus>& result) mutable {
            auto node(node_w.lock());
            if (!node)
            {
                result.insert(result.end(), acks.size(),
                              rai::QueryCallbackStatus::FINISH);
                return;
            }

            if (acks.size() != 1)
            {
                result.insert(result.end(), acks.size(),
                              rai::QueryCallbackStatus::FINISH);
                rai::Stats::Add(rai::ErrorCode::LOGIC_ERROR,
                                "Rewarder::QueryCallback_: invalid ack size");
                return;
            }

            auto& ack = acks[0];
            if (ack.status_ == rai::QueryStatus::FORK
                || ack.status_ == rai::QueryStatus::SUCCESS)
            {
                result.insert(result.end(), 1,
                              rai::QueryCallbackStatus::FINISH);
                if (node->Status() == rai::NodeStatus::RUN)
                {
                    node->syncer_.Add(account, height, previous, false,
                                      rai::Syncer::DEFAULT_BATCH_ID);
                }
                node->alarm_.Add(
                    std::chrono::steady_clock::now() + std::chrono::seconds(10),
                    [node_w]() {
                        std::shared_ptr<rai::Node> node = node_w.lock();
                        if (node)
                        {
                            node->rewarder_.KeepUpToDate();
                        }
                    });
            }
            else if (ack.status_ == rai::QueryStatus::MISS)
            {
                ++count;
                if (count >= 20)
                {
                    result.insert(result.end(), 1,
                                  rai::QueryCallbackStatus::FINISH);
                    node->rewarder_.UpToDate();
                }
                else
                {
                    result.insert(result.end(), 1,
                                  rai::QueryCallbackStatus::CONTINUE);
                }
            }
            else if (ack.status_ == rai::QueryStatus::PRUNED
                     || ack.status_ == rai::QueryStatus::TIMEOUT)
            {
                result.insert(result.end(), 1,
                              rai::QueryCallbackStatus::CONTINUE);
            }
            else
            {
                result.insert(result.end(), 1,
                              rai::QueryCallbackStatus::FINISH);
                rai::Stats::Add(rai::ErrorCode::LOGIC_ERROR,
                                "Rewarder::QueryCallback_: invalid ack status=",
                                static_cast<uint32_t>(ack.status_));
            }
        };

    return callback;
}

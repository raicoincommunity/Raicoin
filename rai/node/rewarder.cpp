#include <rai/node/rewarder.hpp>
#include <functional>
#include <rai/node/node.hpp>

rai::Rewarder::Rewarder(rai::Node& node, const rai::Account& account,
                        uint32_t times)
    : node_(node),
      daily_send_times_(times),
      fan_(account, rai::Fan::FAN_OUT),
      stopped_(false),
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
            if (error_code != rai::ErrorCode::SUCCESS || block == nullptr
                || block->Height() == 0)
            {
                return;
            }
            if (!block->HasRepresentative())
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
        if (node_.Status() != rai::NodeStatus::RUN)
        {
            condition_.wait_for(lock, std::chrono::seconds(5));
            continue;
        }

        if (block_ != nullptr)
        {
            condition_.wait_for(lock, std::chrono::seconds(1));
            continue;
        }
        else if (!actions_.empty())
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
            else if (error_code == rai::ErrorCode::ACCOUNT_LIMITED)
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
    rai::RawKey destination;
    fan_.Get(destination);

    QueueAction(std::bind(&rai::Rewarder::ProcessSend_, this, destination.data_,
                          std::placeholders::_1),
                [](rai::ErrorCode, const std::shared_ptr<rai::Block>&) {});
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
        return rai::ErrorCode::LEDGER_REWARDABLE_INFO_GET;
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
        if (info.forks_ > rai::MaxAllowedForks(timestamp))
        {
            return rai::ErrorCode::ACCOUNT_LIMITED;
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
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::AccountInfo info;
    bool error =
        node_.ledger_.AccountInfoGet(transaction, node_.account_, info);
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
    if (info.forks_ > rai::MaxAllowedForks(timestamp))
    {
        return rai::ErrorCode::ACCOUNT_LIMITED;
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

#include <rai/node/node.hpp>
#include <rai/node/syncer.hpp>

rai::SyncStat::SyncStat() : total_(0), miss_(0)
{
}

void rai::SyncStat::Reset()
{
    total_ = 0;
    miss_  = 0;
}

rai::Syncer::Syncer(rai::Node& node) : node_(node)
{
    node_.observers_.block_.Add(
        [this](const rai::BlockProcessResult& result,
               const std::shared_ptr<rai::Block>& block) {
            this->ProcessorCallback(result, block);
        });
}

void rai::Syncer::Add(const rai::Account& account, uint64_t height, bool stat)
{
    Add(account, height, rai::BlockHash(0), stat);
}

void rai::Syncer::Add(const rai::Account& account, uint64_t height,
                      const rai::BlockHash& previous, bool stat)
{
    rai::SyncInfo info{rai::SyncStatus::QUERY, stat, height, previous,
                       rai::BlockHash(0)};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bool error = Add_(account, info);
        if (error)
        {
            return;
        }

        if (stat)
        {
            ++stat_.total_;
        }
    }

    BlockQuery_(account, info);
}

bool rai::Syncer::Busy() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return syncs_.size() >= rai::Syncer::BUSY_SIZE;
}

bool rai::Syncer::Empty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return syncs_.size() == 0;
}

void rai::Syncer::Erase(const rai::Account& account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = syncs_.find(account);
    if (it != syncs_.end())
    {
        syncs_.erase(it);
    }
}

void rai::Syncer::ProcessorCallback(const rai::BlockProcessResult& result,
                                    const std::shared_ptr<rai::Block>& block)
{
    if (result.operation_ != rai::BlockOperation::APPEND
        && result.operation_ != rai::BlockOperation::DROP)
    {
        std::cout << "==================>>>ProcessorCallback:1\n";
        return;
    }

    rai::SyncInfo info;
    bool query        = false;
    bool source_miss  = false;
    bool sync_related = false;
    do
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = syncs_.find(block->Account());
        if (it == syncs_.end()
            || it->second.status_ != rai::SyncStatus::PROCESS)
        {
            std::cout << "==================>>>ProcessorCallback:2\n";
            return;
        }

        if (it->second.current_ != block->Hash())
        {
            std::cout << "==================>>>ProcessorCallback:3\n";
            return;
        }

        if (result.operation_ == rai::BlockOperation::DROP)
        {
            it->second.status_  = rai::SyncStatus::QUERY;
            it->second.current_ = rai::BlockHash(0);
            info                = it->second;
            query               = true;
            break;
        }

        // APPEND
        if (result.error_code_ == rai::ErrorCode::SUCCESS
            || result.error_code_ == rai::ErrorCode::BLOCK_PROCESS_EXISTS)
        {
            it->second.status_   = rai::SyncStatus::QUERY;
            it->second.current_  = rai::BlockHash(0);
            it->second.height_   = block->Height() + 1;
            it->second.previous_ = block->Hash();
            info                 = it->second;
            query                = true;
            sync_related         = true;
        }
        else if (result.error_code_
                     == rai::ErrorCode::BLOCK_PROCESS_GAP_RECEIVE_SOURCE
                 || result.error_code_
                        == rai::ErrorCode::BLOCK_PROCESS_GAP_REWARD_SOURCE)
        {
            source_miss = true;
            syncs_.erase(it);
        }
        else
        {
            syncs_.erase(it);
            return;
        }
    } while (0);

    if (query)
    {
        BlockQuery_(block->Account(), info);
    }

    if (source_miss)
    {
        BlockQuery_(block->Link());
    }

    if (sync_related)
    {
        SyncRelated(block);
    }
}

void rai::Syncer::QueryCallback(const rai::Account& account,
                                rai::QueryStatus status,
                                const std::shared_ptr<rai::Block>& block)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = syncs_.find(account);
        if (it == syncs_.end())
        {
            return;
        }

        if (it->second.status_ != rai::SyncStatus::QUERY)
        {
            return;
        }

        if (status == rai::QueryStatus::MISS)
        {
            if (it->second.first_)
            {
                ++stat_.miss_;
            }
            syncs_.erase(it);
            return;
        }
        else if (status == rai::QueryStatus::SUCCESS)
        {
            it->second.first_   = false;
            it->second.status_  = rai::SyncStatus::PROCESS;
            it->second.current_ = block->Hash();
            assert(it->second.height_ == block->Height());
        }
        else if (status == rai::QueryStatus::FORK)
        {
            syncs_.erase(it);
        }
        else
        {
            assert(0);
            syncs_.erase(it);
            return;
        }
    }

    node_.block_processor_.Add(block);
}

rai::SyncStat rai::Syncer::Stat() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stat_;
}

void rai::Syncer::ResetStat()
{
    std::lock_guard<std::mutex> lock(mutex_);
    stat_.Reset();
    for (auto it = syncs_.begin(); it != syncs_.end(); ++it)
    {
        it->second.first_ = false;
    }
}

size_t rai::Syncer::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);  
    return syncs_.size();
}

void rai::Syncer::SyncAccount(rai::Transaction& transaction,
                               const rai::Account& account)
{
    rai::AccountInfo account_info;
    bool error =
        node_.ledger_.AccountInfoGet(transaction, account, account_info);
    if (error || !account_info.Valid())
    {
        node_.syncer_.Add(account, 0, false);
    }
    else
    {
        node_.syncer_.Add(account, account_info.head_height_ + 1,
                            account_info.head_, false);
    }
}

void rai::Syncer::SyncRelated(const std::shared_ptr<rai::Block>& block)
{
    if (!block->HasRepresentative()
        && block->Opcode() != rai::BlockOpcode::SEND)
    {
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, node_.ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    if (block->Opcode() == rai::BlockOpcode::SEND)
    {
        SyncAccount(transaction, block->Link());
    }

    if (block->HasRepresentative() && block->Height() > 0)
    {
        rai::Account rep = block->Representative();
        std::shared_ptr<rai::Block> previous(nullptr);
        if (block->Opcode() == rai::BlockOpcode::CHANGE)
        {
            bool error = node_.ledger_.BlockGet(transaction, block->Previous(),
                                                previous);
            if (error)
            {
                return;
            }
            rep = previous->Representative();
        }

        rai::RewardableInfo info;
        bool error = node_.ledger_.RewardableInfoGet(transaction, rep,
                                                     block->Previous(), info);
        if (error || info.valid_timestamp_ > rai::CurrentTimestamp())
        {
            return;
        }
        SyncAccount(transaction, rep);
    }
}

bool rai::Syncer::Add_(const rai::Account& account, const rai::SyncInfo& info)
{
    auto it = syncs_.find(account);
    if (it != syncs_.end())
    {
        return true;
    }

    syncs_[account] = info;
    return false;
}

void rai::Syncer::BlockQuery_(const rai::Account& account,
                              const rai::SyncInfo& info)
{
    if (info.height_ == 0 || info.previous_.IsZero())
    {
        node_.block_queries_.QueryByHeight(account, info.height_, false,
                                           QueryCallbackByAccount_(account));
    }
    else
    {
        node_.block_queries_.QueryByPrevious(account, info.height_,
                                             info.previous_, false,
                                             QueryCallbackByAccount_(account));
    }
}

void rai::Syncer::BlockQuery_(const rai::BlockHash& hash)
{
    node_.block_queries_.QueryByHash(rai::Account(0),
                                     rai::Block::INVALID_HEIGHT, hash, true,
                                     QueryCallbackByHash_(hash));
}

rai::QueryCallback rai::Syncer::QueryCallbackByAccount_(
    const rai::Account& account)
{
    std::weak_ptr<rai::Node> node_w(node_.Shared());
    rai::QueryCallback callback = [node_w, account, count = 0](
                                      const std::vector<rai::QueryAck>& acks,
                                      std::vector<rai::QueryCallbackStatus>&
                                          result) mutable {
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
            node->syncer_.Erase(account);
            return;
        }

        auto& ack = acks[0];
        if (ack.status_ == rai::QueryStatus::FORK
            || ack.status_ == rai::QueryStatus::SUCCESS)
        {
            result.insert(result.end(), 1, rai::QueryCallbackStatus::FINISH);
            node->syncer_.QueryCallback(account, ack.status_, ack.block_);
        }
        else if (ack.status_ == rai::QueryStatus::MISS)
        {
            ++count;
            if (count >= 3)
            {
                result.insert(result.end(), 1,
                              rai::QueryCallbackStatus::FINISH);
                node->syncer_.QueryCallback(account, ack.status_, ack.block_);
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
            result.insert(result.end(), 1, rai::QueryCallbackStatus::CONTINUE);
        }
        else
        {
            result.insert(result.end(), 1, rai::QueryCallbackStatus::FINISH);
            node->syncer_.Erase(account);
        }
    };
    return callback;
}

rai::QueryCallback rai::Syncer::QueryCallbackByHash_(const rai::BlockHash& hash)
{
    std::weak_ptr<rai::Node> node_w(node_.Shared());
    rai::QueryCallback callback = [node_w, count = 0](
                                      const std::vector<rai::QueryAck>& acks,
                                      std::vector<rai::QueryCallbackStatus>&
                                          result) mutable {
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
            return;
        }

        auto& ack = acks[0];
        if (ack.status_ == rai::QueryStatus::SUCCESS)
        {
            result.insert(result.end(), 1, rai::QueryCallbackStatus::FINISH);
            
            rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
            rai::Transaction transaction(error_code, node->ledger_, false);
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                // log
                return;
            }
            
            node->syncer_.SyncAccount(transaction, ack.block_->Account());
        }
        else if (ack.status_ == rai::QueryStatus::MISS)
        {
            ++count;
            if (count >= 3)
            {
                result.insert(result.end(), 1,
                              rai::QueryCallbackStatus::FINISH);
            }
            else
            {
                result.insert(result.end(), 1,
                              rai::QueryCallbackStatus::CONTINUE);
            }
        }
        else if (ack.status_ == rai::QueryStatus::TIMEOUT
                 || ack.status_ == rai::QueryStatus::FORK
                 || ack.status_ == rai::QueryStatus::PRUNED)
        {
            result.insert(result.end(), 1, rai::QueryCallbackStatus::CONTINUE);
        }
        else
        {
            result.insert(result.end(), 1, rai::QueryCallbackStatus::FINISH);
        }
    };
    return callback;
}

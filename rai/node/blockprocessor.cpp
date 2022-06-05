#include <rai/node/blockprocessor.hpp>

#include <rai/common/parameters.hpp>
#include <rai/node/node.hpp>

std::string rai::BlockOperationToString(rai::BlockOperation operation)
{
    switch (operation)
    {
        case rai::BlockOperation::INVALID:
        {
            return "invalid";
        }
        case rai::BlockOperation::APPEND:
        {
            return "append";
        }
        case rai::BlockOperation::PREPEND:
        {
            return "prepend";
        }
        case rai::BlockOperation::ROLLBACK:
        {
            return "rollback";
        }
        case rai::BlockOperation::DROP:
        {
            return "drop";
        }
        case rai::BlockOperation::CONFIRM:
        {
            return "confirm";
        }
        default:
        {
            return std::to_string(static_cast<uint64_t>(operation));
        }
    }
}

rai::BlockOperation rai::StringToBlockOperation(const std::string& str)
{
    if (str == "append")
    {
        return rai::BlockOperation::APPEND;
    }
    else if (str == "prepend")
    {
        return rai::BlockOperation::PREPEND;
    }
    else if (str == "rollback")
    {
        return rai::BlockOperation::ROLLBACK;
    }
    else if (str == "drop")
    {
        return rai::BlockOperation::DROP;
    }
    else if (str == "confirm")
    {
        return rai::BlockOperation::CONFIRM;
    }
    else
    {
        return rai::BlockOperation::INVALID;
    }
}

rai::BlockForced::BlockForced(rai::BlockOperation operation,
                              const std::shared_ptr<rai::Block>& block)
    : operation_(static_cast<uint64_t>(operation)), block_(block)
{
}

rai::BlockForced::BlockForced(uint64_t operation,
                              const std::shared_ptr<rai::Block>& block)
    : operation_(operation), block_(block)
{
}

rai::BlockProcessor::BlockProcessor(rai::Node& node)
    : node_(node),
      ledger_(node.ledger_),
      operation_(static_cast<uint64_t>(rai::BlockOperation::DYNAMIC_BEGIN)),
      stopped_(false),
      thread_([this]() { this->Run(); })
{
}

rai::BlockProcessor::~BlockProcessor()
{
    Stop();
}

void rai::BlockProcessor::Add(const std::shared_ptr<rai::Block>& block)
{
    uint64_t priority = Priority_(block);
    auto now = std::chrono::steady_clock::now();
    OrderedKey key{priority, now};
    BlockInfo block_info{key, block->Hash(), block};

    std::lock_guard<std::mutex> lock(mutex_);
    auto ret = blocks_.insert(block_info);
    if (!ret.second)
    {
        return;
    }

    if (blocks_.size() > rai::BlockProcessor::MAX_BLOCKS)
    {
        auto it = blocks_.rbegin();
        rai::BlockProcessResult result{rai::BlockOperation::DROP,
                                       rai::ErrorCode::SUCCESS, 0};
        block_observer_(result, it->block_);
        node_.dumpers_.block_.Dump(result, it->block_);
        blocks_.erase((++it).base());
    }

    condition_.notify_all();
}

void rai::BlockProcessor::AddForced(const rai::BlockForced& forced)
{
    std::lock_guard<std::mutex> lock(mutex_);
    blocks_forced_.push_back(forced);
    condition_.notify_all();
}

void rai::BlockProcessor::AddFork(const rai::BlockFork& fork)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (blocks_fork_.size() >= rai::BlockProcessor::MAX_BLOCKS_FORK)
    {
        return;
    }
    blocks_fork_.push_back(fork);
    condition_.notify_all();
}

bool rai::BlockProcessor::Busy() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (blocks_.size() * 100 >= rai::BlockProcessor::MAX_BLOCKS
                                    * rai::BlockProcessor::BUSY_PERCENTAGE)
    {
        return true;
    }
    if (blocks_fork_.size() * 100 >= rai::BlockProcessor::MAX_BLOCKS_FORK
                                    * rai::BlockProcessor::BUSY_PERCENTAGE)
    {
        return true;
    }
    return false;
}

void rai::BlockProcessor::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);

    while (!stopped_)
    {
        if (!blocks_fork_.empty())
        {
            rai::BlockFork fork = blocks_fork_.front();
            blocks_fork_.pop_front();

            lock.unlock();
            if (!fork.from_local_)
            {
                ProcessBlock_(fork.first_, true);
                ProcessBlock_(fork.second_, true);
            }
            ProcessBlockFork_(fork.first_, fork.second_);
            lock.lock();
        }
        else if (!blocks_forced_.empty())
        {
            rai::BlockForced forced = blocks_forced_.front();
            blocks_forced_.pop_front();

            lock.unlock();
            ProcessBlockForced_(forced.operation_, forced.block_);
            lock.lock();
        }
        else if (!blocks_.empty())
        {
            auto it = blocks_.begin();
            BlockInfo block_info(*it);
            blocks_.erase(it);

            lock.unlock();
            ProcessBlock_(block_info.block_, false);
            lock.lock();
        }
        else
        {
            condition_.wait(lock);
        }
    }
}

void rai::BlockProcessor::Stop()
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

void rai::BlockProcessor::Status(rai::Ptree& status) const
{
    status.put("operation_id", std::to_string(operation_));

    std::lock_guard<std::mutex> lock(mutex_);

    status.put("blocks_count", std::to_string(blocks_.size()));
    status.put("forks_count", std::to_string(blocks_fork_.size()));
    status.put("forced_count", std::to_string(blocks_forced_.size()));
}

uint64_t rai::BlockProcessor::Priority_(
    const std::shared_ptr<rai::Block>& block)
{
    uint64_t constexpr max_priority =
        rai::MAX_ACCOUNT_CREDIT * rai::TRANSACTIONS_PER_CREDIT;
    uint64_t now = rai::CurrentTimestamp();
    if (now > block->Timestamp() + 3600)  // more than an hour ago
    {
        return max_priority;
    }

    if (block->Opcode() == rai::BlockOpcode::REWARD)
    {
        return max_priority / 2;
    }

    uint64_t credit  = block->Credit();
    uint64_t counter = block->Counter();
    if (counter == 0 || credit == 0)
    {
        return max_priority;
    }

    uint64_t total = credit * rai::TRANSACTIONS_PER_CREDIT;
    if (counter > total)
    {
        return max_priority;
    }
    return counter * max_priority / total;
}

uint64_t rai::BlockProcessor::DynamicOpration_()
{
    uint64_t result = operation_++;
    uint64_t begin = static_cast<uint64_t>(rai::BlockOperation::DYNAMIC_BEGIN);
    while (result < begin)
    {
        result = operation_++;
    }
    return result;
}

void rai::BlockProcessor::ProcessBlock_(const std::shared_ptr<rai::Block>& block,
                                        bool ignore_fork)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    {
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            rai::Stats::Add(error_code, "BlockProcessor::ProcessBlock_");
            rai::BlockProcessResult result{rai::BlockOperation::DROP,
                                           error_code, 0};
            block_observer_(result, block);
            node_.dumpers_.block_.Dump(result, block);
            return;
        }

        error_code = AppendBlock_(transaction, block);
        switch (error_code)
        {
            case rai::ErrorCode::SUCCESS:
            {
                node_.QueueGapCaches(block->Hash());
                node_.Publish(block);
                break;
            }
            case rai::ErrorCode::BLOCK_PROCESS_SIGNATURE:
            case rai::ErrorCode::BLOCK_PROCESS_EXISTS:
            case rai::ErrorCode::BLOCK_PROCESS_PREVIOUS:
            case rai::ErrorCode::BLOCK_PROCESS_OPCODE:
            case rai::ErrorCode::BLOCK_PROCESS_CREDIT:
            case rai::ErrorCode::BLOCK_PROCESS_COUNTER:
            case rai::ErrorCode::BLOCK_PROCESS_TIMESTAMP:
            case rai::ErrorCode::BLOCK_PROCESS_BALANCE:
            case rai::ErrorCode::BLOCK_PROCESS_UNRECEIVABLE:
            case rai::ErrorCode::BLOCK_PROCESS_UNREWARDABLE:
            case rai::ErrorCode::BLOCK_PROCESS_PRUNED:
            case rai::ErrorCode::BLOCK_PROCESS_TYPE_MISMATCH:
            case rai::ErrorCode::BLOCK_PROCESS_REPRESENTATIVE:
            case rai::ErrorCode::BLOCK_PROCESS_LINK:
            case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_PUT:
            case rai::ErrorCode::BLOCK_PROCESS_LEDGER_SUCCESSOR_SET:
            case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET:
            case rai::ErrorCode::BLOCK_PROCESS_TYPE_UNKNOWN:
            case rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_PUT:
            case rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_DEL:
            case rai::ErrorCode::BLOCK_PROCESS_ACCOUNT_EXCEED_TRANSACTIONS:
            case rai::ErrorCode::BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_PUT:
            case rai::ErrorCode::BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_DEL:
            case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT:
            case rai::ErrorCode::BLOCK_PROCESS_CHAIN:
            case rai::ErrorCode::BLOCK_PROCESS_BINDING_COUNT:
            case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BINDING_ENTRY_PUT:
            case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BINDING_COUNT_PUT:
            {
                break;
            }
            case rai::ErrorCode::BLOCK_PROCESS_GAP_PREVIOUS:
            {
                rai::GapInfo gap(block->Previous(), block);
                node_.previous_gap_cache_.Insert(gap);
                break;
            }
            case rai::ErrorCode::BLOCK_PROCESS_GAP_RECEIVE_SOURCE:
            {
                rai::GapInfo gap(block->Link(), block);
                node_.receive_source_gap_cache_.Insert(gap);
                break;
            }
            case rai::ErrorCode::BLOCK_PROCESS_GAP_REWARD_SOURCE:
            {
                rai::GapInfo gap(block->Link(), block);
                node_.reward_source_gap_cache_.Insert(gap);
                break;
            }
            case rai::ErrorCode::BLOCK_PROCESS_FORK:
            {
                if (ignore_fork)
                {
                    transaction.Abort();
                    return;
                }
                rai::Stats::AddDetail(
                    error_code, "account=", block->Account().StringAccount(),
                    ", height=", block->Height(),
                    ", hash=", block->Hash().StringHex());
                std::shared_ptr<rai::Block> block_l(nullptr);
                bool error = ledger_.BlockGet(transaction, block->Account(),
                                              block->Height(), block_l);
                if (error)
                {
                    error_code =
                        rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT;
                    rai::Stats::AddDetail(
                        error_code,
                        "BlockProcessor::ProcessBlock_: get block by account=",
                        block->Account().StringAccount(),
                        ", height=", block->Height());
                    break;
                }
                rai::BlockFork fork{block_l, block, true};
                AddFork(fork);
                break;
            }
            default:
            {
                assert(0);
            }
        }

        if (error_code != rai::ErrorCode::SUCCESS)
        {
            transaction.Abort();
        }
    }

    // stat
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Stats::Add(error_code);
    }

    rai::BlockProcessResult result{rai::BlockOperation::APPEND, error_code, 0};
    block_observer_(result, block);
    node_.dumpers_.block_.Dump(result, block);
}

void rai::BlockProcessor::ProcessBlockFork_(
    const std::shared_ptr<rai::Block>& first,
    const std::shared_ptr<rai::Block>& second)
{
    if (!first->ForkWith(*second))
    {
        return;
    }

    bool broadcast = false;
    bool election = false;
    bool del = false;
    std::shared_ptr<rai::Block> del_first;
    std::shared_ptr<rai::Block> del_second;
    do
    {
        rai::Account account = first->Account();
        uint64_t height = first->Height();
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            return;
        }

        rai::AccountInfo info;
        bool error = ledger_.AccountInfoGet(transaction, account, info);
        if (error || !info.Valid() || height > info.head_height_)
        {
            return;
        }

        if (info.confirmed_height_ == rai::Block ::INVALID_HEIGHT
            || info.confirmed_height_ < height)
        {
            election = true;
        }

        if (ledger_.ForkExists(transaction, account, height))
        {
            break;
        }

        std::shared_ptr<rai::Block> head(nullptr);
        error = node_.ledger_.BlockGet(transaction, info.head_, head);
        if (error || head == nullptr)
        {
            rai::Stats::AddDetail(rai::ErrorCode::LEDGER_BLOCK_GET,
                                  "BlockProcessor::ProcessBlockFork_");
            return;
        }

        if (info.forks_
            < rai::MaxAllowedForks(rai::CurrentTimestamp(), head->Credit()) + 2)
        {
            error =
                ledger_.ForkPut(transaction, account, height, *first, *second);
            if (error)
            {
                rai::Stats::Add(rai::ErrorCode::BLOCK_PROCESS_LEDGER_FORK_PUT);
                transaction.Abort();
                return;
            }
            info.forks_++;
            error = ledger_.AccountInfoPut(transaction, account, info);
            if (error)
            {
                rai::Stats::Add(
                    rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT,
                    "BlockProcessor::ProcessBlockFork_");
                transaction.Abort();
                return;
            }
        }
        else
        {
            uint64_t max_height = height;
            rai::Iterator i = ledger_.ForkLowerBound(transaction, account);
            rai::Iterator n = ledger_.ForkUpperBound(transaction, account);
            for (; i != n; ++i)
            {
                std::shared_ptr<rai::Block> first(nullptr);
                std::shared_ptr<rai::Block> second(nullptr);
                error = ledger_.ForkGet(i, first, second);
                if (error || first->Account() != account)
                {
                    assert(0);
                    rai::Stats::Add(
                        rai::ErrorCode::BLOCK_PROCESS_LEDGER_FORK_GET);
                    transaction.Abort();
                    return;
                }
                if (first->Height() > max_height)
                {
                    max_height = first->Height();
                    del_first = first;
                    del_second = second;
                }
            }
            if (max_height == height)
            {
                return;
            }

            if (del_first == nullptr || del_second == nullptr
                || del_first->Height() != max_height)
            {
                assert(0);
                rai::Stats::Add(
                    rai::ErrorCode::BLOCK_PROCESS_LEDGER_FORK_GET,
                    "BlockProcessor::ProcessBlockFork_: logic error");
                transaction.Abort();
                return;
            }
            del = true;
            error = ledger_.ForkDel(transaction, account, max_height);
            if (error)
            {
                rai::Stats::Add(rai::ErrorCode::BLOCK_PROCESS_LEDGER_FORK_DEL);
                transaction.Abort();
                return;
            }
            error =
                ledger_.ForkPut(transaction, account, height, *first, *second);
            if (error)
            {
                rai::Stats::Add(rai::ErrorCode::BLOCK_PROCESS_LEDGER_FORK_PUT,
                                "replace");
                transaction.Abort();
                return;
            }
        }
        broadcast = true;
    } while(0);

    if (del && fork_observer_)
    {
        fork_observer_(false, del_first, del_second);
    }

    if (broadcast)
    {
        node_.BroadcastFork(first, second);
        if (fork_observer_)
        {
            fork_observer_(true, first, second);
        }
    }

    if (election)
    {
        node_.StartElection(first, second);
    }
}

void rai::BlockProcessor::ProcessBlockForced_(
    uint64_t operation, const std::shared_ptr<rai::Block>& block)
{
    if (operation >= static_cast<uint64_t>(rai::BlockOperation::DYNAMIC_BEGIN))
    {
        auto it = blocks_dynamic_.find(operation);
        if (it == blocks_dynamic_.end())
        {
            return;
        }
        accounts_dynamic_[operation].insert(block->Account());
        it->second.top().block_ = block;
        ProcessBlockDynamic_(operation);
    }
    else if (operation == static_cast<uint64_t>(rai::BlockOperation::APPEND))
    {
        uint64_t operation_new = DynamicOpration_();
        accounts_dynamic_[operation_new] = std::unordered_set<rai::Account>();
        accounts_dynamic_[operation_new].insert(block->Account());
        blocks_dynamic_[operation_new] = std::stack<rai::BlockDynamic>();
        blocks_dynamic_[operation_new].push(
            rai::BlockDynamic{rai::BlockOperation::APPEND, block});
        roots_dynamic_[operation_new] = block->Account();
        ProcessBlockDynamic_(operation_new);
    }
    else if (operation == static_cast<uint64_t>(rai::BlockOperation::CONFIRM))
    {
        uint64_t operation_new = DynamicOpration_();
        accounts_dynamic_[operation_new] = std::unordered_set<rai::Account>();
        accounts_dynamic_[operation_new].insert(block->Account());
        blocks_dynamic_[operation_new] = std::stack<rai::BlockDynamic>();
        blocks_dynamic_[operation_new].push(
            rai::BlockDynamic{rai::BlockOperation::CONFIRM, block});
        roots_dynamic_[operation_new] = block->Account();
        ProcessBlockDynamic_(operation_new);
    }
    else
    {
        assert(0);
    }
}

void rai::BlockProcessor::ProcessBlockDynamic_(uint64_t operation)
{
    auto it = blocks_dynamic_.find(operation);
    if (it == blocks_dynamic_.end())
    {
        return;
    }

    rai::ErrorCode error_code;
    uint64_t last_confirm_height = 0;
    auto& stack(blocks_dynamic_[operation]);
    while (!stack.empty())
    {
        rai::BlockDynamic top(stack.top());
        if (top.operation_ == rai::BlockOperation::APPEND)
        {
            error_code =
                ProcessBlockDynamicAppend_(operation, stack, top.block_);
        }
        else if (top.operation_ == rai::BlockOperation::CONFIRM)
        {
            error_code = ProcessBlockDynamicConfirm_(
                operation, stack, top.block_, last_confirm_height);
        }
        else if (top.operation_ == rai::BlockOperation::PREPEND)
        {
            error_code =
                ProcessBlockDynamicPrepend_(operation, stack, top.block_);
        }
        else if (top.operation_ == rai::BlockOperation::ROLLBACK)
        {
            error_code =
                ProcessBlockDynamicRollback_(operation, stack, top.block_);
        }
        else
        {
            assert(0);
            error_code = rai::ErrorCode::BLOCK_PROCESS_UNKNOWN_OPERATION;
        }

        // stat
        if (error_code == rai::ErrorCode::SUCCESS)
        {

        }
        else
        {
            rai::Stats::Add(error_code);
        }

        rai::BlockProcessResult result{top.operation_, error_code,
                                       last_confirm_height};
        block_observer_(result, top.block_);
        node_.dumpers_.block_.Dump(result, top.block_,
                                   roots_dynamic_[operation]);

        if (error_code == rai::ErrorCode::SUCCESS)
        {
            stack.pop();
            continue;
        }
        else if (error_code == rai::ErrorCode::BLOCK_PROCESS_CONTINUE)
        {
            continue;
        }
        else if (error_code == rai::ErrorCode::BLOCK_PROCESS_WAIT)
        {
            return;
        }
        else if (error_code == rai::ErrorCode::BLOCK_PROCESS_EXISTS)
        {
            if (top.operation_ == rai::BlockOperation::APPEND
                || top.operation_ == rai::BlockOperation::PREPEND)
            {
                stack.pop();
                continue;
            }
            break;
        }
        else if (error_code == rai::ErrorCode::BLOCK_PROCESS_PREPEND_IGNORE)
        {
            stack.pop();
            continue;
        }
        else if (error_code == rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_IGNORE)
        {
            stack.pop();
            continue;
        }
        else
        {
            break;
        }
    }

    blocks_dynamic_.erase(operation);
    UpdateForks_(accounts_dynamic_[operation]);
    accounts_dynamic_.erase(operation);
    roots_dynamic_.erase(operation);
}

rai::ErrorCode rai::BlockProcessor::ProcessBlockDynamicAppend_(
    uint64_t operation, std::stack<rai::BlockDynamic>& stack,
    const std::shared_ptr<rai::Block>& block)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, true);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code = AppendBlock_(transaction, block);
    switch (error_code)
    {
        case rai::ErrorCode::SUCCESS:
        {
            node_.QueueGapCaches(block->Hash());
            return rai::ErrorCode::SUCCESS;
        }
        case rai::ErrorCode::BLOCK_PROCESS_PREVIOUS:
        case rai::ErrorCode::BLOCK_PROCESS_GAP_PREVIOUS:
        case rai::ErrorCode::BLOCK_PROCESS_TYPE_MISMATCH:
        {
            bool error = QueryPrevious_(operation, block);
            IF_ERROR_RETURN(error, error_code);
            stack.push(rai::BlockDynamic{rai::BlockOperation::APPEND, nullptr});
            return rai::ErrorCode::BLOCK_PROCESS_WAIT;
        }
        case rai::ErrorCode::BLOCK_PROCESS_PRUNED:
        {
            bool error =
                QueryPrevious_(transaction, operation, block->Account());
            IF_ERROR_RETURN(error, error_code);
            stack.push(
                rai::BlockDynamic{rai::BlockOperation::PREPEND, nullptr});
            return rai::ErrorCode::BLOCK_PROCESS_WAIT;
        }
        case rai::ErrorCode::BLOCK_PROCESS_GAP_RECEIVE_SOURCE:
        case rai::ErrorCode::BLOCK_PROCESS_GAP_REWARD_SOURCE:
        {
            QuerySource_(operation, block->Link());
            stack.push(
                rai::BlockDynamic{rai::BlockOperation::APPEND, nullptr});
            return rai::ErrorCode::BLOCK_PROCESS_WAIT;
        }
        case rai::ErrorCode::BLOCK_PROCESS_FORK:
        {
            std::shared_ptr<rai::Block> fork_block(nullptr);
            bool error = ledger_.BlockGet(transaction, block->Account(),
                                          block->Height(), fork_block);
            IF_ERROR_RETURN(error,
                            rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET);
            accounts_dynamic_[operation].insert(fork_block->Account());
            stack.push(
                rai::BlockDynamic{rai::BlockOperation::ROLLBACK, fork_block});
            return rai::ErrorCode::BLOCK_PROCESS_CONTINUE;
        }
        case rai::ErrorCode::BLOCK_PROCESS_EXISTS:
        {
            return error_code;
        }
        case rai::ErrorCode::BLOCK_PROCESS_SIGNATURE:
        case rai::ErrorCode::BLOCK_PROCESS_OPCODE:
        case rai::ErrorCode::BLOCK_PROCESS_CREDIT:
        case rai::ErrorCode::BLOCK_PROCESS_COUNTER:
        case rai::ErrorCode::BLOCK_PROCESS_TIMESTAMP:
        case rai::ErrorCode::BLOCK_PROCESS_BALANCE:
        case rai::ErrorCode::BLOCK_PROCESS_UNRECEIVABLE:
        case rai::ErrorCode::BLOCK_PROCESS_UNREWARDABLE:
        case rai::ErrorCode::BLOCK_PROCESS_REPRESENTATIVE:
        case rai::ErrorCode::BLOCK_PROCESS_LINK:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_PUT:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_SUCCESSOR_SET:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET:
        case rai::ErrorCode::BLOCK_PROCESS_TYPE_UNKNOWN:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_PUT:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_DEL:
        case rai::ErrorCode::BLOCK_PROCESS_ACCOUNT_EXCEED_TRANSACTIONS:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_PUT:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_DEL:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT:
        case rai::ErrorCode::BLOCK_PROCESS_CHAIN:
        case rai::ErrorCode::BLOCK_PROCESS_BINDING_COUNT:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BINDING_ENTRY_PUT:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BINDING_COUNT_PUT:
        {
            transaction.Abort();
            return error_code;
        }
        default:
        {
            assert(0);
            transaction.Abort();
        }
    }
    return error_code;
}

rai::ErrorCode rai::BlockProcessor::ProcessBlockDynamicPrepend_(
    uint64_t operation, std::stack<rai::BlockDynamic>& stack,
    const std::shared_ptr<rai::Block>& block)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, true);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code = PrependBlock_(transaction, block);
    switch (error_code)
    {
        case rai::ErrorCode::SUCCESS:
        case rai::ErrorCode::BLOCK_PROCESS_PREPEND_IGNORE:
        {
            return error_code;
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_PUT:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT:
        case rai::ErrorCode::BLOCK_PROCESS_SIGNATURE:
        {
            transaction.Abort();
            return error_code;
        }
        default:
        {
            assert(0);
            transaction.Abort();
        }
    }
    return error_code;
}

rai::ErrorCode rai::BlockProcessor::ProcessBlockDynamicRollback_(
    uint64_t operation, std::stack<rai::BlockDynamic>& stack,
    const std::shared_ptr<rai::Block>& block)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, true);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code = RollbackBlock_(transaction, block);
    switch (error_code)
    {
        case rai::ErrorCode::SUCCESS:
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_IGNORE:
        {
            return error_code;
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_TAIL:
        {
            bool error =
                QueryPrevious_(transaction, operation, block->Account());
            IF_ERROR_RETURN(error, error_code);
            stack.push(
                rai::BlockDynamic{rai::BlockOperation::PREPEND, nullptr});
            return rai::ErrorCode::BLOCK_PROCESS_WAIT;
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_NON_HEAD:
        {
            rai::BlockHash hash;
            bool error =
                ledger_.BlockSuccessorGet(transaction, block->Hash(), hash);
            IF_ERROR_RETURN(error, error_code);
            std::shared_ptr<rai::Block> successor(nullptr);
            error = ledger_.BlockGet(transaction, hash, successor);
            IF_ERROR_RETURN(error, error_code);
            accounts_dynamic_[operation].insert(successor->Account());
            stack.push(
                rai::BlockDynamic{rai::BlockOperation::ROLLBACK, successor});
            return rai::ErrorCode::BLOCK_PROCESS_CONTINUE;
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_REWARDED:
        {
            std::shared_ptr<rai::Block> previous(nullptr);
            bool error =
                ledger_.BlockGet(transaction, block->Previous(), previous);
            IF_ERROR_RETURN(error, error_code);
            rai::AccountInfo info;
            error = ledger_.AccountInfoGet(transaction,
                                           previous->Representative(), info);
            IF_ERROR_RETURN(error, error_code);
            std::shared_ptr<rai::Block> head(nullptr);
            error = ledger_.BlockGet(transaction, info.head_, head);
            IF_ERROR_RETURN(error, error_code);
            accounts_dynamic_[operation].insert(head->Account());
            stack.push(rai::BlockDynamic{rai::BlockOperation::ROLLBACK, head});
            return rai::ErrorCode::BLOCK_PROCESS_CONTINUE;
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_RECEIVED:
        {
            rai::AccountInfo info;
            bool error =
                ledger_.AccountInfoGet(transaction, block->Link(), info);
            IF_ERROR_RETURN(error, error_code);
            std::shared_ptr<rai::Block> head(nullptr);
            error = ledger_.BlockGet(transaction, info.head_, head);
            IF_ERROR_RETURN(error, error_code);
            accounts_dynamic_[operation].insert(head->Account());
            stack.push(rai::BlockDynamic{rai::BlockOperation::ROLLBACK, head});
            return rai::ErrorCode::BLOCK_PROCESS_CONTINUE;
        }
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_SOURCE_PRUNED:
        {
            QuerySource_(operation, block->Link());
            stack.push(
                rai::BlockDynamic{rai::BlockOperation::APPEND, nullptr});
            return rai::ErrorCode::BLOCK_PROCESS_WAIT;
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT:
        case rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_NOT_EQUAL_TO_HEAD:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_DEL:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_DEL:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_DEL:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_SUCCESSOR_SET:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ROLLBACK_BLOCK_PUT:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_PUT:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_PUT:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BINDING_COUNT_PUT:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BINDING_ENTRY_DEL:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_BINDING_COUNT_DEL:
        {
            transaction.Abort();
            return error_code;
        }
        default:
        {
            assert(0);
            transaction.Abort();
        }
    }
    return error_code;
}

rai::ErrorCode rai::BlockProcessor::ProcessBlockDynamicConfirm_(
    uint64_t operation, std::stack<rai::BlockDynamic>& stack,
    const std::shared_ptr<rai::Block>& block, uint64_t& last_confirm_height)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, true);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code = ConfirmBlock_(transaction, block, last_confirm_height);
    switch (error_code)
    {
        case rai::ErrorCode::SUCCESS:
        {
            return error_code;
        }
        case rai::ErrorCode::BLOCK_PROCESS_CONFIRM_BLOCK_MISS:
        {
            accounts_dynamic_[operation].insert(block->Account());
            stack.push(rai::BlockDynamic{rai::BlockOperation::APPEND, block});
            return rai::ErrorCode::BLOCK_PROCESS_CONTINUE;
        }
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT:
        case rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT:
        {
            transaction.Abort();
            return error_code;
        }
        default:
        {
            assert(0);
            transaction.Abort();
        }
    }
    return error_code;
}

namespace
{
class AppendBlockVisitor : public rai::BlockVisitor
{
public:
    AppendBlockVisitor(rai::Transaction& transaction, rai::Ledger& ledger)
        : transaction_(transaction),
          ledger_(ledger)
    {
    }

    rai::ErrorCode CheckCommon(const rai::Block& block)
    {
        rai::BlockType type  = block.Type();
        if (type != rai::BlockType::TX_BLOCK
            && type != rai::BlockType::REP_BLOCK
            && type != rai::BlockType::AD_BLOCK)
        {
            return rai::ErrorCode::BLOCK_PROCESS_TYPE_UNKNOWN;
        }

        bool error = block.CheckSignature();
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_SIGNATURE);

        uint64_t timestamp = block.Timestamp();
        if (timestamp < rai::EpochTimestamp()
            || timestamp > rai::CurrentTimestamp() + rai::MAX_TIMESTAMP_DIFF)
        {
            return rai::ErrorCode::BLOCK_PROCESS_TIMESTAMP;
        }

        if (ledger_.BlockExists(transaction_, block.Hash()))
        {
            return rai::ErrorCode::BLOCK_PROCESS_EXISTS;
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode CheckFirstCommon(const rai::Block& block)
    {
        if (block.Height() != 0)
        {
            return rai::ErrorCode::BLOCK_PROCESS_GAP_PREVIOUS;
        }

        if (block.Credit() == 0)
        {
            return rai::ErrorCode::BLOCK_PROCESS_CREDIT;
        }

        if (!block.Previous().IsZero())
        {
            return rai::ErrorCode::BLOCK_PROCESS_PREVIOUS;
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode CheckSuccessorCommon(const rai::Block& block,
                                        const rai::Block& head_block,
                                        const rai::AccountInfo& info)
    {
        uint64_t height = block.Height();
        if (height < info.tail_height_)
        {
            return rai::ErrorCode::BLOCK_PROCESS_PRUNED;
        }
        else if (height <= info.head_height_)
        {
            return rai::ErrorCode::BLOCK_PROCESS_FORK;
        }
        else if (height > info.head_height_ + 1)
        {
            return rai::ErrorCode::BLOCK_PROCESS_GAP_PREVIOUS;
        }

        if (block.Type() != info.type_)
        {
            return rai::ErrorCode::BLOCK_PROCESS_TYPE_MISMATCH;
        }

        if (block.Previous() != info.head_)
        {
            return rai::ErrorCode::BLOCK_PROCESS_PREVIOUS;
        }

        if (block.Timestamp() < head_block.Timestamp())
        {
            return rai::ErrorCode::BLOCK_PROCESS_TIMESTAMP;
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode CheckCounterSame(const rai::Block& previous,
                                    const rai::Block& block)
    {
        if (rai::SameDay(block.Timestamp(), previous.Timestamp()))
        {
            if (block.Counter() == previous.Counter())
            {
                return rai::ErrorCode::SUCCESS;
            }
        }
        else
        {
            if (block.Counter() == 0)
            {
                return rai::ErrorCode::SUCCESS;
            }
        }

        return rai::ErrorCode::BLOCK_PROCESS_COUNTER;
    }

    rai::ErrorCode CheckCounterIncrease(const rai::Block& previous,
                                        const rai::Block& block)
    {
        if (rai::SameDay(block.Timestamp(), previous.Timestamp()))
        {
            if (block.Counter() != previous.Counter() + 1)
            {
                return rai::ErrorCode::BLOCK_PROCESS_COUNTER;
            }
            if (block.Counter() > block.Credit() * rai::TRANSACTIONS_PER_CREDIT)
            {
                return rai::ErrorCode::
                    BLOCK_PROCESS_ACCOUNT_EXCEED_TRANSACTIONS;
            }
        }
        else
        {
            if (block.Counter() != 1)
            {
                return rai::ErrorCode::BLOCK_PROCESS_COUNTER;
            }
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode CheckRepresentativeSame(const rai::Block& previous,
                                           const rai::Block& block)
    {
        if (!block.HasRepresentative())
        {
            return rai::ErrorCode::SUCCESS;
        }

        if (previous.Representative() == block.Representative())
        {
            return rai::ErrorCode::SUCCESS;
        }

        return rai::ErrorCode::BLOCK_PROCESS_REPRESENTATIVE;
    }

    rai::ErrorCode PutRewardableInfo(const rai::Block& block,
                                     const rai::Block& successor)
    {
        if (!block.HasRepresentative())
        {
            return rai::ErrorCode::SUCCESS;
        }

        rai::Amount reward_amount = rai::RewardAmount(
            block.Balance(), block.Timestamp(), successor.Timestamp());
        uint64_t reward_timestamp =
            rai::RewardTimestamp(block.Timestamp(), successor.Timestamp());
        if (!reward_amount.IsZero() && reward_timestamp != 0)
        {
            rai::RewardableInfo rewardable_info(block.Account(), reward_amount,
                                                reward_timestamp);
            bool error =
                ledger_.RewardableInfoPut(transaction_, block.Representative(),
                                          block.Hash(), rewardable_info);
            IF_ERROR_RETURN(
                error,
                rai::ErrorCode::BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_PUT);
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode PutBlockSuccessor(const rai::Block& block)
    {
        bool error;
        error = ledger_.BlockPut(transaction_, block.Hash(), block);
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_PUT);
        error = ledger_.BlockSuccessorSet(transaction_, block.Previous(),
                                          block.Hash());
        IF_ERROR_RETURN(error,
                        rai::ErrorCode::BLOCK_PROCESS_LEDGER_SUCCESSOR_SET);
        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode UpdateAccountInfo(const rai::Block& block,
                                     const rai::AccountInfo& info)
    {
        rai::AccountInfo info_l(info);
        info_l.head_height_ = block.Height();
        info_l.head_ = block.Hash();
        bool error =
            ledger_.AccountInfoPut(transaction_, block.Account(), info_l);
        IF_ERROR_RETURN(error,
                        rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT);
        ledger_.UpdateRichList(block);
        return rai::ErrorCode::SUCCESS;
    }

    void UpdateRepInfo(const rai::Block& previous, const rai::Block& block)
    {
        if (!block.HasRepresentative())
        {
            return;
        }
        ledger_.RepWeightSub(transaction_, previous.Representative(),
                             previous.Balance());
        ledger_.RepWeightAdd(transaction_, block.Representative(),
                             block.Balance());
        ledger_.UpdateDelegatorList(block);
    }

    rai::ErrorCode Send(const rai::Block& block) override
    {
        // 1. check block
        rai::ErrorCode error_code = CheckCommon(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::AccountInfo account_info;
        bool error = ledger_.AccountInfoGet(transaction_, block.Account(),
                                            account_info);
        bool account_exists = !error && account_info.Valid();
        if (!account_exists)
        {
            if (block.Height() != 0)
            {
                return rai::ErrorCode::BLOCK_PROCESS_GAP_PREVIOUS;
            }
            return rai::ErrorCode::BLOCK_PROCESS_OPCODE;
        }

        std::shared_ptr<rai::Block> head_block(nullptr);
        error = ledger_.BlockGet(transaction_, account_info.head_, head_block);
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET);

        error_code = CheckSuccessorCommon(block, *head_block, account_info);
        IF_NOT_SUCCESS_RETURN(error_code);

        if (block.Credit() != head_block->Credit())
        {
            return rai::ErrorCode::BLOCK_PROCESS_CREDIT;
        }

        error_code = CheckCounterIncrease(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = CheckRepresentativeSame(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        if (block.Balance() >= head_block->Balance())
        {
            return rai::ErrorCode::BLOCK_PROCESS_BALANCE;
        }

        // 2. update ledger
        error_code = PutBlockSuccessor(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = UpdateAccountInfo(block, account_info);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateRepInfo(*head_block, block);

        error_code = PutRewardableInfo(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::Amount send_amount(head_block->Balance() - block.Balance());
        rai::ReceivableInfo receivable_info(block.Account(), send_amount,
                                            block.Timestamp());
        error = ledger_.ReceivableInfoPut(transaction_, block.Link(),
                                          block.Hash(), receivable_info);
        IF_ERROR_RETURN(
            error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_PUT);

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Receive(const rai::Block& block) override
    {
        rai::ErrorCode error_code = CheckCommon(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::AccountInfo account_info;
        bool error = ledger_.AccountInfoGet(transaction_, block.Account(),
                                            account_info);
        bool account_exists = !error && account_info.Valid();
        if (!account_exists)
        {
            error_code = CheckFirstCommon(block);
            IF_NOT_SUCCESS_RETURN(error_code);

            if (block.Counter() != 1)
            {
                return rai::ErrorCode::BLOCK_PROCESS_COUNTER;
            }

            std::shared_ptr<rai::Block> source(nullptr);
            bool error = ledger_.BlockGet(transaction_, block.Link(), source);
            IF_ERROR_RETURN(error,
                            rai::ErrorCode::BLOCK_PROCESS_GAP_RECEIVE_SOURCE);
            if (block.Timestamp() < source->Timestamp())
            {
                return rai::ErrorCode::BLOCK_PROCESS_TIMESTAMP;
            }

            rai::ReceivableInfo receivable_info;
            error = ledger_.ReceivableInfoGet(transaction_, block.Account(),
                                              block.Link(), receivable_info);
            IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_UNRECEIVABLE);

            rai::Amount price = rai::CreditPrice(block.Timestamp());
            rai::uint256_t available_s(receivable_info.amount_.Number());
            rai::uint256_t balance_s(block.Balance().Number());
            rai::uint256_t price_s(price.Number());
            rai::uint256_t total_s(price_s * block.Credit() + balance_s);
            if (available_s != total_s)
            {
                return rai::ErrorCode::BLOCK_PROCESS_BALANCE;
            }

            error = ledger_.BlockPut(transaction_, block.Hash(), block);
            IF_ERROR_RETURN(error,
                            rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_PUT);

            error = ledger_.AccountInfoPut(
                transaction_, block.Account(),
                rai::AccountInfo(block.Type(), block.Hash()));
            IF_ERROR_RETURN(
                error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT);
            ledger_.UpdateRichList(block);

            if (block.HasRepresentative())
            {
                ledger_.RepWeightAdd(transaction_, block.Representative(),
                                     block.Balance());
                ledger_.UpdateDelegatorList(block);
            }

            error = ledger_.ReceivableInfoDel(transaction_, block.Account(),
                                              block.Link());
            IF_ERROR_RETURN(
                error,
                rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_DEL);
        }
        else
        {
            std::shared_ptr<rai::Block> head_block(nullptr);
            error =
                ledger_.BlockGet(transaction_, account_info.head_, head_block);
            IF_ERROR_RETURN(error,
                            rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET);

            error_code = CheckSuccessorCommon(block, *head_block, account_info);
            IF_NOT_SUCCESS_RETURN(error_code);

            if (block.Credit() != head_block->Credit())
            {
                return rai::ErrorCode::BLOCK_PROCESS_CREDIT;
            }

            error_code = CheckCounterIncrease(*head_block, block);
            IF_NOT_SUCCESS_RETURN(error_code);

            error_code = CheckRepresentativeSame(*head_block, block);
            IF_NOT_SUCCESS_RETURN(error_code);

            std::shared_ptr<rai::Block> source(nullptr);
            bool error = ledger_.BlockGet(transaction_, block.Link(), source);
            IF_ERROR_RETURN(error,
                            rai::ErrorCode::BLOCK_PROCESS_GAP_RECEIVE_SOURCE);
            if (block.Timestamp() < source->Timestamp())
            {
                return rai::ErrorCode::BLOCK_PROCESS_TIMESTAMP;
            }

            if (block.Balance() <= head_block->Balance())
            {
                return rai::ErrorCode::BLOCK_PROCESS_BALANCE;
            }
            rai::ReceivableInfo receivable_info;
            error = ledger_.ReceivableInfoGet(transaction_, block.Account(),
                                              block.Link(), receivable_info);
            IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_UNRECEIVABLE);
            rai::Amount receive_amount(block.Balance() - head_block->Balance());
            if (receivable_info.amount_ != receive_amount)
            {
                return rai::ErrorCode::BLOCK_PROCESS_BALANCE;
            }

            error_code = PutBlockSuccessor(block);
            IF_NOT_SUCCESS_RETURN(error_code);

            error_code = UpdateAccountInfo(block, account_info);
            IF_NOT_SUCCESS_RETURN(error_code);

            UpdateRepInfo(*head_block, block);

            error_code = PutRewardableInfo(*head_block, block);
            IF_NOT_SUCCESS_RETURN(error_code);

            error = ledger_.ReceivableInfoDel(transaction_, block.Account(),
                                              block.Link());
            IF_ERROR_RETURN(
                error,
                rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_DEL);
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Change(const rai::Block& block) override
    {
        rai::ErrorCode error_code = CheckCommon(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::AccountInfo account_info;
        bool error = ledger_.AccountInfoGet(transaction_, block.Account(),
                                            account_info);
        bool account_exists = !error && account_info.Valid();
        if (!account_exists)
        {
            if (block.Height() != 0)
            {
                return rai::ErrorCode::BLOCK_PROCESS_GAP_PREVIOUS;
            }
            return rai::ErrorCode::BLOCK_PROCESS_OPCODE;
        }

        std::shared_ptr<rai::Block> head_block(nullptr);
        error = ledger_.BlockGet(transaction_, account_info.head_, head_block);
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET);

        error_code = CheckSuccessorCommon(block, *head_block, account_info);
        IF_NOT_SUCCESS_RETURN(error_code);

        if (block.Credit() != head_block->Credit())
        {
            return rai::ErrorCode::BLOCK_PROCESS_CREDIT;
        }

        error_code = CheckCounterIncrease(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        if (block.Balance() != head_block->Balance())
        {
            return rai::ErrorCode::BLOCK_PROCESS_BALANCE;
        }

        if (!block.Link().IsZero())
        {
            return rai::ErrorCode::BLOCK_PROCESS_LINK;
        }

        error_code = PutBlockSuccessor(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = UpdateAccountInfo(block, account_info);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateRepInfo(*head_block, block);

        error_code = PutRewardableInfo(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Credit(const rai::Block& block) override
    {
        rai::ErrorCode error_code = CheckCommon(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::AccountInfo account_info;
        bool error = ledger_.AccountInfoGet(transaction_, block.Account(),
                                            account_info);
        bool account_exists = !error && account_info.Valid();
        if (!account_exists)
        {
            if (block.Height() != 0)
            {
                return rai::ErrorCode::BLOCK_PROCESS_GAP_PREVIOUS;
            }
            return rai::ErrorCode::BLOCK_PROCESS_OPCODE;
        }

        std::shared_ptr<rai::Block> head_block(nullptr);
        error = ledger_.BlockGet(transaction_, account_info.head_, head_block);
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET);

        error_code = CheckSuccessorCommon(block, *head_block, account_info);
        IF_NOT_SUCCESS_RETURN(error_code);

        if (block.Credit() <= head_block->Credit())
        {
            return rai::ErrorCode::BLOCK_PROCESS_CREDIT;
        }

        error_code = CheckCounterIncrease(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = CheckRepresentativeSame(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::Amount price = rai::CreditPrice(block.Timestamp());
        rai::uint256_t price_s(price.Number());
        rai::uint256_t total_s(head_block->Balance().Number());
        rai::uint256_t balance_s(block.Balance().Number());
        if (total_s
            != balance_s + price_s * (block.Credit() - head_block->Credit()))
        {
            return rai::ErrorCode::BLOCK_PROCESS_BALANCE;
        }

        if (!block.Link().IsZero())
        {
            return rai::ErrorCode::BLOCK_PROCESS_LINK;
        }

        error_code = PutBlockSuccessor(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = UpdateAccountInfo(block, account_info);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateRepInfo(*head_block, block);
        
        error_code = PutRewardableInfo(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Reward(const rai::Block& block) override
    {
        rai::ErrorCode error_code = CheckCommon(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::AccountInfo account_info;
        bool error = ledger_.AccountInfoGet(transaction_, block.Account(),
                                            account_info);
        bool account_exists = !error && account_info.Valid();
        if (!account_exists)
        {
            error_code = CheckFirstCommon(block);
            IF_NOT_SUCCESS_RETURN(error_code);

            if (block.Counter() != 0)
            {
                return rai::ErrorCode::BLOCK_PROCESS_COUNTER;
            }

            std::shared_ptr<rai::Block> source(nullptr);
            bool error = ledger_.BlockGet(transaction_, block.Link(), source);
            IF_ERROR_RETURN(error,
                            rai::ErrorCode::BLOCK_PROCESS_GAP_REWARD_SOURCE);
            if (block.Timestamp() < source->Timestamp())
            {
                return rai::ErrorCode::BLOCK_PROCESS_TIMESTAMP;
            }

            rai::RewardableInfo rewardable_info;
            error = ledger_.RewardableInfoGet(transaction_, block.Account(),
                                              block.Link(), rewardable_info);
            IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_UNREWARDABLE);
            if (block.Timestamp() < rewardable_info.valid_timestamp_)
            {
                return rai::ErrorCode::BLOCK_PROCESS_TIMESTAMP;
            }

            rai::Amount price = rai::CreditPrice(block.Timestamp());
            rai::uint256_t available_s(rewardable_info.amount_.Number());
            rai::uint256_t balance_s(block.Balance().Number());
            rai::uint256_t price_s(price.Number());
            rai::uint256_t total_s(price_s * block.Credit() + balance_s);
            if (available_s != total_s)
            {
                return rai::ErrorCode::BLOCK_PROCESS_BALANCE;
            }

            error = ledger_.BlockPut(transaction_, block.Hash(), block);
            IF_ERROR_RETURN(error,
                            rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_PUT);

            error = ledger_.AccountInfoPut(
                transaction_, block.Account(),
                rai::AccountInfo(block.Type(), block.Hash()));
            IF_ERROR_RETURN(
                error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT);

            error = ledger_.RewardableInfoDel(transaction_, block.Account(),
                                              block.Link());
            IF_ERROR_RETURN(
                error,
                rai::ErrorCode::BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_DEL);
        }
        else
        {
            std::shared_ptr<rai::Block> head_block(nullptr);
            error =
                ledger_.BlockGet(transaction_, account_info.head_, head_block);
            IF_ERROR_RETURN(error,
                            rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET);

            error_code = CheckSuccessorCommon(block, *head_block, account_info);
            IF_NOT_SUCCESS_RETURN(error_code);

            if (block.Credit() != head_block->Credit())
            {
                return rai::ErrorCode::BLOCK_PROCESS_CREDIT;
            }

            error_code = CheckCounterSame(*head_block, block);
            IF_NOT_SUCCESS_RETURN(error_code);

            std::shared_ptr<rai::Block> source(nullptr);
            bool error = ledger_.BlockGet(transaction_, block.Link(), source);
            IF_ERROR_RETURN(error,
                            rai::ErrorCode::BLOCK_PROCESS_GAP_REWARD_SOURCE);
            if (block.Timestamp() < source->Timestamp())
            {
                return rai::ErrorCode::BLOCK_PROCESS_TIMESTAMP;
            }

            if (block.Balance() <= head_block->Balance())
            {
                return rai::ErrorCode::BLOCK_PROCESS_BALANCE;
            }
            rai::RewardableInfo rewardable_info;
            error = ledger_.RewardableInfoGet(transaction_, block.Account(),
                                              block.Link(), rewardable_info);
            IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_UNREWARDABLE);
            if (block.Timestamp() < rewardable_info.valid_timestamp_)
            {
                return rai::ErrorCode::BLOCK_PROCESS_TIMESTAMP;
            }
            rai::Amount reward_amount(block.Balance() - head_block->Balance());
            if (rewardable_info.amount_ != reward_amount)
            {
                return rai::ErrorCode::BLOCK_PROCESS_BALANCE;
            }

            error_code = PutBlockSuccessor(block);
            IF_NOT_SUCCESS_RETURN(error_code);

            error_code = UpdateAccountInfo(block, account_info);
            IF_NOT_SUCCESS_RETURN(error_code);

            error = ledger_.RewardableInfoDel(transaction_, block.Account(),
                                              block.Link());
            IF_ERROR_RETURN(
                error,
                rai::ErrorCode::BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_DEL);
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Destroy(const rai::Block& block) override
    {
        // 1. check block
        rai::ErrorCode error_code = CheckCommon(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::AccountInfo account_info;
        bool error =
            ledger_.AccountInfoGet(transaction_, block.Account(), account_info);
        bool account_exists = !error && account_info.Valid();
        if (!account_exists)
        {
            if (block.Height() != 0)
            {
                return rai::ErrorCode::BLOCK_PROCESS_GAP_PREVIOUS;
            }
            return rai::ErrorCode::BLOCK_PROCESS_OPCODE;
        }

        std::shared_ptr<rai::Block> head_block(nullptr);
        error = ledger_.BlockGet(transaction_, account_info.head_, head_block);
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET);

        error_code = CheckSuccessorCommon(block, *head_block, account_info);
        IF_NOT_SUCCESS_RETURN(error_code);

        if (block.Credit() != head_block->Credit())
        {
            return rai::ErrorCode::BLOCK_PROCESS_CREDIT;
        }

        error_code = CheckCounterIncrease(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = CheckRepresentativeSame(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        if (block.Balance() >= head_block->Balance())
        {
            return rai::ErrorCode::BLOCK_PROCESS_BALANCE;
        }

        // 2. update ledger
        error_code = PutBlockSuccessor(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = UpdateAccountInfo(block, account_info);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateRepInfo(*head_block, block);

        error_code = PutRewardableInfo(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Bind(const rai::Block& block) override
    {
        rai::ErrorCode error_code = CheckCommon(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::AccountInfo account_info;
        bool error = ledger_.AccountInfoGet(transaction_, block.Account(),
                                            account_info);
        bool account_exists = !error && account_info.Valid();
        if (!account_exists)
        {
            if (block.Height() != 0)
            {
                return rai::ErrorCode::BLOCK_PROCESS_GAP_PREVIOUS;
            }
            return rai::ErrorCode::BLOCK_PROCESS_OPCODE;
        }

        std::shared_ptr<rai::Block> head_block(nullptr);
        error = ledger_.BlockGet(transaction_, account_info.head_, head_block);
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET);

        error_code = CheckSuccessorCommon(block, *head_block, account_info);
        IF_NOT_SUCCESS_RETURN(error_code);

        if (block.Credit() != head_block->Credit())
        {
            return rai::ErrorCode::BLOCK_PROCESS_CREDIT;
        }

        error_code = CheckCounterIncrease(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        if (block.Balance() != head_block->Balance())
        {
            return rai::ErrorCode::BLOCK_PROCESS_BALANCE;
        }

        if (!block.HasChain() || block.Chain() == rai::Chain::INVALID)
        {
            return rai::ErrorCode::BLOCK_PROCESS_CHAIN;
        }

        uint64_t count;
        error = ledger_.BindingCountGet(transaction_, block.Account(), count);
        if (error)
        {
            count = 0;
        }
        else
        {
            if (count >= rai::AllowedBindings(head_block->Credit()))
            {
                return rai::ErrorCode::BLOCK_PROCESS_BINDING_COUNT;
            }
        }

        rai::BindingEntry entry(block.Chain(), block.Link());
        error = ledger_.BindingEntryPut(transaction_, block.Account(),
                                        block.Height(), entry);
        IF_ERROR_RETURN(error,
                        rai::ErrorCode::BLOCK_PROCESS_LEDGER_BINDING_ENTRY_PUT);

        ++count;
        error = ledger_.BindingCountPut(transaction_, block.Account(), count);
        IF_ERROR_RETURN(error,
                        rai::ErrorCode::BLOCK_PROCESS_LEDGER_BINDING_COUNT_PUT);

        error_code = PutBlockSuccessor(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = UpdateAccountInfo(block, account_info);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateRepInfo(*head_block, block);

        error_code = PutRewardableInfo(*head_block, block);
        IF_NOT_SUCCESS_RETURN(error_code);

        return rai::ErrorCode::SUCCESS;
    }

    rai::Transaction& transaction_;
    rai::Ledger& ledger_;
};
}  // namespace

rai::ErrorCode rai::BlockProcessor::AppendBlock_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block)
{
    AppendBlockVisitor visitor(transaction, ledger_);
    return block->Visit(visitor);
}

rai::ErrorCode rai::BlockProcessor::PrependBlock_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block)
{
    bool error = block->CheckSignature();
    IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_SIGNATURE);

    rai::AccountInfo info;
    error = ledger_.AccountInfoGet(transaction, block->Account(), info);
    IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_PREPEND_IGNORE);

    if (info.tail_height_ != block->Height() + 1)
    {
        return rai::ErrorCode::BLOCK_PROCESS_PREPEND_IGNORE;
    }
    
    std::shared_ptr<rai::Block> block_tail(nullptr);
    error = ledger_.BlockGet(transaction, info.tail_, block_tail);
    IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_GET);
    if (block_tail->Previous() != block->Hash())
    {
        return rai::ErrorCode::BLOCK_PROCESS_PREPEND_IGNORE;
    }

    error = ledger_.BlockPut(transaction, block->Hash(), *block, info.tail_);
    IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_PUT);

    info.tail_ = block->Hash();
    info.tail_height_ = block->Height();
    error = ledger_.AccountInfoPut(transaction, block->Account(), info);
    IF_ERROR_RETURN(error,
                    rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT);

    return rai::ErrorCode::SUCCESS;
}

namespace
{
class RollbackBlockVisitor : public rai::BlockVisitor
{
public:
    RollbackBlockVisitor(rai::Transaction& transaction, rai::Ledger& ledger)
        : transaction_(transaction),
          ledger_(ledger),
          previous_(nullptr),
          delete_rewardable_(false),
          account_info_()
    {
    }

    rai::ErrorCode Check(const rai::Block& block)
    {
        if (!ledger_.BlockExists(transaction_, block.Hash()))
        {
            return rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_IGNORE;
        }

        bool error = ledger_.AccountInfoGet(transaction_, block.Account(),
                                            account_info_);
        if (error || !account_info_.Valid())
        {
            rai::Stats::AddDetail(
                rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT,
                "RollbackBlockVisitor::Check: failed to get account info, "
                "account=",
                block.Account().StringAccount());
            return rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT;
        }

        if (block.Height() < account_info_.tail_height_
            || block.Height() > account_info_.head_height_)
        {
            rai::Stats::AddDetail(
                rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT,
                "RollbackBlockVisitor::Check: invalid block height, account=",
                block.Account().StringAccount(), ", height=", block.Height());
            return rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT;
        }

        if (account_info_.tail_height_ == account_info_.head_height_
            && account_info_.tail_height_ != 0)
        {
            return rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_TAIL;
        }

        if (block.Height() != account_info_.head_height_)
        {
            return rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_NON_HEAD;
        }
        if (block.Hash() != account_info_.head_)
        {
            rai::Stats::AddDetail(
                rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT,
                "RollbackBlockVisitor::Check: invalid block hash, hash=",
                block.Hash().StringHex());
            return rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT;
        }
        std::shared_ptr<rai::Block> head(nullptr);
        error = ledger_.BlockGet(transaction_, account_info_.head_, head);
        if (error)
        {
            rai::Stats::AddDetail(
                rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT,
                "RollbackBlockVisitor::Check: failed to get head block, hash=",
                account_info_.head_.StringHex());
            return rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT;
        }
        if (block != *head)
        {
            return rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_NOT_EQUAL_TO_HEAD;
        }

        previous_ = nullptr;
        if (block.Height() != 0)
        {
            error = ledger_.BlockGet(transaction_, block.Previous(), previous_);
            if (error)
            {
                rai::Stats::AddDetail(
                    rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT,
                    "RollbackBlockVisitor::Check: failed to get previous "
                    "block, "
                    "hash=",
                    block.Previous().StringHex());
                return rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT;
            }
        }

        delete_rewardable_ = false;
        if (block.HasRepresentative() && block.Height() != 0)
        {
            rai::Amount amount =
                rai::RewardAmount(previous_->Balance(), previous_->Timestamp(),
                                  block.Timestamp());
            if (!amount.IsZero())
            {
                rai::RewardableInfo rewardable_info;
                error = ledger_.RewardableInfoGet(
                    transaction_, previous_->Representative(),
                    previous_->Hash(), rewardable_info);
                IF_ERROR_RETURN(
                    error, rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_REWARDED);
                delete_rewardable_ = true;
            }
        }

        return rai::ErrorCode::SUCCESS;
    }

    void UpdateRepWeight(const rai::Block& block)
    {
        if (!block.HasRepresentative())
        {
            return;
        }

        ledger_.RepWeightSub(transaction_, block.Representative(),
                             block.Balance());
        if (previous_)
        {
            ledger_.RepWeightAdd(transaction_, previous_->Representative(),
                                 previous_->Balance());
        }
    }

    rai::ErrorCode DeleteRewardableInfo(const rai::Block& block)
    {
        if (!delete_rewardable_)
        {
            return rai::ErrorCode::SUCCESS;
        }
        bool error = ledger_.RewardableInfoDel(
            transaction_, previous_->Representative(), previous_->Hash());
        IF_ERROR_RETURN(
            error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_DEL);

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode UpdateAccountInfo(const rai::Block& block)
    {
        bool error;
        if (block.Height() == 0)
        {
            error = ledger_.AccountInfoDel(transaction_, block.Account());
            IF_ERROR_RETURN(
                error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_DEL);
        }
        else
        {
            account_info_.head_        = block.Previous();
            account_info_.head_height_ = block.Height() - 1;
            if (account_info_.confirmed_height_ != rai::Block::INVALID_HEIGHT
                && account_info_.confirmed_height_ > account_info_.head_height_)
            {
                account_info_.confirmed_height_ = rai::Block::INVALID_HEIGHT;
            }
            error = ledger_.AccountInfoPut(transaction_, block.Account(),
                                           account_info_);
            IF_ERROR_RETURN(
                error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT);
        }
        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode DeleteBlockSuccessor(const rai::Block& block)
    {
        bool error = ledger_.BlockDel(transaction_, block.Hash());
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_DEL);
        if (block.Height() != 0)
        {
            error = ledger_.BlockSuccessorSet(transaction_, block.Previous(),
                                              rai::BlockHash(0));
            IF_ERROR_RETURN(error,
                            rai::ErrorCode::BLOCK_PROCESS_LEDGER_SUCCESSOR_SET);
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode PutRollbackBlock(const rai::Block& block)
    {
        bool error =
            ledger_.RollbackBlockPut(transaction_, block.Hash(), block);
        IF_ERROR_RETURN(
            error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_ROLLBACK_BLOCK_PUT);
        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Send(const rai::Block& block) override
    {
        rai::ErrorCode error_code = Check(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::ReceivableInfo receivable_info;
        bool error = ledger_.ReceivableInfoGet(transaction_, block.Link(),
                                               block.Hash(), receivable_info);
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_RECEIVED);

        error =
            ledger_.ReceivableInfoDel(transaction_, block.Link(), block.Hash());
        IF_ERROR_RETURN(
            error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_DEL);

        error_code = DeleteBlockSuccessor(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = PutRollbackBlock(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = UpdateAccountInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = DeleteRewardableInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateRepWeight(block);

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Receive(const rai::Block& block) override
    {
        rai::ErrorCode error_code = Check(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        std::shared_ptr<rai::Block> source(nullptr);
        bool error = ledger_.BlockGet(transaction_, block.Link(), source);
        IF_ERROR_RETURN(error,
                        rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_SOURCE_PRUNED);
        
        rai::Amount amount(0);
        if (block.Height() != 0)
        {
            amount = block.Balance() - previous_->Balance();
        }
        else
        {
            rai::Amount price_amount = rai::CreditPrice(block.Timestamp());
            rai::uint128_t balance(block.Balance().Number());
            rai::uint128_t price(price_amount.Number());
            amount = price * block.Credit() + balance;
        }
        rai::ReceivableInfo receivable_info(source->Account(), amount,
                                            source->Timestamp());
        error = ledger_.ReceivableInfoPut(transaction_, block.Account(),
                                          block.Link(), receivable_info);
        IF_ERROR_RETURN(
            error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_PUT);

        error_code = DeleteBlockSuccessor(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = PutRollbackBlock(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = UpdateAccountInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = DeleteRewardableInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateRepWeight(block);

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Change(const rai::Block& block) override
    {
        rai::ErrorCode error_code = Check(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = DeleteBlockSuccessor(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = PutRollbackBlock(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = UpdateAccountInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = DeleteRewardableInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateRepWeight(block);

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Credit(const rai::Block& block) override
    {
        rai::ErrorCode error_code = Check(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = DeleteBlockSuccessor(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = PutRollbackBlock(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = UpdateAccountInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = DeleteRewardableInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateRepWeight(block);

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Reward(const rai::Block& block) override
    {
        rai::ErrorCode error_code = Check(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        std::shared_ptr<rai::Block> source(nullptr);
        rai::BlockHash successor_hash;
        bool error = ledger_.BlockGet(transaction_, block.Link(), source,
                                      successor_hash);
        IF_ERROR_RETURN(error,
                        rai::ErrorCode::BLOCK_PROCESS_ROLLBACK_SOURCE_PRUNED);
        std::shared_ptr<rai::Block> successor(nullptr);
        error = ledger_.BlockGet(transaction_, successor_hash, successor);
        if (error)
        {
            rai::Stats::AddDetail(
                rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT,
                "RollbackBlockVisitor::Reward: failed to get successor block, "
                "hash=",
                successor_hash.StringHex());
            return rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT;
        }

        rai::Amount amount(0);
        if (block.Height() != 0)
        {
            amount = block.Balance() - previous_->Balance();
        }
        else
        {
            rai::Amount price_amount = rai::CreditPrice(block.Timestamp());
            rai::uint128_t balance(block.Balance().Number());
            rai::uint128_t price(price_amount.Number());
            amount = price * block.Credit() + balance;
        }
        uint64_t timestamp =
            rai::RewardTimestamp(source->Timestamp(), successor->Timestamp());
        rai::RewardableInfo rewardable_info(source->Account(), amount,
                                            timestamp);
        error = ledger_.RewardableInfoPut(transaction_, block.Account(),
                                          block.Link(), rewardable_info);
        IF_ERROR_RETURN(
            error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_PUT);

        error_code = DeleteBlockSuccessor(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = PutRollbackBlock(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = UpdateAccountInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = DeleteRewardableInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateRepWeight(block);

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Destroy(const rai::Block& block) override
    {
        rai::ErrorCode error_code = Check(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = DeleteBlockSuccessor(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = PutRollbackBlock(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = UpdateAccountInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = DeleteRewardableInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateRepWeight(block);

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Bind(const rai::Block& block) override
    {
        rai::ErrorCode error_code = Check(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::SUCCESS;
        std::string error_info;
        do
        {
            rai::BindingEntry entry;
            bool error = ledger_.BindingEntryGet(transaction_, block.Account(),
                                                 block.Height(), entry);
            if (error)
            {
                error_info = rai::ToString(
                    "RollbackBlockVisitor::Bind: failed to get binding entry, "
                    "hash=",
                    block.Hash().StringHex());
                error_code = rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT;
                break;
            }

            uint64_t count = 0;
            error =
                ledger_.BindingCountGet(transaction_, block.Account(), count);
            if (error)
            {
                error_info = rai::ToString(
                    "RollbackBlockVisitor::Bind: failed to get binding count, "
                    "hash=",
                    block.Hash().StringHex());
                error_code = rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT;
                break;
            }
            else if (count == 0)
            {
                error_info = rai::ToString(
                    "RollbackBlockVisitor::Bind: binding count is 0, "
                    "hash=",
                    block.Hash().StringHex());
                error_code = rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT;
                break;
            }

            error = ledger_.BindingEntryDel(transaction_, block.Account(),
                                            block.Height());
            IF_ERROR_RETURN(
                error, rai::ErrorCode::BLOCK_PROCESS_LEDGER_BINDING_ENTRY_DEL);

            --count;
            if (count == 0)
            {
                error = ledger_.BindingCountDel(transaction_, block.Account());
                IF_ERROR_RETURN(
                    error,
                    rai::ErrorCode::BLOCK_PROCESS_LEDGER_BINDING_COUNT_DEL);
            }
            else
            {
                error = ledger_.BindingCountPut(transaction_, block.Account(),
                                                count);
                IF_ERROR_RETURN(
                    error,
                    rai::ErrorCode::BLOCK_PROCESS_LEDGER_BINDING_COUNT_PUT);
            }
        } while (0);

        if (error_code != rai::ErrorCode::SUCCESS)
        {
            if (!error_info.empty())
            {
                rai::Log::Error(error_info);
                rai::Stats::AddDetail(error_code, error_info);
            }
            return error_code;
        }

        error_code = DeleteBlockSuccessor(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = PutRollbackBlock(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = UpdateAccountInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = DeleteRewardableInfo(block);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateRepWeight(block);

        return rai::ErrorCode::SUCCESS;
    }

    rai::Transaction& transaction_;
    rai::Ledger& ledger_;
    std::shared_ptr<rai::Block> previous_;
    bool delete_rewardable_;
    rai::AccountInfo account_info_;
};
}  // namespace

rai::ErrorCode rai::BlockProcessor::RollbackBlock_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block)
{
    RollbackBlockVisitor visitor(transaction, ledger_);
    return block->Visit(visitor);
}

rai::ErrorCode rai::BlockProcessor::ConfirmBlock_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    uint64_t& last_confirm_height)
{
    if (!ledger_.BlockExists(transaction, block->Hash()))
    {
        return rai::ErrorCode::BLOCK_PROCESS_CONFIRM_BLOCK_MISS;
    }

    rai::AccountInfo account_info;
    bool error =
        ledger_.AccountInfoGet(transaction, block->Account(), account_info);
    if (error)
    {
        rai::Stats::AddDetail(
            rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT,
            "BlockProcessor::ConfirmBlock_: failed to get account info, "
            "account=",
            block->Account().StringAccount());
        return rai::ErrorCode::BLOCK_PROCESS_LEDGER_INCONSISTENT;
    }

    last_confirm_height = account_info.confirmed_height_;
    if (account_info.confirmed_height_ == rai::Block::INVALID_HEIGHT
        || block->Height() > account_info.confirmed_height_)
    {
        account_info.confirmed_height_ = block->Height();
        error =
            ledger_.AccountInfoPut(transaction, block->Account(), account_info);
        IF_ERROR_RETURN(error,
                        rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT);
    }

    return rai::ErrorCode::SUCCESS;
}

rai::QueryCallback rai::BlockProcessor::QueryCallback_(uint64_t operation)
{
    std::weak_ptr<rai::Node> node_w(node_.Shared());
    rai::QueryCallback callback =
        [node_w, operation](const std::vector<rai::QueryAck>& acks,
                            std::vector<rai::QueryCallbackStatus>& result) {
            bool success = false;
            for (auto& ack : acks)
            {
                if (ack.status_ == rai::QueryStatus::SUCCESS)
                {
                    auto node(node_w.lock());
                    if (node)
                    {
                        rai::BlockForced forced(operation, ack.block_);
                        node->block_processor_.AddForced(forced);
                    }
                    success = true;
                    break;
                }
            }

            rai::QueryCallbackStatus status =
                success ? rai::QueryCallbackStatus::FINISH
                        : rai::QueryCallbackStatus::CONTINUE;
            result.insert(result.end(), acks.size(), status);
        };
    return callback;
}

bool rai::BlockProcessor::QueryPrevious_(
    uint64_t operation, const std::shared_ptr<rai::Block>& block)
{
    if (block->Height() == 0)
    {
        return true;
    }
    node_.block_queries_.QueryByHash(block->Account(), block->Height() - 1,
                                     block->Previous(), false,
                                     QueryCallback_(operation));
    return false;
}

bool rai::BlockProcessor::QueryPrevious_(rai::Transaction& transaction,
                                         uint64_t operation,
                                         const rai::Account& account)
{
    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN(error, true);

    if (info.tail_height_ == rai::Block::INVALID_HEIGHT
        || info.tail_height_ == 0)
    {
        return true;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    error = ledger_.BlockGet(transaction, info.tail_, block);
    IF_ERROR_RETURN(error, true);

    node_.block_queries_.QueryByHash(block->Account(), block->Height() - 1,
                                     block->Previous(), true,
                                     QueryCallback_(operation));
    return false;
}

void rai::BlockProcessor::QuerySource_(uint64_t operation,
                                       const rai::BlockHash& hash)
{
    node_.block_queries_.QueryByHash(rai::Account(0),
                                     rai::Block::INVALID_HEIGHT, hash, false,
                                     QueryCallback_(operation));
}

void rai::BlockProcessor::UpdateForks_(
    const std::unordered_set<rai::Account>& accounts)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, true);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Stats::Add(error_code, "BlockProcessor::UpdateForks_");
        return;
    }

    for (const auto& account : accounts)
    {
        rai::AccountInfo info;
        bool error = ledger_.AccountInfoGet(transaction, account, info);
        if (error || !info.Valid())
        {
            bool error = ledger_.ForkDel(transaction, account);
            if (error)
            {
                // log
                rai::Stats::Add(rai::ErrorCode::BLOCK_PROCESS_LEDGER_FORK_DEL);
            }
            continue;
        }

        uint16_t count = 0;
        std::vector<uint64_t> heights;
        rai::Iterator i = ledger_.ForkLowerBound(transaction, account);
        rai::Iterator n = ledger_.ForkUpperBound(transaction, account);
        for (; i != n; ++i)
        {
            std::shared_ptr<rai::Block> first(nullptr);
            std::shared_ptr<rai::Block> second(nullptr);
            error = ledger_.ForkGet(i, first, second);
            if (error || first->Account() != account)
            {
                // log
                rai::Stats::Add(rai::ErrorCode::BLOCK_PROCESS_LEDGER_FORK_GET,
                                "BlockProcessor::UpdateForks_");
                continue;
            }
            if (first->Height() > info.head_height_)
            {
                heights.push_back(first->Height());
            }
            else
            {
                ++count;
            }
        }

        for (uint64_t height : heights)
        {
            bool error = ledger_.ForkDel(transaction, account, height);
            if (error)
            {
                // log
                rai::Stats::Add(rai::ErrorCode::BLOCK_PROCESS_LEDGER_FORK_DEL);
            }
        }

        if (count != info.forks_)
        {
            info.forks_ = count;
            bool error = ledger_.AccountInfoPut(transaction, account, info);
            if (error)
            {
                // log
                rai::Stats::Add(
                    rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT,
                    "BlockProcessor::UpdateForks_");
            }
        }
    }
}

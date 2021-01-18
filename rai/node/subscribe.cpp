#include <rai/node/subscribe.hpp>
#include <rai/node/node.hpp>

std::chrono::seconds constexpr rai::Subscriptions::CUTOFF_TIME;

rai::Subscriptions::Subscriptions(rai::Node& node) : node_(node)
{
    node_.observers_.block_.Add(
        [this](const rai::BlockProcessResult& result,
               const std::shared_ptr<rai::Block>& block) {
            if (result.error_code_ != rai::ErrorCode::SUCCESS)
            {
                return;
            }

            if (result.operation_ == rai::BlockOperation::APPEND)
            {
                BlockAppend(block);
            }
            else if (result.operation_ == rai::BlockOperation::CONFIRM)
            {
                BlockConfirm(block, result.last_confirm_height_);
            } 
            else if (result.operation_ == rai::BlockOperation::ROLLBACK)
            {
                BlockRollback(block);
            }
            else if (result.operation_ == rai::BlockOperation::DROP)
            {
                BlockDrop(block);
            }
            else
            {
                // do nothing
            }
        });

    node_.observers_.fork_.Add(
        [this](bool add, const std::shared_ptr<rai::Block>& first,
               const std::shared_ptr<rai::Block>& second) {
            BlockFork(add, first, second);
        });
}

void rai::Subscriptions::Add(const rai::Account& account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rai::Subscription sub{account, std::chrono::steady_clock::now()};
    auto it = subscriptions_.find(sub.account_);
    if (it != subscriptions_.end())
    {
        subscriptions_.modify(it, [&](rai::Subscription& data){
            data.time_ = sub.time_;
        });
    }
    else
    {
        subscriptions_.insert(sub);
    }
}

void rai::Subscriptions::BlockAppend(const std::shared_ptr<rai::Block>& block)
{
    if (Exists(block->Account()))
    {
        rai::Ptree ptree;
        ptree.put("notify", "block_append");
        ptree.put("account", block->Account().StringAccount());
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        ptree.put_child("block", block_ptree);
        node_.PostJson(node_.config_.callback_url_, ptree);

        node_.StartElection(block);
    }

    if (block->Opcode() == rai::BlockOpcode::SEND && Exists(block->Link()))
    {
        node_.StartElection(block);
    }
}

void rai::Subscriptions::BlockConfirm(const std::shared_ptr<rai::Block>& block,
                                      uint64_t last_confirm_height)
{
    if (last_confirm_height != rai::Block::INVALID_HEIGHT
        && block->Height() <= last_confirm_height)
    {
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, node_.ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        rai::Stats::Add(error_code, "Subscriptions::BlockConfirm");
        return;
    }

    do
    {
        if ((last_confirm_height == rai::Block::INVALID_HEIGHT
             && block->Height() == 0)
            || last_confirm_height + 1 == block->Height())
        {
            BlockConfirm_(transaction, block);
            break;
        }

        rai::AccountInfo info;
        bool error =
            node_.ledger_.AccountInfoGet(transaction, block->Account(), info);
        if (error || !info.Valid())
        {
            rai::Stats::Add(rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET,
                            "Subscriptions::BlockConfirm account=",
                            block->Account().StringAccount());
            break;
        }

        if (info.confirmed_height_ == rai::Block::INVALID_HEIGHT
            || info.confirmed_height_ < block->Height())
        {
            break;
        }

        uint64_t height = last_confirm_height == rai::Block::INVALID_HEIGHT
                              ? 0
                              : last_confirm_height + 1;
        rai::BlockHash successor(0);
        while (height <= block->Height())
        {
            std::shared_ptr<rai::Block> block_l(nullptr);
            if (successor.IsZero())
            {
                error = node_.ledger_.BlockGet(transaction, block->Account(),
                                               height, block_l, successor);
            }
            else
            {
                error = node_.ledger_.BlockGet(transaction, successor, block_l,
                                               successor);
            }
            if (error)
            {
                rai::Stats::Add(rai::ErrorCode::LEDGER_BLOCK_GET,
                                "Subscriptions::BlockConfirm account=",
                                block->Account().StringAccount(),
                                ", height=", height,
                                ", successor=", successor.StringHex());
                break;
            }

            BlockConfirm_(transaction, block_l);

            if (successor.IsZero())
            {
                break;
            }
            height = block_l->Height() + 1;
        }
    } while (0);

    if (NeedConfirm_(transaction, block->Account()))
    {
        StartElection_(transaction, block->Account());
    }
}

void rai::Subscriptions::BlockRollback(const std::shared_ptr<rai::Block>& block)
{
    if (Exists(block->Account()))
    {
        rai::Ptree ptree;
        ptree.put("notify", "block_rollback");
        ptree.put("account", block->Account().StringAccount());
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        ptree.put_child("block", block_ptree);
        node_.PostJson(node_.config_.callback_url_, ptree);
    }
}

void rai::Subscriptions::BlockDrop(const std::shared_ptr<rai::Block>& block)
{
    if (Exists(block->Account()))
    {
        rai::Ptree ptree;
        ptree.put("notify", "block_drop");
        ptree.put("account", block->Account().StringAccount());
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        ptree.put_child("block", block_ptree);
        node_.PostJson(node_.config_.callback_url_, ptree);
    }
}

void rai::Subscriptions::BlockFork(bool add,
                                   const std::shared_ptr<rai::Block>& first,
                                   const std::shared_ptr<rai::Block>& second)
{
    if (!Exists(first->Account()))
    {
        return;
    }

    rai::Ptree ptree;
    ptree.put("notify", add ? "fork_add" : "fork_delete");
    ptree.put("account", first->Account().StringAccount());
    rai::Ptree ptree_first;
    first->SerializeJson(ptree_first);
    ptree.put_child("block_first", ptree_first);
    rai::Ptree ptree_second;
    second->SerializeJson(ptree_second);
    ptree.put_child("block_second", ptree_second);
    node_.PostJson(node_.config_.callback_url_, ptree);
}

void rai::Subscriptions::ConfirmReceivables(const rai::Account& account)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, node_.ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        rai::Stats::Add(error_code, "Subscriptions::ConfirmReceivables");
        return;
    }

    rai::ReceivableInfos receivables;
    bool error = node_.ledger_.ReceivableInfosGet(
        transaction, account, rai::ReceivableInfosType::NOT_CONFIRMED,
        receivables);
    if (error)
    {
        rai::Stats::Add(rai::ErrorCode::LEDGER_RECEIVABLES_GET,
                        "Subscriptions::ConfirmReceivables");
        return;
    }

    std::unordered_set<rai::Account> accounts;
    for (const auto& i : receivables)
    {
        accounts.insert(i.first.source_);
    }
    for (const auto& i : accounts)
    {
        StartElection_(transaction, i);
    }
}

void rai::Subscriptions::Cutoff()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
    std::chrono::steady_clock::time_point cutoff(
        now - rai::Subscriptions::CUTOFF_TIME);

    auto begin = subscriptions_.get<rai::SubscriptionByTime>().begin();
    auto end = subscriptions_.get<rai::SubscriptionByTime>().lower_bound(cutoff);
    subscriptions_.get<rai::SubscriptionByTime>().erase(begin, end);
}

void rai::Subscriptions::Erase(const rai::Account& account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscriptions_.find(account);
    if (it != subscriptions_.end())
    {
        subscriptions_.erase(it);
    }
}


bool rai::Subscriptions::Exists(const rai::Account& account) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return subscriptions_.find(account) != subscriptions_.end();
}

size_t rai::Subscriptions::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return subscriptions_.size();
}

void rai::Subscriptions::StartElection(const rai::Account& account)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, node_.ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        rai::Stats::Add(error_code, "Subscriptions::StartElection");
        return;
    }

    StartElection_(transaction, account);
}


rai::ErrorCode rai::Subscriptions::Subscribe(const rai::Account& account,
                                             uint64_t timestamp)
{
    uint64_t now = rai::CurrentTimestamp();
    if (timestamp < now - rai::Subscriptions::TIME_DIFF
        || timestamp > now + rai::Subscriptions::TIME_DIFF)
    {
        rai::Stats::AddDetail(rai::ErrorCode::SUBSCRIBE_TIMESTAMP,
                              "account=", account.StringAccount(),
                              ", timestamp=", timestamp);
        return rai::ErrorCode::SUBSCRIBE_TIMESTAMP;
    }

    if (!node_.config_.callback_url_)
    {
        return rai::ErrorCode::SUBSCRIBE_NO_CALLBACK;
    }

    if (node_.Status() != rai::NodeStatus::RUN)
    {
        return rai::ErrorCode::NODE_STATUS;
    }

    Add(account);
    StartElection(account);
    ConfirmReceivables(account);
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Subscriptions::Subscribe(const rai::Account& account,
                                             uint64_t timestamp,
                                             const rai::Signature& signature)
{
    int ret;
    rai::uint256_union hash;
    blake2b_state state;

    ret = blake2b_init(&state, hash.bytes.size());
    assert(0 == ret);

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, account.bytes);
        rai::Write(stream, timestamp);
    }
    blake2b_update(&state, bytes.data(), bytes.size());

    ret = blake2b_final(&state, hash.bytes.data(), hash.bytes.size());
    assert(0 == ret);

    if (rai::ValidateMessage(account, hash, signature))
    {
        return rai::ErrorCode::SUBSCRIBE_SIGNATURE;
    }

    return Subscribe(account, timestamp);
}

void rai::Subscriptions::Unsubscribe(const rai::Account& account)
{
    Erase(account);
}

void rai::Subscriptions::StartElection_(rai::Transaction& transaction,
                                        const rai::Account& account)
{
    rai::AccountInfo info;
    bool error = node_.ledger_.AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN_VOID(error);

    if (info.confirmed_height_ == info.head_height_)
    {
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    error = node_.ledger_.BlockGet(transaction, info.head_, block);
    if (error)
    {
        rai::Stats::Add(rai::ErrorCode::LEDGER_BLOCK_GET,
                        "Subscriptions::StartElection_");
        return;
    }

    node_.StartElection(block);
}

void rai::Subscriptions::BlockConfirm_(rai::Transaction& transaction,
                                       const std::shared_ptr<rai::Block>& block)
{
    if (Exists(block->Account()))
    {
        rai::Ptree ptree;
        ptree.put("notify", "block_confirm");
        ptree.put("account", block->Account().StringAccount());
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        ptree.put_child("block", block_ptree);
        node_.PostJson(node_.config_.callback_url_, ptree);
    }

    if (block->Opcode() == rai::BlockOpcode::SEND && Exists(block->Link()))
    {
        rai::ReceivableInfo info;
        bool error = node_.ledger_.ReceivableInfoGet(transaction, block->Link(),
                                                     block->Hash(), info);
        if (!error)
        {
            rai::Ptree ptree;
            ptree.put("notify", "receivable_info");
            ptree.put("account", block->Link().StringAccount());
            ptree.put("source", info.source_.StringAccount());
            ptree.put("amount", info.amount_.StringDec());
            ptree.put("hash", block->Hash().StringHex());
            ptree.put("timestamp", std::to_string(info.timestamp_));
            rai::Ptree source_block;
            block->SerializeJson(source_block);
            ptree.put_child("source_block", source_block);
            node_.PostJson(node_.config_.callback_url_, ptree);
        }
    }
}

bool rai::Subscriptions::NeedConfirm_(rai::Transaction& transaction,
                                      const rai::Account& account)
{
    if (Exists(account))
    {
        return true;
    }

    rai::AccountInfo info;
    bool error =
        node_.ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        return false;
    }

    if (info.confirmed_height_ == info.head_height_)
    {
        return false;
    }

    rai::BlockHash hash(info.head_);
    while (!hash.IsZero())
    {
        std::shared_ptr<rai::Block> block(nullptr);
        error = node_.ledger_.BlockGet(transaction, hash, block);
        if (error)
        {
            rai::Stats::Add(
                rai::ErrorCode::LEDGER_BLOCK_GET,
                "Subscriptions::NeedConfirm_: hash=", hash.StringHex());
            return true;
        }

        if (block->Opcode() == rai::BlockOpcode::SEND && Exists(block->Link()))
        {
            return true;
        }

        if ((info.confirmed_height_ != rai::Block::INVALID_HEIGHT
             && block->Height() == info.confirmed_height_ + 1)
            || block->Height() == 0)
        {
            break;
        }
        hash = block->Previous();
    }

    return false;
}

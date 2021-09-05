#include <rai/app/blockconfirm.hpp>

#include <rai/app/app.hpp>

rai::BlockConfirmEntry::BlockConfirmEntry(
    const std::shared_ptr<rai::Block>& block)
    : key_{block->Account(), block->Height()}, hash_(block->Hash())
{
}

rai::BlockConfirm::BlockConfirm(rai::App& app)
    : app_(app), stopped_(false), thread_([this]() { Run(); })
{
}

rai::BlockConfirm::~BlockConfirm()
{
    Stop();
}

void rai::BlockConfirm::Add(const std::shared_ptr<rai::Block>& block)
{
    rai::BlockConfirmEntry entry(block);
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = entries_.find(entry.key_);
        if (it == entries_.end())
        {
            entries_.insert(entry);
        }
        else
        {
            entries_.modify(it, [&](rai::BlockConfirmEntry& data){
                data = entry;
            });
        }
    }
    condition_.notify_all();
}

void rai::BlockConfirm::Remove(const std::shared_ptr<rai::Block>& block)
{
    rai::BlockConfirmEntry entry(block);
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = entries_.find(entry.key_);
        if (it == entries_.end())
        {
           return;
        }
        entries_.erase(it);
    }
    condition_.notify_all();
}

void rai::BlockConfirm::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        if (entries_.empty())
        {
            condition_.wait(lock);
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        auto it = entries_.get<BlockConfirmByRetry>().begin();
        if (it->retry_ > now)
        {
            condition_.wait_until(lock, it->retry_);
            continue;
        }

        rai::Account account = it->key_.account_;
        uint64_t height = it->key_.height_;
        rai::BlockHash hash = it->hash_;
        entries_.get<BlockConfirmByRetry>().modify(
            it, [&](BlockConfirmEntry& data) {
                data.retry_ = now + std::chrono::minutes(5);
            });

        RequestConfirm_(lock, account, height, hash);
    }
}

void rai::BlockConfirm::Stop()
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

void rai::BlockConfirm::RequestConfirm_(std::unique_lock<std::mutex>& lock,
                                        const rai::Account& account,
                                        uint64_t height,
                                        const rai::BlockHash& hash)
{
    rai::Ptree ptree;
    
    ptree.put("action", "block_confirm");
    ptree.put("account", account.StringAccount());
    ptree.put("height", std::to_string(height));
    ptree.put("hash", hash.StringHex());
    ptree.put("notify", "true");

    lock.unlock();
    app_.SendToGateway(ptree);
    lock.lock();
}

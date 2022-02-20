#include <rai/app/blockwaiting.hpp>

#include <rai/app/app.hpp>

rai::BlockWaitingEntry::BlockWaitingEntry(
    const rai::Account& account, uint64_t height,
    const std::shared_ptr<rai::Block>& block, bool confirmed)
    : for_{account, height},
      account_(block->Account()),
      hash_(block->Hash()),
      arrival_(std::chrono::steady_clock::now()),
      block_(block),
      confirmed_(confirmed)
{
}

void rai::BlockWaiting::Insert(const rai::Account& account, uint64_t height,
                               const std::shared_ptr<rai::Block>& block,
                               bool confirmed)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rai::BlockWaitingEntry entry(account, height, block, confirmed);

    auto& index = entries_.get<rai::BlockWaitingByHash>();
    auto it = index.find(entry.hash_);
    if (it != index.end())
    {
        index.modify(it, [&](rai::BlockWaitingEntry& data) {
            data.arrival_ = entry.arrival_;
            data.confirmed_ = entry.confirmed_;
        });
    }
    else
    {
        entries_.insert(entry);
    }
}

bool rai::BlockWaiting::Exists(const rai::Account& account,
                               uint64_t height) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    rai::AccountHeight key{account, height};
    return entries_.lower_bound(key) != entries_.upper_bound(key);
}

bool rai::BlockWaiting::IsAccountWaiting(const rai::Account& account) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto& index = entries_.get<rai::BlockWaitingByAccount>();
    return index.lower_bound(account) != index.upper_bound(account);
}

void rai::BlockWaiting::Age(uint64_t seconds)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto cutoff =
        std::chrono::steady_clock::now() - std::chrono::seconds(seconds);
    auto& index = entries_.get<rai::BlockWaitingByArrival>();
    index.erase(index.begin(), index.lower_bound(cutoff));
}

std::vector<rai::BlockWaitingEntry> rai::BlockWaiting::Remove(
    const rai::Account& account, uint64_t height)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<rai::BlockWaitingEntry> result;
    rai::AccountHeight begin{account, 0};
    rai::AccountHeight end{account, height};

    while (true)
    {
        auto it = entries_.lower_bound(begin);
        if (it == entries_.upper_bound(end))
        {
            return result;
        }
        
        result.push_back(*it);
        entries_.erase(it);
    }

    return result;
}

std::vector<rai::BlockWaitingEntry> rai::BlockWaiting::List() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<rai::BlockWaitingEntry> result;
    for (auto i = entries_.begin(), n = entries_.end(); i != n; ++i)
    {
        result.push_back(*i);
    }
    return result;
}

std::vector<rai::AccountHeight> rai::BlockWaiting::WaitFor(
    const rai::Account& account) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<rai::AccountHeight> result;
    const auto& index = entries_.get<rai::BlockWaitingByAccount>();
    auto i = index.lower_bound(account);
    auto n = index.upper_bound(account);
    for (; i != n; ++i)
    {
        result.push_back(i->for_);
    }
    return result;
}

size_t rai::BlockWaiting::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

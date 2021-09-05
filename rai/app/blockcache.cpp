
#include <rai/app/blockcache.hpp>

rai::BlockCacheEntry::BlockCacheEntry(const std::shared_ptr<rai::Block>& block)
    : previous_(block->Previous()),
      arrival_(std::chrono::steady_clock::now()),
      block_(block)
{
}

void rai::BlockCache::Insert(const std::shared_ptr<rai::Block>& block)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rai::BlockHash previous = block->Previous();
    if (previous.IsZero())
    {
      return;
    }

    auto it = entries_.find(previous);
    if (it != entries_.end())
    {
        entries_.modify(it, [](rai::BlockCacheEntry& entry) {
            entry.arrival_ = std::chrono::steady_clock::now();
        });
    }
    else
    {
      entries_.insert(rai::BlockCacheEntry(block));
    }
}

std::shared_ptr<rai::Block> rai::BlockCache::Query(
    const rai::BlockHash& previous) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::shared_ptr<rai::Block> block(nullptr);

    auto it = entries_.find(previous);
    if (it != entries_.end())
    {
      block = it->block_;
    }

    return block;
}

void rai::BlockCache::Age(uint64_t seconds)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto cutoff =
        std::chrono::steady_clock::now() - std::chrono::seconds(seconds);
    auto begin = entries_.get<rai::BlockCacheByArrival>().begin();
    auto end = entries_.get<rai::BlockCacheByArrival>().lower_bound(cutoff);
    entries_.get<rai::BlockCacheByArrival>().erase(begin, end);
}

size_t rai::BlockCache::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

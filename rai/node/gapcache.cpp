#include <rai/node/gapcache.hpp>

rai::GapInfo::GapInfo(const rai::BlockHash& gap,
                      const std::shared_ptr<rai::Block>& block)
    : hash_(gap),
      account_(block->Account()),
      arrival_(std::chrono::steady_clock::now()),
      block_(block)
{
}

bool rai::GapCache::Insert(const rai::GapInfo& info)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (caches_.size() >= rai::GapCache::MAX_CACHES)
    {
        return true;
    }

    auto count = caches_.get<GapInfoByAccount>().count(info.account_);
    if (count >= rai::GapCache::MAX_CACHES_PER_ACCOUNT)
    {
        return true;
    }

    auto ret = caches_.insert(info);
    return !ret.second;
}

void rai::GapCache::Remove(const rai::BlockHash& hash)
{
    std::lock_guard<std::mutex> lock(mutex_);
    caches_.erase(hash);
}

boost::optional<rai::GapInfo> rai::GapCache::Query(
    const rai::BlockHash& hash) const
{
    boost::optional<rai::GapInfo> result(boost::none);
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = caches_.find(hash);
    if (it != caches_.end())
    {
        result = *it;
    }
    return result;
}

std::vector<rai::GapInfo> rai::GapCache::Age(uint64_t seconds)
{
    std::vector<rai::GapInfo> result;
    auto cutoff =
        std::chrono::steady_clock::now() - std::chrono::seconds(seconds);

    std::lock_guard<std::mutex> lock(mutex_);
    auto begin = caches_.get<rai::GapInfoByArrival>().begin();
    auto end = caches_.get<rai::GapInfoByArrival>().lower_bound(cutoff);
    for (auto i = begin; i != end; ++i)
    {
        result.push_back(*i);
    }
    caches_.get<rai::GapInfoByArrival>().erase(begin, end);
    return result;
}




#include <rai/rai_token/chainwaiting.hpp>

rai::ChainWaitingEntry::ChainWaitingEntry(
    rai::Chain chain, uint64_t height, const std::shared_ptr<rai::Block>& block)
    : chain_(chain),
      height_(height),
      hash_(block->Hash()),
      arrival_(std::chrono::steady_clock::now()),
      block_(block)
{
}

void rai::ChainWaiting::Insert(rai::Chain chain, uint64_t height,
                               const std::shared_ptr<rai::Block>& block)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rai::ChainWaitingEntry entry(chain, height, block);

    auto& index = entries_.get<rai::ChainWaitingByHash>();
    auto it = index.find(entry.hash_);
    if (it != index.end())
    {
        index.modify(it, [&](rai::ChainWaitingEntry& data) {
            data.arrival_ = entry.arrival_;
        });
    }
    else
    {
        entries_.insert(entry);
    }
}

void rai::ChainWaiting::Age(uint64_t seconds)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto cutoff =
        std::chrono::steady_clock::now() - std::chrono::seconds(seconds);
    auto& index = entries_.get<rai::ChainWaitingByArrival>();
    index.erase(index.begin(), index.lower_bound(cutoff));
}

std::vector<rai::ChainWaitingEntry> rai::ChainWaiting::Remove(rai::Chain chain,
                                                              uint64_t height)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<rai::ChainWaitingEntry> result;

    auto begin = boost::make_tuple(chain, 0);
    auto end = boost::make_tuple(chain, height);
    while (true)
    {
        auto it = entries_.lower_bound(begin);
        if (it == entries_.upper_bound(end))
        {
            break;
        }

        result.push_back(*it);
        entries_.erase(it);
    }
    return result;
}

size_t rai::ChainWaiting::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}
#pragma once

#include <chrono>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <rai/common/blocks.hpp>


namespace rai
{
class BlockCacheEntry
{
public:
    BlockCacheEntry(const std::shared_ptr<rai::Block>&);
    rai::BlockHash previous_;
    std::chrono::steady_clock::time_point arrival_;
    std::shared_ptr<rai::Block> block_;
};

class BlockCacheByArrival
{
};

class BlockCache
{
public:
    void Insert(const std::shared_ptr<rai::Block>&);
    void remove(const rai::BlockHash&);
    std::shared_ptr<rai::Block> Query(const rai::BlockHash&) const;
    void Age(uint64_t);
    size_t Size() const;

private:
    mutable std::mutex mutex_;
    boost::multi_index_container<
        rai::BlockCacheEntry,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::member<rai::BlockCacheEntry, rai::BlockHash,
                                           &rai::BlockCacheEntry::previous_>>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<rai::BlockCacheByArrival>,
                boost::multi_index::member<
                    rai::BlockCacheEntry, std::chrono::steady_clock::time_point,
                    &rai::BlockCacheEntry::arrival_>>>>
        entries_;
};

} // end of namespace
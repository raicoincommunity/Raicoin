#pragma once

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <chrono>
#include <mutex>
#include <rai/common/blocks.hpp>
#include <rai/common/chain.hpp>
#include <rai/common/numbers.hpp>

namespace rai
{
class ChainWaitingEntry
{
public:
    ChainWaitingEntry(rai::Chain, uint64_t, const std::shared_ptr<rai::Block>&);

    rai::Chain chain_;
    uint64_t height_;
    rai::BlockHash hash_;
    std::chrono::steady_clock::time_point arrival_;
    std::shared_ptr<rai::Block> block_;
};

class ChainWaitingByHash
{
};

class ChainWaitingByArrival
{
};

class ChainWaiting
{
public:
    void Insert(rai::Chain, uint64_t, const std::shared_ptr<rai::Block>&);
    void Age(uint64_t);
    std::vector<rai::ChainWaitingEntry> Remove(rai::Chain, uint64_t);
    size_t Size() const;

private:
    mutable std::mutex mutex_;
    boost::multi_index_container<
        rai::ChainWaitingEntry,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_non_unique<
                boost::multi_index::composite_key<
                    rai::ChainWaitingEntry,
                    boost::multi_index::member<rai::ChainWaitingEntry,
                                               rai::Chain,
                                               &rai::ChainWaitingEntry::chain_>,
                    boost::multi_index::member<
                        rai::ChainWaitingEntry, uint64_t,
                        &rai::ChainWaitingEntry::height_>>>,
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<ChainWaitingByHash>,
                boost::multi_index::member<rai::ChainWaitingEntry,
                                           rai::BlockHash,
                                           &rai::ChainWaitingEntry::hash_>>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<ChainWaitingByArrival>,
                boost::multi_index::member<
                    rai::ChainWaitingEntry,
                    std::chrono::steady_clock::time_point,
                    &rai::ChainWaitingEntry::arrival_>>>>
        entries_;
};

}  // namespace rai

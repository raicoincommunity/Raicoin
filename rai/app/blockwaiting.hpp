#pragma once

#include <chrono>
#include <mutex>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/blocks.hpp>

namespace rai
{
class BlockWaitingEntry
{
public:
    BlockWaitingEntry(const rai::Account&, uint64_t,
                      const std::shared_ptr<rai::Block>&, bool);

    rai::AccountHeight for_;
    rai::Account account_;
    rai::BlockHash hash_;
    std::chrono::steady_clock::time_point arrival_;
    std::shared_ptr<rai::Block> block_;
    bool confirmed_;
};

class BlockWaitingByAccount
{
};

class BlockWaitingByHash
{
};

class BlockWaitingByArrival
{
};

class BlockWaiting
{
public:
    void Insert(const rai::Account&, uint64_t,
                const std::shared_ptr<rai::Block>&, bool);
    bool Exists(const rai::Account&, uint64_t) const;
    bool IsAccountWaiting(const rai::Account&) const;
    void Age(uint64_t);
    std::vector<rai::BlockWaitingEntry> Remove(const rai::Account&, uint64_t);
    std::vector<rai::BlockWaitingEntry> List() const;
    std::vector<rai::AccountHeight> WaitFor(const rai::Account&) const;
    size_t Size() const;

private:
    mutable std::mutex mutex_;
    boost::multi_index_container<
        BlockWaitingEntry,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                BlockWaitingEntry, rai::AccountHeight,
                &BlockWaitingEntry::for_>>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<BlockWaitingByAccount>,
                boost::multi_index::member<BlockWaitingEntry, rai::Account,
                                           &BlockWaitingEntry::account_>>,
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<BlockWaitingByHash>,
                boost::multi_index::member<BlockWaitingEntry, rai::BlockHash,
                                           &BlockWaitingEntry::hash_>>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<BlockWaitingByArrival>,
                boost::multi_index::member<
                    BlockWaitingEntry, std::chrono::steady_clock::time_point,
                    &BlockWaitingEntry::arrival_>>>>
        entries_;
};

}
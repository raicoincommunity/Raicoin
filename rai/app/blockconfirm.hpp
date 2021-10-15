#pragma once

#include <chrono>
#include <thread>
#include <condition_variable>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/blocks.hpp>

namespace rai
{
class App;

class BlockConfirmEntry
{
public:
    BlockConfirmEntry(const std::shared_ptr<rai::Block>&);
    BlockConfirmEntry(const rai::Account&, uint64_t, const rai::BlockHash&);

    rai::AccountHeight key_;
    rai::BlockHash hash_;
    std::chrono::steady_clock::time_point retry_;
};

class BlockConfirm
{
public:
    BlockConfirm(rai::App&);
    ~BlockConfirm();

    void Run();
    void Stop();
    void Add(const std::shared_ptr<rai::Block>&);
    void Add(const rai::Account&, uint64_t, const rai::BlockHash&);
    void Add(const rai::BlockConfirmEntry&);
    void Remove(const std::shared_ptr<rai::Block>&);
    size_t Size() const;

    rai::App& app_;

private:
    void RequestConfirm_(std::unique_lock<std::mutex>&, const rai::Account&,
                         uint64_t, const rai::BlockHash&);

    class BlockConfirmByRetry
    {
    };

    mutable std::mutex mutex_;
    bool stopped_;
    boost::multi_index_container<
        BlockConfirmEntry,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<boost::multi_index::member<
                BlockConfirmEntry, rai::AccountHeight, &BlockConfirmEntry::key_>>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<BlockConfirmByRetry>,
                boost::multi_index::member<
                    BlockConfirmEntry, std::chrono::steady_clock::time_point,
                    &BlockConfirmEntry::retry_>>>>
        entries_;

    std::condition_variable condition_;
    std::thread thread_;
};

}// end of namespace
#pragma once
#include <thread>
#include <condition_variable>
#include <rai/common/numbers.hpp>
#include <rai/common/util.hpp>
#include <rai/common/blocks.hpp>

namespace rai
{

class Token;

class TakeNackEntry
{
public:
    TakeNackEntry(const rai::Account&, uint64_t,
                  const std::shared_ptr<rai::Block>&);
    rai::AccountHeight swap_;
    std::chrono::steady_clock::time_point timeout_;
    std::shared_ptr<rai::Block> block_;
    uint64_t retries_;
};

class TakeNackByTimeout
{
};

class SwapHelper
{
public:
    SwapHelper(rai::Token&);
    ~SwapHelper();

    void Run();
    void Stop();
    void Add(const rai::Account&, uint64_t, const std::shared_ptr<rai::Block>&);
    void Remove(const rai::Account&, uint64_t);
    void Process(const rai::TakeNackEntry&);
    size_t Size() const;

    rai::Token& token_;

private:
    mutable std::mutex mutex_;
    bool stopped_;
    boost::multi_index_container<
        TakeNackEntry,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<boost::multi_index::member<
                TakeNackEntry, rai::AccountHeight, &TakeNackEntry::swap_>>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<TakeNackByTimeout>,
                boost::multi_index::member<
                    TakeNackEntry, std::chrono::steady_clock::time_point,
                    &TakeNackEntry::timeout_>>>>
        entries_;

    std::condition_variable condition_;
    std::thread thread_;

};


}
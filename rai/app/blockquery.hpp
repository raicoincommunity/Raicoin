#pragma once
#include <deque>
#include <condition_variable>
#include <thread>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <rai/common/numbers.hpp>

namespace rai
{
class App;

class AppBlockQuery
{
public:
    AppBlockQuery(const rai::Account&, uint64_t, uint64_t);

    rai::Account account_;
    uint64_t height_;
    uint64_t count_;
};


class AppBlockQueries
{
public:
    AppBlockQueries(rai::App&);
    ~AppBlockQueries();

    void Run();
    void Stop();

    void Add(const rai::Account&, uint64_t, uint64_t);
    void Remove(const rai::Account&, uint64_t);
    size_t Size() const;
    bool Busy() const;

    static size_t constexpr CONCURRENCY = 5;

    class Entry
    {
    public:
        Entry(const rai::AppBlockQuery&);
        rai::AccountHeight key_;
        std::chrono::steady_clock::time_point wakeup_;
        uint64_t retries_;
        uint64_t count_;
    };

private:
    rai::App& app_;
    mutable std::mutex mutex_; 
    boost::multi_index_container<
        Entry,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<boost::multi_index::member<
                Entry, rai::AccountHeight, &Entry::key_>>,
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                Entry, std::chrono::steady_clock::time_point,
                &Entry::wakeup_>>>>
        pendings_;
    boost::multi_index_container<
        AppBlockQuery,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<boost::multi_index::member<
                AppBlockQuery, rai::Account, &AppBlockQuery::account_>>,
            boost::multi_index::random_access<>>>
        queries_;
    bool stopped_;
    std::condition_variable condition_;
    std::thread thread_;
};

}
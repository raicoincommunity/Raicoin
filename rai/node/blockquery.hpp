#pragma once
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <condition_variable>
#include <memory>
#include <rai/common/numbers.hpp>
#include <rai/node/message.hpp>
#include <thread>


namespace rai
{
class Node;

class QueryFrom
{
public:
    bool operator==(const rai::QueryFrom& other) const;
    bool operator!=(const rai::QueryFrom& other) const;
    rai::Endpoint endpoint_;
    boost::optional<rai::Endpoint> proxy_endpoint_;
};

class QueryAck
{
public:
    QueryAck();
    QueryAck(rai::QueryStatus, const std::shared_ptr<rai::Block>&);

    rai::QueryStatus status_;
    std::shared_ptr<rai::Block> block_;
};

enum class QueryCallbackStatus : uint32_t
{
    CONTINUE       = 0,
    FINISH         = 1,

    MAX
};

using QueryCallback = std::function<void(
    const std::vector<rai::QueryAck>&, std::vector<rai::QueryCallbackStatus>&)>;

class BlockQuery
{
public:
    BlockQuery() = default;
    BlockQuery(uint64_t, rai::QueryBy, const rai::Account&, uint64_t,
               const rai::BlockHash&, bool, bool, const rai::QueryCallback&);
    BlockQuery(uint64_t, rai::QueryBy, const rai::Account&, uint64_t,
               const rai::BlockHash&, bool, bool,
               const std::vector<rai::QueryFrom>&, const rai::QueryCallback&);

    uint64_t sequence_;
    std::chrono::steady_clock::time_point wakeup_;
    rai::QueryBy by_;
    rai::Account account_;
    uint64_t height_;
    rai::BlockHash hash_;
    bool only_full_node_;
    bool only_specified_node_;
    uint32_t count_;
    std::vector<rai::QueryFrom> from_;
    std::vector<rai::QueryAck> ack_;
    rai::QueryCallback callback_;
};

class BlockQueries
{
public:
    BlockQueries(rai::Node&);
    ~BlockQueries();
    void Insert(const rai::BlockQuery&);
    void ProcessQuery(rai::BlockQuery&);
    void ProcessQueryAck(uint64_t, rai::QueryBy, const rai::Account&, uint64_t,
                         const rai::BlockHash&, rai::QueryStatus,
                         const std::shared_ptr<rai::Block>&,
                         const rai::Endpoint&,
                         const boost::optional<rai::Endpoint>&);
    void QueryByHash(const rai::Account&, uint64_t, const rai::BlockHash&, bool,
                     const rai::QueryCallback&);
    void QueryByHash(const rai::Account&, uint64_t, const rai::BlockHash&,
                     const std::vector<rai::QueryFrom>&,
                     const rai::QueryCallback&);
    void QueryByHeight(const rai::Account&, uint64_t, bool,
                       const rai::QueryCallback&);
    void QueryByHeight(const rai::Account&, uint64_t,
                       const std::vector<rai::QueryFrom>&,
                       const rai::QueryCallback&);
    void QueryByPrevious(const rai::Account&, uint64_t, const rai::BlockHash&,
                         bool, const rai::QueryCallback&);
    void QueryByPrevious(const rai::Account&, uint64_t, const rai::BlockHash&,
                         const std::vector<rai::QueryFrom>&,
                         const rai::QueryCallback&);
    void Run();
    void Stop();
    uint64_t Sequence();
    size_t Size() const;

private:
    void SendQuery_(rai::BlockQuery&);
    void UpdateWakeup_(rai::BlockQuery&) const;

    rai::Node& node_;
    mutable std::mutex mutex_; 
    std::atomic<uint64_t> sequence_;
    boost::multi_index_container<
        rai::BlockQuery,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<boost::multi_index::member<
                rai::BlockQuery, uint64_t, &rai::BlockQuery::sequence_>>,
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                rai::BlockQuery, std::chrono::steady_clock::time_point,
                &rai::BlockQuery::wakeup_>>>>
        queries_;
    bool stopped_;
    std::condition_variable condition_;
    std::thread thread_;
};
}  // namespace rai
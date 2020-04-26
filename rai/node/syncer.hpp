#pragma once

#include <mutex>
#include <unordered_map>
#include <rai/common/numbers.hpp>
#include <rai/node/blockquery.hpp>
#include <rai/node/blockprocessor.hpp>

namespace rai
{
class Node;

enum class SyncStatus
{
    INVALID = 0,
    QUERY   = 1,
    PROCESS = 2,

    MAX
};

class SyncInfo
{
public:
    rai::SyncStatus status_;
    bool first_;
    uint32_t batch_id_;
    uint64_t height_;
    rai::BlockHash previous_;
    rai::BlockHash current_;
};

class SyncStat
{
public:
    SyncStat();

    void Reset();

    uint64_t total_;
    uint64_t miss_;
};

class Syncer 
{
public:
    Syncer(rai::Node&);
    void Add(const rai::Account&, uint64_t, bool, uint32_t);
    void Add(const rai::Account&, uint64_t, const rai::BlockHash&, bool,
             uint32_t);
    uint64_t AddQuery(uint32_t);
    uint32_t BatchId(uint64_t) const;
    bool Busy() const;
    bool Empty() const;
    void Erase(const rai::Account&);
    void EraseQuery(uint64_t);
    bool Exists(const rai::Account&) const;
    bool Finished(uint32_t) const;
    void ProcessorCallback(const rai::BlockProcessResult&,
                           const std::shared_ptr<rai::Block>&);
    void QueryCallback(const rai::Account&, rai::QueryStatus,
                       const std::shared_ptr<rai::Block>&);
    rai::SyncStat Stat() const;
    void ResetStat();
    size_t Size() const;
    size_t Queries() const;
    void SyncAccount(rai::Transaction&, const rai::Account&, uint32_t);
    void SyncRelated(const std::shared_ptr<rai::Block>&, uint32_t);

    static size_t constexpr BUSY_SIZE = 10240;
    static uint32_t constexpr DEFAULT_BATCH_ID =
        std::numeric_limits<uint32_t>::max();

private:
    bool Add_(const rai::Account&, const rai::SyncInfo&);
    void BlockQuery_(const rai::Account&, const rai::SyncInfo&, uint32_t);
    void BlockQuery_(const rai::BlockHash&, uint32_t);
    rai::QueryCallback QueryCallbackByAccount_(const rai::Account&, uint64_t);
    rai::QueryCallback QueryCallbackByHash_(const rai::BlockHash&, uint64_t);

    rai::Node& node_;
    mutable std::mutex mutex_;
    uint64_t current_query_id_;
    rai::SyncStat stat_;
    std::unordered_map<rai::Account, rai::SyncInfo> syncs_;
    std::unordered_map<uint64_t, uint32_t> queries_;
};
}
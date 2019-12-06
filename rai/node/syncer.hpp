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
    void Add(const rai::Account&, uint64_t, bool = true);
    void Add(const rai::Account&, uint64_t, const rai::BlockHash&,
             bool = true);
    bool Busy() const;
    bool Empty() const;
    void Erase(const rai::Account&);
    void ProcessorCallback(const rai::BlockProcessResult&,
                           const std::shared_ptr<rai::Block>&);
    void QueryCallback(const rai::Account&, rai::QueryStatus,
                       const std::shared_ptr<rai::Block>&);
    rai::SyncStat Stat() const;
    void ResetStat();
    size_t Size() const;
    void SyncAccount(rai::Transaction&, const rai::Account&);
    void SyncRelated(const std::shared_ptr<rai::Block>&);

    static size_t constexpr BUSY_SIZE = 10240;

private:
    bool Add_(const rai::Account&, const rai::SyncInfo&);
    void BlockQuery_(const rai::Account&, const rai::SyncInfo&);
    void BlockQuery_(const rai::BlockHash&);
    rai::QueryCallback QueryCallbackByAccount_(const rai::Account&);
    rai::QueryCallback QueryCallbackByHash_(const rai::BlockHash&);

    rai::Node& node_;
    mutable std::mutex mutex_;
    rai::SyncStat stat_;
    std::unordered_map<rai::Account, rai::SyncInfo> syncs_;
};
}
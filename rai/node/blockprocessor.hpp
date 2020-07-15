#pragma once

#include <memory>
#include <condition_variable>
#include <unordered_set>
#include <stack>
#include <thread>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/blocks.hpp>
#include <rai/secure/store.hpp>
#include <rai/secure/ledger.hpp>
#include <rai/node/blockquery.hpp>

namespace rai
{
enum class BlockOperation : uint64_t
{
    INVALID  = 0,
    APPEND   = 1,
    PREPEND  = 2,
    ROLLBACK = 3,
    DROP     = 4,
    CONFIRM  = 5,

    DYNAMIC_BEGIN
};

class BlockForced
{
public:
    BlockForced(rai::BlockOperation, const std::shared_ptr<rai::Block>&);
    BlockForced(uint64_t, const std::shared_ptr<rai::Block>&);
    uint64_t operation_;
    std::shared_ptr<rai::Block> block_;
};

class BlockDynamic
{
public:
    rai::BlockOperation operation_;
    std::shared_ptr<rai::Block> block_;
};

class BlockFork
{
public:
    std::shared_ptr<rai::Block> first_;
    std::shared_ptr<rai::Block> second_;
    bool from_local_;
};

class BlockProcessResult
{
public:
    rai::BlockOperation operation_;
    rai::ErrorCode error_code_;

    // block confirm extra info
    uint64_t last_confirm_height_;
};

class Node;
class BlockProcessor
{
public:
    BlockProcessor(rai::Node&);
    ~BlockProcessor();
    void Add(const std::shared_ptr<rai::Block>&);
    void AddForced(const rai::BlockForced&);
    void AddFork(const rai::BlockFork&);
    bool Busy() const;
    void Run();
    void Stop();

    static size_t constexpr MAX_BLOCKS = 256 * 1024;
    static size_t constexpr MAX_BLOCKS_FORK = 128 * 1024;
    static size_t constexpr BUSY_PERCENTAGE = 60;

    class OrderedKey
    {
    public:
        uint32_t priority_;
        std::chrono::steady_clock::time_point arrival_;

        bool operator<(const OrderedKey& other) const
        {
            if (priority_ < other.priority_)
            {
                return true;
            }
            if (arrival_ < other.arrival_)
            {
                return true;
            }
            return false;
        }
    };

    class BlockInfo
    {
    public:
        OrderedKey key_;
        rai::BlockHash hash_;
        std::shared_ptr<rai::Block> block_;
    };

    std::function<void(const rai::BlockProcessResult&,
                       const std::shared_ptr<rai::Block>&)>
        block_observer_;
    std::function<void(bool, const std::shared_ptr<rai::Block>&,
                       const std::shared_ptr<rai::Block>&)>
        fork_observer_;

private:
    static uint32_t Priority_(const std::shared_ptr<rai::Block>&);
    uint64_t DynamicOpration_();
    void ProcessBlock_(const std::shared_ptr<rai::Block>&, bool);
    void ProcessBlockFork_(const std::shared_ptr<rai::Block>&,
                           const std::shared_ptr<rai::Block>&);
    void ProcessBlockForced_(uint64_t, const std::shared_ptr<rai::Block>&);
    void ProcessBlockDynamic_(uint64_t);
    rai::ErrorCode ProcessBlockDynamicAppend_(
        uint64_t, std::stack<rai::BlockDynamic>&,
        const std::shared_ptr<rai::Block>&);
    rai::ErrorCode ProcessBlockDynamicPrepend_(
        uint64_t, std::stack<rai::BlockDynamic>&,
        const std::shared_ptr<rai::Block>&);
    rai::ErrorCode ProcessBlockDynamicRollback_(
        uint64_t, std::stack<rai::BlockDynamic>&,
        const std::shared_ptr<rai::Block>&);
    rai::ErrorCode ProcessBlockDynamicConfirm_(
        uint64_t, std::stack<rai::BlockDynamic>&,
        const std::shared_ptr<rai::Block>&, uint64_t&);
    rai::ErrorCode AppendBlock_(rai::Transaction&,
                                const std::shared_ptr<rai::Block>&);
    rai::ErrorCode PrependBlock_(rai::Transaction&,
                                 const std::shared_ptr<rai::Block>&);
    rai::ErrorCode RollbackBlock_(rai::Transaction&,
                                  const std::shared_ptr<rai::Block>&);
    rai::ErrorCode ConfirmBlock_(rai::Transaction&,
                                 const std::shared_ptr<rai::Block>&, uint64_t&);
    rai::QueryCallback QueryCallback_(uint64_t);
    bool QueryPrevious_(uint64_t, const std::shared_ptr<rai::Block>&);
    bool QueryPrevious_(rai::Transaction&, uint64_t, const rai::Account&);
    void QuerySource_(uint64_t, const rai::BlockHash&);
    void UpdateForks_(const std::unordered_set<rai::Account>&);

    rai::Node& node_;
    rai::Ledger& ledger_;
    std::atomic<uint64_t> operation_;
    std::unordered_map<uint64_t, std::stack<rai::BlockDynamic>> blocks_dynamic_;
    std::unordered_map<uint64_t, std::unordered_set<rai::Account>>
        accounts_dynamic_;

    // mutex begin
    mutable std::mutex mutex_;
    boost::multi_index_container<
        BlockInfo,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                BlockInfo, OrderedKey, &BlockInfo::key_>>,
            boost::multi_index::hashed_unique<boost::multi_index::member<
                BlockInfo, rai::BlockHash, &BlockInfo::hash_>>>>
        blocks_;
    std::deque<rai::BlockForced> blocks_forced_;
    std::deque<rai::BlockFork> blocks_fork_;
    bool stopped_;
    //mutex end

    std::condition_variable condition_;
    std::thread thread_;
};
}  // namespace rai
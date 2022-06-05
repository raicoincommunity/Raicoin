#pragma once
#include <condition_variable>
#include <thread>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <rai/common/numbers.hpp>
#include <rai/common/blocks.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/common/chain.hpp>
#include <rai/secure/common.hpp>
#include <rai/node/blockquery.hpp>


namespace rai
{
class Node;

typedef std::function<rai::ErrorCode(std::shared_ptr<rai::Block>&)>
    RewarderAction;
typedef std::function<void(rai::ErrorCode, const std::shared_ptr<rai::Block>&)>
    RewarderCallback;

class RewarderInfo
{
public:
    uint64_t timestamp_;
    rai::BlockHash hash_;
    rai::Amount amount_;
};

class RewarderInfoByTime
{
};

class RewarderInfoByAmount
{
};

typedef boost::multi_index_container<
    rai::RewarderInfo,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<boost::multi_index::member<
            rai::RewarderInfo, rai::BlockHash, &rai::RewarderInfo::hash_>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::RewarderInfoByTime>,
            boost::multi_index::member<rai::RewarderInfo, uint64_t,
                                       &rai::RewarderInfo::timestamp_>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::RewarderInfoByAmount>,
            boost::multi_index::member<rai::RewarderInfo, rai::Amount,
                                       &rai::RewarderInfo::amount_>,
            std::greater<rai::Amount>>>>
    RewarderInfos;

class Rewarder
{
public:
    Rewarder(rai::Node&, const rai::Account&, uint32_t);

    void Add(const rai::BlockHash&, const rai::Amount&, uint64_t);
    void Add(const rai::RewarderInfo&);
    void UpToDate();
    void KeepUpToDate() const;
    void QueueAction(const rai::RewarderAction&, const rai::RewarderCallback&);
    void Run();
    void Send();
    rai::ErrorCode Bind(rai::Chain, const rai::SignerAddress&);
    uint32_t SendInterval() const;
    void Start();
    void Status(rai::Ptree&) const;
    void Stop();
    void Sync();

private:
    bool Confirm_(std::unique_lock<std::mutex>&);
    rai::ErrorCode ProcessReward_(const rai::Amount&, const rai::BlockHash&,
                                  std::shared_ptr<rai::Block>&);
    rai::ErrorCode ProcessSend_(const rai::Account&,
                                std::shared_ptr<rai::Block>&);
    rai::QueryCallback QueryCallback_(const rai::Account&, uint64_t,
                                      const rai::BlockHash&) const;

    rai::Node& node_;
    uint32_t daily_send_times_;
    rai::Fan fan_;
    std::condition_variable condition_;
    mutable std::mutex mutex_;
    bool stopped_;
    bool up_to_date_;
    std::shared_ptr<rai::Block> block_;
    std::deque<std::pair<rai::RewarderAction, rai::RewarderCallback>> actions_;
    rai::RewarderInfos rewardables_;
    rai::RewarderInfos waitings_;
    std::thread thread_;
};
}  // namespace rai
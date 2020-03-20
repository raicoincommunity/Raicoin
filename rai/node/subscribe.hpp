#pragma once
#include <mutex>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <rai/common/numbers.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/blocks.hpp>
#include <rai/secure/ledger.hpp>

namespace rai
{
class Node;

class Subscription
{
public:
    rai::Account account_;
    std::chrono::steady_clock::time_point time_;
};

class SubscriptionByTime
{
};

class Subscriptions
{
public:
    Subscriptions(rai::Node&);
    void Add(const rai::Account&);
    void BlockAppend(const std::shared_ptr<rai::Block>&);
    void BlockConfirm(const std::shared_ptr<rai::Block>&, uint64_t);
    void BlockRollback(const std::shared_ptr<rai::Block>&);
    void BlockDrop(const std::shared_ptr<rai::Block>&);
    void ConfirmReceivables(const rai::Account&);
    void Cutoff();
    void Erase(const rai::Account&);
    bool Exists(const rai::Account&) const;
    void StartElection(const rai::Account&);
    rai::ErrorCode Subscribe(const rai::Account&, uint64_t);
    rai::ErrorCode Subscribe(const rai::Account&, uint64_t,
                             const rai::Signature&);
    void Unsubscribe(const rai::Account&);

    static std::chrono::seconds constexpr CUTOFF_TIME =
        std::chrono::seconds(900);
    static uint64_t constexpr TIME_DIFF = 60;

private:
    void StartElection_(rai::Transaction&, const rai::Account&);
    void BlockConfirm_(rai::Transaction&, const std::shared_ptr<rai::Block>&);

    rai::Node& node_;
    mutable std::mutex mutex_;
    boost::multi_index_container<
        rai::Subscription,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<boost::multi_index::member<
                rai::Subscription, rai::Account, &rai::Subscription::account_>>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<rai::SubscriptionByTime>,
                boost::multi_index::member<
                    rai::Subscription, std::chrono::steady_clock::time_point,
                    &rai::Subscription::time_>>>>
        subscriptions_;
};
}
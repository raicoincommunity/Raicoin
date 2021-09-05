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

enum class SubscriptionEvent
{
    INVALID         = 0,
    BLOCK_APPEND    = 1,
    BLOCK_ROLLBACK  = 2,
};
rai::SubscriptionEvent StringToSubscriptionEvent(const std::string&);

class Subscriptions
{
public:
    Subscriptions(rai::Node&);
    void Add(const rai::Account&);
    void Add(rai::SubscriptionEvent);
    void BlockAppend(const std::shared_ptr<rai::Block>&);
    void BlockConfirm(const std::shared_ptr<rai::Block>&, uint64_t);
    void BlockRollback(const std::shared_ptr<rai::Block>&);
    void BlockDrop(const std::shared_ptr<rai::Block>&);
    void BlockFork(bool, const std::shared_ptr<rai::Block>&,
                   const std::shared_ptr<rai::Block>&);
    void ConfirmReceivables(const rai::Account&);
    void Cutoff();
    void Erase(const rai::Account&);
    void Erase(rai::SubscriptionEvent);
    void Erase(const rai::Block&);
    bool Exists(const rai::Account&) const;
    bool Exists(rai::SubscriptionEvent) const;
    bool Exists(const rai::Block&) const;
    size_t Size() const;
    std::vector<rai::Account> List() const;
    void StartElection(const rai::Account&);
    rai::ErrorCode Subscribe(const rai::Account&, uint64_t);
    rai::ErrorCode Subscribe(const rai::Account&, uint64_t,
                             const rai::Signature&);
    rai::ErrorCode Subscribe(const std::string&);
    rai::ErrorCode Subscribe(const rai::Block&);
    void Unsubscribe(const rai::Account&);
    void Unsubscribe(rai::SubscriptionEvent);

    static std::chrono::seconds constexpr CUTOFF_TIME =
        std::chrono::seconds(900);
    static uint64_t constexpr TIME_DIFF = 150;

private:
    void StartElection_(rai::Transaction&, const rai::Account&);
    void BlockConfirm_(rai::Transaction&, const std::shared_ptr<rai::Block>&,
                       uint64_t);
    bool NeedConfirm_(rai::Transaction&, const rai::Account&);


    class ConfirmRequest
    {
    public:
        rai::AccountHeight key_;
        std::chrono::steady_clock::time_point time_;
    };

    class ConfirmRequestByTime
    {
    };

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
        
    std::unordered_map<rai::SubscriptionEvent,
                       std::chrono::steady_clock::time_point>
        event_subscriptions_;

    boost::multi_index_container<
        ConfirmRequest,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<boost::multi_index::member<
                ConfirmRequest, rai::AccountHeight, &ConfirmRequest::key_>>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<ConfirmRequestByTime>,
                boost::multi_index::member<
                    ConfirmRequest, std::chrono::steady_clock::time_point,
                    &ConfirmRequest::time_>>>>
        confirm_requests_;
    
};
}
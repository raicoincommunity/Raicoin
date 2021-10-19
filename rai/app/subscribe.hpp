#pragma once

#include <boost/multi_index/composite_key.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/blocks.hpp>
#include <rai/secure/ledger.hpp>

namespace rai
{
class App;

class AppSubscriptionData
{
public:
    AppSubscriptionData();
    void Json(rai::Ptree&);
    
    virtual void SerializeJson(rai::Ptree&);

    bool synced_;
};

class AppSubscriptionByUid
{
};

class AppSubscriptionByTime
{
};


class AppSubscription
{
public:
    rai::Account account_;
    rai::UniqueId uid_;
    std::chrono::steady_clock::time_point time_;
};

typedef boost::multi_index_container<
    rai::AppSubscription,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::composite_key<
            rai::AppSubscription,
            boost::multi_index::member<rai::AppSubscription, rai::Account,
                                       &rai::AppSubscription::account_>,
            boost::multi_index::member<rai::AppSubscription, rai::UniqueId,
                                       &rai::AppSubscription::uid_>>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::AppSubscriptionByUid>,
            boost::multi_index::member<rai::AppSubscription, rai::UniqueId,
                                       &rai::AppSubscription::uid_>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::AppSubscriptionByTime>,
            boost::multi_index::member<rai::AppSubscription,
                                       std::chrono::steady_clock::time_point,
                                       &rai::AppSubscription::time_>>>>
    AppSubscriptionContainer;

class AppSubscriptions
{
public:
    AppSubscriptions(rai::App&);

    virtual void AfterSubscribe(
        const rai::Account&, const std::shared_ptr<rai::AppSubscriptionData>&);
    virtual void AfterUnsubscribe(
        const rai::Account&, const std::shared_ptr<rai::AppSubscriptionData>&);
    virtual std::shared_ptr<rai::AppSubscriptionData> MakeData(
        const rai::Account&);

    void Cutoff();
    void Synced(const rai::Account&);
    void Notify(const rai::Account&, const rai::Ptree&);
    void NotifyAccountSynced(const rai::Account&);
    rai::ErrorCode Subscribe(const rai::Account&, const rai::UniqueId&);
    void Unsubscribe(const rai::UniqueId&);
    size_t Size() const;
    size_t Size(const rai::UniqueId&) const;
    size_t AccountSize() const;
    void Json(rai::Ptree&) const;
    void JsonByUid(const rai::UniqueId&, rai::Ptree&) const;
    void JsonByAccount(const rai::Account&, rai::Ptree&) const;
    bool IsSynced(const rai::Account&) const;

    rai::App& app_;

    static std::chrono::seconds constexpr CUTOFF_TIME =
        std::chrono::seconds(900);

private:
    void Json_(std::unique_lock<std::mutex>&, const rai::Account&,
               rai::Ptree&) const;

    mutable std::mutex mutex_;
    rai::AppSubscriptionContainer subscriptions_;
    std::unordered_map<rai::Account, std::shared_ptr<rai::AppSubscriptionData>>
        accounts_;
};
}
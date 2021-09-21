#pragma once

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

    bool synced_;
};

class AppSubscriptionByTime
{
};


class AppSubscription
{
public:
    rai::Account account_;
    std::chrono::steady_clock::time_point time_;
    std::shared_ptr<rai::AppSubscriptionData> data_;
};

class AppSubscriptions
{
public:
    AppSubscriptions(rai::App&);

    virtual void AfterSubscribe(const rai::Account&, bool);
    virtual void PreUnsubscribe(
        const rai::Account&, const std::shared_ptr<rai::AppSubscriptionData>&);
    virtual std::shared_ptr<rai::AppSubscriptionData> MakeData(
        const rai::Account&);

    bool Add(const rai::Account&, rai::AppSubscription&);
    void Synced(const rai::Account&);
    rai::ErrorCode Subscribe(const rai::Account&);

    rai::App& app_;

private:
    mutable std::mutex mutex_;
    boost::multi_index_container<
        rai::AppSubscription,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::member<rai::AppSubscription, rai::Account,
                                           &rai::AppSubscription::account_>>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<rai::AppSubscriptionByTime>,
                boost::multi_index::member<
                    rai::AppSubscription, std::chrono::steady_clock::time_point,
                    &rai::AppSubscription::time_>>>>
        subscriptions_;
};
}
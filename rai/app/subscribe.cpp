#include <rai/app/subscribe.hpp>

#include <rai/app/app.hpp>

rai::AppSubscriptionData::AppSubscriptionData(): synced_(false)
{
}

rai::AppSubscriptions::AppSubscriptions(rai::App& app) : app_(app)
{
}

void rai::AppSubscriptions::AfterSubscribe(const rai::Account& account,
                                           bool existing)
{
}

void rai::AppSubscriptions::PreUnsubscribe(
    const rai::Account& account,
    const std::shared_ptr<rai::AppSubscriptionData>& data)
{
}

std::shared_ptr<rai::AppSubscriptionData> rai::AppSubscriptions::MakeData(
    const rai::Account& account)
{
    return std::make_shared<rai::AppSubscriptionData>();
}

bool rai::AppSubscriptions::Add(const rai::Account& account,
                                rai::AppSubscription& modified)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rai::AppSubscription sub{account, std::chrono::steady_clock::now(),
                             MakeData(account)};
    auto it = subscriptions_.find(sub.account_);
    if (it != subscriptions_.end())
    {
        subscriptions_.modify(it, [&](rai::AppSubscription& data) {
            data.time_ = sub.time_;
            modified = data;
        });
        return true;
    }
    else
    {
        subscriptions_.insert(sub);
        modified = sub;
        return false;
    }
}

void rai::AppSubscriptions::Synced(const rai::Account& account)
{
    bool notify = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.find(account);
        if (it == subscriptions_.end())
        {
            return;
        }
        if (it->data_ && !it->data_->synced_)
        {
            subscriptions_.modify(it, [&](rai::AppSubscription& data) {
                data.data_->synced_ = true;
            });
            notify = true;
        }
    }

    if (notify)
    {
        // todo:
    }
}

rai::ErrorCode rai::AppSubscriptions::Subscribe(const rai::Account& account)
{
    if (!app_.config_.callback_)
    {
        return rai::ErrorCode::SUBSCRIBE_NO_CALLBACK;
    }

    rai::AppSubscription sub;
    bool existing = Add(account, sub);
    if (sub.data_ && !sub.data_->synced_)
    {
        app_.SyncAccountAsync(account);
    }

    AfterSubscribe(account, existing);

    return rai::ErrorCode::SUCCESS;
}
#include <rai/app/subscribe.hpp>

#include <rai/app/app.hpp>

std::chrono::seconds constexpr rai::AppSubscriptions::CUTOFF_TIME;

rai::AppSubscriptionData::AppSubscriptionData(): synced_(false)
{
}

rai::AppSubscriptions::AppSubscriptions(rai::App& app) : app_(app)
{
}

void rai::AppSubscriptions::AfterSubscribe(
    const rai::Account& account,
    const std::shared_ptr<rai::AppSubscriptionData>& data)
{
}

void rai::AppSubscriptions::AfterUnsubscribe(
    const rai::Account& account,
    const std::shared_ptr<rai::AppSubscriptionData>& data)
{
}

std::shared_ptr<rai::AppSubscriptionData> rai::AppSubscriptions::MakeData(
    const rai::Account& account)
{
    return std::make_shared<rai::AppSubscriptionData>();
}

void rai::AppSubscriptions::Cutoff()
{
    std::unique_lock<std::mutex> lock(mutex_);

    auto cutoff =
        std::chrono::steady_clock::now() - rai::AppSubscriptions::CUTOFF_TIME;

    while (true)
    {
        auto it = subscriptions_.get<rai::AppSubscriptionByTime>().begin();
        if (it == subscriptions_.get<rai::AppSubscriptionByTime>().end())
        {
            return;
        }
        
        if (it->time_ > cutoff)
        {
            return;
        }
        rai::Account account = it->account_;
        subscriptions_.get<rai::AppSubscriptionByTime>().erase(it);

        bool erase = false;
        std::shared_ptr<rai::AppSubscriptionData> data(nullptr);
        std::pair<rai::AppSubscriptionContainer::iterator,
                  rai::AppSubscriptionContainer::iterator>
            it_pair = subscriptions_.equal_range(boost::make_tuple(account));
        if (it_pair.first == it_pair.second)
        {
            erase = true;
            data = accounts_[account];
            accounts_.erase(account);
        }

        if (erase)
        {
            lock.unlock();
            AfterUnsubscribe(account, data);
            lock.lock();
        }
    }
}

void rai::AppSubscriptions::Synced(const rai::Account& account)
{
    std::vector<rai::UniqueId> clients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = accounts_.find(account);
        if (it == accounts_.end())
        {
            return;
        }

        if (it->second->synced_)
        {
            return;
        }

        it->second->synced_ = true;
    }

    NotifyAccountSynced(account);
}

void rai::AppSubscriptions::Notify(const rai::Account& account,
                                   const rai::Ptree& notify)
{
    std::vector<rai::UniqueId> clients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::pair<rai::AppSubscriptionContainer::iterator,
                  rai::AppSubscriptionContainer::iterator>
            it_pair = subscriptions_.equal_range(boost::make_tuple(account));
        for (auto& i = it_pair.first; i != it_pair.second; ++i)
        {
            clients.push_back(i->uid_);
        }
    }

    for (const auto& i : clients)
    {
        app_.SendToClient(notify, i);
    }
}

void rai::AppSubscriptions::NotifyAccountSynced(const rai::Account& account)
{
    using P = rai::Provider;
    rai::Ptree ptree;
    P::PutAction(ptree, P::Action::APP_ACCOUNT_SYNC);
    P::PutId(ptree, app_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_ACCOUNT, account.StringAccount());
    Notify(account, ptree);
}

rai::ErrorCode rai::AppSubscriptions::Subscribe(const rai::Account& account,
                                                const rai::UniqueId& uid)
{
    bool existing = false;
    std::shared_ptr<rai::AppSubscriptionData> data(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = accounts_.find(account);
        if (it == accounts_.end())
        {
            data = MakeData(account);
            accounts_[account] = data;
        }
        else
        {
            existing = true;
            data = it->second;
        }

        auto now = std::chrono::steady_clock::now();
        auto it_sub = subscriptions_.find(boost::make_tuple(account, uid));
        if (it_sub == subscriptions_.end())
        {
            rai::AppSubscription sub{account, uid, now};
            subscriptions_.insert(sub);
        }
        else
        {
            subscriptions_.modify(it_sub, [&](rai::AppSubscription& sub){
                sub.time_ = now;
            });
        }
    }

    if (!data->synced_)
    {
        app_.SyncAccountAsync(account);
    }

    if (!existing)
    {
        AfterSubscribe(account, data);
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::AppSubscriptions::Unsubscribe(const rai::UniqueId& uid)
{
    std::unique_lock<std::mutex> lock(mutex_);

    while (true)
    {
        auto it = subscriptions_.get<rai::AppSubscriptionByUid>().lower_bound(uid);
        if (it == subscriptions_.get<rai::AppSubscriptionByUid>().upper_bound(uid))
        {
            return;
        }

        rai::Account account = it->account_;
        subscriptions_.get<rai::AppSubscriptionByUid>().erase(it);

        bool erase = false;
        std::shared_ptr<rai::AppSubscriptionData> data(nullptr);
        std::pair<rai::AppSubscriptionContainer::iterator,
                  rai::AppSubscriptionContainer::iterator>
            it_pair = subscriptions_.equal_range(boost::make_tuple(account));
        if (it_pair.first == it_pair.second)
        {
            erase = true;
            data = accounts_[account];
            accounts_.erase(account);
        }

        if (erase)
        {
            lock.unlock();
            AfterUnsubscribe(account, data);
            lock.lock();
        }
    }
}

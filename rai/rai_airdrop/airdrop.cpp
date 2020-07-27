#include <rai/rai_airdrop/airdrop.hpp>

#include <rai/common/parameters.hpp>
#include <rai/secure/http.hpp>

rai::AirdropConfig::AirdropConfig()
{
}

rai::ErrorCode rai::AirdropConfig::DeserializeJson(bool& upgraded,
                                                   rai::Ptree& ptree)
{
    if (ptree.empty())
    {
        upgraded = true;
        SerializeJson(ptree);
        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    try
    {
        error_code = rai::ErrorCode::JSON_CONFIG_VERSION;
        std::string version_str = ptree.get<std::string>("version");
        uint32_t version = 0;
        bool error = rai::StringToUint(version_str, version);
        IF_ERROR_RETURN(error, error_code);

        error_code = UpgradeJson(upgraded, version, ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_ONLINE_STATS_URL;
        std::string online_stats =
            ptree.get<std::string>("online_statistics_url");
        error = online_stats_.Parse(online_stats);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_INVITED_REPS_URL;
        std::string invited_reps =
            ptree.get<std::string>("invited_representatives_url");
        error = invited_reps_.Parse(invited_reps);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_WALLET;
        rai::Ptree wallet_ptree = ptree.get_child("wallet");
        error_code = wallet_.DeserializeJson(upgraded, wallet_ptree);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    catch (const std::exception&)
    {
        return error_code;
    }
    return rai::ErrorCode::SUCCESS;
}

void rai::AirdropConfig::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("version", "1");

    ptree.put("online_statistics_url", online_stats_.String());
    ptree.put("invited_representatives_url", invited_reps_.String());

    rai::Ptree wallet_ptree;
    wallet_.SerializeJson(wallet_ptree);
    ptree.add_child("wallet", wallet_ptree);
}

rai::ErrorCode rai::AirdropConfig::UpgradeJson(bool& upgraded, uint32_t version,
                                               rai::Ptree& ptree) const
{
    switch (version)
    {
        case 1:
        {
            break;
        }
        default:
        {
            return rai::ErrorCode::CONFIG_VERSION;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

bool rai::AirdropInfo::operator>(const rai::AirdropInfo& other) const
{
    return wakeup_ > other.wakeup_;
}

rai::Airdrop::Airdrop(const std::shared_ptr<rai::Wallets>& wallets,
                      const rai::AirdropConfig& config)
    : wallets_(wallets),
      config_(config),
      stopped_(false),
      last_request_(0),
      prev_updated_(false),
      online_updated_(false),
      invited_updated_(false),
      prev_stat_ts_(0),
      online_stat_ts_(0),
      valid_weight_(0),
      thread_([this]() { this->Run(); })

{
}

void rai::Airdrop::Add(const rai::Account& account, uint64_t delay)
{
    rai::AirdropInfo info{
        std::chrono::steady_clock::now() + std::chrono::seconds(delay),
        account};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        airdrops_.push(info);
    }
    condition_.notify_all();
}

void rai::Airdrop::Join()
{
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void rai::Airdrop::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (airdrops_.size() < rai::AIRDROP_ACCOUNTS)
    {
        condition_.wait(lock);
    }
    
    while (!stopped_)
    {
        bool do_request = false;
        uint64_t now = rai::CurrentTimestamp();
        if (last_request_ + 300 < now)
        {
            do_request = true;
            last_request_ = now;
            std::cout << "queue size:" << airdrops_.size() << std::endl;
            if (!airdrops_.empty())
            {
                std::cout << "queue head:"
                          << airdrops_.top().account_.StringAccount()
                          << std::endl;
            }
        }
        lock.unlock();

        if (do_request)
        {
            prev_updated_ = false;
            online_updated_ = false;
            invited_updated_ = false;
            UpdateStatTs_();
            StatsRequest(true);
            StatsRequest(false);
            InvitedRepsRequest();
        }
        ProcessReceivables();

        lock.lock();
        if (airdrops_.empty())
        {
            std::cout << "Airdrop finished" << std::endl;
            break;
        }
        
        rai::AirdropInfo info = airdrops_.top();
        if (info.wakeup_ <= std::chrono::steady_clock::now())
        {
            airdrops_.pop();
            lock.unlock();

            ProcessAirdrop(info.account_);

            lock.lock();
        }
        else
        {
            condition_.wait_for(lock, std::chrono::seconds(5));
        }
    }
}

void rai::Airdrop::Start()
{
    uint32_t wallet_id = wallets_->SelectedWalletId();
    if (wallet_id == 0)
    {
        std::cout << "Start with invalid wallet id" << std::endl;
        return;
    }

    auto wallet = wallets_->SelectedWallet();
    std::cout << "1:" << wallet->Account(1).StringAccount() << std::endl;

    while (wallets_->WalletAccounts(wallet_id) < rai::AIRDROP_ACCOUNTS)
    {
        uint32_t account_id       = 0;
        rai::ErrorCode error_code = wallets_->CreateAccount(account_id);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            std::cout << rai::ErrorString(error_code) << std::endl;
            return;
        }
        std::cout << account_id << ":"
                  << wallet->Account(account_id).StringAccount() << std::endl;
    }

    auto accounts = wallet->Accounts();
    for (const auto& i : accounts)
    {
        account_ids_.emplace(i.second.first, i.first);
    }

    uint32_t count = 0;
    for (const auto& i : accounts)
    {
        ++count;
        Add(i.second.first, count/100 + 5);
    }
}

std::shared_ptr<rai::Airdrop> rai::Airdrop::Shared()
{
    return shared_from_this();
}

void rai::Airdrop::ProcessReceivables()
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, wallets_->ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    rai::ReceivableInfosAll receivables;
    bool error = wallets_->ledger_.ReceivableInfosGet(
        transaction, rai::ReceivableInfosType::ALL, receivables);
    if (error)
    {
        std::cout << "Failed to get receivables" << std::endl;
        return;
    }

    std::unordered_map<rai::Account, std::pair<uint32_t, uint32_t>> counts;
    for (const auto& i : receivables)
    {
        rai::Account account(i.second.first);
        rai::BlockHash hash(i.second.second);

        if (counts.find(account) == counts.end())
        {
            rai::AccountInfo info;
            error =
                wallets_->ledger_.AccountInfoGet(transaction, account, info);
            if (!error && info.Valid())
            {
                std::shared_ptr<rai::Block> head(nullptr);
                error =
                    wallets_->ledger_.BlockGet(transaction, info.head_, head);
                if (!error)
                {
                    counts.emplace(
                        account,
                        std::make_pair(
                            head->Counter(),
                            head->Credit() * rai::TRANSACTIONS_PER_CREDIT));
                }
                else
                {
                    std::cout << "Failed to get head block: account="
                              << account.StringAccount()
                              << ", head=" << info.head_.StringHex()
                              << std::endl;
                    return;
                }
            }
            else
            {
                counts.emplace(account,
                               std::make_pair(0, rai::TRANSACTIONS_PER_CREDIT));
            }
        }

        auto it = counts.find(account);
        if (it == counts.end())
        {
            std::cout << "Airdrop::ProcessReceivables: logic error"
                      << std::endl;
            return;
        }
        if (it->second.first >= it->second.second - 1)
        {
            continue;
        }
        it->second.first++;

        if (account_ids_.find(account) == account_ids_.end())
        {
            throw std::runtime_error("Failed to find account id "
                                     + account.StringAccount());
        }
        wallets_->SelectAccount(account_ids_[account]);
        error_code = wallets_->AccountReceive(
            account, hash,
            [account, hash](rai::ErrorCode error_code,
                            const std::shared_ptr<rai::Block>& block) {
                if (error_code != rai::ErrorCode::SUCCESS)
                {
                    std::cout << "Failed to receive: account="
                              << account.StringAccount()
                              << ", hash=" << hash.StringHex()
                              << ", error=" << rai::ErrorString(error_code)
                              << "(" << static_cast<uint32_t>(error_code) << ")"
                              << std::endl;
                }
                else
                {
                    std::cout << "Receive: account=" << account.StringAccount()
                              << ", height=" << block->Height()
                              << ", hash=" << block->Hash().StringHex()
                              << std::endl;
                }
                
            });
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            std::cout << "Failed to receive: account="
                      << account.StringAccount()
                      << ", hash=" << hash.StringHex()
                      << ", error=" << rai::ErrorString(error_code) << "("
                      << static_cast<uint32_t>(error_code) << ")" << std::endl;
        }
    }
}

void rai::Airdrop::StatsRequest(bool prev)
{
    std::weak_ptr<rai::Airdrop> airdrop_w(Shared());
    auto client = std::make_shared<rai::HttpClient>(wallets_->service_);

    rai::Url url(config_.online_stats_);
    uint64_t ts = prev ? prev_stat_ts_ : online_stat_ts_;
    url.AddQuery("ts", std::to_string(ts));

    rai::ErrorCode error_code = client->Get(
        url,
        [airdrop_w, prev](rai::ErrorCode error_code, const std::string& body) {
            auto airdrop = airdrop_w.lock();
            if (airdrop == nullptr) return;
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                std::cout << rai::ErrorString(error_code) << ":"
                          << static_cast<uint32_t>(error_code) << std::endl;
                return;
            }
            airdrop->ProcessStats(prev, body);
        });
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        std::cout << rai::ErrorString(error_code) << ":"
                  << static_cast<uint32_t>(error_code) << std::endl;
        return;
    }
    wallets_->service_runner_.Notify();
}

void rai::Airdrop::ProcessStats(bool prev, const std::string& response)
{
    try
    {
        rai::Ptree ptree;
        std::stringstream stream(response);
        boost::property_tree::read_json(stream, ptree);
        std::string status = ptree.get<std::string>("status");
        if (status != "success")
        {
            std::cout << "Airdrop::ProcessStats: invalid status=" << status
                      << std::endl;
            return;
        }
        uint32_t count = ptree.get<uint32_t>("count");
        if (count == 0)
        {
            std::cout << "Airdrop::ProcessStats: count=0" << std::endl;
            if (prev)
            {
                prev_updated_ = true;
            }
            else
            {
                online_updated_ = true;
            }
            return;
        }

        std::unordered_map<rai::Account, uint32_t> stats;
        rai::Ptree records = ptree.get_child("records");
        for (const auto& i : records)
        {
            const auto& record = i.second;
            uint64_t ts = record.get<uint64_t>("timestamp");
            uint64_t ts_expect = prev ? prev_stat_ts_ : online_stat_ts_;
            if (ts != ts_expect)
            {
                std::cout
                    << "Airdrop::ProcessStats: Invalid timestamp field"
                    << std::endl;
                return;
            }

            uint32_t count = record.get<uint32_t>("count");
            rai::Account rep;
            bool error = rep.DecodeAccount(record.get<std::string>("rep"));
            if (error)
            {
                std::cout << "Airdrop::ProcessStats: Invalid rep field"
                          << std::endl;
                return;
            }
            stats.emplace(rep, count);
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (prev)
        {
            prev_updated_ = true;
            prev_stats_ = std::move(stats);
        }
        else
        {
            online_updated_ = true;
            online_stats_ = std::move(stats);
        }
        UpdateValidReps_();
    }
    catch(const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
}

void rai::Airdrop::InvitedRepsRequest()
{
    if (rai::CurrentTimestamp()
        > rai::EpochTimestamp() + rai::AIRDROP_INVITED_ONLY_DURATION)
    {
        return;
    }

    std::weak_ptr<rai::Airdrop> airdrop_w(Shared());
    auto client = std::make_shared<rai::HttpClient>(wallets_->service_);
    rai::ErrorCode error_code = client->Get(
        config_.invited_reps_,
        [airdrop_w](rai::ErrorCode error_code, const std::string& body) {
            auto airdrop = airdrop_w.lock();
            if (airdrop == nullptr) return;
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                // log
                std::cout << rai::ErrorString(error_code) << ":"
                          << static_cast<uint32_t>(error_code) << std::endl;
                return;
            }
            airdrop->ProcessInvitedReps(body);
        });
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        std::cout << rai::ErrorString(error_code) << ":"
                  << static_cast<uint32_t>(error_code) << std::endl;
        return;
    }
    wallets_->service_runner_.Notify();
}

void rai::Airdrop::ProcessInvitedReps(const std::string& response)
{
    try
    {
        rai::Ptree ptree;
        std::stringstream stream(response);
        boost::property_tree::read_json(stream, ptree);

        std::unordered_set<rai::Account> reps;
        rai::Ptree reps_ptree = ptree.get_child("representatives");
        for (const auto& i : reps_ptree)
        {
            rai::Account rep;
            bool error =
                rep.DecodeAccount(i.second.get<std::string>("account"));
            if (error)
            {
                std::cout
                    << "Airdrop::ProcessInvitedReps: Invalid account field"
                    << std::endl;
                return;
            }
            reps.insert(rep);
        }

        std::lock_guard<std::mutex> lock(mutex_);
        invited_updated_ = true;
        invited_reps_ = std::move(reps);
        UpdateValidReps_();
    }
    catch(const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
}

void rai::Airdrop::ProcessAirdrop(const rai::Account& account)
{
    if (!wallets_->Synced(account))
    {
        std::cout << "The account is synchronizing, account="
                  << account.StringAccount() << std::endl;
        Add(account, 300);
        return;
    }

    uint64_t now = rai::CurrentTimestamp();
    uint64_t day = 86400;
    if (now % day < AirdropDelay_(account))
    {
        Add(account, 300);
        return;
    }
    
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, wallets_->ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        throw std::runtime_error("Failed to construct ledger transaction");
    }

    uint64_t last_airdrop = LastAirdrop_(transaction, account);
    if (last_airdrop == 0)
    {
        // account not created
        Add(account, 300);
        return;
    }

    if (rai::SameDay(last_airdrop, now))
    {
        Add(account, rai::DayEnd(now) + AirdropDelay_(account) - now);
        return;
    }

    bool confirmed = false;
    bool destroyed = Destroyed_(transaction, account, confirmed);
    if (destroyed)
    {
        if (confirmed)
        {
            return;
        }
        Add(account, 5);
        return;
    }

    if (account_ids_.find(account) == account_ids_.end())
    {
        throw std::runtime_error("Failed to find account id "
                                    + account.StringAccount());
    }
    wallets_->SelectAccount(account_ids_[account]);

    if (now > rai::EpochTimestamp() + rai::AIRDROP_DURATION)
    {
        error_code = wallets_->AccountDestroy(
            [account](rai::ErrorCode error_code,
                      const std::shared_ptr<rai::Block>& block) {
                if (error_code != rai::ErrorCode::SUCCESS)
                {
                    std::cout << "Failed to destroy: account="
                              << account.StringAccount()
                              << ", error=" << rai::ErrorString(error_code)
                              << "(" << static_cast<uint32_t>(error_code) << ")"
                              << std::endl;
                }
                else
                {
                    std::cout << "Destory: account=" << account.StringAccount()
                              << ", height=" << block->Height()
                              << ", hash=" << block->Hash().StringHex()
                              << std::endl;
                }
            });

        if (error_code != rai::ErrorCode::SUCCESS)
        {
            std::cout << "Failed to destroy: account="
                      << account.StringAccount()
                      << ", error=" << rai::ErrorString(error_code) << "("
                      << static_cast<uint32_t>(error_code) << ")" << std::endl;
        }

        Add(account, 300);
        return;
    }

    rai::Account rep;
    bool error = RandomRep(rep);
    if (error)
    {
        std::cout << "Failed to get rep" << std::endl;
        Add(account, 300);
        return;
    }

    error_code = wallets_->AccountChange(
        rep, [account, rep](rai::ErrorCode error_code,
                            const std::shared_ptr<rai::Block>& block) {
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                std::cout << "Failed to change: account="
                          << account.StringAccount()
                          << ", representative=" << rep.StringAccount()
                          << ", error=" << rai::ErrorString(error_code) << "("
                          << static_cast<uint32_t>(error_code) << ")"
                          << std::endl;
            }
            else
            {
                std::cout << "Airdrop: account=" << account.StringAccount()
                          << ", height=" << block->Height()
                          << ", hash=" << block->Hash().StringHex()
                          << ", to=" << rep.StringAccount() << std::endl;
            }
        });

    if (error_code != rai::ErrorCode::SUCCESS)
    {
        std::cout << "Failed to change: account=" << account.StringAccount()
                  << ", representative=" << rep.StringAccount()
                  << ", error=" << rai::ErrorString(error_code) << "("
                  << static_cast<uint32_t>(error_code) << ")" << std::endl;
    }
    Add(account, 300);
}

bool rai::Airdrop::RandomRep(rai::Account& rep) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (valid_reps_.empty() || valid_weight_ == 0)
    {
        return true;
    }

    uint64_t rand = rai::Random(0, valid_weight_ - 1);
    for (const auto& i : valid_reps_)
    {
        if (rand < i.second)
        {
            rep = i.first;
            return false;
        }
        rand -= i.second;
    }

    return true;
}

uint64_t rai::Airdrop::AirdropDelay_(const rai::Account& account) const
{
    uint64_t seconds = static_cast<uint64_t>(account.Number() % 80000);
    return seconds + 3600;
}

bool rai::Airdrop::Destroyed_(rai::Transaction& transaction,
                              const rai::Account& account, bool& confirmed)
{
    rai::AccountInfo info;
    bool error = wallets_->ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        confirmed = false;
        return false;
    }

    std::shared_ptr<rai::Block> head(nullptr);
    error = wallets_->ledger_.BlockGet(transaction, info.head_, head);
    if (error)
    {
        std::cout << "Failed to get block: account="
                    << account.StringAccount()
                    << ", hash=" << info.head_.StringHex() << std::endl;
        throw std::runtime_error("Ledger inconsistent");
    }

    if (head->Balance() >= rai::Amount(999 * rai::RAI))
    {
        confirmed = false;
        return false;
    }

    confirmed = info.confirmed_height_ == info.head_height_;
    return true;
}

uint64_t rai::Airdrop::LastAirdrop_(rai::Transaction& transaction,
                                    const rai::Account& account)
{
    rai::AccountInfo info;
    bool error = wallets_->ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        return 0;
    }

    rai::BlockHash hash(info.head_);
    while (!hash.IsZero())
    {
        std::shared_ptr<rai::Block> block(nullptr);
        error = wallets_->ledger_.BlockGet(transaction, hash, block);
        if (error)
        {
            std::cout << "Failed to get block: account="
                      << account.StringAccount()
                      << ", hash=" << hash.StringHex() << std::endl;
            throw std::runtime_error("Ledger inconsistent");
        }
        hash = block->Previous();

        if (block->Opcode() == rai::BlockOpcode::CHANGE
            || block->Opcode() == rai::BlockOpcode::DESTROY
            || block->Height() == 0)
        {
            return block->Timestamp();
        }
    }

    return 0;
}

void rai::Airdrop::UpdateStatTs_()
{
    uint64_t day = 86400;
    uint64_t now = rai::CurrentTimestamp();
    online_stat_ts_ = now - now % day - day;
    prev_stat_ts_  = online_stat_ts_ - day;
}

void rai::Airdrop::UpdateValidReps_()
{
    if (!prev_updated_ || !online_updated_)
    {
        return;
    }

    uint64_t weight = 0;
    std::vector<std::pair<rai::Account, uint32_t>> reps;
    if (rai::CurrentTimestamp()
        < rai::EpochTimestamp() + rai::AIRDROP_INVITED_ONLY_DURATION)
    {
        if (!invited_updated_)
        {
            return;
        }

        for (const auto& i : invited_reps_)
        {
            if (online_stats_.find(i) == online_stats_.end())
            {
                continue;
            }

            if (prev_stats_.find(i) != prev_stats_.end()
                && prev_stats_[i] >= online_stats_[i])
            {
                continue;
            }

            reps.emplace_back(i, online_stats_[i]);
            weight += online_stats_[i];
        }
    }
    else
    {
        for (const auto& i : online_stats_)
        {
            if (prev_stats_.find(i.first) != prev_stats_.end()
                && prev_stats_[i.first] >= i.second)
            {
                continue;
            }
            reps.push_back(i);
            weight += i.second;
        }
    }

    if (reps.empty() || weight == 0)
    {
        return;
    }
    valid_weight_ = weight;
    valid_reps_ = std::move(reps);

    for (const auto& i : valid_reps_)
    {
        std::cout << "rep:" << i.first.StringAccount() << ", weight:" << i.second << std::endl;
    }
}

#pragma once

#include <atomic>
#include <mutex>
#include <unordered_set>
#include <rai/common/numbers.hpp>
#include <rai/common/util.hpp>

namespace rai
{

class App;

class AccountHead
{
public:
    bool DeserializeJson(const rai::Ptree&);

    rai::Account account_;
    rai::BlockHash head_;
    uint64_t height_;
};

class AppBootstrap 
{
public:
    AppBootstrap(rai::App&);

    bool Ready() const;
    void Run();
    void Reset();
    void Stop();
    void Status(rai::Ptree&) const;
    void ReceiveAccountHeadMessage(const std::shared_ptr<rai::Ptree>&);
    void ProcessAccountHeads(const std::vector<rai::AccountHead>&, bool);
    void AddSyncingAccount(const rai::Account&);
    void RemoveSyncingAccount(const rai::Account&);
    void UpdateSyncingStatus();
    std::vector<rai::Account> SyncingAccounts(size_t) const;

    rai::App& app_;

    static std::chrono::seconds constexpr BOOTSTRAP_INTERVAL =
        std::chrono::seconds(300);
    static uint32_t constexpr FULL_BOOTSTRAP_INTERVAL = 12;  // an hour
    static uint32_t constexpr INITIAL_FULL_BOOTSTRAPS = 3;

private:
    uint64_t NewRequestId_(std::unique_lock<std::mutex>&);
    void SendRequest_(std::unique_lock<std::mutex>&);

    mutable std::mutex mutex_;
    bool stopped_;
    bool running_;
    uint64_t count_;
    uint64_t request_id_;
    std::chrono::steady_clock::time_point last_run_;
    std::chrono::steady_clock::time_point last_request_;
    rai::Account next_;

    mutable std::mutex syncings_mutex_;
    std::unordered_set<rai::Account> syncings_;
    std::atomic<bool> synced_;
};

} // end of namespace
#pragma once

#include <boost/filesystem.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/secure/ledger.hpp>
#include <rai/wallet/wallet.hpp>

namespace rai
{
class AirdropConfig
{
public:
    AirdropConfig();
    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;

    rai::Url online_stats_;
    rai::Url invited_reps_;
    rai::WalletConfig wallet_;
};

class AirdropInfo
{
public:
    bool operator>(const rai::AirdropInfo&) const;
    std::chrono::steady_clock::time_point wakeup_;
    rai::Account account_;
};

class Airdrop : public std::enable_shared_from_this<rai::Airdrop>
{
public:
    Airdrop(const std::shared_ptr<rai::Wallets>&, const rai::AirdropConfig&);

    void Add(const rai::Account&, uint64_t);
    void Join();
    void Run();
    void Start();
    std::shared_ptr<rai::Airdrop> Shared();
    void ProcessReceivables();

    void OnlineStatsRequest();
    void ProcessOnlineStat(const std::string&);
    void InvitedRepsRequest();
    void ProcessInvitedReps(const std::string&);
    void ProcessAirdrop(const rai::Account&);
    bool RandomRep(rai::Account&) const;

    std::shared_ptr<rai::Wallets> wallets_;
    rai::AirdropConfig config_;
    std::unordered_map<rai::Account, uint32_t> account_ids_;
private:
    uint64_t AirdropDelay_(const rai::Account&) const;
    bool Destroyed_(rai::Transaction&, const rai::Account&, bool&);
    uint64_t LastAirdrop_(rai::Transaction&, const rai::Account&);
    void UpdateOnlineStatTs_();
    void UpdateValidReps_();

    std::condition_variable condition_;
    mutable std::mutex mutex_;
    bool stopped_;
    uint64_t last_request_;
    std::atomic<uint64_t> online_stat_ts_;
    std::unordered_map<rai::Account, uint32_t> online_stats_;
    std::vector<std::pair<rai::Account, uint32_t>> valid_reps_;
    uint64_t valid_weight_;
    std::unordered_set<rai::Account> invited_reps_;
    std::priority_queue<rai::AirdropInfo, std::vector<rai::AirdropInfo>,
                        std::greater<rai::AirdropInfo>>
        airdrops_;
    std::thread thread_;
};

}  // namespace rai

#pragma once

#include <memory>
#include <mutex>
#include <unordered_set>
#include <boost/asio.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/alarm.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/blocks.hpp>
#include <rai/secure/store.hpp>
#include <rai/secure/ledger.hpp>
#include <rai/secure/common.hpp>
#include <rai/secure/websocket.hpp>
#include <rai/wallet/config.hpp>

namespace rai
{

class Wallets;

class Wallet
{
public:
    Wallet(rai::Wallets&);
    Wallet(rai::Wallets&, const rai::WalletInfo&);
    Wallet(rai::Wallets&, const rai::RawKey&);
    
    rai::Account Account(uint32_t) const;
    rai::ErrorCode CreateAccount(uint32_t&);
    std::vector<std::pair<uint32_t, std::pair<rai::Account, bool>>> Accounts()
        const;
    bool AttemptPassword(const std::string&);
    bool ValidPassword() const;
    bool EmptyPassword() const;
    bool IsMyAccount(const rai::Account&) const;
    rai::ErrorCode ChangePassword(const std::string&);
    rai::ErrorCode ImportAccount(const rai::KeyPair&, uint32_t&);
    void Lock();
    rai::ErrorCode PrivateKey(const rai::Account&, rai::RawKey&) const;
    rai::ErrorCode Seed(rai::RawKey&) const;
    bool Sign(const rai::Account&, const rai::uint256_union&,
              rai::Signature&) const;
    size_t Size() const;
    rai::ErrorCode Store(rai::Transaction&, uint32_t);
    rai::ErrorCode StoreInfo(rai::Transaction&, uint32_t);
    rai::ErrorCode StoreAccount(rai::Transaction&, uint32_t, uint32_t);
    bool SelectAccount(uint32_t);
    rai::Account SelectedAccount() const;
    uint32_t SelectedAccountId() const;

    static uint32_t constexpr VERSION_1 = 1;
    static uint32_t constexpr CURRENT_VERSION = 1;

private:
    friend class Wallets;
    rai::RawKey Decrypt_(const rai::uint256_union&) const;
    rai::RawKey Key_() const;
    rai::RawKey Seed_() const;
    uint32_t CreateAccount_();
    uint32_t NewAccountId_() const;
    bool ValidPassword_() const;
    rai::WalletInfo Info_() const;
    bool PrivateKey_(const rai::Account&, rai::RawKey&) const;

    rai::Wallets& wallets_;
    mutable std::mutex mutex_;
    uint32_t version_;
    uint32_t index_;
    uint32_t selected_account_id_;
    rai::uint256_union salt_;
    rai::uint256_union key_;
    rai::uint256_union seed_;
    rai::uint256_union check_;
    rai::Fan fan_;
    std::vector<std::pair<uint32_t, rai::WalletAccountInfo>> accounts_;
};

class WalletServiceRunner
{
public:
    WalletServiceRunner(boost::asio::io_service&);
    ~WalletServiceRunner();
    void Notify();
    void Run();
    void Stop();
    void Join();

private:
    boost::asio::io_service& service_;
    std::condition_variable condition_;
    mutable std::mutex mutex_;
    bool stopped_;
    std::thread thread_;
};

enum class WalletActionPri : uint32_t
{
    LOW    = 0,
    NORMAL = 1,
    HIGH   = 2,
    URGENT = 3
};

class WalletObservers
{
public:
    rai::ObserverContainer<rai::WebsocketStatus> connection_status_;
    rai::ObserverContainer<const std::shared_ptr<rai::Block>&, bool> block_;
    rai::ObserverContainer<const rai::Account&> selected_account_;
    rai::ObserverContainer<uint32_t> selected_wallet_;
    rai::ObserverContainer<bool> wallet_locked_;
    rai::ObserverContainer<> wallet_password_set_;
    rai::ObserverContainer<const rai::Account&> receivable_;
    rai::ObserverContainer<const rai::Account&, bool> synced_;
    rai::ObserverContainer<const rai::Account&> fork_;
    rai::ObserverContainer<> time_synced_;
};

typedef std::function<void(rai::ErrorCode, const std::shared_ptr<rai::Block>&)> AccountActionCallback;

class Wallets : public std::enable_shared_from_this<rai::Wallets>
{
public:
    Wallets(rai::ErrorCode&, boost::asio::io_service&, rai::Alarm&,
            const boost::filesystem::path&, const rai::WalletConfig&, rai::BlockType, const rai::RawKey& = rai::RawKey(), uint32_t = 1);
    ~Wallets();
    std::shared_ptr<rai::Wallets> Shared();

    void AccountBalance(const rai::Account&, rai::Amount&);
    void AccountBalanceConfirmed(const rai::Account&, rai::Amount&);
    void AccountBalanceAll(const rai::Account&, rai::Amount&, rai::Amount&,
                           rai::Amount&);
    bool AccountRepresentative(const rai::Account&, rai::Account&);
    void AccountTransactionsLimit(const rai::Account&, uint32_t&, uint32_t&);
    void AccountInfoQuery(const rai::Account&);
    void AccountForksQuery(const rai::Account&);
    rai::ErrorCode AccountChange(const rai::Account&,
                                 const rai::AccountActionCallback&);
    rai::ErrorCode AccountChange(const rai::Account&,
                                 const std::vector<uint8_t>&,
                                 const rai::AccountActionCallback&);
    rai::ErrorCode AccountCredit(uint16_t, const rai::AccountActionCallback&);
    rai::ErrorCode AccountDestroy(const rai::AccountActionCallback&);
    rai::ErrorCode AccountSend(const rai::Account&, const rai::Amount&,
                               const rai::AccountActionCallback&);
    rai::ErrorCode AccountSend(const rai::Account&, const rai::Amount&,
                               const std::vector<uint8_t>&,
                               const rai::AccountActionCallback&);
    rai::ErrorCode AccountReceive(const rai::Account&, const rai::BlockHash&,
                                  const rai::AccountActionCallback&);

    rai::ErrorCode AccountActionPreCheck(const std::shared_ptr<rai::Wallet>&,
                                         const rai::Account&);
    rai::ErrorCode AccountReceive(const std::shared_ptr<rai::Wallet>&,
                                  const rai::Account&, const rai::BlockHash&,
                                  const rai::AccountActionCallback&);
    void BlockQuery(const rai::Account&, uint64_t, const rai::BlockHash&);
    void BlockPublish(const std::shared_ptr<rai::Block>&);
    void ConnectToServer();
    bool Connected() const;
    rai::ErrorCode ChangePassword(const std::string&);
    rai::ErrorCode CreateAccount();
    rai::ErrorCode CreateAccount(uint32_t&);
    rai::ErrorCode CreateWallet();
    bool CurrentTimestamp(uint64_t&) const;
    bool EnterPassword(const std::string&);
    rai::ErrorCode ImportAccount(const rai::KeyPair&);
    rai::ErrorCode ImportWallet(const rai::RawKey&, uint32_t&);
    bool IsMyAccount(const rai::Account&) const;
    void LockWallet(uint32_t);
    void Run();
    void Ongoing(const std::function<void()>&, const std::chrono::seconds&);
    void ProcessAccountInfo(const rai::Account&, const rai::AccountInfo&);
    void ProcessAccountForks(
        const rai::Account&,
        const std::vector<std::pair<std::shared_ptr<rai::Block>,
                                    std::shared_ptr<rai::Block>>>&);
    void ProcessAccountChange(const std::shared_ptr<rai::Wallet>&,
                              const rai::Account&, const rai::Account&,
                              const std::vector<uint8_t>&,
                              const rai::AccountActionCallback&);
    void ProcessAccountCredit(const std::shared_ptr<rai::Wallet>&,
                              const rai::Account&, uint16_t,
                              const rai::AccountActionCallback&);
    void ProcessAccountDestroy(const std::shared_ptr<rai::Wallet>&,
                               const rai::Account&,
                               const rai::AccountActionCallback&);
    void ProcessAccountSend(const std::shared_ptr<rai::Wallet>&,
                            const rai::Account&, const rai::Account&,
                            const rai::Amount&, const std::vector<uint8_t>&,
                            const rai::AccountActionCallback&);
    void ProcessAccountReceive(const std::shared_ptr<rai::Wallet>&,
                               const rai::Account&, const rai::BlockHash&,
                               const rai::AccountActionCallback&);
    void ProcessBlock(const std::shared_ptr<rai::Block>&, bool);
    void ProcessBlockRollback(const std::shared_ptr<rai::Block>&);
    void ProcessReceivableInfo(const rai::Account&, const rai::BlockHash&,
                               const rai::ReceivableInfo&,
                               const std::shared_ptr<rai::Block>&);
    void QueueAccountInfo(const rai::Account&, const rai::AccountInfo&);
    void QueueAccountForks(
        const rai::Account&,
        const std::vector<std::pair<std::shared_ptr<rai::Block>,
                                    std::shared_ptr<rai::Block>>>&);
    void QueueAction(rai::WalletActionPri, const std::function<void()>&);
    void QueueBlock(const std::shared_ptr<rai::Block>&, bool);
    void QueueBlockRollback(const std::shared_ptr<rai::Block>&);
    void QueueReceivable(const rai::Account&, const rai::BlockHash&,
                         const rai::Amount&, const rai::Account&, uint64_t,
                         const std::shared_ptr<rai::Block>&);
    rai::Account RandomRepresentative() const;
    void ReceivablesQuery(const rai::Account&);
    void ReceiveAccountInfoQueryAck(const std::shared_ptr<rai::Ptree>&);
    void ReceiveAccountForksQueryAck(const std::shared_ptr<rai::Ptree>&);
    void ReceiveBlockAppendNotify(const std::shared_ptr<rai::Ptree>&);
    void ReceiveBlockConfirmNotify(const std::shared_ptr<rai::Ptree>&);
    void ReceiveBlockRollbackNotify(const std::shared_ptr<rai::Ptree>&);
    void ReceiveBlockQueryAck(const std::shared_ptr<rai::Ptree>&);
    void ReceiveCurrentTimestampAck(const std::shared_ptr<rai::Ptree>&);
    void ReceiveForkNotify(const std::shared_ptr<rai::Ptree>&);
    void ReceiveReceivablesQueryAck(const std::shared_ptr<rai::Ptree>&);
    void ReceiveReceivableInfoNotify(const std::shared_ptr<rai::Ptree>&);
    void ReceiveMessage(const std::shared_ptr<rai::Ptree>&);
    void Send(const rai::Ptree&);
    std::vector<std::shared_ptr<rai::Wallet>> SharedWallets() const;
    void Start();
    void Stop();
    void SelectAccount(uint32_t);
    void SelectWallet(uint32_t);
    rai::Account SelectedAccount() const;
    std::shared_ptr<rai::Wallet> SelectedWallet() const;
    uint32_t SelectedWalletId() const;
    void Subscribe(const std::shared_ptr<rai::Wallet>&, uint32_t);
    void Subscribe(const std::shared_ptr<rai::Wallet>&);
    void SubscribeAll();
    void SubscribeSelected();
    void Sync(const rai::Account&);
    void Sync(const std::shared_ptr<rai::Wallet>&);
    void SyncAll();
    void SyncSelected();
    void SyncAccountInfo(const rai::Account&);
    void SyncAccountInfo(const std::shared_ptr<rai::Wallet>&);
    void SyncBlocks(const rai::Account&);
    void SyncBlocks(const std::shared_ptr<rai::Wallet>&);
    void SyncForks(const rai::Account&);
    void SyncForks(const std::shared_ptr<rai::Wallet>&);
    void SyncReceivables(const rai::Account&);
    void SyncReceivables(const std::shared_ptr<rai::Wallet>&);
    void SyncTime();
    bool Synced(const rai::Account&) const;
    void SyncedAdd(const rai::Account&);
    void SyncedClear();
    bool TimeSynced() const;
    void Unsubscribe(const std::shared_ptr<rai::Wallet>&);
    void UnsubscribeAll();
    std::vector<uint32_t> WalletIds() const;
    void WalletBalanceAll(uint32_t, rai::Amount&, rai::Amount&, rai::Amount&);
    uint32_t WalletAccounts(uint32_t) const;
    rai::ErrorCode WalletSeed(uint32_t, rai::RawKey&) const;
    bool WalletLocked(uint32_t) const;
    bool WalletVulnerable(uint32_t) const;
    void LastSubClear();
    void LastSyncClear();

    template <typename T>
    void Background(T action)
    {
        service_.post(action);
    }

    boost::asio::io_service& service_;
    rai::Alarm& alarm_;
    const rai::WalletConfig& config_;
    rai::Store store_;
    rai::Ledger ledger_;
    std::vector<std::shared_ptr<rai::WebsocketClient>> websockets_;
    rai::WalletServiceRunner service_runner_;
    rai::WalletObservers observers_;

private:
    rai::ErrorCode ActionCommonCheck_() const;
    uint32_t NewWalletId_() const;
    void RegisterObservers_();
    bool Rollback_(rai::Transaction&, const rai::Account&);
    void Subscribe_(const std::shared_ptr<rai::Wallet>&, uint32_t);
    void SyncBlocks_(rai::Transaction&, const rai::Account&);
    std::shared_ptr<rai::Wallet> Wallet_(uint32_t) const;

    rai::BlockType block_type_;
    std::atomic<uint32_t> send_count_;
    std::atomic<uint64_t> last_sync_;
    std::atomic<uint64_t> last_sub_;

    mutable std::mutex mutex_timesync_;
    bool time_synced_;
    uint64_t server_time_;
    std::chrono::steady_clock::time_point local_time_;

    std::condition_variable condition_;
    mutable std::mutex mutex_;
    bool stopped_;
    uint32_t selected_wallet_id_;
    std::unordered_set<rai::Account> synced_;
    std::vector<std::pair<uint32_t, std::shared_ptr<rai::Wallet>>> wallets_;
    std::multimap<rai::WalletActionPri, std::function<void()>,
                  std::greater<rai::WalletActionPri>>
        actions_;
    std::thread thread_;
    std::function<void(const std::shared_ptr<rai::Block>&, bool)>
        block_observer_;
    std::function<void(const rai::Account&)> selected_account_observer_;
    std::function<void(uint32_t)> selected_wallet_observer_;
    std::function<void(bool)> wallet_locked_observer_;
    std::function<void()> wallet_password_set_observer_;
    std::function<void(const rai::Account&)> receivable_observer_;
    std::function<void(const rai::Account&, bool)> synced_observer_;
    std::function<void(const rai::Account&)> fork_observer_;
    std::function<void()> time_synced_observer_;
};
}  // namespace rai
#include <rai/wallet/wallet.hpp>
#include <rai/common/util.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/extensions.hpp>

rai::WalletServiceRunner::WalletServiceRunner(boost::asio::io_service& service)
    : service_(service), stopped_(false), thread_([this]() { this->Run(); })
{
}

rai::WalletServiceRunner::~WalletServiceRunner()
{
    Stop();
}
void rai::WalletServiceRunner::Notify()
{
    condition_.notify_all();
}

void rai::WalletServiceRunner::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        condition_.wait_until(
            lock, std::chrono::steady_clock::now() + std::chrono::seconds(3));
        if (stopped_)
        {
            break;
        }
        lock.unlock();

        if (service_.stopped())
        {
            service_.reset();
        }

        try
        {
            service_.run();
        }
        catch(const std::exception& e)
        {
            // log
            std::cout << "service exception: " << e.what() << std::endl;
        }
        
        lock.lock();
    }
}

void rai::WalletServiceRunner::Stop()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
        service_.stop();
    }

    condition_.notify_all();
    Join();
}

void rai::WalletServiceRunner::Join()
{
    if (thread_.joinable())
    {
        thread_.join();
    }
}

rai::Wallet::Wallet(rai::Wallets& wallets)
    : wallets_(wallets),
      version_(rai::Wallet::VERSION_1),
      index_(0),
      selected_account_id_(0)
{
    rai::random_pool.GenerateBlock(salt_.bytes.data(), salt_.bytes.size());

    rai::RawKey key;
    rai::random_pool.GenerateBlock(key.data_.bytes.data(),
                                   key.data_.bytes.size());
    rai::RawKey password;
    fan_.Get(password);
    key_.Encrypt(key, password, salt_.owords[0]);

    rai::RawKey seed;
    rai::random_pool.GenerateBlock(seed.data_.bytes.data(),
                                   seed.data_.bytes.size());
    seed_.Encrypt(seed, key, salt_.owords[0]);

    rai::RawKey zero;
    zero.data_.Clear();
    check_.Encrypt(zero, key, salt_.owords[0]);

    selected_account_id_ = CreateAccount_();
}

rai::Wallet::Wallet(rai::Wallets& wallets, const rai::WalletInfo& info)
    : wallets_(wallets),
      version_(info.version_),
      index_(info.index_),
      selected_account_id_(info.selected_account_id_),
      salt_(info.salt_),
      key_(info.key_),
      seed_(info.seed_),
      check_(info.check_)
{
}

rai::Wallet::Wallet(rai::Wallets& wallets, const rai::RawKey& seed)
    : wallets_(wallets),
      version_(rai::Wallet::VERSION_1),
      index_(0),
      selected_account_id_(0)
{
    rai::random_pool.GenerateBlock(salt_.bytes.data(), salt_.bytes.size());

    rai::RawKey key;
    rai::random_pool.GenerateBlock(key.data_.bytes.data(),
                                   key.data_.bytes.size());
    rai::RawKey password;
    fan_.Get(password);
    key_.Encrypt(key, password, salt_.owords[0]);

    seed_.Encrypt(seed, key, salt_.owords[0]);

    rai::RawKey zero;
    zero.data_.Clear();
    check_.Encrypt(zero, key, salt_.owords[0]);

    selected_account_id_ = CreateAccount_();
}

rai::Account rai::Wallet::Account(uint32_t account_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& i : accounts_)
    {
        if (i.first == account_id)
        {
            return i.second.public_key_;
        }
    }

    return rai::Account(0);
}

rai::ErrorCode rai::Wallet::CreateAccount(uint32_t& account_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    account_id = CreateAccount_();
    return rai::ErrorCode::SUCCESS;
}

std::vector<std::pair<uint32_t, std::pair<rai::Account, bool>>>
rai::Wallet::Accounts() const
{
    std::vector<std::pair<uint32_t, std::pair<rai::Account, bool>>> result;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& i : accounts_)
    {
        bool is_adhoc = i.second.index_ == std::numeric_limits<uint32_t>::max();
        result.emplace_back(i.first,
                            std::make_pair(i.second.public_key_, is_adhoc));
    }

    return result;
}

bool rai::Wallet::AttemptPassword(const std::string& password)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rai::RawKey kdf;
    rai::Kdf::DeriveKey(kdf, password, salt_);
    fan_.Set(kdf);
    return !ValidPassword_();
}

bool rai::Wallet::ValidPassword() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return ValidPassword_();
}

bool rai::Wallet::EmptyPassword() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return false;
    }
    rai::RawKey password;
    fan_.Get(password);
    return password.data_.IsZero();
}

bool rai::Wallet::IsMyAccount(const rai::Account& account) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& i: accounts_)
    {
        if (account == i.second.public_key_)
        {
            return true;
        }
    }
    return false;
}

rai::ErrorCode rai::Wallet::ChangePassword(const std::string& password)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    rai::RawKey kdf;
    rai::Kdf::DeriveKey(kdf, password, salt_);

    rai::RawKey key = Key_();
    fan_.Set(kdf);
    key_.Encrypt(key, kdf, salt_.owords[0]);

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallet::ImportAccount(const rai::KeyPair& key_pair,
                                          uint32_t& account_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    for (const auto& i : accounts_)
    {
        if (i.second.public_key_ == key_pair.public_key_)
        {
            return rai::ErrorCode::WALLET_ACCOUNT_EXISTS;
        }
    }

    rai::WalletAccountInfo info;
    info.index_ = std::numeric_limits<uint32_t>::max();
    info.private_key_.Encrypt(key_pair.private_key_, Key_(), salt_.owords[0]);
    info.public_key_ = key_pair.public_key_;
    account_id       = NewAccountId_();
    accounts_.emplace_back(account_id, info);

    return rai::ErrorCode::SUCCESS;
}

void rai::Wallet::Lock()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return;
    }

    rai::RawKey zero;
    zero.data_.Clear();
    fan_.Set(zero);
}

rai::ErrorCode rai::Wallet::PrivateKey(const rai::Account& account,
                                       rai::RawKey& private_key) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    bool error = PrivateKey_(account, private_key);
    IF_ERROR_RETURN(error, rai::ErrorCode::WALLET_ACCOUNT_GET);

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallet::Seed(rai::RawKey& seed) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidPassword_())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    seed = Seed_();
    return rai::ErrorCode::SUCCESS;
}

bool rai::Wallet::Sign(const rai::Account& account,
                       const rai::uint256_union& message,
                       rai::Signature& signature) const
{
    rai::RawKey private_key;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ValidPassword_())
        {
            return true;
        }

        bool error = PrivateKey_(account, private_key);
        IF_ERROR_RETURN(error, true);
    }

    signature = rai::SignMessage(private_key, account, message);
    return false;
}

size_t rai::Wallet::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return accounts_.size();
}

rai::ErrorCode rai::Wallet::Store(rai::Transaction& transaction,
                                  uint32_t wallet_id)
{
    std::lock_guard<std::mutex> lock(mutex_);

    bool error =
        wallets_.ledger_.WalletInfoPut(transaction, wallet_id, Info_());
    IF_ERROR_RETURN(error, rai::ErrorCode::WALLET_INFO_PUT);
    for (const auto& i : accounts_)
    {
        error = wallets_.ledger_.WalletAccountInfoPut(transaction, wallet_id,
                                                      i.first, i.second);
        IF_ERROR_RETURN(error, rai::ErrorCode::WALLET_ACCOUNT_INFO_PUT);
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallet::StoreInfo(rai::Transaction& transaction,
                                  uint32_t wallet_id)
{
    std::lock_guard<std::mutex> lock(mutex_);

    bool error =
        wallets_.ledger_.WalletInfoPut(transaction, wallet_id, Info_());
    IF_ERROR_RETURN(error, rai::ErrorCode::WALLET_INFO_PUT);

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallet::StoreAccount(rai::Transaction& transaction,
                                         uint32_t wallet_id,
                                         uint32_t account_id)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& i : accounts_)
    {
        if (i.first == account_id)
        {
            bool error = wallets_.ledger_.WalletAccountInfoPut(
                transaction, wallet_id, i.first, i.second);
            if (!error)
            {
                return rai::ErrorCode::SUCCESS;
            }
            return rai::ErrorCode::WALLET_ACCOUNT_INFO_PUT;
        }
    }

    return rai::ErrorCode::WALLET_ACCOUNT_GET;
}

bool rai::Wallet::SelectAccount(uint32_t account_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (account_id == selected_account_id_)
    {
        return true;
    }

    for (const auto& i : accounts_)
    {
        if (account_id == i.first)
        {
            selected_account_id_ = account_id;
            return false;
        }
    }

    return true;
}


rai::Account rai::Wallet::SelectedAccount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& i : accounts_)
    {
        if (selected_account_id_ == i.first)
        {
            return i.second.public_key_;
        }
    }
    return rai::Account(0);
}

uint32_t rai::Wallet::SelectedAccountId() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return selected_account_id_;
}

rai::RawKey rai::Wallet::Decrypt_(const rai::uint256_union& encrypted) const
{
    rai::RawKey result;
    result.Decrypt(encrypted, Key_(), salt_.owords[0]);
    return result;
}

rai::RawKey rai::Wallet::Key_() const
{
    rai::RawKey password;
    fan_.Get(password);
    rai::RawKey key;
    key.Decrypt(key_, password, salt_.owords[0]);
    return key;
}

rai::RawKey rai::Wallet::Seed_() const
{
    rai::RawKey seed;
    seed.Decrypt(seed_, Key_(), salt_.owords[0]);
    return seed;
}

uint32_t rai::Wallet::CreateAccount_()
{
    blake2b_state hash;
    rai::RawKey private_key;
    rai::RawKey seed = Seed_();
    blake2b_init(&hash, private_key.data_.bytes.size());
    blake2b_update(&hash, seed.data_.bytes.data(), seed.data_.bytes.size());
    rai::uint256_union index(index_++);
    blake2b_update(&hash, reinterpret_cast<uint8_t*>(&index.dwords[7]),
                   sizeof(uint32_t));
    blake2b_final(&hash, private_key.data_.bytes.data(),
                  private_key.data_.bytes.size());
    rai::WalletAccountInfo info;
    info.index_ = index_ - 1;
    info.private_key_.Encrypt(private_key, Key_(), salt_.owords[0]);
    info.public_key_ = rai::GeneratePublicKey(private_key.data_);
    uint32_t account_id = NewAccountId_();
    accounts_.emplace_back(account_id, info);
    return account_id;
}

uint32_t rai::Wallet::NewAccountId_() const
{
    uint32_t result = 1;

    for (const auto& i : accounts_)
    {
        if (i.first >= result)
        {
            result = i.first + 1;
        }
    }

    return result;
}

bool rai::Wallet::ValidPassword_() const
{
    rai::RawKey zero;
    zero.data_.Clear();
    rai::uint256_union check;
    check.Encrypt(zero, Key_(), salt_.owords[0]);
    return check == check_;
}


rai::WalletInfo rai::Wallet::Info_() const
{
    rai::WalletInfo result;

    result.version_ = version_;
    result.index_ = index_;
    result.selected_account_id_ = selected_account_id_;
    result.salt_ = salt_;
    result.key_ = key_;
    result.seed_ = seed_;
    result.check_ = check_;

    return result;
}

bool rai::Wallet::PrivateKey_(const rai::Account& account,
                              rai::RawKey& private_key) const
{
    boost::optional<rai::uint256_union> encrypted_key(boost::none);
    for (const auto& i : accounts_)
    {
        if (i.second.public_key_ == account)
        {
            encrypted_key = i.second.private_key_;
        }
    }

    if (!encrypted_key)
    {
        return true;
    }
    private_key = Decrypt_(*encrypted_key);
    return false;
}

rai::Wallets::Wallets(rai::ErrorCode& error_code,
                      boost::asio::io_service& service, rai::Alarm& alarm,
                      const boost::filesystem::path& data_path,
                      const rai::WalletConfig& config,
                      rai::BlockType block_type, const rai::RawKey& seed,
                      uint32_t websockets)
    : service_(service),
      alarm_(alarm),
      config_(config),
      store_(error_code, data_path / "wallet_data.ldb"),
      ledger_(error_code, store_, rai::LedgerType::WALLET),
      service_runner_(service),
      block_type_(block_type),
      send_count_(0),
      last_sync_(0),
      last_sub_(0),
      time_synced_(false),
      server_time_(0),
      stopped_(false),
      selected_wallet_id_(0),
      thread_([this]() { this->Run(); })
{
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    if (config_.preconfigured_reps_.empty())
    {
        error_code = rai::ErrorCode::JSON_CONFIG_WALLET_PRECONFIGURED_REP;
        return;
    }

    for (uint32_t count = 0; count < websockets; ++count)
    {
        auto websocket = std::make_shared<rai::WebsocketClient>(
            service_, config.server_.host_, config.server_.port_,
            config.server_.path_, config.server_.protocol_ == "wss");
        websockets_.push_back(websocket);
    }

    rai::Transaction transaction(error_code, ledger_, true);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);
    std::vector<std::pair<uint32_t, rai::WalletInfo>> infos;
    bool error = ledger_.WalletInfoGetAll(transaction, infos);
    if (!error && !infos.empty())
    {
        for (const auto& info : infos)
        {
            auto wallet = std::make_shared<rai::Wallet>(*this, info.second);
            error = ledger_.WalletAccountInfoGetAll(transaction, info.first,
                                                    wallet->accounts_);
            if (error)
            {
                throw std::runtime_error("WalletAccountInfoGetAll error");
            }
            wallets_.emplace_back(info.first, wallet);
        }
    }
    else
    {
        std::shared_ptr<rai::Wallet> wallet(nullptr);
        if (seed.data_.IsZero())
        {
            wallet = std::make_shared<rai::Wallet>(*this);
        }
        else
        {
            wallet = std::make_shared<rai::Wallet>(*this, seed);
        }
        uint32_t wallet_id = 1;
        error_code = wallet->Store(transaction, wallet_id);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            transaction.Abort();
            throw std::runtime_error("Wallet store error");
            return;
        }
        wallets_.emplace_back(wallet_id, wallet);
    }

    error = ledger_.SelectedWalletIdGet(transaction, selected_wallet_id_);
    if (error)
    {
        selected_wallet_id_ = wallets_[0].first;
        error = ledger_.SelectedWalletIdPut(transaction, selected_wallet_id_);
        if (error)
        {
            throw std::runtime_error("SelectedWalletIdPut error");
            return;
        }
    }
}

rai::Wallets::~Wallets()
{
    Stop();
}

std::shared_ptr<rai::Wallets> rai::Wallets::Shared()
{
    return shared_from_this();
}

void rai::Wallets::AccountBalance(const rai::Account& account,
                                  rai::Amount& balance)
{
    balance.Clear();
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN_VOID(error);

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        //log
        return;
    }
    balance = head->Balance();
}

void rai::Wallets::AccountBalanceConfirmed(const rai::Account& account,
                                           rai::Amount& confirmed)
{
    confirmed.Clear();

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN_VOID(error);

    if (info.confirmed_height_ != rai::Block::INVALID_HEIGHT)
    {
        std::shared_ptr<rai::Block> confirmed_block(nullptr);
        error = ledger_.BlockGet(transaction, account, info.confirmed_height_,
                                 confirmed_block);
        if (error || confirmed_block == nullptr)
        {
            // log
            return;
        }
        confirmed = confirmed_block->Balance();
    }
}

void rai::Wallets::AccountBalanceAll(const rai::Account& account,
                                     rai::Amount& balance,
                                     rai::Amount& confirmed,
                                     rai::Amount& receivable)
{
    balance.Clear();
    confirmed.Clear();
    receivable.Clear();

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    rai::Amount sum(0);
    rai::Iterator i = ledger_.ReceivableInfoLowerBound(transaction, account);
    rai::Iterator n = ledger_.ReceivableInfoUpperBound(transaction, account);
    for (; i != n; ++i)
    {
        rai::Account account_l;
        rai::BlockHash hash;
        rai::ReceivableInfo info;
        bool error = ledger_.ReceivableInfoGet(i, account_l, hash, info);
        if (error)
        {
            // log
            return;
        }
        sum += info.amount_;
    }
    receivable = sum;

    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN_VOID(error);

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        //log
        return;
    }
    balance = head->Balance();

    if (info.confirmed_height_ == info.head_height_)
    {
        confirmed = balance;
    }
    else if (info.confirmed_height_ != rai::Block::INVALID_HEIGHT)
    {
        std::shared_ptr<rai::Block> confirmed_block(nullptr);
        error = ledger_.BlockGet(transaction, account, info.confirmed_height_,
                                 confirmed_block);
        if (error || confirmed_block == nullptr)
        {
            //log
            return;
        }
        confirmed = confirmed_block->Balance();
    }
    else
    {
        /* do nothing */
    }
}

bool rai::Wallets::AccountRepresentative(const rai::Account& account,
                                         rai::Account& rep)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN(error, true);

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        return true;
    }

    if (!head->HasRepresentative())
    {
        return true;
    }

    rep = head->Representative();
    return false;
}

void rai::Wallets::AccountTransactionsLimit(const rai::Account& account,
                                            uint32_t& total, uint32_t& used)
{
    total = 0;
    used = 0;
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN_VOID(error);

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        return;
    }

    uint64_t now = 0;
    error = CurrentTimestamp(now);
    IF_ERROR_RETURN_VOID(error);

    total = head->Credit() * rai::TRANSACTIONS_PER_CREDIT;
    if (rai::SameDay(head->Timestamp(), now))
    {
        used = head->Counter();
    }
    else
    {
        used = 0;
    }
}

void rai::Wallets::AccountInfoQuery(const rai::Account& account)
{
    rai::Ptree ptree;
    ptree.put("action", "account_info");
    ptree.put("account", account.StringAccount());
    Send(ptree);
}

void rai::Wallets::AccountForksQuery(const rai::Account& account)
{
    rai::Ptree ptree;
    ptree.put("action", "account_forks");
    ptree.put("account", account.StringAccount());
    Send(ptree);
}

rai::ErrorCode rai::Wallets::AccountChange(
    const rai::Account& rep, const rai::AccountActionCallback& callback)
{
    return AccountChange(rep, std::vector<uint8_t>(), callback);
}

rai::ErrorCode rai::Wallets::AccountChange(
    const rai::Account& rep, const std::vector<uint8_t>& extensions,
    const rai::AccountActionCallback& callback)
{
    rai::ErrorCode error_code = ActionCommonCheck_();
    IF_NOT_SUCCESS_RETURN(error_code);

    auto wallet = SelectedWallet();
    rai::Account account = wallet->SelectedAccount();

    std::weak_ptr<rai::Wallets> this_w(Shared());
    QueueAction(rai::WalletActionPri::HIGH,
                [this_w, wallet, account, rep, extensions, callback]() {
                    if (auto this_s = this_w.lock())
                    {
                        this_s->ProcessAccountChange(wallet, account, rep,
                                                     extensions, callback);
                    }
                });
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::AccountCredit(
    uint16_t credit, const rai::AccountActionCallback& callback)
{
    rai::ErrorCode error_code = ActionCommonCheck_();
    IF_NOT_SUCCESS_RETURN(error_code);

    auto wallet = SelectedWallet();
    rai::Account account = wallet->SelectedAccount();

    std::weak_ptr<rai::Wallets> this_w(Shared());
    QueueAction(rai::WalletActionPri::HIGH, [this_w, wallet, account, credit,
                                             callback]() {
        if (auto this_s = this_w.lock())
        {
            this_s->ProcessAccountCredit(wallet, account, credit, callback);
        }
    });
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::AccountDestroy(
    const rai::AccountActionCallback& callback)
{
    rai::ErrorCode error_code = ActionCommonCheck_();
    IF_NOT_SUCCESS_RETURN(error_code);

    auto wallet = SelectedWallet();
    rai::Account account = wallet->SelectedAccount();

    std::weak_ptr<rai::Wallets> this_w(Shared());
    QueueAction(rai::WalletActionPri::HIGH, [this_w, wallet, account, 
                                             callback]() {
        if (auto this_s = this_w.lock())
        {
            this_s->ProcessAccountDestroy(wallet, account, callback);
        }
    });
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::AccountSend(
    const rai::Account& destination, const rai::Amount& amount,
    const rai::AccountActionCallback& callback)
{
    return AccountSend(destination, amount, std::vector<uint8_t>(), callback);
}

rai::ErrorCode rai::Wallets::AccountSend(
    const rai::Account& destination, const rai::Amount& amount,
    const std::vector<uint8_t>& extensions,
    const rai::AccountActionCallback& callback)
{
    rai::ErrorCode error_code = ActionCommonCheck_();
    IF_NOT_SUCCESS_RETURN(error_code);

    auto wallet = SelectedWallet();
    rai::Account account = wallet->SelectedAccount();

    std::weak_ptr<rai::Wallets> this_w(Shared());
    QueueAction(
        rai::WalletActionPri::HIGH,
        [this_w, wallet, account, destination, amount, extensions, callback]() {
            if (auto this_s = this_w.lock())
            {
                this_s->ProcessAccountSend(wallet, account, destination, amount,
                                           extensions, callback);
            }
        });
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::AccountReceive(
    const rai::Account& account, const rai::BlockHash& hash,
    const rai::AccountActionCallback& callback)
{
    rai::ErrorCode error_code = ActionCommonCheck_();
    IF_NOT_SUCCESS_RETURN(error_code);

    auto wallet = SelectedWallet();
    if (account != wallet->SelectedAccount())
    {
        return rai::ErrorCode::WALLET_NOT_SELECTED_ACCOUNT;
    }

    std::weak_ptr<rai::Wallets> this_w(Shared());
    QueueAction(rai::WalletActionPri::HIGH,
                [this_w, wallet, account, hash, callback]() {
                    if (auto this_s = this_w.lock())
                    {
                        this_s->ProcessAccountReceive(wallet, account, hash,
                                                      callback);
                    }
                });
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::AccountActionPreCheck(
    const std::shared_ptr<rai::Wallet>& wallet, const rai::Account& account)
{
    if (!wallet->ValidPassword())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    if (!wallet->IsMyAccount(account))
    {
        return rai::ErrorCode::WALLET_ACCOUNT_GET;
    }

    if (!Synced(account))
    {
        return rai::ErrorCode::WALLET_ACCOUNT_IN_SYNC;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::AccountReceive(
    const std::shared_ptr<rai::Wallet>& wallet, const rai::Account& account,
    const rai::BlockHash& source, const rai::AccountActionCallback& callback)
{
    rai::ErrorCode error_code = AccountActionPreCheck(wallet, account);
    IF_NOT_SUCCESS_RETURN(error_code);

    std::weak_ptr<rai::Wallets> this_w(Shared());
    QueueAction(rai::WalletActionPri::HIGH, [this_w, wallet, account, source,
                                             callback]() {
        if (auto this_s = this_w.lock())
        {
            this_s->ProcessAccountReceive(wallet, account, source, callback);
        }
    });
    return rai::ErrorCode::SUCCESS;
}

void rai::Wallets::BlockQuery(const rai::Account& account, uint64_t height,
                              const rai::BlockHash& previous)
{
    rai::Ptree ptree;
    ptree.put("action", "block_query");
    ptree.put("account", account.StringAccount());
    ptree.put("height", std::to_string(height));
    ptree.put("previous", previous.StringHex());
    ptree.put("request_id", account.StringAccount());
    Send(ptree);
}

void rai::Wallets::BlockPublish(const std::shared_ptr<rai::Block>& block)
{
    rai::Ptree ptree;
    ptree.put("action", "block_publish");
    rai::Ptree block_ptree;
    block->SerializeJson(block_ptree);
    ptree.put_child("block", block_ptree);
    Send(ptree);
}

void rai::Wallets::ConnectToServer()
{
    for (const auto& i : websockets_)
    {
        i->Run();
        service_runner_.Notify();
    }
}

bool rai::Wallets::Connected() const
{
    for (const auto& i : websockets_)
    {
        if (i->Status() == rai::WebsocketStatus::CONNECTED)
        {
            return true;
        }
    }

    return false;
}

rai::ErrorCode rai::Wallets::ChangePassword(const std::string& password)
{
    bool empty_password = false;
    {
        // construct writing transaction first to avoid deadlock
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return error_code;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto wallet = Wallet_(selected_wallet_id_);
        if (wallet == nullptr)
        {
            return rai::ErrorCode::WALLET_GET;
        }
        empty_password = wallet->EmptyPassword();

        error_code = wallet->ChangePassword(password);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = wallet->StoreInfo(transaction, selected_wallet_id_);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            transaction.Abort();
            return error_code;
        }
    }

    if (empty_password && wallet_password_set_observer_)
    {
        wallet_password_set_observer_();
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::CreateAccount()
{
    uint32_t ignore;
    return CreateAccount(ignore);
}

rai::ErrorCode rai::Wallets::CreateAccount(uint32_t& account_id)
{
    account_id = 0;
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        // construct writing transaction before mutex to avoid dead lock
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return error_code;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(selected_wallet_id_);
        if (wallet == nullptr)
        {
            // log
            return rai::ErrorCode::WALLET_GET;
        }

        
        error_code = wallet->CreateAccount(account_id);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code =
            wallet->StoreAccount(transaction, selected_wallet_id_, account_id);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            transaction.Abort();
            return error_code;
        }
        error_code = wallet->StoreInfo(transaction, selected_wallet_id_);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            transaction.Abort();
            return error_code;
        }
    }

    Subscribe(wallet, account_id);
    Sync(wallet->Account(account_id));
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::CreateWallet()
{
    auto wallet = std::make_shared<rai::Wallet>(*this);
    {
        // construct writing transaction first to avoid deadlock
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return error_code;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t wallet_id = NewWalletId_();
        if (wallet_id == 0)
        {
            return rai::ErrorCode::GENERIC;
        }

        error_code = wallet->Store(transaction, wallet_id);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            transaction.Abort();
            return error_code;
        }
        wallets_.emplace_back(wallet_id, wallet);
    }

    Subscribe(wallet);
    Sync(wallet);
    return rai::ErrorCode::SUCCESS;
}

bool rai::Wallets::CurrentTimestamp(uint64_t& timestamp) const
{
    using std::chrono::duration_cast;
    using std::chrono::seconds;

    std::lock_guard<std::mutex> lock(mutex_timesync_);
    if (!time_synced_)
    {
        return true;
    }

    auto now = std::chrono::steady_clock::now();
    uint64_t diff = duration_cast<seconds>(now - local_time_).count();
    timestamp = server_time_ + diff;
    return false;
}

bool rai::Wallets::EnterPassword(const std::string& password)
{
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(selected_wallet_id_);
        if (wallet == nullptr)
        {
            return true;
        }
    }

    bool error = wallet->AttemptPassword(password);
    IF_ERROR_RETURN(error, true);

    if (wallet_locked_observer_)
    {
        wallet_locked_observer_(false);
    }

    return false;
}

rai::ErrorCode rai::Wallets::ImportAccount(const rai::KeyPair& key_pair)
{
    // construct writing transaction first to avoid deadlock
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, true);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return error_code;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto wallet = Wallet_(selected_wallet_id_);
    uint32_t account_id = 0;
    error_code = wallet->ImportAccount(key_pair, account_id);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        wallet->StoreAccount(transaction, selected_wallet_id_, account_id);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Wallets::ImportWallet(const rai::RawKey& seed,
                                          uint32_t& wallet_id)
{
    auto wallet = std::make_shared<rai::Wallet>(*this, seed);
    {
        // construct writing transaction first to avoid deadlock
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return error_code;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        rai::Account account = wallet->SelectedAccount();
        for (const auto& i : wallets_)
        {
            if (i.second->IsMyAccount(account))
            {
                wallet_id = i.first;
                return rai::ErrorCode::WALLET_EXISTS;
            }
        }

        wallet_id = NewWalletId_();
        if (wallet_id == 0)
        {
            return rai::ErrorCode::GENERIC;
        }

        error_code = wallet->Store(transaction, wallet_id);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            transaction.Abort();
            return error_code;
        }
        wallets_.emplace_back(wallet_id, wallet);
    }

    Subscribe(wallet);
    Sync(wallet);
    return rai::ErrorCode::SUCCESS;
}

bool rai::Wallets::IsMyAccount(const rai::Account& account) const
{
    std::unique_lock<std::mutex> lock(mutex_);
    for (const auto& i : wallets_)
    {
        if (i.second->IsMyAccount(account))
        {
            return true;
        }
    }
    return false;
}

void rai::Wallets::LockWallet(uint32_t wallet_id)
{
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(wallet_id);
        if (wallet == nullptr)
        {
            return;
        }
    }

    wallet->Lock();
    if (wallet_locked_observer_)
    {
        wallet_locked_observer_(true);
    }
}

void rai::Wallets::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        if (actions_.empty())
        {
            condition_.wait(lock);
            continue;
        }

        auto front = actions_.begin();
        auto action(std::move(front->second));
        actions_.erase(front);

        lock.unlock();
        action();
        lock.lock();
    }
}

void rai::Wallets::Ongoing(const std::function<void()>& process,
                           const std::chrono::seconds& delay)
{
    process();
    std::weak_ptr<rai::Wallets> wallets(Shared());
    alarm_.Add(std::chrono::steady_clock::now() + delay,
               [wallets, process, delay]() {
                   auto wallets_l = wallets.lock();
                   if (wallets_l)
                   {
                       wallets_l->Ongoing(process, delay);
                   }
               });
}

void rai::Wallets::ProcessAccountInfo(const rai::Account& account,
                                      const rai::AccountInfo& info)
{
    if (!IsMyAccount(account))
    {
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    rai::AccountInfo info_l;
    bool error = ledger_.AccountInfoGet(transaction, account, info_l);
    if (error || !info_l.Valid())
    {
        return;
    }

    if (info_l.head_height_ <= info.head_height_
        && info.head_height_ != rai::Block::INVALID_HEIGHT)
    {
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::BlockHash successor;
    if (info.head_height_ == rai::Block::INVALID_HEIGHT)
    {
        error = ledger_.BlockGet(transaction, info_l.tail_, block, successor);
        if (error || block == nullptr)
        {
            return;
        }
        BlockPublish(block);
    }
    else
    {
        error = ledger_.BlockGet(transaction, info.head_, block, successor);
        if (error || block == nullptr)
        {
            return;
        }
    }

    while (block->Height() < info_l.head_height_)
    {
        error = ledger_.BlockGet(transaction, successor, block, successor);
        if (error)
        {
            // log
            std::cout << "ProcessAccountInfo::BlockGet error(account="
                      << account.StringAccount()
                      << ",hash=" << successor.StringHex() << ")\n";
            return;
        }
        BlockPublish(block);
    }
}

void rai::Wallets::ProcessAccountForks(
    const rai::Account& account,
    const std::vector<std::pair<std::shared_ptr<rai::Block>,
                                std::shared_ptr<rai::Block>>>& forks)
{
    if (!IsMyAccount(account))
    {
        return;
    }

    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            return;
        }

        rai::AccountInfo info;
        bool error = ledger_.AccountInfoGet(transaction, account, info);
        if (error || !info.Valid())
        {
            return;
        }

        if (forks.size() == 0 && info.forks_ == 0)
        {
            return;
        }

        std::vector<uint64_t> heights;
        for (auto const& i : forks)
        {
            if (!i.first->ForkWith(*i.second) || i.first->Account() != account)
            {
                return;
            }
            uint64_t height = i.first->Height();
            if (std::find(heights.begin(), heights.end(), height)
                != heights.end())
            {
                return;
            }
            heights.push_back(height);
        }

        std::vector<uint64_t> heights_l;
        if (info.forks_ > 0)
        {
            rai::Iterator i = ledger_.ForkLowerBound(transaction, account);
            rai::Iterator n = ledger_.ForkUpperBound(transaction, account);
            for (; i != n; ++i)
            {
                std::shared_ptr<rai::Block> first(nullptr);
                std::shared_ptr<rai::Block> second(nullptr);
                error = ledger_.ForkGet(i, first, second);
                if (error || first->Account() != account)
                {
                    transaction.Abort();
                    std::cout
                        << "Wallets::ProcessAccountForks: fork get, account="
                        << account.StringAccount() << std::endl;
                    return;
                }
                heights_l.push_back(first->Height());
            }
        }

        if (heights_l.size() == heights.size())
        {
            bool same = true;
            for (const auto& i : heights_l)
            {
                if (std::find(heights.begin(), heights.end(), i)
                    == heights.end())
                {
                    same = false;
                    break;
                }
            }

            if (same)
            {
                return;
            }
        }

        for (const auto& i : heights_l)
        {
            error = ledger_.ForkDel(transaction, account, i);
            if (error)
            {
                transaction.Abort();
                std::cout
                    << "Wallets::ProcessAccountForks: fork delete, account="
                    << account.StringAccount() << ", height=" << i << std::endl;
                return;
            }
        }

        for (const auto& i : forks)
        {
            error = ledger_.ForkPut(transaction, account, i.first->Height(),
                                    *i.first, *i.second);
            if (error)
            {
                transaction.Abort();
                std::cout << "Wallets::ProcessAccountForks: fork put, account="
                          << account.StringAccount()
                          << ", height=" << i.first->Height() << std::endl;
                return;
            }
        }

        info.forks_ = forks.size();
        error       = ledger_.AccountInfoPut(transaction, account, info);
        if (error)
        {
            transaction.Abort();
            std::cout
                << "Wallets::ProcessAccountForks: account info put, account="
                << account.StringAccount() << std::endl;
            return;
        }
    }

    if (fork_observer_)
    {
        fork_observer_(account);
    }
}

void rai::Wallets::ProcessAccountChange(
    const std::shared_ptr<rai::Wallet>& wallet, const rai::Account& account,
    const rai::Account& rep, const std::vector<uint8_t>& extensions,
    const rai::AccountActionCallback& callback)
{
    std::shared_ptr<rai::Block> block(nullptr);
    if (!wallet->ValidPassword())
    {
        callback(rai::ErrorCode::WALLET_LOCKED, block);
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    rai::AccountInfo info;
    bool error =
        ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        callback(rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET, block);
        return;
    }

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        callback(rai::ErrorCode::LEDGER_BLOCK_GET, block);
        return;
    }
    rai::Account rep_l = rep.IsZero() ? head->Representative() : rep;

    rai::BlockOpcode opcode = rai::BlockOpcode::CHANGE;
    uint16_t credit = head->Credit();

    uint64_t now = 0;
    error = CurrentTimestamp(now);
    if (error)
    {
        callback(rai::ErrorCode::WALLET_TIME_SYNC, block);
        return;
    }

    uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
    if (timestamp > now + 60)
    {
        callback(rai::ErrorCode::BLOCK_TIMESTAMP, block);
        return;
    }
    if (info.forks_ > rai::MaxAllowedForks(timestamp, credit))
    {
        callback(rai::ErrorCode::ACCOUNT_RESTRICTED, block);
        return;
    }

    uint32_t counter =
        rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        callback(rai::ErrorCode::ACCOUNT_ACTION_CREDIT, block);
        return;
    }
    uint64_t height = head->Height() + 1;
    rai::BlockHash previous = info.head_;
    rai::Amount balance = head->Balance();
    rai::uint256_union link(0);

    if (extensions.size() > rai::TxBlock::MaxExtensionsLength())
    {
        callback(rai::ErrorCode::EXTENSIONS_LENGTH, block);
        return;
    }

    rai::Ptree extensions_ptree;
    if (rai::ExtensionsToPtree(extensions, extensions_ptree))
    {
        callback(rai::ErrorCode::UNEXPECTED, block);
        return;
    }
    uint32_t extensions_length = static_cast<uint32_t>(extensions.size());

    rai::RawKey private_key;
    error_code = wallet->PrivateKey(account, private_key);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    if (info.type_ == rai::BlockType::TX_BLOCK)
    {
        block = std::make_shared<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account, previous,
            rep_l, balance, link, extensions_length, extensions, private_key,
            account);
    }
    else if (info.type_ == rai::BlockType::AD_BLOCK)
    {
        block = std::make_shared<rai::AdBlock>(
            opcode, credit, counter, timestamp, height, account, previous,
            rep_l, balance, link, private_key, account);
    }
    else
    {
        callback(rai::ErrorCode::BLOCK_TYPE, block);
        return;
    }

    ProcessBlock(block, false);
    BlockPublish(block);
    callback(rai::ErrorCode::SUCCESS, block);
}

void rai::Wallets::ProcessAccountCredit(
    const std::shared_ptr<rai::Wallet>& wallet, const rai::Account& account,
    uint16_t credit_inc, const rai::AccountActionCallback& callback)
{
    std::shared_ptr<rai::Block> block(nullptr);
    if (!wallet->ValidPassword())
    {
        callback(rai::ErrorCode::WALLET_LOCKED, block);
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    rai::AccountInfo info;
    bool error =
        ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        callback(rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET, block);
        return;
    }

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        callback(rai::ErrorCode::LEDGER_BLOCK_GET, block);
        return;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::CREDIT;

    uint16_t credit = head->Credit() + credit_inc;
    if (credit <= head->Credit())
    {
        callback(rai::ErrorCode::ACCOUNT_MAX_CREDIT, block);
        return;
    }

    uint64_t now = 0;
    error = CurrentTimestamp(now);
    if (error)
    {
        callback(rai::ErrorCode::WALLET_TIME_SYNC, block);
        return;
    }

    uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
    if (timestamp > now + 60)
    {
        callback(rai::ErrorCode::BLOCK_TIMESTAMP, block);
        return;
    }

    uint32_t counter =
        rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        callback(rai::ErrorCode::ACCOUNT_ACTION_CREDIT, block);
        return;
    }

    uint64_t height = head->Height() + 1;
    rai::BlockHash previous = info.head_;

    rai::Amount cost(rai::CreditPrice(timestamp).Number() * credit_inc);
    if (cost > head->Balance())
    {
        callback(rai::ErrorCode::ACCOUNT_ACTION_BALANCE, block);
        return;
    }
    rai::Amount balance = head->Balance() - cost;

    rai::uint256_union link(0);
    rai::RawKey private_key;
    error_code = wallet->PrivateKey(account, private_key);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    if (info.type_ == rai::BlockType::TX_BLOCK)
    {
        block = std::make_shared<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account, previous,
            head->Representative(), balance, link, 0, std::vector<uint8_t>(),
            private_key, account);
    }
    else
    {
        callback(rai::ErrorCode::BLOCK_TYPE, block);
        return;
    }
    
    ProcessBlock(block, false);
    BlockPublish(block);
    callback(rai::ErrorCode::SUCCESS, block);
}

void rai::Wallets::ProcessAccountDestroy(
    const std::shared_ptr<rai::Wallet>& wallet, const rai::Account& account,
    const rai::AccountActionCallback& callback)
{
    std::shared_ptr<rai::Block> block(nullptr);
    if (!wallet->ValidPassword())
    {
        callback(rai::ErrorCode::WALLET_LOCKED, block);
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    rai::AccountInfo info;
    bool error =
        ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        callback(rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET, block);
        return;
    }

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        callback(rai::ErrorCode::LEDGER_BLOCK_GET, block);
        return;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::DESTROY;
    uint16_t credit = head->Credit();

    uint64_t now = 0;
    error = CurrentTimestamp(now);
    if (error)
    {
        callback(rai::ErrorCode::WALLET_TIME_SYNC, block);
        return;
    }

    uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
    if (timestamp > now + 60)
    {
        callback(rai::ErrorCode::BLOCK_TIMESTAMP, block);
        return;
    }
    if (info.forks_ > rai::MaxAllowedForks(timestamp, credit))
    {
        callback(rai::ErrorCode::ACCOUNT_RESTRICTED, block);
        return;
    }

    uint32_t counter =
        rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        callback(rai::ErrorCode::ACCOUNT_ACTION_CREDIT, block);
        return;
    }

    uint64_t height = head->Height() + 1;
    rai::BlockHash previous = info.head_;
    rai::Amount balance = 0;
    rai::uint256_union link(0);
    rai::RawKey private_key;
    error_code = wallet->PrivateKey(account, private_key);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    if (info.type_ == rai::BlockType::AD_BLOCK)
    {
        block = std::make_shared<rai::AdBlock>(
            opcode, credit, counter, timestamp, height, account, previous,
            head->Representative(), balance, link, private_key, account);
    }
    else
    {
        callback(rai::ErrorCode::BLOCK_TYPE, block);
        return;
    }

    ProcessBlock(block, false);
    BlockPublish(block);
    callback(rai::ErrorCode::SUCCESS, block);
}

void rai::Wallets::ProcessAccountSend(
    const std::shared_ptr<rai::Wallet>& wallet, const rai::Account& account,
    const rai::Account& destination, const rai::Amount& amount,
    const std::vector<uint8_t>& extensions,
    const rai::AccountActionCallback& callback)
{
    std::shared_ptr<rai::Block> block(nullptr);
    if (!wallet->ValidPassword())
    {
        callback(rai::ErrorCode::WALLET_LOCKED, block);
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    rai::AccountInfo info;
    bool error =
        ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        callback(rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET, block);
        return;
    }

    std::shared_ptr<rai::Block> head(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, head);
    if (error || head == nullptr)
    {
        callback(rai::ErrorCode::LEDGER_BLOCK_GET, block);
        return;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::SEND;
    uint16_t credit = head->Credit();

    uint64_t now = 0;
    error = CurrentTimestamp(now);
    if (error)
    {
        callback(rai::ErrorCode::WALLET_TIME_SYNC, block);
        return;
    }

    uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
    if (timestamp > now + 60)
    {
        callback(rai::ErrorCode::BLOCK_TIMESTAMP, block);
        return;
    }
    if (info.forks_ > rai::MaxAllowedForks(timestamp, credit))
    {
        callback(rai::ErrorCode::ACCOUNT_RESTRICTED, block);
        return;
    }
    
    uint32_t counter =
        rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        callback(rai::ErrorCode::ACCOUNT_ACTION_CREDIT, block);
        return;
    }
    uint64_t height = head->Height() + 1;
    rai::BlockHash previous = info.head_;
    if (head->Balance() < amount)
    {
        callback(rai::ErrorCode::ACCOUNT_ACTION_BALANCE, block);
        return;
    }
    rai::Amount balance = head->Balance() - amount;
    rai::uint256_union link = destination;

    if (extensions.size() > rai::TxBlock::MaxExtensionsLength())
    {
        callback(rai::ErrorCode::EXTENSIONS_LENGTH, block);
        return;
    }

    rai::Ptree extensions_ptree;
    if (rai::ExtensionsToPtree(extensions, extensions_ptree))
    {
        callback(rai::ErrorCode::UNEXPECTED, block);
        return;
    }
    uint32_t extensions_length = static_cast<uint32_t>(extensions.size());

    rai::RawKey private_key;
    error_code = wallet->PrivateKey(account, private_key);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    if (info.type_ == rai::BlockType::TX_BLOCK)
    {
        block = std::make_shared<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account, previous,
            head->Representative(), balance, link, extensions_length,
            extensions, private_key, account);
    }
    else
    {
        callback(rai::ErrorCode::BLOCK_TYPE, block);
        return;
    }
    
    ProcessBlock(block, false);
    BlockPublish(block);
    callback(rai::ErrorCode::SUCCESS, block);
}

void rai::Wallets::ProcessAccountReceive(
    const std::shared_ptr<rai::Wallet>& wallet, const rai::Account& account,
    const rai::BlockHash& hash, const rai::AccountActionCallback& callback)
{
    std::shared_ptr<rai::Block> block(nullptr);
    if (!wallet->ValidPassword())
    {
        callback(rai::ErrorCode::WALLET_LOCKED, block);
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        callback(error_code, block);
        return;
    }

    rai::ReceivableInfo receivable_info;
    bool error =
        ledger_.ReceivableInfoGet(transaction, account, hash, receivable_info);
    if (error)
    {
        callback(rai::ErrorCode::LEDGER_RECEIVABLE_INFO_GET, block);
        return;
    }

    rai::AccountInfo account_info;
    error =
        ledger_.AccountInfoGet(transaction, account, account_info);
    if (error || !account_info.Valid())
    {
        rai::BlockOpcode opcode = rai::BlockOpcode::RECEIVE;
        uint16_t credit = 1;
        uint64_t now = 0;
        error = CurrentTimestamp(now);
        if (error)
        {
            callback(rai::ErrorCode::WALLET_TIME_SYNC, block);
            return;
        }
        uint64_t timestamp = now <= receivable_info.timestamp_
                                 ? receivable_info.timestamp_
                                 : now;
        if (timestamp > now + 60)
        {
            callback(rai::ErrorCode::ACCOUNT_ACTION_TOO_QUICKLY, block);
            return;
        }
        uint32_t counter = 1;
        uint64_t height = 0;
        rai::BlockHash previous(0);
        if (receivable_info.amount_ < rai::CreditPrice(timestamp))
        {
            callback(rai::ErrorCode::WALLET_RECEIVABLE_LESS_THAN_CREDIT, block);
            return;
        }
        rai::Amount balance =
            receivable_info.amount_ - rai::CreditPrice(timestamp);
        rai::uint256_union link = hash;
        rai::Account rep = RandomRepresentative();
        rai::RawKey private_key;
        error_code = wallet->PrivateKey(account, private_key);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            callback(error_code, block);
            return;
        }

        if (block_type_ == rai::BlockType::TX_BLOCK)
        {
            block = std::make_shared<rai::TxBlock>(
                opcode, credit, counter, timestamp, height, account, previous,
                rep, balance, link, 0, std::vector<uint8_t>(),
                private_key, account);
        }
        else if (block_type_ == rai::BlockType::AD_BLOCK)
        {
            block = std::make_shared<rai::AdBlock>(
                opcode, credit, counter, timestamp, height, account, previous,
                rep, balance, link, private_key, account);
        }
        else
        {
            callback(rai::ErrorCode::BLOCK_TYPE, block);
            return;
        }
    }
    else
    {
        std::shared_ptr<rai::Block> head(nullptr);
        error = ledger_.BlockGet(transaction, account_info.head_, head);
        if (error || head == nullptr)
        {
            callback(rai::ErrorCode::LEDGER_BLOCK_GET, block);
            return;
        }

        rai::BlockOpcode opcode = rai::BlockOpcode::RECEIVE;
        uint16_t credit = head->Credit();

        uint64_t now = 0;
        error = CurrentTimestamp(now);
        if (error)
        {
            callback(rai::ErrorCode::WALLET_TIME_SYNC, block);
            return;
        }
        uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
        if (timestamp < receivable_info.timestamp_)
        {
            timestamp = receivable_info.timestamp_;
        }
        if (timestamp > now + 60)
        {
            callback(rai::ErrorCode::BLOCK_TIMESTAMP, block);
            return;
        }
        
        if (account_info.forks_ > rai::MaxAllowedForks(timestamp, credit))
        {
            callback(rai::ErrorCode::ACCOUNT_RESTRICTED, block);
            return;
        }

        uint32_t counter =
            rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
        if (counter
            > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
        {
            callback(rai::ErrorCode::ACCOUNT_ACTION_CREDIT, block);
            return;
        }
        uint64_t height = head->Height() + 1;
        rai::BlockHash previous = account_info.head_;
        rai::Amount balance = head->Balance() + receivable_info.amount_;
        rai::uint256_union link = hash;
        rai::RawKey private_key;
        error_code = wallet->PrivateKey(account, private_key);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            callback(error_code, block);
            return;
        }

        if (account_info.type_ == rai::BlockType::TX_BLOCK)
        {
            block = std::make_shared<rai::TxBlock>(
                opcode, credit, counter, timestamp, height, account, previous,
                head->Representative(), balance, link, 0, std::vector<uint8_t>(),
                private_key, account);
        }
        else if (account_info.type_ == rai::BlockType::AD_BLOCK)
        {
            block = std::make_shared<rai::AdBlock>(
                opcode, credit, counter, timestamp, height, account, previous,
                head->Representative(), balance, link, private_key, account);
        }
        else
        {
            callback(rai::ErrorCode::BLOCK_TYPE, block);
            return;
        }
    }

    ProcessBlock(block, false);
    BlockPublish(block);
    callback(rai::ErrorCode::SUCCESS, block);
}

void rai::Wallets::ProcessBlock(const std::shared_ptr<rai::Block>& block,
                                 bool confirmed)
{
    if (!IsMyAccount(block->Account()))
    {
        return;
    }

    do
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return;
        }

        rai::AccountInfo info;
        bool error =
            ledger_.AccountInfoGet(transaction, block->Account(), info);
        bool account_exist = !error && info.Valid();
        if (!account_exist)
        {
            if (block->Height() != 0)
            {
                return;
            }
            rai::AccountInfo new_info(block->Type(), block->Hash());
            if (confirmed)
            {
                new_info.confirmed_height_ = 0;
            }
            error =
                ledger_.AccountInfoPut(transaction, block->Account(), new_info);
            if (error)
            {
                transaction.Abort();
                // log
                return;
            }
            error = ledger_.BlockPut(transaction, block->Hash(), *block);
            if (error)
            {
                transaction.Abort();
                // log
                return;
            }

            if (block->Opcode() == rai::BlockOpcode::RECEIVE)
            {
                ledger_.ReceivableInfoDel(transaction, block->Account(),
                                          block->Link());
                if (!ledger_.SourceExists(transaction, block->Link()))
                {
                    ledger_.SourcePut(transaction, block->Link());
                }
            }
            break;
        }

        if (block->Height() > info.head_height_ + 1)
        {
            return;
        }
        else if (block->Height() == info.head_height_ + 1)
        {
            if (block->Previous() != info.head_)
            {
                if (!confirmed || info.confirmed_height_ != info.head_height_)
                {
                    return;
                }
                // log
                info.confirmed_height_ = info.head_height_ - 1;
                if (info.head_height_ == 0)
                {
                    info.confirmed_height_ = rai::Block::INVALID_HEIGHT;
                }
                error =
                    ledger_.AccountInfoPut(transaction, block->Account(), info);
                if (error)
                {
                    // log
                    return;
                }
            }
            else
            {
                info.head_height_ = block->Height();
                info.head_        = block->Hash();
                if (confirmed)
                {
                    info.confirmed_height_ = block->Height();
                }
                error =
                    ledger_.AccountInfoPut(transaction, block->Account(), info);
                if (error)
                {
                    // log
                    transaction.Abort();
                    return;
                }
                error = ledger_.BlockPut(transaction, block->Hash(), *block);
                if (error)
                {
                    transaction.Abort();
                    // log
                    return;
                }
                error = ledger_.BlockSuccessorSet(
                    transaction, block->Previous(), block->Hash());
                if (error)
                {
                    transaction.Abort();
                    // log
                    return;
                }

                if (block->Opcode() == rai::BlockOpcode::RECEIVE)
                {
                    ledger_.ReceivableInfoDel(transaction, block->Account(),
                                              block->Link());
                    if (!ledger_.SourceExists(transaction, block->Link()))
                    {
                        ledger_.SourcePut(transaction, block->Link());
                    }
                }
            }
        }
        else
        {
            if (!confirmed)
            {
                return;
            }

            if (ledger_.BlockExists(transaction, block->Hash()))
            {
                if (info.confirmed_height_ != rai::Block::INVALID_HEIGHT
                    && info.confirmed_height_ >= block->Height())
                {
                    return;
                }
                info.confirmed_height_ = block->Height();
                error =
                    ledger_.AccountInfoPut(transaction, block->Account(), info);
                if (error)
                {
                    // log
                    return;
                }
            }
            else
            {
                error = Rollback_(transaction, block->Account());
                if (error)
                {
                    transaction.Abort();
                    return;
                }
                QueueBlock(block, confirmed);
            }
        }
    } while (0);

    if (block_observer_)
    {
        block_observer_(block, false);
    }
}

void rai::Wallets::ProcessBlockRollback(
    const std::shared_ptr<rai::Block>& block)
{
    if (!IsMyAccount(block->Account()))
    {
        return;
    }

    bool callback = false;
    do
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return;
        }

        bool error = false;
        while (ledger_.BlockExists(transaction, block->Hash()))
        {
            callback = true;
            error = Rollback_(transaction, block->Account());
            if (error)
            {
                //log
                std::cout << "Rollback error\n";
                transaction.Abort();
                return;
            }
        }
    } while (0);

    if (callback && block_observer_)
    {
        block_observer_(block, true);
    }
}

void rai::Wallets::ProcessReceivableInfo(
    const rai::Account& account, const rai::BlockHash& hash,
    const rai::ReceivableInfo& info, const std::shared_ptr<rai::Block>& block)
{
    if (!IsMyAccount(account))
    {
        return;
    }

    uint64_t now = 0;
    bool error = CurrentTimestamp(now);
    IF_ERROR_RETURN_VOID(error);

    if (info.timestamp_ > now + 30)
    {
        return;
    }

    if (hash != block->Hash())
    {
        std::cout << "Wallets::ProcessReceivableInfo: hash inconsistent, "
                  << hash.StringHex() << ", " << block->Hash().StringHex()
                  << std::endl;
        return;
    }

    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return;
        }

        if (ledger_.SourceExists(transaction, hash))
        {
            return;
        }

        if (ledger_.ReceivableInfoExists(transaction, account, hash))
        {
            return;
        }

        bool error = ledger_.ReceivableInfoPut(transaction, account, hash, info);
        if (error)
        {
            // log
            transaction.Abort();
            return;
        }

        error = ledger_.SourcePut(transaction, hash, *block);
        if (error)
        {
            std::cout << "Wallets::ProcessReceivableInfo: put source error\n";
            transaction.Abort();
            return;
        }
    }

    if (receivable_observer_)
    {
        receivable_observer_(account);
    }
}

void rai::Wallets::QueueAccountInfo(const rai::Account& account,
                                    const rai::AccountInfo& info)
{
    std::weak_ptr<rai::Wallets> wallets(Shared());
    QueueAction(rai::WalletActionPri::URGENT, [wallets, account, info]() {
        if (auto wallets_s = wallets.lock())
        {
            wallets_s->ProcessAccountInfo(account, info);
        }
    });
}

void rai::Wallets::QueueAccountForks(
    const rai::Account& account,
    const std::vector<std::pair<std::shared_ptr<rai::Block>,
                                std::shared_ptr<rai::Block>>>& forks)
{
    std::weak_ptr<rai::Wallets> wallets(Shared());
    QueueAction(rai::WalletActionPri::URGENT, [wallets, account, forks]() {
        if (auto wallets_s = wallets.lock())
        {
            wallets_s->ProcessAccountForks(account, forks);
        }
    });
}

void rai::Wallets::QueueAction(rai::WalletActionPri pri,
                               const std::function<void()>& action)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        actions_.insert(std::make_pair(pri, action));
    }
    condition_.notify_all();
}

void rai::Wallets::QueueBlock(const std::shared_ptr<rai::Block>& block,
                              bool confirmed)
{
    std::weak_ptr<rai::Wallets> wallets(Shared());
    QueueAction(rai::WalletActionPri::URGENT, [wallets, block, confirmed]() {
        if (auto wallets_s = wallets.lock())
        {
            wallets_s->ProcessBlock(block, confirmed);
        }
    });
}

void rai::Wallets::QueueBlockRollback(const std::shared_ptr<rai::Block>& block)
{
    std::weak_ptr<rai::Wallets> wallets(Shared());
    QueueAction(rai::WalletActionPri::URGENT, [wallets, block]() {
        if (auto wallets_s = wallets.lock())
        {
            wallets_s->ProcessBlockRollback(block);
        }
    });

}

void rai::Wallets::QueueReceivable(const rai::Account& account,
                                   const rai::BlockHash& hash,
                                   const rai::Amount& amount,
                                   const rai::Account& source,
                                   uint64_t timestamp,
                                   const std::shared_ptr<rai::Block>& block)
{
    rai::ReceivableInfo info(source, amount, timestamp);
    std::weak_ptr<rai::Wallets> wallets(Shared());
    QueueAction(
        rai::WalletActionPri::URGENT, [wallets, account, hash, info, block]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->ProcessReceivableInfo(account, hash, info, block);
            }
        });
}

rai::Account rai::Wallets::RandomRepresentative() const
{
    size_t size = config_.preconfigured_reps_.size();
    if (size == 0)
    {
        throw std::runtime_error("No preconfigured representatives");
        return rai::Account(0);
    }

    if (config_.preconfigured_reps_.size() == 1)
    {
        return config_.preconfigured_reps_[0];
    }

    auto index = rai::random_pool.GenerateWord32(0, size - 1);
    return config_.preconfigured_reps_[index];
}


void rai::Wallets::ReceivablesQuery(const rai::Account& account)
{
    rai::Ptree ptree;
    ptree.put("action", "receivables");
    ptree.put("account", account.StringAccount());
    ptree.put("type", "confirmed");
    ptree.put("count", std::to_string(1000));

    Send(ptree);
}

void rai::Wallets::ReceiveAccountInfoQueryAck(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto account_o = message->get_optional<std::string>("account");
    if (!account_o)
    {
        std::cout
            << "Wallets::ReceiveAccountInfoQueryAck: Failed to get account"
            << std::endl;
        return;
    }

    rai::Account account;
    bool error = account.DecodeAccount(*account_o);
    if (error)
    {
        std::cout
            << "Wallets::ReceiveAccountInfoQueryAck: Failed to decode account="
            << *account_o << std::endl;
        return;
    }

    rai::AccountInfo info;
    auto block_o = message->get_child_optional("head_block");
    if (block_o)
    {
        std::shared_ptr<rai::Block> block(nullptr);
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        block = rai::DeserializeBlockJson(error_code, *block_o);
        if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
        {
            std::cout << "Wallets::ReceiveAccountInfoQueryAck: Failed to "
                         "deserialize json block, error="
                      << rai::ErrorString(error_code) << std::endl;
            return;
        }

        info.type_ = block->Type();
        info.head_ = block->Hash();
        info.head_height_ = block->Height();
    }

    QueueAccountInfo(account, info);
}

void rai::Wallets::ReceiveAccountForksQueryAck(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto account_o = message->get_optional<std::string>("account");
    if (!account_o)
    {
        std::cout
            << "Wallets::ReceiveAccountForksQueryAck: Failed to get account"
            << std::endl;
        return;
    }

    rai::Account account;
    bool error = account.DecodeAccount(*account_o);
    if (error)
    {
        std::cout
            << "Wallets::ReceiveAccountForksQueryAck: Failed to decode account="
            << *account_o << std::endl;
        return;
    }

    auto forks_o = message->get_child_optional("forks");
    if (!forks_o)
    {
        return;
    }

    std::vector<
        std::pair<std::shared_ptr<rai::Block>, std::shared_ptr<rai::Block>>>
        forks;
    for (const auto& i : *forks_o)
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        auto first_o = i.second.get_child_optional("block_first");
        if (!first_o)
        {
            std::cout << "Wallets::ReceiveAccountForksQueryAck: Failed to get "
                         "first block, account="
                      << account.StringAccount() << std::endl;
            return;
        }
        std::shared_ptr<rai::Block> first(nullptr);
        first = rai::DeserializeBlockJson(error_code, *first_o);
        if (error_code != rai::ErrorCode::SUCCESS || first == nullptr)
        {
            std::cout
                << "Wallets::ReceiveAccountForksQueryAck: Failed to parse "
                   "first block, account="
                << account.StringAccount() << std::endl;
            return;
        }

        auto second_o = i.second.get_child_optional("block_second");
        if (!second_o)
        {
            std::cout << "Wallets::ReceiveAccountForksQueryAck: Failed to get "
                         "second block, account="
                      << account.StringAccount() << std::endl;
            return;
        }
        std::shared_ptr<rai::Block> second(nullptr);
        second = rai::DeserializeBlockJson(error_code, *second_o);
        if (error_code != rai::ErrorCode::SUCCESS || second == nullptr)
        {
            std::cout
                << "Wallets::ReceiveAccountForksQueryAck: Failed to parse "
                   "second block, account="
                << account.StringAccount() << std::endl;
            return;
        }

        if (first->Account() != account || second->Account() != account
            || !first->ForkWith(*second))
        {
            std::cout
                << "Wallets::ReceiveAccountForksQueryAck: data inconsistent"
                << std::endl;
            return;
        }

        forks.push_back({first, second});
    }

    QueueAccountForks(account, forks);
}

void rai::Wallets::ReceiveBlockAppendNotify(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto block_o = message->get_child_optional("block");
    if (!block_o)
    {
        // log
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockJson(error_code, *block_o);
    if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
    {
        //log
        return;
    }

    QueueBlock(block, false);
}

void rai::Wallets::ReceiveBlockConfirmNotify(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto block_o = message->get_child_optional("block");
    if (!block_o)
    {
        // log
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockJson(error_code, *block_o);
    if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
    {
        // log
        return;
    }

    QueueBlock(block, true);
}

void rai::Wallets::ReceiveBlockRollbackNotify(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto block_o = message->get_child_optional("block");
    if (!block_o)
    {
        // log
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block = rai::DeserializeBlockJson(error_code, *block_o);
    if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
    {
        // log
        return;
    }

    QueueBlockRollback(block);
}

void rai::Wallets::ReceiveBlockQueryAck(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto status_o = message->get_optional<std::string>("status");
    if (!status_o)
    {
        //log
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    auto block_o = message->get_child_optional("block");
    if (block_o)
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        block = rai::DeserializeBlockJson(error_code, *block_o);
        if (error_code != rai::ErrorCode::SUCCESS || block == nullptr)
        {
            return;
        }
    }

    bool confirmed;
    auto confirmed_o = message->get_optional<std::string>("confirmed");
    if (confirmed_o)
    {
        if (*confirmed_o != "true" && *confirmed_o != "false")
        {
            //log
            return;
        }
        confirmed = *confirmed_o == "true" ? true : false;
    }

    if (*status_o == "success")
    {
        if (!block)
        {
            // log
            return;
        }
        if (!confirmed_o)
        {
            //log
            return;
        }
        QueueBlock(block, confirmed);
    }
    else if (*status_o == "fork")
    {
        if (!block)
        {
            // log
            return;
        }
        if (!confirmed_o)
        {
            //log
            return;
        }
        QueueBlock(block, confirmed);
        SyncForks(block->Account());
    }
    else if (*status_o == "miss")
    {
        auto account_o = message->get_optional<std::string>("request_id");
        if (!account_o)
        {
            std::cout << "ReceiveBlockQueryAck::get account error\n";
            return;
        }
        rai::Account account;
        bool error = account.DecodeAccount(*account_o);
        if (error)
        {
            std::cout << "ReceiveBlockQueryAck::invalid account:" << *account_o
                      << std::endl;
            return;
        }
        SyncedAdd(account);
    }
    else if (*status_o == "pruned")
    {
        //log
    }
    else
    {
        //log
    }
    
}

void rai::Wallets::ReceiveCurrentTimestampAck(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto timestamp_o = message->get_optional<std::string>("timestamp");
    if (!timestamp_o)
    {
        // log
        std::cout
            << "Wallets::ReceiveCurrentTimestampAck: failed to get timestamp"
            << std::endl;
        return;
    }

    uint64_t timestamp = 0;
    bool error = rai::StringToUint(*timestamp_o, timestamp);
    if (error)
    {
        // log
        std::cout << "Wallets::ReceiveCurrentTimestampAck: invalid timestamp "
                  << *timestamp_o << std::endl;
        return;
    }

    bool notify = false;
    {
        std::lock_guard<std::mutex> lock(mutex_timesync_);
        notify = !time_synced_;
        time_synced_ = true;
        server_time_ = timestamp;
        local_time_ = std::chrono::steady_clock::now();
    }

    if (notify && time_synced_observer_)
    {
        time_synced_observer_();
    }
}

void rai::Wallets::ReceiveForkNotify(const std::shared_ptr<rai::Ptree>& message)
{
    auto account_o = message->get_optional<std::string>("account");
    if (!account_o)
    {
        std::cout << "Wallets::ReceiveForkNotify: Failed to get account"
                  << std::endl;
        return;
    }

    rai::Account account;
    bool error = account.DecodeAccount(*account_o);
    if (error)
    {
        std::cout << "Wallets::ReceiveForkNotify: Failed to decode account="
                  << *account_o << std::endl;
        return;
    }

    SyncForks(account);
}

void rai::Wallets::ReceiveReceivablesQueryAck(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto account_o = message->get_optional<std::string>("account");
    if (!account_o)
    {
        // log
        std::cout << "ReceiveReceivablesQueryAck::get account error\n";
        return;
    }
    rai::Account account;
    bool error = account.DecodeAccount(*account_o);
    if (error)
    {
        // log
        std::cout << "ReceiveReceivablesQueryAck::DecodeAccount error\n";
        return;
    }

    auto receivables_o = message->get_child_optional("receivables");
    if (!receivables_o)
    {
        // log
        std::cout << "ReceiveReceivablesQueryAck::get receivables error\n";
        return;
    }

    for (const auto& i : *receivables_o)
    {
        auto amount_o = i.second.get_optional<std::string>("amount");
        if (!amount_o)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::get amount error\n";
            return;
        }
        rai::Amount amount;
        error = amount.DecodeDec(*amount_o);
        if (error)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::decode amount error\n";
            return;
        }

        auto source_o = i.second.get_optional<std::string>("source");
        if (!source_o)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::get source error\n";
            return;
        }
        rai::Account source;
        error = source.DecodeAccount(*source_o);
        if (error)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::decode source error\n";
            return;
        }

        auto hash_o = i.second.get_optional<std::string>("hash");
        if (!hash_o)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::get hash error\n";
            return;
        }
        rai::BlockHash hash;
        error = hash.DecodeHex(*hash_o);
        if (error)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::decode hash error\n";
            return;
        }

        auto timestamp_o = i.second.get_optional<std::string>("timestamp");
        if (!timestamp_o)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck::get timestamp error\n";
            return;
        }
        uint64_t timestamp;
        error = rai::StringToUint(*timestamp_o, timestamp);
        if (error)
        {
            // log
            std::cout << "ReceiveReceivablesQueryAck: invalid timestamp\n";
            return;
        }


        auto block_o = i.second.get_child_optional("source_block");
        if (!block_o)
        {
            //log
            std::cout << "ReceiveReceivablesQueryAck: get source block error\n";
            return;
        }

        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        std::shared_ptr<rai::Block> block(nullptr);
        block = rai::DeserializeBlockJson(error_code, *block_o);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            std::cout
                << "ReceiveReceivablesQueryAck: deserialize source block error="
                << rai::ErrorString(error_code) << std::endl;
            return;
        }

        QueueReceivable(account, hash, amount, source, timestamp, block);
    }
}

void rai::Wallets::ReceiveReceivableInfoNotify(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto account_o = message->get_optional<std::string>("account");
    if (!account_o)
    {
        //log
        return;
    }
    rai::Account account;
    bool error = account.DecodeAccount(*account_o);
    if (error)
    {
        // log
        return;
    }

    auto amount_o = message->get_optional<std::string>("amount");
    if (!amount_o)
    {
        // log
        return;
    }
    rai::Amount amount;
    error = amount.DecodeDec(*amount_o);
    if (error)
    {
        // log
        return;
    }

    auto source_o = message->get_optional<std::string>("source");
    if (!source_o)
    {
        // log
        return;
    }
    rai::Account source;
    error = source.DecodeAccount(*source_o);
    if (error)
    {
        // log
        return;
    }

    auto hash_o = message->get_optional<std::string>("hash");
    if (!hash_o)
    {
        // log
        return;
    }
    rai::BlockHash hash;
    error = hash.DecodeHex(*hash_o);
    if (error)
    {
        // log
        return;
    }

    auto timestamp_o = message->get_optional<std::string>("timestamp");
    if (!timestamp_o)
    {
        // log
        return;
    }
    uint64_t timestamp;
    error = rai::StringToUint(*timestamp_o, timestamp);
    if (error)
    {
        // log
        std::cout << "ReceiveReceivableInfoNotify: invalid timestamp\n";
        return;
    }

    auto block_o = message->get_child_optional("source_block");
    if (!block_o)
    {
        // log
        std::cout << "ReceiveReceivableInfoNotify: get source block error\n";
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    std::shared_ptr<rai::Block> block(nullptr);
    block = rai::DeserializeBlockJson(error_code, *block_o);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        std::cout
            << "ReceiveReceivableInfoNotify: deserialize source block error="
            << rai::ErrorString(error_code) << std::endl;
        return;
    }

    QueueReceivable(account, hash, amount, source, timestamp, block);
}

void rai::Wallets::ReceiveMessage(const std::shared_ptr<rai::Ptree>& message)
{
    #if 0
    std::cout << "receive message<<=====:\n";
    std::stringstream ostream;
    boost::property_tree::write_json(ostream, *message);
    ostream.flush();
    std::string body = ostream.str();
    std::cout << body << std::endl;
    #endif

    auto ack_o = message->get_optional<std::string>("ack");
    if (ack_o)
    {
        std::string ack(*ack_o);
        if (ack == "block_query")
        {
            ReceiveBlockQueryAck(message);
        }
        else if (ack == "receivables")
        {
            ReceiveReceivablesQueryAck(message);
        }
        else if (ack == "account_info")
        {
            ReceiveAccountInfoQueryAck(message);
        }
        else if (ack == "account_forks")
        {
            ReceiveAccountForksQueryAck(message);
        }
        else if (ack == "current_timestamp")
        {
            ReceiveCurrentTimestampAck(message);
        }
    }

    auto notify_o = message->get_optional<std::string>("notify");
    if (notify_o)
    {
        std::string notify(*notify_o);
        if (notify == "block_append")
        {
            ReceiveBlockAppendNotify(message);
        }
        else if (notify == "block_confirm")
        {
            ReceiveBlockConfirmNotify(message);
        }
        else if (notify == "block_rollback")
        {
            ReceiveBlockRollbackNotify(message);
        }
        else if (notify == "receivable_info")
        {
            ReceiveReceivableInfoNotify(message);
        }
        else if (notify == "fork_add" || notify == "fork_delete")
        {
            ReceiveForkNotify(message);
        }
    }
}

void rai::Wallets::Send(const rai::Ptree& ptree)
{
    uint32_t count = send_count_++;
    websockets_[count % websockets_.size()]->Send(ptree);
}

std::vector<std::shared_ptr<rai::Wallet>> rai::Wallets::SharedWallets() const
{
    std::vector<std::shared_ptr<rai::Wallet>> result;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& i : wallets_)
    {
        result.push_back(i.second);
    }
    return result;
}

void rai::Wallets::Start()
{
    RegisterObservers_();

    Ongoing(std::bind(&rai::Wallets::ConnectToServer, this),
            std::chrono::seconds(5));

    Ongoing([this]() { this->SyncTime(); }, std::chrono::seconds(300));
    Ongoing([this]() { this->SubscribeAll(); }, std::chrono::seconds(300));
    Ongoing([this]() { this->SyncAll(); }, std::chrono::seconds(300));
}

void rai::Wallets::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
    }

    UnsubscribeAll();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    condition_.notify_all();
    if (thread_.joinable())
    {
        thread_.join();
    }
    std::cout << "wallets run thread closed\n";

    alarm_.Stop();
    std::cout << "alarm thread closed\n";
    for (const auto& i : websockets_)
    {
        i->Close();
    }
    std::cout << "websocket closed\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    service_runner_.Stop();
    std::cout << "io service thread closed\n";

    std::cout << "all closed\n";
}

void rai::Wallets::SelectAccount(uint32_t account_id)
{
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        // construct writing transaction first to avoid deadlock
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(selected_wallet_id_);
        if (wallet == nullptr)
        {
            // log
            return;
        }

        if (wallet->SelectAccount(account_id))
        {
            return;
        }

        error_code = wallet->StoreInfo(transaction, selected_wallet_id_);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            transaction.Abort();
            return;
        }
    }

    if (selected_account_observer_)
    {
        selected_account_observer_(wallet->SelectedAccount());
    }
}

void rai::Wallets::SelectWallet(uint32_t wallet_id)
{
    rai::Account selected_account;
    {
        // construct writing transaction first to avoid deadlock
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, true);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // log
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (wallet_id == selected_wallet_id_)
        {
            return;
        }

        bool exists = false;
        for (const auto& i : wallets_)
        {
            if (wallet_id == i.first)
            {
                exists = true;
                selected_account = i.second->SelectedAccount();
                break;
            }
        }
        if (!exists)
        {
            return;
        }

        bool error = ledger_.SelectedWalletIdPut(transaction, wallet_id);
        if (error)
        {
            // log
            transaction.Abort();
            return;
        }
        selected_wallet_id_ = wallet_id;
    }

    if (selected_wallet_observer_)
    {
        selected_wallet_observer_(wallet_id);
    }

    if (selected_account_observer_)
    {
        selected_account_observer_(selected_account);
    }
}

rai::Account rai::Wallets::SelectedAccount() const
{
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(selected_wallet_id_);
    }

    if (wallet)
    {
        return wallet->SelectedAccount();
    }

    return rai::Account(0);
}

std::shared_ptr<rai::Wallet> rai::Wallets::SelectedWallet() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return Wallet_(selected_wallet_id_);
}

uint32_t rai::Wallets::SelectedWalletId() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return selected_wallet_id_;
}

void rai::Wallets::Subscribe(const std::shared_ptr<rai::Wallet>& wallet,
                             uint32_t account_id)
{
    Subscribe_(wallet, account_id);
}

void rai::Wallets::Subscribe(const std::shared_ptr<rai::Wallet>& wallet)
{
    if (wallet == nullptr)
    {
        return;
    }
    auto accounts = wallet->Accounts();

    for (const auto& i : accounts)
    {
        Subscribe_(wallet, i.first);
    }
}

void rai::Wallets::SubscribeAll()
{
    uint64_t now = 0;
    bool error = CurrentTimestamp(now);
    IF_ERROR_RETURN_VOID(error);
    if (last_sub_ > now - 150)
    {
        return;
    }
    last_sub_ = now;

    auto wallets = SharedWallets();
    for (const auto& wallet : wallets)
    {
        Subscribe(wallet);
    }
}

void rai::Wallets::SubscribeSelected()
{
    auto wallet = SelectedWallet();
    Subscribe(wallet, wallet->SelectedAccountId());
}

void rai::Wallets::Sync(const rai::Account& account)
{
    SyncBlocks(account);
    SyncForks(account); 
    SyncAccountInfo(account);
    SyncReceivables(account);
}

void rai::Wallets::Sync(const std::shared_ptr<rai::Wallet>& wallet)
{
    SyncBlocks(wallet);
    SyncForks(wallet); 
    SyncAccountInfo(wallet);
    SyncReceivables(wallet);
}

void rai::Wallets::SyncAll()
{
    uint64_t now = 0;
    bool error = CurrentTimestamp(now);
    IF_ERROR_RETURN_VOID(error);
    if (last_sync_ > now - 150)
    {
        return;
    }
    last_sync_ = now;

    auto wallets = SharedWallets();
    for (const auto& wallet : wallets)
    {
        Sync(wallet);
    }
}

void rai::Wallets::SyncSelected()
{
    rai::Account account = SelectedAccount();
    if (!account.IsZero())
    {
        Sync(account);
    }
}

void rai::Wallets::SyncAccountInfo(const rai::Account& account)
{
    AccountInfoQuery(account);
}

void rai::Wallets::SyncAccountInfo(const std::shared_ptr<rai::Wallet>& wallet)
{
    if (wallet == nullptr)
    {
        return;
    }
    auto accounts = wallet->Accounts();

    for (const auto& i : accounts)
    {
        AccountInfoQuery(i.second.first);
    }
}

void rai::Wallets::SyncBlocks(const rai::Account& account)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    SyncBlocks_(transaction, account);
}

void rai::Wallets::SyncBlocks(const std::shared_ptr<rai::Wallet>& wallet)
{
    if (wallet == nullptr)
    {
        return;
    }
    auto accounts = wallet->Accounts();

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    for (const auto& i : accounts)
    {
       SyncBlocks_(transaction, i.second.first);
    }
}

void rai::Wallets::SyncForks(const rai::Account& account)
{
    AccountForksQuery(account);
}

void rai::Wallets::SyncForks(const std::shared_ptr<rai::Wallet>& wallet)
{
    if (wallet == nullptr)
    {
        return;
    }
    auto accounts = wallet->Accounts();

    for (const auto& i : accounts)
    {
        AccountForksQuery(i.second.first);
    }
}

void rai::Wallets::SyncReceivables(const rai::Account& account)
{
    ReceivablesQuery(account);
}

void rai::Wallets::SyncTime()
{
    rai::Ptree ptree;
    ptree.put("action", "current_timestamp");

    Send(ptree);
}

void rai::Wallets::SyncReceivables(const std::shared_ptr<rai::Wallet>& wallet)
{
    if (wallet == nullptr)
    {
        return;
    }
    auto accounts = wallet->Accounts();

    for (const auto& i : accounts)
    {
       ReceivablesQuery(i.second.first);
    }
}

bool rai::Wallets::Synced(const rai::Account& account) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return synced_.find(account)  != synced_.end();
}

void rai::Wallets::SyncedAdd(const rai::Account& account)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        synced_.insert(account);
    }

    if (synced_observer_)
    {
        synced_observer_(account, true);
    }
}

void rai::Wallets::SyncedClear()
{
    std::unordered_set<rai::Account> accounts;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        synced_.swap(accounts);
    }

    if (synced_observer_)
    {
        for (const auto& account : accounts)
        {
            synced_observer_(account, false);
        }
    }
}

bool rai::Wallets::TimeSynced() const
{
    std::lock_guard<std::mutex> lock(mutex_timesync_);
    return time_synced_;
}

void rai::Wallets::Unsubscribe(const std::shared_ptr<rai::Wallet>& wallet)
{
    if (wallet == nullptr)
    {
        return;
    }
    auto accounts = wallet->Accounts();

    for (const auto& i : accounts)
    {
        rai::Account account = i.second.first;

        rai::Ptree ptree;
        ptree.put("action", "account_unsubscribe");
        ptree.put("account", i.second.first.StringAccount());

        Send(ptree);
    }
}

void rai::Wallets::UnsubscribeAll()
{
    auto wallets = SharedWallets();
    for (const auto& wallet : wallets)
    {
        Unsubscribe(wallet);
    }
}

std::vector<uint32_t> rai::Wallets::WalletIds() const
{
    std::vector<uint32_t> result;
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& i : wallets_)
    {
        result.push_back(i.first);
    }
    return result;
}

void rai::Wallets::WalletBalanceAll(uint32_t wallet_id, rai::Amount& balance,
                                    rai::Amount& confirmed,
                                    rai::Amount& receivable)
{
    balance.Clear();
    confirmed.Clear();
    receivable.Clear();

    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(wallet_id);
        if (wallet == nullptr)
        {
            return;
        }
    }

    auto accounts = wallet->Accounts();
    for (const auto& i : accounts)
    {
        rai::Amount balance_l;
        rai::Amount confirmed_l;
        rai::Amount receivable_l;
        AccountBalanceAll(i.second.first, balance_l, confirmed_l, receivable_l);
        balance += balance_l;
        confirmed += confirmed_l;
        receivable += receivable_l;
    }
}

uint32_t rai::Wallets::WalletAccounts(uint32_t wallet_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto wallet = Wallet_(wallet_id);
    if (wallet == nullptr)
    {
        return 0;
    }

    return static_cast<uint32_t>(wallet->Size());
}

rai::ErrorCode rai::Wallets::WalletSeed(uint32_t wallet_id,
                                        rai::RawKey& seed) const
{
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        wallet = Wallet_(wallet_id);
        if (wallet == nullptr)
        {
            return rai::ErrorCode::WALLET_GET;
        }
    }

    return wallet->Seed(seed);
}

bool rai::Wallets::WalletLocked(uint32_t wallet_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto wallet = Wallet_(wallet_id);
    if (wallet != nullptr && wallet->ValidPassword())
    {
        return false;
    }

    return true;
}

bool rai::Wallets::WalletVulnerable(uint32_t wallet_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto wallet = Wallet_(wallet_id);
    if (wallet != nullptr && wallet->EmptyPassword())
    {
        return true;
    }

    return false;
}

void rai::Wallets::LastSubClear()
{
    last_sub_ = 0;
}

void rai::Wallets::LastSyncClear()
{
    last_sync_ = 0;
}

rai::ErrorCode rai::Wallets::ActionCommonCheck_() const
{
    auto wallet = SelectedWallet();
    if (wallet == nullptr)
    {
        return rai::ErrorCode::WALLET_GET;
    }

    if (!wallet->ValidPassword())
    {
        return rai::ErrorCode::WALLET_LOCKED;
    }

    rai::Account account = wallet->SelectedAccount();
    if (account.IsZero())
    {
        return rai::ErrorCode::WALLET_ACCOUNT_GET;
    }

    if (!Synced(account))
    {
        return rai::ErrorCode::WALLET_ACCOUNT_IN_SYNC;
    }

    return rai::ErrorCode::SUCCESS;
}

uint32_t rai::Wallets::NewWalletId_() const
{
    uint32_t result = 1;

    for (const auto& i : wallets_)
    {
        if (i.first >= result)
        {
            result = i.first + 1;
        }
    }

    return result;
}

void rai::Wallets::RegisterObservers_()
{
    std::weak_ptr<rai::Wallets> wallets(Shared());
    for (const auto& i : websockets_)
    {
        i->message_processor_ =
            [wallets](const std::shared_ptr<rai::Ptree>& message) {
                auto wallets_s = wallets.lock();
                if (wallets_s == nullptr) return;
                wallets_s->ReceiveMessage(message);
            };

        i->status_observer_ = [wallets](rai::WebsocketStatus status) {
            auto wallets_s = wallets.lock();
            if (wallets_s == nullptr) return;

            wallets_s->Background([wallets, status]() {
                if (auto wallets_s = wallets.lock())
                {
                    wallets_s->observers_.connection_status_.Notify(status);
                }
            });
        };
    }

    block_observer_ = [wallets](const std::shared_ptr<rai::Block>& block,
                                bool rollback) {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, block, rollback]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.block_.Notify(block, rollback);
            }
        });
    };

    selected_account_observer_ = [wallets](const rai::Account& account) {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, account]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.selected_account_.Notify(account);
            }
        });
    };

    selected_wallet_observer_ = [wallets](uint32_t wallet_id){
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, wallet_id]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.selected_wallet_.Notify(wallet_id);
            }
        });
    };

    wallet_locked_observer_ = [wallets](bool locked) {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, locked]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.wallet_locked_.Notify(locked);
            }
        });
    };

    wallet_password_set_observer_ = [wallets]() {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.wallet_password_set_.Notify();
            }
        });
    };

    receivable_observer_ = [wallets](const rai::Account& account) {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, account]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.receivable_.Notify(account);
            }
        });
    };

    synced_observer_ = [wallets](const rai::Account& account, bool synced) {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, account, synced]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.synced_.Notify(account, synced);
            }
        });
    };

    fork_observer_ = [wallets](const rai::Account& account) {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets, account]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.fork_.Notify(account);
            }
        });
    };

    time_synced_observer_ = [wallets]() {
        auto wallets_s = wallets.lock();
        if (wallets_s == nullptr) return;

        wallets_s->Background([wallets]() {
            if (auto wallets_s = wallets.lock())
            {
                wallets_s->observers_.time_synced_.Notify();
            }
        });
    };

    observers_.connection_status_.Add([this](rai::WebsocketStatus status) {
        if (status == rai::WebsocketStatus::CONNECTED)
        {
            std::cout << "websocket connected\n";
            SyncTime();
            if (TimeSynced())
            {
                SubscribeSelected();
                SyncSelected();
                SubscribeAll();
                SyncAll();
            }
        }
        else if (status == rai::WebsocketStatus::CONNECTING)
        {
            std::cout << "websocket connecting\n";
        }
        else
        {
            std::cout << "websocket disconnected\n";
            SyncedClear();
            LastSubClear();
            LastSyncClear();
        }
    });

    observers_.block_.Add(
        [this](const std::shared_ptr<rai::Block>& block, bool rollback) {
            SyncBlocks(block->Account());

            if (rollback && block->Opcode() == rai::BlockOpcode::RECEIVE)
            {
                SyncReceivables(block->Account());
            }
        });

    observers_.selected_account_.Add([this](const rai::Account&) {
        SubscribeSelected();
        SyncSelected();
    });

    observers_.time_synced_.Add([this]() {
        SubscribeSelected();
        SyncSelected();
        SubscribeAll();
        SyncAll();
    });
}

bool rai::Wallets::Rollback_(rai::Transaction& transaction,
                             const rai::Account& account)
{
    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        std::cout << "Wallets::Rollback_::AccountInfoGet error!" << std::endl;
        return true;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    error = ledger_.BlockGet(transaction, info.head_, block);
    if (error)
    {
        //log
        std::cout << "Wallets::Rollback_::BlockGet error!" << std::endl;
        return true;
    }

    error = ledger_.RollbackBlockPut(transaction, info.head_, *block);
    if (error)
    {
        //log
        std::cout << "Wallets::Rollback_::RollbackBlockPut error!" << std::endl;
        return true;
    }

    error = ledger_.BlockDel(transaction, info.head_);
    if (error)
    {
        //log
        std::cout << "Wallets::Rollback_::BlockDel error!" << std::endl;
        return true;
    }
    if (block->Height() != 0)
    {
        error = ledger_.BlockSuccessorSet(transaction, block->Previous(),
                                          rai::BlockHash(0));
        IF_ERROR_RETURN(error, true);
    }

    if (info.head_height_ == 0)
    {
        error = ledger_.AccountInfoDel(transaction, account);
        if (error)
        {
            //log
            std::cout << "Wallets::Rollback_::AccountInfoDel error!" << std::endl;
            return true;
        }
    }
    else
    {
        info.head_ = block->Previous();
        info.head_height_ -= 1;
        if (info.confirmed_height_ != rai::Block::INVALID_HEIGHT
            && info.confirmed_height_ > info.head_height_)
        {
            info.confirmed_height_ = info.head_height_;
        }
        error = ledger_.AccountInfoPut(transaction, account, info);
        if (error)
        {
            //log
            std::cout << "Wallets::Rollback_::AccountInfoPut error!" << std::endl;
            return true;
        }
    }

    if (block->Opcode() == rai::BlockOpcode::RECEIVE)
    {
        error = ledger_.SourceDel(transaction, block->Link());
        IF_ERROR_RETURN(error, true);
    }

    return false;
}

void rai::Wallets::Subscribe_(const std::shared_ptr<rai::Wallet>& wallet,
                              uint32_t account_id)
{
    rai::Account account = wallet->Account(account_id);
    if (account.IsZero())
    {
        return;
    }

    uint64_t now = 0;
    bool error = CurrentTimestamp(now);
    if (error)
    {
        // log
        std::cout << "Wallets::Subscribe_: Failed to get current timestamp"
                  << std::endl;
        return;
    }

    uint64_t timestamp   = now;

    rai::Ptree ptree;
    ptree.put("action", "account_subscribe");
    ptree.put("account", account.StringAccount());
    ptree.put("timestamp", std::to_string(timestamp));

    if (wallet->ValidPassword())
    {
        int ret;
        rai::uint256_union hash;
        blake2b_state state;

        ret = blake2b_init(&state, hash.bytes.size());
        assert(0 == ret);

        std::vector<uint8_t> bytes;
        {
            rai::VectorStream stream(bytes);
            rai::Write(stream, account.bytes);
            rai::Write(stream, timestamp);
        }
        blake2b_update(&state, bytes.data(), bytes.size());

        ret = blake2b_final(&state, hash.bytes.data(), hash.bytes.size());
        assert(0 == ret);

        rai::Signature signature;
        bool error = wallet->Sign(account, hash, signature);
        if (error)
        {
            // log
            std::cout << "Failed to sign subscribe message (account="
                      << account.StringAccount() << ")\n";
        }
        else
        {
            ptree.put("signature", signature.StringHex());
        }
    }

    Send(ptree);
}

void rai::Wallets::SyncBlocks_(rai::Transaction& transaction,
                               const rai::Account& account)
{
    rai::AccountInfo info;
    bool error = ledger_.AccountInfoGet(transaction, account, info);
    if (error)
    {
        BlockQuery(account, 0, rai::BlockHash(0));
        return;
    }
    
    if (info.confirmed_height_ == rai::Block::INVALID_HEIGHT)
    {
        BlockQuery(account, 0, rai::BlockHash(0));
    }
    else
    {
        std::shared_ptr<rai::Block> block(nullptr);
        error = ledger_.BlockGet(transaction, account, info.confirmed_height_,
                                 block);
        if (error)
        {
            // log
            return;
        }
        BlockQuery(account, info.confirmed_height_ + 1, block->Hash());
    }

    if (info.head_height_ != info.confirmed_height_)
    {
        BlockQuery(account, info.head_height_ + 1, info.head_);
    }
}

std::shared_ptr<rai::Wallet> rai::Wallets::Wallet_(uint32_t wallet_id) const
{
    std::shared_ptr<rai::Wallet> result(nullptr);
    for (const auto& i : wallets_)
    {
        if (i.first == wallet_id)
        {
            result = i.second;
            break;
        }
    }
    return result;
}

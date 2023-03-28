#include <rai/node/node.hpp>

#include <atomic>
#include <iostream>
#include <boost/beast.hpp>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <rai/node/network.hpp>
#include <rai/node/message.hpp>
#include <rai/secure/http.hpp>

std::chrono::seconds constexpr rai::RecentBlocks::AGE_TIME;
std::chrono::seconds constexpr rai::ActiveAccounts::AGE_TIME;

bool rai::RecentBlocks::Insert(const rai::BlockHash& hash)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto inserted = blocks_.insert(rai::RecentBlock{hash, now});
    IF_ERROR_RETURN(!inserted.second, true);

    if (blocks_.size() > rai::RecentBlocks::MAX_SIZE)
    {
        blocks_.erase(blocks_.begin());
    }
    return false;
}

bool rai::RecentBlocks::Exists(const rai::BlockHash& hash) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return blocks_.get<1>().find(hash) != blocks_.get<1>().end();
}

void rai::RecentBlocks::Remove(const rai::BlockHash& hash)
{
    std::lock_guard<std::mutex> lock(mutex_);
    blocks_.get<1>().erase(hash);
}

void rai::RecentBlocks::Age()
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto cutoff =
        std::chrono::steady_clock::now() - rai::RecentBlocks::AGE_TIME;
    blocks_.erase(blocks_.begin(), blocks_.lower_bound(cutoff));
}

size_t rai::RecentBlocks::Size()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return blocks_.size();
}

bool rai::RecentForks::Insert(const rai::BlockHash& first,
                              const rai::BlockHash& second)
{
    std::lock_guard<std::mutex> lock(mutex_);
    bool error_first = blocks_.Insert(first);
    bool error_second = blocks_.Insert(second);
    return error_first && error_second;
}

bool rai::RecentForks::Exists(const rai::BlockHash& first,
                              const rai::BlockHash& second) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    bool exists_first = blocks_.Exists(first);
    bool exists_second = blocks_.Exists(second);
    return exists_first && exists_second;
}

void rai::RecentForks::Remove(const rai::BlockHash& first,
                              const rai::BlockHash& second)
{
    std::lock_guard<std::mutex> lock(mutex_);
    blocks_.Remove(first);
    blocks_.Remove(second);
}

void rai::RecentForks::Age()
{
    std::lock_guard<std::mutex> lock(mutex_);
    blocks_.Age();
}

#if 0
bool rai::RecentForks::Insert(const rai::Account& account, uint64_t height)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto inserted = forks_.insert(rai::RecentFork{account, height, now});
    IF_ERROR_RETURN(!inserted.second, true);

    if (forks_.size() > rai::RecentForks::MAX_SIZE)
    {
        forks_.erase(forks_.begin());
    }
    return false;
}

bool rai::RecentForks::Exists(const rai::Account& account,
                              uint64_t height) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return forks_.get<1>().find(boost::make_tuple(account, height))
           != forks_.get<1>().end();
}

void rai::RecentForks::Remove(const rai::Account& account, uint64_t height)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = forks_.get<1>().find(boost::make_tuple(account, height));
    if (it != forks_.get<1>().end())
    {
        forks_.get<1>().erase(it);
    }
}
#endif

bool rai::ConfirmRequests::Append(const rai::BlockHash& hash,
                                  const rai::Account& account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = items_.find(hash);
    if (it == items_.end())
    {
        return true;
    }
    return Insert_(hash, account);
}

bool rai::ConfirmRequests::Insert(const rai::BlockHash& hash)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = items_.find(hash);
    if (it != items_.end())
    {
        return true;
    }

    std::vector<rai::Account> accounts;
    auto ret = items_.emplace(hash,accounts);
    return !ret.second;
}

bool rai::ConfirmRequests::Insert(const rai::BlockHash& hash,
                                  const rai::Account& account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return Insert_(hash, account);
}

std::vector<rai::Account> rai::ConfirmRequests::Remove(const rai::BlockHash& hash)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<rai::Account> result = Query_(hash);
    items_.erase(hash);
    return result;
}

bool rai::ConfirmRequests::Insert_(const rai::BlockHash& hash,
                                  const rai::Account& account)
{
    auto it = items_.find(hash);
    if (it == items_.end())
    {
        std::vector<rai::Account> accounts;
        accounts.push_back(account);
        auto ret = items_.insert(std::make_pair(hash,accounts));
        return !ret.second;
    }

    if (it->second.size() >= rai::ConfirmRequests::MAX_CONFIRMATIONS_PER_BLOCK)
    {
        return true;
    }

    for (const auto& i : it->second)
    {
        if (account == i)
        {
            return true;
        }
    }

    it->second.push_back(account);
    return false;
}

std::vector<rai::Account> rai::ConfirmRequests::Query_(
    const rai::BlockHash& hash)
{
    auto it = items_.find(hash);
    if (it == items_.end())
    {
        std::vector<rai::Account> empty;
        return empty;
    }

    return it->second;
}

uint64_t rai::ConfirmManager::GetTimestamp(const rai::Account& account,
                                           uint64_t height,
                                           const rai::BlockHash& hash)
{
    uint64_t now = rai::CurrentTimestamp();
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = confirms_.get<1>().find(boost::make_tuple(account, height));
    if (it == confirms_.get<1>().end())
    {
        confirms_.insert(rai::ConfirmInfo{account, height, now, hash});
        return now;
    }

    if (it->timestamp_ + rai::MIN_CONFIRM_INTERVAL <= now)
    {
        confirms_.get<1>().modify(it, [&](rai::ConfirmInfo& data) {
            data.timestamp_ = now;
            data.hash_ = hash;
        });
        return now;
    }

    if (it->hash_ == hash)
    {
        return it->timestamp_;
    }

    uint64_t result = it->timestamp_ + rai::MIN_CONFIRM_INTERVAL;
    confirms_.get<1>().modify(it, [&](rai::ConfirmInfo& data) {
        data.timestamp_ = result;
        data.hash_ = hash;
    });
    return result;
}

void rai::ConfirmManager::Age()
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto cutoff = rai::CurrentTimestamp() - rai::MIN_CONFIRM_INTERVAL;
    confirms_.erase(confirms_.begin(), confirms_.lower_bound(cutoff));
}

rai::Ptree rai::ConfirmManager::Status() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    rai::Ptree status;
    rai::Ptree confirms;
    for (auto i = confirms_.begin(), n = confirms_.end(); i != n; ++i)
    {
        rai::Ptree confirm;
        confirm.put("account", i->account_.StringAccount());
        confirm.put("height", std::to_string(i->height_));
        confirm.put("timestamp", std::to_string(i->timestamp_));
        confirm.put("hash", i->hash_.StringHex());
        confirms.push_back(std::make_pair("", confirm));
    }
    status.put_child("confirms", confirms);
    status.put("count", std::to_string(confirms_.size()));
    
    return status;
}

void rai::ActiveAccounts::Add(const rai::Account& account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto it = accounts_.find(account);
    if (it == accounts_.end())
    {
        accounts_.insert(rai::ActiveAccount{account, now});
        return;
    }

    accounts_.modify(it, [&](rai::ActiveAccount& data) { data.active_ = now; });
}

void rai::ActiveAccounts::Age()
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto cutoff =
        std::chrono::steady_clock::now() - rai::ActiveAccounts::AGE_TIME;
    accounts_.get<1>().erase(accounts_.get<1>().begin(),
                             accounts_.get<1>().lower_bound(cutoff));
}

bool rai::ActiveAccounts::Next(rai::Account& account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = accounts_.lower_bound(account);
    if (it == accounts_.end())
    {
        return true;
    }

    account = it->account_;
    return false;
}

rai::Node::Node(rai::ErrorCode& error_code, boost::asio::io_service& service,
                const boost::filesystem::path& data_path, rai::Alarm& alarm,
                const rai::NodeConfig& config, rai::Fan& key)
    : status_(rai::NodeStatus::OFFLINE),
      config_(config),
      service_(service),
      alarm_(alarm),
      key_(key),
      store_(error_code, data_path / "data.ldb"),
      ledger_(error_code, store_, rai::LedgerType::NODE,
              config.enable_rich_list_, config.enable_delegator_list_),
      network_(*this, config.address_, config.port_),
      peers_(*this),
      stopped_(ATOMIC_FLAG_INIT),
      block_processor_(*this),
      block_queries_(*this),
      elections_(*this, config.election_concurrency_),
      syncer_(*this),
      bootstrap_(*this),
      bootstrap_listener_(*this, service, config.address_, config.port_),
      subscriptions_(*this),
      rewarder_(*this, config_.forward_reward_to_, config_.daily_forward_times_),
      validator_(*this, service_, alarm_, config.validator_url_)
{
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    {
        rai::RawKey private_key;
        key_.Get(private_key);
        account_ = rai::GeneratePublicKey(private_key.data_);
    }
    rai::random_pool.GenerateBlock(secure_.bytes.data(), secure_.bytes.size());

    if (config_.callback_url_)
    {
        if (config_.callback_url_.protocol_ == "ws"
            || config_.callback_url_.protocol_ == "wss")
        {
            websocket_ = std::make_shared<rai::WebsocketClient>(
                service_, config_.callback_url_.host_,
                config_.callback_url_.port_, config_.callback_url_.path_,
                config_.callback_url_.protocol_ == "wss", true);
        }
    }

    InitLedger(error_code);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    block_processor_.block_observer_ = [this](
                                     const rai::BlockProcessResult& result,
                                     const std::shared_ptr<rai::Block>& block) {
        Background([this, result, block]() {
            observers_.block_.Notify(result, block);
        });
    };

    block_processor_.fork_observer_ =
        [this](bool add, const std::shared_ptr<rai::Block>& first,
               const std::shared_ptr<rai::Block>& second) {
            Background([this, add, first, second]() {
                observers_.fork_.Notify(add, first, second);
            });
        };

    observers_.block_.Add([this](const rai::BlockProcessResult& result,
                                 const std::shared_ptr<rai::Block>& block) {
            this->OnBlockProcessed(result, block);
    });
}

rai::Node::~Node()
{
    Stop();
}

std::shared_ptr<rai::Node> rai::Node::Shared()
{
    return shared_from_this();
}

void rai::Node::ConnectToServer()
{
    if (websocket_)
    {
        websocket_->Run();
    }
}

void rai::Node::RegisterNetworkHandler()
{
    std::weak_ptr<rai::Node> node(Shared());
    rai::Network::RegisterHandler(
        *this, [node](const rai::Endpoint& remote, rai::Stream& stream) {
            std::shared_ptr<rai::Node> node_l = node.lock();
            if (node_l)
            {
                node_l->ProcessMessage(remote, stream);
            }
        });
}

void rai::Node::Start()
{
    RegisterNetworkHandler();
    network_.Start();
    bootstrap_listener_.Start();
    rewarder_.Start();
    validator_.Start();
    Ongoing(std::bind(&rai::Node::ResolvePreconfiguredPeers, this),
            std::chrono::seconds(300));
    Ongoing(std::bind(&rai::Peers::SynCookies, &peers_),
            rai::Peers::COOKIE_CUTOFF_TIME);
    Ongoing(std::bind(&rai::Peers::Keeplive, &peers_,
                      rai::KeepliveMessage::MAX_PEERS),
            rai::Peers::KEEPLIVE_PERIOD);
    Ongoing(std::bind(&rai::Node::UpdatePeerWeights, this),
            std::chrono::seconds(60));
    Ongoing(std::bind(&rai::RecentBlocks::Age, &recent_blocks_),
            std::chrono::seconds(5));
    Ongoing(std::bind(&rai::RecentForks::Age, &recent_forks_),
            std::chrono::seconds(5));
    Ongoing(std::bind(&rai::ConfirmManager::Age, &confirm_manager_),
            std::chrono::seconds(1));
    Ongoing(std::bind(&rai::Node::AgeGapCaches, this), std::chrono::seconds(1));
    Ongoing(std::bind(&rai::Subscriptions::Cutoff, &subscriptions_),
            std::chrono::seconds(60));
    if (rewarder_.SendInterval() > 0)
    {
        Ongoing(std::bind(&rai::Rewarder::Send, &rewarder_),
                std::chrono::seconds(rewarder_.SendInterval()));
    }
    Ongoing(std::bind(&rai::Rewarder::Sync, &rewarder_),
            std::chrono::seconds(600));
    Ongoing(std::bind(&rai::ActiveAccounts::Age, &active_accounts_),
            std::chrono::seconds(10));
    if (websocket_)
    {
        websocket_->message_processor_ =
            [this](const std::shared_ptr<rai::Ptree>& message) {
                ReceiveWsMessage(message);
            };
        Ongoing(std::bind(&rai::Node::ConnectToServer, this),
                std::chrono::seconds(5));
    }
    std::cout << "Node start: " << account_.StringAccount() << std::endl;
}

void rai::Node::Stop()
{
    if (stopped_.test_and_set())
    {
        return;
    }
    if (websocket_)
    {
        websocket_->Close();
    }
    validator_.Stop();
    bootstrap_.Stop();
    bootstrap_listener_.Stop();
    alarm_.Stop();
    network_.Stop();
    rewarder_.Stop();
    block_processor_.Stop();
    block_queries_.Stop();
    elections_.Stop();
}

namespace
{
class NodeMessageVisitor : public rai::MessageVisitor
{
public:
    NodeMessageVisitor(rai::Node& node, const rai::Endpoint& sender)
        : node_(node), sender_(sender)
    {
    }
    ~NodeMessageVisitor() = default;

    void Handshake(const rai::HandshakeMessage& message) override
    {
        bool from_proxy = message.GetFlag(rai::MessageFlags::PROXY);
        rai::Endpoint peer_endpoint =
            from_proxy ? message.PeerEndpoint() : sender_;

        if (from_proxy && rai::IsReservedIp(peer_endpoint.address().to_v4()))
        {
            return;
        }

        if (message.IsRequest())
        {
            rai::Log::MessageHandshake(
                boost::str(boost::format("Received handshake request "
                                         "message from %1% with timestamp %2% "
                                         "cookie %3% account %4%")
                           % peer_endpoint % message.timestamp_
                           % message.cookie_.StringHex()
                           % message.account_.StringAccount()));
            // TODO: stat
            uint64_t now       = rai::CurrentTimestamp();
            uint64_t timestamp = message.timestamp_;
            uint64_t diff = now > timestamp ? now - timestamp : timestamp - now;
            if (diff > rai::Peers::MAX_TIMESTAMP_DIFF)
            {
                rai::Stats::Add(
                    rai::ErrorCode::HANDSHAKE_TIMESTAMP, "peer=", peer_endpoint,
                    " local timestamp=", now, " peer timestamp=", timestamp);
                return;
            }

            if (from_proxy)
            {
                rai::Cookie cookie(peer_endpoint, sender_, message.account_);
                node_.peers_.SynCookie(cookie);
                node_.HandshakeResponse(peer_endpoint, message.cookie_,
                                        sender_);
            }
            else
            {
                rai::Cookie cookie(sender_, message.account_);
                node_.peers_.SynCookie(cookie);
                node_.HandshakeResponse(sender_, message.cookie_, boost::none);
            }
        }
        else
        {
            rai::Log::MessageHandshake(
                boost::str(boost::format("Received handshake response "
                                         "message from %1% with account %2%")
                           % peer_endpoint % message.account_.StringAccount()));
            // TODO: stat
            auto cookie = node_.peers_.ValidateCookie(
                peer_endpoint, message.account_, message.signature_);
            if (cookie)
            {
                rai::Peer peer(*cookie, node_.secure_, message.Version(),
                               message.VersionMin(),
                               node_.RepWeight(message.account_));
                node_.peers_.Insert(peer);
            }
        }
    }

    void Keeplive(const rai::KeepliveMessage& message) override
    {
        bool from_proxy = message.GetFlag(rai::MessageFlags::PROXY);
        rai::Endpoint peer_endpoint =
            from_proxy ? message.PeerEndpoint() : sender_;
        auto peer = node_.peers_.Query(message.account_);

        if (message.GetFlag(rai::MessageFlags::ACK))
        {
            if (!peer)
            {
                return;
            }

            bool error = node_.peers_.ValidateKeepliveAck(message.account_,
                                                          message.hash_);
            if (error)
            {
                rai::Stats::Add(rai::ErrorCode::KEEPLIVE_ACK, "from ",
                                peer_endpoint, ", lost acks ",
                                static_cast<uint32_t>(peer->lost_acks_));
            }
            return;
        }

        if (!peer || peer->Endpoint() != peer_endpoint)
        {
            if (from_proxy)
            {
                rai::Cookie cookie(peer_endpoint, sender_, message.account_);
                node_.peers_.SynCookie(cookie);
            }
            else
            {
                rai::Cookie cookie(peer_endpoint, message.account_);
                node_.peers_.SynCookie(cookie);
            }
        }

        if (!peer)
        {
            return;
        }

        if (peer->Endpoint() != peer_endpoint && !peer->GetProxy() && !from_proxy)
        {
            rai::Stats::Add(rai::ErrorCode::NODE_ACCOUNT_DUPLICATED, "account=",
                            message.account_.StringAccount(), ", Endpoint 1=",
                            peer->Endpoint(), ", Endpoint 2=", peer_endpoint);
            if (node_.account_ == peer->account_)
            {
                std::cout << "Duplicated node account found: "
                        << message.account_.StringAccount() << std::endl;
                std::cout << "--Endpoint 1:" << peer->Endpoint() << std::endl;
                std::cout << "--Endpoint 2:" << peer_endpoint << std::endl;
            }
            return;
        }

        if (node_.peers_.CheckTimestamp(message.account_, message.timestamp_))
        {
            /* Some vps send dunplicated UDP packets
            std::cout << "Invalid keeplive timestamp from " << sender_ << std::endl;
            std::cout << "--Local timestamp:" << rai::CurrentTimestamp() << std::endl;
            std::cout << "--Remote timestamp:" << message.timestamp_ << std::endl;
            auto peer = node_.peers_.Query(message.account_);
            if (peer)
            {
                std::cout << "--Last timestamp:" << peer->timestamp_ << std::endl;
            }
            */
            return;
        }

        if (message.CheckSignature())
        {
            // TODO: stat
            std::cout << "Invalid keeplive signature from " << sender_ << std::endl;
            return;
        }

        if (from_proxy)
        {
            node_.KeepliveAck(peer_endpoint, message.Hash(), node_.account_,
                              sender_);
        }
        else
        {
            node_.KeepliveAck(peer_endpoint, message.Hash(), node_.account_,
                              boost::none);
        }

        node_.peers_.Contact(message.account_, message.timestamp_,
                             message.Version(), message.VersionMin());
        uint8_t count = 0;
        for (const auto& peer : message.peers_)
        {
            if (rai::IsReservedIp(peer.second.address().to_v4()))
            {
                ++count;
                continue;
            }

            if (!from_proxy && (count < message.ReachablePeers())
                && (peer_endpoint != peer.second))
            {
                rai::Cookie cookie(peer.second, peer_endpoint, peer.first);
                node_.peers_.SynCookie(cookie);
            }
            else
            {
                rai::Cookie cookie(peer.second, peer.first);
                node_.peers_.SynCookie(cookie);
            }

            ++count;
        }
    }

    void Publish(const rai::PublishMessage& message) override
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, node_.ledger_, false);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            // stat
            std::cout << "Failed to construct Transaction" << std::endl;
            return;
        }

        bool confirmed = false;
        if (message.NeedConfirm())
        {
            std::shared_ptr<rai::Block> block_l(nullptr);
            bool error =
                node_.ledger_.BlockGet(transaction, message.block_->Account(),
                                       message.block_->Height(), block_l);
            if (!error)
            {
                node_.Confirm(message.account_, block_l);
                confirmed = true;
            }
        }

        //check
        uint64_t constexpr day = 24 * 60 * 60;
        if (message.block_->Timestamp() < rai::CurrentTimestamp() - day)
        {
            return;
        }

        rai::AccountInfo account_info;
        bool error = node_.ledger_.AccountInfoGet(
            transaction, message.block_->Account(), account_info);
        if (!error && account_info.Valid())
        {
            std::shared_ptr<rai::Block> head(nullptr);
            error =
                node_.ledger_.BlockGet(transaction, account_info.head_, head);
            if (error || head == nullptr)
            {
                rai::Stats::Add(rai::ErrorCode::LEDGER_BLOCK_GET,
                                "NodeMessageVisitor::Publish: hash=",
                                account_info.head_.StringHex());
                return;
            }
            
            if (account_info.Restricted(head->Credit())
                && message.block_->Opcode() != rai::BlockOpcode::CREDIT)
            {
                rai::Stats::Add(rai::ErrorCode::ACCOUNT_RESTRICTED,
                                message.block_->Account().StringAccount());
                return;
            }
        }

        if (message.NeedConfirm() && !confirmed)
        {
            node_.ReceiveBlock(message.block_, message.account_);
        }
        else
        {
            node_.ReceiveBlock(message.block_, boost::none);
        }
    }

    void Relay(rai::RelayMessage& message) override
    {
        rai::Endpoint receiver(message.PeerEndpoint());
        if (!node_.peers_.Reachable(receiver))
        {
            return;
        }

        message.SetPeerEndpoint(sender_);
        node_.Send(
            message, receiver,
            [receiver](rai::Node& node, const rai::Endpoint& peer_endpoint,
                       const std::string& error) {
                // TODO: stat
                std::cout << "Failed to send relay message to " << receiver
                          << std::endl;
            });
    }

    void Confirm(const rai::ConfirmMessage& message) override
    {
        uint64_t now = rai::CurrentTimestamp();
        if (message.timestamp_ > now + rai::MAX_TIMESTAMP_DIFF * 2
            || message.timestamp_ < now - rai::MAX_TIMESTAMP_DIFF * 2)
        {
            rai::Stats::Add(rai::ErrorCode::MESSAGE_CONFIRM_TIMESTAMP);
            return;
        }

        rai::Amount weight = node_.RepWeight(message.representative_);
        if (weight < rai::QUALIFIED_REP_WEIGHT)
        {
            return;
        }

        node_.elections_.ProcessConfirm(message.representative_,
                                        message.timestamp_, message.signature_,
                                        message.block_, weight);
    }

    void Query(const rai::QueryMessage& message) override
    {
        if (message.GetFlag(rai::MessageFlags::ACK))
        {
            bool from_proxy = message.GetFlag(rai::MessageFlags::PROXY);
            rai::Endpoint peer_endpoint =
                from_proxy ? message.PeerEndpoint() : sender_;
            boost::optional<rai::Endpoint> proxy(boost::none);
            if (from_proxy)
            {
                proxy = sender_;
            }
            node_.block_queries_.ProcessQueryAck(
                message.sequence_, message.QueryBy(), message.account_,
                message.height_, message.hash_, message.QueryStatus(),
                message.block_, peer_endpoint, proxy);
        }
        else
        {
            rai::QueryMessage response(message);
            response.ClearFlag(rai::MessageFlags::RELAY);
            response.SetFlag(rai::MessageFlags::ACK);

            rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
            rai::Ledger& ledger = node_.ledger_;
            rai::Transaction transaction(error_code, ledger, false);
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                return;
            }

            bool error;
            bool account_exists = false;
            rai::AccountInfo account_info;
            if (!message.account_.IsZero())
            {
                error = ledger.AccountInfoGet(transaction, message.account_,
                                              account_info);
                account_exists = !error && account_info.Valid();
            }
            bool height_valid = message.height_ != rai::Block::INVALID_HEIGHT;

            do
            {
                rai::QueryBy by = message.QueryBy();
                if (by == rai::QueryBy::HASH)
                {
                    error = ledger.RollbackBlockGet(transaction, message.hash_,
                                                    response.block_);
                    if (!error)
                    {
                        response.SetStatus(rai::QueryStatus::SUCCESS);
                        break;
                    }

                    if (height_valid && account_exists)
                    {
                        if (message.height_ < account_info.tail_height_)
                        {
                            response.SetStatus(rai::QueryStatus::PRUNED);
                            break;
                        }
                        else if (message.height_ > account_info.head_height_)
                        {
                            response.SetStatus(rai::QueryStatus::MISS);
                            break;
                        }
                    }

                    error = ledger.BlockGet(transaction, message.hash_,
                                            response.block_);
                    if (!error)
                    {
                        response.SetStatus(rai::QueryStatus::SUCCESS);
                        break;
                    }
                }
                else if (by == rai::QueryBy::HEIGHT)
                {
                    if (!height_valid)
                    {
                        return;
                    }
                    if (!account_exists)
                    {
                        response.SetStatus(rai::QueryStatus::MISS);
                        break;
                    }
                    if (message.height_ < account_info.tail_height_)
                    {
                        response.SetStatus(rai::QueryStatus::PRUNED);
                        break;
                    }
                    else if (message.height_ > account_info.head_height_)
                    {
                        response.SetStatus(rai::QueryStatus::MISS);
                        break;
                    }
                    error = ledger.BlockGet(transaction, message.account_,
                                            message.height_, response.block_);
                    if (!error)
                    {
                        response.SetStatus(rai::QueryStatus::SUCCESS);
                        break;
                    }
                }
                else if (by == rai::QueryBy::PREVIOUS)
                {
                    if (!height_valid)
                    {
                        return;
                    }
                    if (!account_exists)
                    {
                        response.SetStatus(rai::QueryStatus::MISS);
                        break;
                    }
                    if (message.height_ < account_info.tail_height_ + 1)
                    {
                        response.SetStatus(rai::QueryStatus::PRUNED);
                        break;
                    }
                    else if (message.height_ > account_info.head_height_ + 1)
                    {
                        response.SetStatus(rai::QueryStatus::MISS);
                        break;
                    }
                    
                    rai::BlockHash successor;
                    error = ledger.BlockSuccessorGet(transaction, message.hash_,
                                                     successor);
                    if (error)
                    {
                        error = ledger.BlockGet(transaction, message.account_,
                                                message.height_ - 1,
                                                response.block_);
                        if (!error)
                        {
                            response.SetStatus(rai::QueryStatus::FORK);
                            break;
                        }
                    }
                    else
                    {
                        if (successor.IsZero())
                        {
                            response.SetStatus(rai::QueryStatus::MISS);
                            break;
                        }
                        error = ledger.BlockGet(transaction, successor,
                                                response.block_);
                        if (!error)
                        {
                            response.SetStatus(rai::QueryStatus::SUCCESS);
                            break;
                        }
                    }
                }
                else
                {
                    return;
                }
                response.SetStatus(rai::QueryStatus::MISS);
            } while (0);

            node_.Send(response, sender_,
                       [](rai::Node& node, const rai::Endpoint& peer_endpoint,
                          const std::string& error) {
                           // TODO: stat
                           std::cout << "Failed to send query_ack message to "
                                     << peer_endpoint << std::endl;
                       });
        }

    }

    void Fork(const rai::ForkMessage& message) override
    {
        node_.ReceiveBlockFork(message.first_, message.second_);
    }

    void Conflict(const rai::ConflictMessage& message) override
    {
        // TODO: stat
        rai::Amount weight = node_.RepWeight(message.representative_);
        if (weight < rai::QUALIFIED_REP_WEIGHT)
        {
            return;
        }

        node_.elections_.ProcessConflict(
            message.representative_, message.timestamp_first_,
            message.timestamp_second_, message.signature_first_,
            message.signature_second_, message.block_first_,
            message.block_second_, weight);
    }

    void Weight(const rai::WeightMessage& message) override
    {
        if (message.GetFlag(rai::MessageFlags::ACK))
        {
            node_.validator_.ProcessWeightQueryAck(message);
        }
        else
        {
            rai::WeightMessage response(message);
            response.ClearFlag(rai::MessageFlags::RELAY);
            response.SetFlag(rai::MessageFlags::ACK);
            node_.validator_.QueryWeight(message.rep_, response.epoch_,
                                         response.weight_);
            response.replier_ = node_.account_;
            node_.Send(response, sender_,
                       [](rai::Node& node, const rai::Endpoint& peer_endpoint,
                          const std::string& error) {
                           std::cout << "Failed to send weight_ack message to "
                                     << peer_endpoint << std::endl;
                       });
        }
    }

    void Crosschain(const rai::CrosschainMessage& message) override
    {
        node_.validator_.ProcessCrosschainMessage(message);
    }

private:
    rai::Node& node_;
    rai::Endpoint sender_;
};
} // namespace

void rai::Node::ProcessMessage(const rai::Endpoint& remote, rai::Stream& stream)
{
    NodeMessageVisitor visitor(*this, remote);
    rai::MessageParser parser(visitor);
    rai::ErrorCode error_code = parser.Parse(stream);
    if (rai::ErrorCode::SUCCESS == error_code)
    {
        return;
    }
    rai::Stats::Add(error_code);
}

void rai::Node::Send(const rai::Message& message, const rai::Endpoint& remote,
                     std::function<void(rai::Node&, const rai::Endpoint&,
                                        const std::string&)> error_callback)
{
    std::shared_ptr<std::vector<uint8_t>> bytes(new std::vector<uint8_t>);
    message.ToBytes(*bytes);
    dumpers_.message_.Dump(true, remote, *bytes);

    std::weak_ptr<rai::Node> node(Shared());
    rai::Endpoint peer_endpoint(remote);
    if (message.GetFlag(rai::MessageFlags::PROXY))
    {
        peer_endpoint = message.PeerEndpoint();
    }
    network_.Send(bytes->data(), bytes->size(), remote,
                  [node, bytes, peer_endpoint, error_callback](
                      const boost::system::error_code& ec, size_t size) {
                      if (!ec)
                      {
                          return;
                      }
                      auto node_l(node.lock());
                      if (!node_l)
                      {
                          return;
                      }
                      error_callback(*node_l, peer_endpoint, ec.message());
                  });
}

void rai::Node::SendToPeer(const rai::Peer& peer, rai::Message& message)
{
    SendByRoute(peer.Route(), message);
}

void rai::Node::SendToRep(const rai::Account& rep, rai::Message& message)
{
    auto route = peers_.Route(rep);
    if (!route)
    {
        return;
    }
    SendByRoute(*route, message);
}

void rai::Node::SendByRoute(const rai::Route& route, rai::Message& message)
{
    rai::Endpoint receiver(route.peer_endpoint_);
    if (route.use_proxy_)
    {
        message.EnableProxy(receiver);
        receiver = route.proxy_endpoint_;
    }
    else
    {
        message.DisableProxy();
    }

    Send(message, receiver,
         [](rai::Node& node, const rai::Endpoint& peer_endpoint,
            const std::string& error) {
             // TODO: stat
         });
}

void rai::Node::SendCallback(const rai::Ptree& notify)
{
    if (!config_.callback_url_) return;
    if (config_.callback_url_.protocol_ == "http"
        || config_.callback_url_.protocol_ == "https")
    {
        rai::HttpCallback handler = [](rai::ErrorCode error_code,
                                       const std::string&) {
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                rai::Stats::Add(error_code, "Node::SendCallback");
            }
        };

        auto http = std::make_shared<rai::HttpClient>(service_);
        rai::ErrorCode error_code =
            http->Post(config_.callback_url_, notify, handler);
        handler(error_code, "");
    }
    else if (config_.callback_url_.protocol_ == "ws"
             || config_.callback_url_.protocol_ == "wss")
    {
        if (!websocket_) return;
        websocket_->Send(notify);
    }
    else
    {
    }
}

void rai::Node::Broadcast(rai::Message& message)
{
    std::vector<rai::Peer> peers =
        peers_.RandomPeers(rai::Node::PEERS_PER_BROADCAST);
    if (peers.empty())
    {
        return;
    }

    for (const auto& peer : peers)
    {
        SendToPeer(peer, message);
    }
}

void rai::Node::BroadcastAsync(const std::shared_ptr<rai::Message>& message)
{
    std::weak_ptr<rai::Node> node_w(Shared());
    Background([node_w, message]() {
        auto node(node_w.lock());
        if (node)
        {
            node->Broadcast(*message);
        }
    });
}

void rai::Node::BroadcastFork(const std::shared_ptr<rai::Block>& first,
                            const std::shared_ptr<rai::Block>& second)
{
    std::shared_ptr<rai::Message> message(new rai::ForkMessage(first, second));
    BroadcastAsync(message);
}

void rai::Node::BroadcastConfirm(const rai::Account& representative,
                                 uint64_t timestamp,
                                 const rai::Signature& signature,
                                 const std::shared_ptr<rai::Block>& block)
{
    std::shared_ptr<rai::Message> message(
        new rai::ConfirmMessage(timestamp, representative, signature, block));
    BroadcastAsync(message);
}

void rai::Node::BroadcastConfirm(const rai::Account& account, uint64_t height)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // stat
        return;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    bool error = ledger_.BlockGet(transaction, account, height, block);
    if (error)
    {
        return;
    }

    uint64_t timestamp =
        confirm_manager_.GetTimestamp(account, height, block->Hash());
    rai::ConfirmMessage message(timestamp, account_, block);
    BroadcastConfirm(account_, timestamp, Sign(message.Hash()), block);
}

void rai::Node::BroadcastConflict(
    const rai::Account& representative, uint64_t timestamp_first,
    uint64_t timestamp_second, const rai::Signature& signature_first,
    const rai::Signature& signature_second,
    const std::shared_ptr<rai::Block>& block_first,
    const std::shared_ptr<rai::Block>& block_second)
{
    std::shared_ptr<rai::Message> message(new rai::ConflictMessage(
        representative, timestamp_first, timestamp_second, signature_first,
        signature_second, block_first, block_second));
    BroadcastAsync(message);
}

bool rai::Node::Busy() const
{
    return block_processor_.Busy() || syncer_.Busy();
}

bool rai::Node::CallbackEnabled() const
{
    return !!config_.callback_url_;
}

void rai::Node::Confirm(const rai::Account& to,
                        const std::shared_ptr<rai::Block>& block)
{
    std::vector<rai::Account> vec;
    vec.push_back(to);
    Confirm(vec, block);
}

void rai::Node::Confirm(const std::vector<rai::Account>& to,
                        const std::shared_ptr<rai::Block>& block)
{
    if (to.empty())
    {
        return;
    }

    uint64_t timestamp = confirm_manager_.GetTimestamp(
        block->Account(), block->Height(), block->Hash());
    rai::ConfirmMessage message(timestamp, account_, block);
    message.SetSignature(Sign(message.Hash()));

    for (const auto& i : to)
    {
        boost::optional<rai::Peer> peer = peers_.Query(i);
        if (!peer)
        {
            rai::Stats::Add(rai::ErrorCode::PEER_QUERY,
                            "Node::Confirm account=", i.StringAccount());
            continue;
        }
        SendToPeer(*peer, message);
    }
}

void rai::Node::RequestConfirm(const rai::Route& route,
                               const std::shared_ptr<rai::Block>& block)
{
    rai::PublishMessage publish(block, account_);
    SendByRoute(route, publish);
}

void rai::Node::RequestConfirms(const std::shared_ptr<rai::Block>& block,
                                std::unordered_set<rai::Account>&& filter)

{
    std::weak_ptr<rai::Node> node_w(Shared());
    Background([node_w, block, filter = std::move(filter)]() {
        auto node(node_w.lock());
        if (node)
        {
            std::vector<rai::Route> routes;
            node->peers_.Routes(filter, false, routes);
            for (const auto& route : routes)
            {
                node->RequestConfirm(route, block);
            }
        }
    });
}

void rai::Node::RequestConfirms(const std::shared_ptr<rai::Block>& block,
                                std::vector<rai::Account>&& targets)
{
    std::weak_ptr<rai::Node> node_w(Shared());
    Background([node_w, block, targets = std::move(targets)]() {
        auto node(node_w.lock());
        if (node)
        {
            std::vector<rai::Route> routes;
            node->peers_.Routes(targets, routes);
            for (const auto& route : routes)
            {
                node->RequestConfirm(route, block);
            }
        }
    });
}

void rai::Node::HandshakeRequest(const rai::Cookie& cookie,
                                 const boost::optional<rai::Endpoint>& proxy)
{
    rai::HandshakeMessage request(account_, cookie.cookie_);
    rai::Endpoint receiver(cookie.endpoint_);
    if (proxy)
    {
        request.EnableProxy(cookie.endpoint_);
        receiver = *proxy;
    }

    rai::Log::MessageHandshake(boost::str(
        boost::format("Send handshake request with cookie %1% to %2%")
        % cookie.cookie_.StringHex() % cookie.endpoint_));
    // TODO: stat
    Send(request, receiver,
         [](rai::Node& node, const rai::Endpoint& peer_endpoint,
            const std::string& error) {
             rai::Log::MessageHandshake(boost::str(
                 boost::format("Failed to send handshake request to %1% (%2%)")
                 % peer_endpoint % error));
             // TODO: stat
         });
}

void rai::Node::HandshakeResponse(const rai::Endpoint& peer_endpoint,
                                  const rai::uint256_union& cookie,
                                  const boost::optional<rai::Endpoint>& proxy)
{
    rai::uint512_union signature(Sign(cookie));
    rai::HandshakeMessage response(account_, signature);
    rai::Endpoint receiver(peer_endpoint);
    if (proxy)
    {
        response.EnableProxy(peer_endpoint);
        receiver = *proxy;
    }

    rai::Log::MessageHandshake(boost::str(
        boost::format("Send handshake response with account %1% signature %2% "
                      "to %3% for cookie %4% ")
        % account_.StringAccount() % signature.StringHex() % peer_endpoint
        % cookie.StringHex()));
    // TODO: stat

    Send(response, receiver,
         [](rai::Node& node, const rai::Endpoint& peer_endpoint,
            const std::string& error) {
             rai::Log::MessageHandshake(boost::str(
                 boost::format("Failed to send handshake response to %1% (%2%)")
                 % peer_endpoint % error));
             // TODO: stat
         });
}

void rai::Node::Keeplive(
    const rai::Peer& peer, const std::vector<rai::Peer>& peer_vec,
    const std::function<void(const rai::BlockHash&)>& callback)
{
    uint8_t reachable_peers = 0;
    std::vector<std::pair<rai::Account, rai::Endpoint>> peers;
    for (const auto& i : peer_vec)
    {
        if (i.GetProxy())
        {
            continue;
        }
        peers.push_back(std::make_pair(i.account_, i.Endpoint()));
        ++reachable_peers;
    }

    for (const auto& i : peer_vec)
    {
        if (!i.GetProxy())
        {
            continue;
        }
        peers.push_back(std::make_pair(i.account_, i.Endpoint()));
    }

    rai::KeepliveMessage keeplive(peers, account_, reachable_peers);
    rai::BlockHash hash(keeplive.Hash());
    rai::uint512_union signature(Sign(hash));
    keeplive.SetSignature(signature);

    callback(hash);
    SendToPeer(peer, keeplive);
}

void rai::Node::KeepliveAck(const rai::Endpoint& peer_endpoint,
                            const rai::BlockHash& hash,
                            const rai::Account& account,
                            const boost::optional<rai::Endpoint>& proxy)
{
    rai::KeepliveMessage keeplive_ack(hash, account);
    rai::Endpoint receiver(peer_endpoint);
    if (proxy)
    {
        keeplive_ack.EnableProxy(peer_endpoint);
        receiver = *proxy;
    }

    Send(keeplive_ack, receiver,
         [](rai::Node& node, const rai::Endpoint& peer_endpoint,
            const std::string& error) {
             // TODO: stat
         });
}

void rai::Node::BlockQuery(uint64_t sequence, rai::QueryBy by,
                           const rai::Account& account, uint64_t height,
                           const rai::BlockHash& hash,
                           const rai::Endpoint& peer_endpoint,
                           const boost::optional<rai::Endpoint>& proxy)
{
    rai::QueryMessage query(sequence, by, account, height, hash);
    rai::Endpoint receiver(peer_endpoint);
    if (proxy)
    {
        query.EnableProxy(peer_endpoint);
        receiver = *proxy;
    }

    Send(query, receiver,
         [](rai::Node& node, const rai::Endpoint& peer_endpoint,
            const std::string& error) {
             // TODO: stat
         });
}

void rai::Node::Publish(const std::shared_ptr<rai::Block>& block)
{
    std::shared_ptr<rai::Message> message(new rai::PublishMessage(block));
    BroadcastAsync(message);
}

void rai::Node::Push(const std::shared_ptr<rai::Block>& block)
{
    std::weak_ptr<rai::Node> node_w(Shared());
    Background([node_w, block]() {
        auto node(node_w.lock());
        if (!node) return;
        std::vector<rai::Route> routes;
        std::unordered_set<rai::Account> filter;
        node->peers_.Routes(filter, false, routes);
        rai::PublishMessage message(block);
        for (const auto& route : routes)
        {
            node->SendByRoute(route,  message);
        }
    });
}

void rai::Node::OnBlockProcessed(const rai::BlockProcessResult& result,
                                 const std::shared_ptr<rai::Block>& block)
{
    ConfirmRequestAck_(result, block);
    ElectNextFork_(result, block);
    UpdateActiveAccounts_(result, block);
    ProcessReceivable_(result, block);

    if (result.operation_ == rai::BlockOperation::DROP)
    {
        recent_blocks_.Remove(block->Hash());
    }
}

void rai::Node::Ongoing(const std::function<void()>& process,
                        const std::chrono::seconds& delay)
{
    process();
    std::weak_ptr<rai::Node> node(Shared());
    alarm_.Add(std::chrono::steady_clock::now() + delay,
               [node, process, delay]() {
                   std::shared_ptr<rai::Node> node_l = node.lock();
                   if (node_l)
                   {
                       node_l->Ongoing(process, delay);
                   }
               });
}

void rai::Node::PostJson(const rai::Url& url, const rai::Ptree& ptree)
{
    if (!url || url.protocol_ != "http")
    {
        assert(0);
        return;
    }

    std::weak_ptr<rai::Node> node(Shared());
    std::stringstream stream;
    boost::property_tree::write_json(stream, ptree);
    stream.flush();
    auto body     = std::make_shared<std::string>(stream.str());
    auto resolver = std::make_shared<boost::asio::ip::tcp::resolver>(service_);
    resolver->async_resolve(
        boost::asio::ip::tcp::resolver::query(url.host_,
                                              std::to_string(url.port_)),
        [node, url, body, resolver](
            const boost::system::error_code& ec,
            boost::asio::ip::tcp::resolver::iterator it) {
            if (ec)
            {
                rai::Stats::Add(rai::ErrorCode::DNS_RESOLVE, "Node::PostJson");
                return;
            }

            auto node_s(node.lock());
            if (node_s == nullptr) return;
            for (auto i = it, n = boost::asio::ip::tcp::resolver::iterator{};
                 i != n; ++i)
            {
                auto socket = std::make_shared<boost::asio::ip::tcp::socket>(
                    node_s->service_);
                socket->async_connect(i->endpoint(), [node, url, body, socket](
                                                         const boost::system::
                                                             error_code& ec) {
                    if (ec)
                    {
                        rai::Stats::Add(rai::ErrorCode::TCP_CONNECT,
                                        "Node::PostJson");
                        return;
                    }

                    auto req = std::make_shared<boost::beast::http::request<
                        boost::beast::http::string_body>>();
                    req->method(boost::beast::http::verb::post);
                    req->target(url.path_);
                    req->version(11);
                    req->insert(boost::beast::http::field::host, url.host_);
                    req->insert(boost::beast::http::field::content_type,
                                "application/json");
                    req->body() = *body;
                    req->prepare_payload();
                    boost::beast::http::async_write(
                        *socket, *req,
                        [node, url, socket, req](
                            const boost::system::error_code& ec, size_t size) {
                            if (ec)
                            {
                                rai::Stats::Add(rai::ErrorCode::HTTP_POST,
                                                "Node::PostJson::async_write:",
                                                ec.message());
                                return;
                            }

                            auto buffer =
                                std::make_shared<boost::beast::flat_buffer>();
                            auto response =
                                std::make_shared<boost::beast::http::response<
                                    boost::beast::http::string_body>>();
                            boost::beast::http::async_read(
                                *socket, *buffer, *response,
                                [node, url, socket, buffer, response](
                                    const boost::system::error_code& ec,
                                    size_t size) {
                                    if (ec)
                                    {
                                        rai::Stats::Add(
                                            rai::ErrorCode::HTTP_POST,
                                            "Node::PostJson::async_read:",
                                            ec.message());
                                        return;
                                    }

                                    if (response->result()
                                        != boost::beast::http::status::ok)
                                    {
                                        rai::Stats::Add(
                                            rai::ErrorCode::HTTP_POST,
                                            "Node::PostJson::response_"
                                            "status:",
                                            static_cast<uint32_t>(
                                                response->result()));
                                    }
                                });
                        });
                });
            }
        });
}

void rai::Node::PrivateKey(rai::RawKey& private_key)
{
    key_.Get(private_key);
}

void rai::Node::ResolvePreconfiguredPeers()
{
    uint16_t port = rai::Network::DEFAULT_PORT;
    for (const auto& address : config_.preconfigured_peers_)
    {
        std::weak_ptr<rai::Node> node(Shared());
        network_.Resolve(
            address, std::to_string(port),
            [node, address, port](const boost::system::error_code& ec,
                                  boost::asio::ip::udp::resolver::iterator it) {
                std::shared_ptr<rai::Node> node_l = node.lock();
                if (!node_l)
                {
                    return;
                }
                                      
                if (ec)
                {
                    rai::Log::Network(boost::str(
                        boost::format("Failed to resolve address:%1%:%2%(%3%)")
                        % address % port % ec.message()));
                    return;
                }

                for (auto i = it,
                          n = boost::asio::ip::udp::resolver::iterator{};
                     i != n; ++i)
                {
                    rai::Cookie cookie(i->endpoint());
                    node_l->peers_.SynCookie(cookie);
                }
            });
    }
}

rai::uint512_union rai::Node::Sign(const rai::uint256_union& data) const
{
    rai::RawKey private_key;
    key_.Get(private_key);
    return rai::SignMessage(private_key, account_, data);
}

void rai::Node::ReceiveBlock(const std::shared_ptr<rai::Block>& block,
                             const boost::optional<rai::Account>& confirm_to)
{
    rai::BlockHash hash = block->Hash();
    if (recent_blocks_.Exists(hash))
    {
        if (!confirm_to)
        {
            return;
        }
        bool error = confirm_requests_.Append(hash, *confirm_to);
        if (!error)
        {
            return;
        }
    }

    // CPU consuming operation
    if (block->CheckSignature())
    {
        rai::Stats::Add(rai::ErrorCode::SIGNATURE);
        return;
    }
    
    if (confirm_to)
    {
        confirm_requests_.Insert(hash, *confirm_to);
    }
    else
    {
        confirm_requests_.Insert(hash);
    }

    recent_blocks_.Insert(block->Hash());
    block_processor_.Add(block);
}

void rai::Node::ReceiveBlockFork(const std::shared_ptr<rai::Block>& first,
                                 const std::shared_ptr<rai::Block>& second)
{
    if (recent_forks_.Exists(first->Hash(), second->Hash()))
    {
        return;
    }

    recent_forks_.Insert(first->Hash(), second->Hash());
    rai::BlockFork fork{first, second, false};
    block_processor_.AddFork(fork);
}

void rai::Node::StartElection(const std::shared_ptr<rai::Block>& block)
{
    std::vector<std::shared_ptr<rai::Block>> blocks{block};
    StartElection(std::move(blocks));
}

void rai::Node::StartElection(const std::shared_ptr<rai::Block>& first,
                              const std::shared_ptr<rai::Block>& second)
{
    std::vector<std::shared_ptr<rai::Block>> blocks{first, second};
    StartElection(std::move(blocks));
}

void rai::Node::StartElection(std::vector<std::shared_ptr<rai::Block>>&& blocks)
{
    std::weak_ptr<rai::Node> node_w(Shared());
    Background([node_w, blocks = std::move(blocks)]() {
        auto node(node_w.lock());
        if (node)
        {
            node->elections_.Add(blocks);
        }
    });
}

void rai::Node::ForceConfirmBlock(std::shared_ptr<rai::Block>& block)
{
    rai::BlockForced forced(rai::BlockOperation::CONFIRM, block);
    block_processor_.AddForced(forced);
}

void rai::Node::ForceAppendBlock(std::shared_ptr<rai::Block>& block)
{
    rai::BlockForced forced(rai::BlockOperation::APPEND, block);
    block_processor_.AddForced(forced);
}

void rai::Node::QueueGapCaches(const rai::BlockHash& hash)
{
    boost::optional<rai::GapInfo> previous_gap =
        previous_gap_cache_.Query(hash);
    if (previous_gap)
    {
        block_processor_.Add(previous_gap->block_);
        previous_gap_cache_.Remove(hash);
    }

    boost::optional<rai::GapInfo> receive_source_gap =
        receive_source_gap_cache_.Query(hash);
    if (receive_source_gap)
    {
        block_processor_.Add(receive_source_gap->block_);
        receive_source_gap_cache_.Remove(hash);
    }

    boost::optional<rai::GapInfo> reward_source_gap =
        reward_source_gap_cache_.Query(hash);
    if (reward_source_gap)
    {
        block_processor_.Add(reward_source_gap->block_);
        reward_source_gap_cache_.Remove(hash);
    }
}

void rai::Node::AgeGapCaches()
{
    uint64_t cutoff = 5;
    auto previous = previous_gap_cache_.Age(cutoff);
    auto receive_source = receive_source_gap_cache_.Age(cutoff);
    auto reward_source = reward_source_gap_cache_.Age(cutoff);
    if (Status() != rai::NodeStatus::RUN)
    {
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        rai::Stats::Add(error_code, "Node::AgeGapCaches");
        return;
    }

    for (const auto& i : previous)
    {
        syncer_.SyncAccount(transaction, i.account_,
                            rai::Syncer::DEFAULT_BATCH_ID);
    }

    for (const auto& i : receive_source)
    {
        syncer_.SyncAccount(transaction, i.account_,
                            rai::Syncer::DEFAULT_BATCH_ID);
    }

    for (const auto& i : reward_source)
    {
        syncer_.SyncAccount(transaction, i.account_,
                            rai::Syncer::DEFAULT_BATCH_ID);
    }
}

rai::Amount rai::Node::RepWeight(const rai::Account& account)
{
    rai::Amount result;
    bool error = ledger_.RepWeightGet(account, result);
    if (error)
    {
        return rai::Amount(0);
    }
    return result;
}

rai::Amount rai::Node::RepWeightTotal()
{
    rai::Amount weight;
    ledger_.RepWeightTotalGet(weight);
    return weight;
}

void rai::Node::RepWeights(rai::RepWeights& result)
{
    ledger_.RepWeightsGet(result.total_, result.weights_);
}

void rai::Node::UpdatePeerWeights()
{
    auto peer_weights = peers_.PeerWeights();
    rai::Amount rep_total;
    std::unordered_map<rai::Account, rai::Amount> rep_weights;
    ledger_.RepWeightsGet(rep_total, rep_weights);
    for (const auto& i : peer_weights)
    {
        rai::Amount weight(0);
        auto it = rep_weights.find(i.first);
        if (it != rep_weights.end())
        {
            weight = it->second;
        }
        if (weight != i.second)
        {
            peers_.SetPeerWeight(i.first, weight);
        }
    }
}

bool rai::Node::IsQualifiedRepresentative()
{
    rai::Amount weight = RepWeight(account_);
    return weight >= rai::QUALIFIED_REP_WEIGHT;
}

void rai::Node::InitLedger(rai::ErrorCode& error_code)
{
    rai::Transaction transaction(error_code, ledger_, true);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    if (!ledger_.Empty(transaction))
    {
        return;
    }
    
    do
    {
        const rai::Block& block = *genesis_.block_;
        bool error = ledger_.BlockPut(transaction, block.Hash(), block);
        if (error)
        {
            error_code = rai::ErrorCode::BLOCK_PROCESS_LEDGER_BLOCK_PUT;
            break;
        }

        ledger_.RepWeightAdd(transaction, block.Representative(),
                             block.Balance());

        rai::AccountInfo info(block.Type(), block.Hash());
        error = ledger_.AccountInfoPut(transaction, block.Account(), info);
        if (error)
        {
            error_code = rai::ErrorCode::BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT;
            break;
        }
    } while (0);

    if (error_code != rai::ErrorCode::SUCCESS)
    {
        transaction.Abort();
    }
}

rai::NodeStatus rai::Node::Status() const
{
    return status_;
}

void rai::Node::SetStatus(rai::NodeStatus status)
{
    if (status == rai::NodeStatus::OFFLINE
        && status_ != rai::NodeStatus::OFFLINE)
    {
        status_ = status;
        bootstrap_.Restart();
        return;
    }

    status_ = status;
}

rai::RpcHandlerMaker rai::Node::RpcHandlerMaker()
{
    return [this](rai::Rpc& rpc, const std::string& body,
                  const boost::asio::ip::address_v4& ip,
                  const std::function<void(const rai::Ptree&)>& send_response)
               -> std::unique_ptr<rai::RpcHandler> {
        return std::make_unique<rai::NodeRpcHandler>(
            *this, rpc, body, ip, send_response);
    };
}

rai::Amount rai::Node::Supply()
{
    rai::Amount total = RepWeightTotal();
    rai::Amount genesis = rai::GenesisBalance();
    rai::Amount airdrop(genesis.Number() - rai::RAI * rai::AIRDROP_ACCOUNTS);
    if (total <= airdrop)
    {
        return rai::Amount(0);
    }

    uint64_t now = rai::CurrentTimestamp();
    if (now < rai::EpochTimestamp() + rai::AIRDROP_DURATION + 3600)
    {
        return total - airdrop;
    }

    if (now >= rai::EpochTimestamp() + rai::AIRDROP_DURATION + 3600 + 80000)
    {
        return total;
    }

    rai::uint128_t destroyed =
        airdrop.Number()
        * (now - rai::EpochTimestamp() - rai::AIRDROP_DURATION - 3600) / 80000;
    return total - rai::Amount(destroyed);
}

void rai::Node::ReceiveWsMessage(const std::shared_ptr<rai::Ptree>& message)
{
    if (!rpc_)
    {
        rai::Ptree response;
        response.put("error", "rpc not enabled");
        return;
    }
    std::stringstream ostream;
    boost::property_tree::write_json(ostream, *message);
    ostream.flush();
    rai::NodeRpcHandler handler(*this, *rpc_, ostream.str(),
                                boost::asio::ip::address_v4::any(),
                                [this](const rai::Ptree& response) {
                                    SendCallback(response);
                                });
    handler.Process();
}

boost::optional<rai::SignerAddress> rai::Node::BindingQuery(
    const rai::Account& account, rai::Chain chain)
{
    boost::optional<rai::SignerAddress> result(boost::none);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return result;
    }

    rai::SignerAddress signer;
    bool error = ledger_.BindingGet(transaction, account, chain, signer);
    if (!error)
    {
        result = signer;
    }
    return result;
}

void rai::Node::ConfirmRequestAck_(const rai::BlockProcessResult& result,
                                   const std::shared_ptr<rai::Block>& block)
{
    if (result.operation_ != rai::BlockOperation::APPEND)
    {
        return;
    }
    if (result.error_code_ != rai::ErrorCode::SUCCESS
        && result.error_code_ != rai::ErrorCode::BLOCK_PROCESS_EXISTS)
    {
        return;
    }
    auto to = confirm_requests_.Remove(block->Hash());
    Confirm(to, block);
}

void rai::Node::ElectNextFork_(const rai::BlockProcessResult& result,
                               const std::shared_ptr<rai::Block>& block)
{
    if (result.operation_ != rai::BlockOperation::CONFIRM)
    {
        return;
    }
    if (result.error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    rai::Account account(block->Account());
    uint64_t height = block->Height();
    height++;
    std::shared_ptr<rai::Block> first(nullptr);
    std::shared_ptr<rai::Block> second(nullptr);
    bool error = ledger_.NextFork(transaction, account, height, first, second);
    if (error || account != block->Account())
    {
        return;
    }

    rai::AccountInfo info;
    error = ledger_.AccountInfoGet(transaction, account, info);
    if (error || !info.Valid())
    {
        return;
    }

    if (height > info.head_height_)
    {
        return;
    }

    if (info.confirmed_height_ != rai::Block::INVALID_HEIGHT
        && info.confirmed_height_ >= height)
    {
        return;
    }

    StartElection(first, second);
}

void rai::Node::UpdateActiveAccounts_(const rai::BlockProcessResult& result,
                                      const std::shared_ptr<rai::Block>& block)
{
    if (result.error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }
    if (result.operation_ != rai::BlockOperation::APPEND
        && result.operation_ != rai::BlockOperation::ROLLBACK)
    {
        return;
    }
    active_accounts_.Add(block->Account());
}

void rai::Node::ProcessReceivable_(const rai::BlockProcessResult& result,
                                   const std::shared_ptr<rai::Block>& block)
{
    if (result.error_code_ != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    if (block->Opcode() != rai::BlockOpcode::SEND || block->Link() != account_)
    {
        return;
    }

    if (result.operation_ == rai::BlockOperation::APPEND)
    {
        StartElection(block);
    }
    else if (result.operation_ == rai::BlockOperation::CONFIRM)
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        rai::Transaction transaction(error_code, ledger_, false);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            return;
        }

        rai::ReceivableInfo info;
        bool error = ledger_.ReceivableInfoGet(transaction, account_,
                                               block->Hash(), info);
        IF_ERROR_RETURN_VOID(error);

        if (rewarder_.CanReceive(info))
        {
            rewarder_.AddReceivable(block->Hash(), info.amount_);
        }
    }
    else
    {
        // do nothing
    }
}
#pragma once

#include <queue>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/common/stat.hpp>
#include <rai/common/alarm.hpp>
#include <rai/common/log.hpp>
#include <rai/node/network.hpp>
#include <rai/node/message.hpp>
#include <rai/node/peer.hpp>
#include <rai/secure/common.hpp>
#include <rai/secure/rpc.hpp>
#include <rai/secure/websocket.hpp>
#include <rai/node/blockprocessor.hpp>
#include <rai/node/blockquery.hpp>
#include <rai/node/gapcache.hpp>
#include <rai/node/election.hpp>
#include <rai/node/syncer.hpp>
#include <rai/node/bootstrap.hpp>
#include <rai/node/subscribe.hpp>
#include <rai/node/dumper.hpp>
#include <rai/node/rewarder.hpp>
#include <rai/node/rpc.hpp>

namespace rai
{
class NodeConfig
{
public:
    NodeConfig();

    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;
    rai::ErrorCode UpgradeV1V2(rai::Ptree&) const;
    rai::ErrorCode UpgradeV2V3(rai::Ptree&) const;

    static uint32_t constexpr DEFAULT_DAILY_FORWARD_TIMES = 12;

    uint16_t port_;
    rai::LogConfig log_;
    uint32_t io_threads_;
    std::vector<std::string> preconfigured_peers_;
    rai::Url callback_url_;
    rai::Account forward_reward_to_;
    uint32_t daily_forward_times_;
    bool enable_rich_list_;
    bool enable_delegator_list_;
};

class RecentBlock
{
public:
    rai::BlockHash hash_;
    std::chrono::steady_clock::time_point arrival_;
};

class RecentBlocks
{
public:
    bool Insert(const rai::BlockHash&);
    bool Exists(const rai::BlockHash&) const;
    void Remove(const rai::BlockHash&);
    void Age();
    size_t Size();

    static size_t constexpr MAX_SIZE = 1024 * 1024;
    static std::chrono::seconds constexpr AGE_TIME = std::chrono::seconds(60);

private:
    mutable std::mutex mutex_;
    boost::multi_index_container<
        rai::RecentBlock,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                rai::RecentBlock, std::chrono::steady_clock::time_point,
                &rai::RecentBlock::arrival_>>,
            boost::multi_index::hashed_unique<boost::multi_index::member<
                rai::RecentBlock, rai::BlockHash, &rai::RecentBlock::hash_>>>>
        blocks_;
};

class RecentForks
{
public:
    bool Insert(const rai::BlockHash&, const rai::BlockHash&);
    bool Exists(const rai::BlockHash&, const rai::BlockHash&) const;
    void Remove(const rai::BlockHash&, const rai::BlockHash&);
    void Age();

private:
    mutable std::mutex mutex_;
    rai::RecentBlocks blocks_;
};

class ConfirmRequests
{
public:
    bool Append(const rai::BlockHash&, const rai::Account&);
    bool Insert(const rai::BlockHash&);
    bool Insert(const rai::BlockHash&, const rai::Account&);
    std::vector<rai::Account> Remove(const rai::BlockHash&);

    static constexpr uint32_t MAX_CONFIRMATIONS_PER_BLOCK = 8;
private:
    bool Insert_(const rai::BlockHash&, const rai::Account&);
    std::vector<rai::Account> Query_(const rai::BlockHash&);

    std::mutex mutex_;
    std::map<rai::BlockHash, std::vector<rai::Account>> items_;
};

class ConfirmInfo
{
public:
    rai::Account account_;
    uint64_t height_;
    uint64_t timestamp_;
    rai::BlockHash hash_;
};

class ConfirmManager
{
public:
    uint64_t GetTimestamp(const rai::Account&, uint64_t, const rai::BlockHash&);
    void Age();
    rai::Ptree Status() const;

private:
    mutable std::mutex mutex_;
    boost::multi_index_container<
        rai::ConfirmInfo,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                rai::ConfirmInfo, uint64_t, &rai::ConfirmInfo::timestamp_>>,
            boost::multi_index::hashed_unique<boost::multi_index::composite_key<
                rai::ConfirmInfo,
                boost::multi_index::member<rai::ConfirmInfo, rai::Account,
                                           &rai::ConfirmInfo::account_>,
                boost::multi_index::member<rai::ConfirmInfo, uint64_t,
                                           &rai::ConfirmInfo::height_>>>>>
        confirms_;
};

class ActiveAccount
{
public:
    rai::Account account_;
    std::chrono::steady_clock::time_point active_;
};

class ActiveAccounts
{
public:
    void Add(const rai::Account&);
    void Age();
    bool Next(rai::Account&);

    static std::chrono::seconds constexpr AGE_TIME = std::chrono::seconds(600);
private:
    mutable std::mutex mutex_;
    boost::multi_index_container<
        rai::ActiveAccount,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
                boost::multi_index::member<rai::ActiveAccount, rai::Account,
                                           &rai::ActiveAccount::account_>>,
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                rai::ActiveAccount, std::chrono::steady_clock::time_point,
                &rai::ActiveAccount::active_>>>>
        accounts_;
};

class Observers
{
public:
    rai::ObserverContainer<const rai::BlockProcessResult&,
                           const std::shared_ptr<rai::Block>&>
        block_;
    rai::ObserverContainer<bool, const std::shared_ptr<rai::Block>&,
                           const std::shared_ptr<rai::Block>&>
        fork_;
};

class RepWeights
{
public:
    rai::Amount total_;
    std::unordered_map<rai::Account, rai::Amount> weights_;
};

enum class NodeStatus
{
    OFFLINE = 0,
    SYNC    = 1,
    RUN     = 2,

    MAX
};

class Node : public std::enable_shared_from_this<rai::Node>
{
public:
    Node(rai::ErrorCode&, boost::asio::io_service&,
         const boost::filesystem::path&, rai::Alarm&, const rai::NodeConfig&,
         rai::Fan&);
    ~Node();
    std::shared_ptr<rai::Node> Shared();
    void ConnectToServer();
    void RegisterNetworkHandler();
    void Start();
    void Stop();
    void ProcessMessage(const rai::Endpoint&, rai::Stream&);
    void Send(const rai::Message&, const rai::Endpoint&,
              std::function<void(rai::Node&, const rai::Endpoint&,
                                 const std::string&)>);
    void SendToPeer(const rai::Peer&, rai::Message&);
    void SendByRoute(const rai::Route&, rai::Message&);
    void SendCallback(const rai::Ptree&);
    void Broadcast(rai::Message&);
    void BroadcastAsync(const std::shared_ptr<rai::Message>&);
    void BroadcastFork(const std::shared_ptr<rai::Block>&,
                       const std::shared_ptr<rai::Block>&);
    void BroadcastConfirm(const rai::Account&, uint64_t, const rai::Signature&,
                          const std::shared_ptr<rai::Block>&);
    void BroadcastConfirm(const rai::Account&, uint64_t);
    void BroadcastConflict(const rai::Account&, uint64_t, uint64_t,
                           const rai::Signature&, const rai::Signature&,
                           const std::shared_ptr<rai::Block>&,
                           const std::shared_ptr<rai::Block>&);
    bool Busy() const;
    bool CallbackEnabled() const;
    void Confirm(const rai::Account&, const std::shared_ptr<rai::Block>&);
    void Confirm(const std::vector<rai::Account>&,
                 const std::shared_ptr<rai::Block>&);
    void RequestConfirm(const rai::Route&, const std::shared_ptr<rai::Block>&);
    void RequestConfirms(const std::shared_ptr<rai::Block>&,
                         std::unordered_set<rai::Account>&&);
    void HandshakeRequest(const rai::Cookie&,
                          const boost::optional<rai::Endpoint>&);
    void HandshakeResponse(const rai::Endpoint&, const rai::uint256_union&,
                           const boost::optional<rai::Endpoint>&);
    void Keeplive(const rai::Peer&, const std::vector<rai::Peer>&,
                  const std::function<void(const rai::BlockHash&)>&);
    void KeepliveAck(const rai::Endpoint&, const rai::BlockHash&,
                     const rai::Account&,
                     const boost::optional<rai::Endpoint>&);
    void BlockQuery(uint64_t, rai::QueryBy, const rai::Account&, uint64_t,
                    const rai::BlockHash&, const rai::Endpoint&,
                    const boost::optional<rai::Endpoint>&);
    void Publish(const std::shared_ptr<rai::Block>&);
    void Push(const std::shared_ptr<rai::Block>&);
    void OnBlockProcessed(const rai::BlockProcessResult&,
                          const std::shared_ptr<rai::Block>&);
    void Ongoing(const std::function<void()>&, const std::chrono::seconds&);
    void PostJson(const rai::Url&, const rai::Ptree&);
    void PrivateKey(rai::RawKey&);
    void ResolvePreconfiguredPeers();
    rai::uint512_union Sign(const rai::uint256_union&) const;
    void ReceiveBlock(const std::shared_ptr<rai::Block>&,
                      const boost::optional<rai::Account>&);
    void ReceiveBlockFork(const std::shared_ptr<rai::Block>&,
                          const std::shared_ptr<rai::Block>&);
    void StartElection(const std::shared_ptr<rai::Block>&);
    void StartElection(const std::shared_ptr<rai::Block>&,
                       const std::shared_ptr<rai::Block>&);
    void StartElection(std::vector<std::shared_ptr<rai::Block>>&&);
    void ForceConfirmBlock(std::shared_ptr<rai::Block>&);
    void ForceAppendBlock(std::shared_ptr<rai::Block>&);
    void QueueGapCaches(const rai::BlockHash&);
    void AgeGapCaches();
    rai::Amount RepWeight(const rai::Account&);
    rai::Amount RepWeightTotal();
    void RepWeights(rai::RepWeights&);
    void UpdatePeerWeights();
    bool IsQualifiedRepresentative();
    void InitLedger(rai::ErrorCode&);
    rai::NodeStatus Status() const;
    void SetStatus(rai::NodeStatus);
    rai::RpcHandlerMaker RpcHandlerMaker();
    rai::Amount Supply();
    void ReceiveWsMessage(const std::shared_ptr<rai::Ptree>&);

    template <typename T>
    void Background(T action)
    {
        service_.post(action);
    }

    static size_t constexpr PEERS_PER_BROADCAST = 16;
 
private:
    std::atomic<rai::NodeStatus> status_;

public:
    rai::NodeConfig config_;
    boost::asio::io_service& service_;
    rai::Alarm& alarm_;
    rai::Fan& key_;
    std::shared_ptr<rai::Rpc> rpc_;
    rai::Genesis genesis_;
    rai::Account account_;
    rai::Account secure_;
    rai::Observers observers_;
    rai::Store store_;
    rai::Ledger ledger_;
    rai::Network network_;
    rai::Peers peers_;
    std::atomic_flag stopped_;
    rai::RecentBlocks recent_blocks_;
    rai::RecentForks recent_forks_;
    rai::ConfirmRequests confirm_requests_;
    rai::ConfirmManager confirm_manager_;
    rai::BlockProcessor block_processor_;
    rai::BlockQueries block_queries_;
    rai::GapCache previous_gap_cache_;
    rai::GapCache receive_source_gap_cache_;
    rai::GapCache reward_source_gap_cache_;
    rai::Elections elections_;
    rai::Syncer syncer_;
    rai::Bootstrap bootstrap_;
    rai::BootstrapListener bootstrap_listener_;
    rai::Subscriptions subscriptions_;
    rai::Dumpers dumpers_;
    rai::Rewarder rewarder_;
    rai::ActiveAccounts active_accounts_;
    std::shared_ptr<rai::WebsocketClient> websocket_;
};

} // namespace rai
#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/secure/rpc.hpp>
#include <rai/secure/ledger.hpp>

namespace rai
{
class NodeRpcConfig
{
public:
    NodeRpcConfig();
    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;
    rai::RpcConfig RpcConfig() const;

    static uint16_t constexpr DEFAULT_PORT =
        rai::RAI_NETWORK == rai::RaiNetworks::LIVE ? 7176 : 54301;

    bool enable_;
    boost::asio::ip::address_v4 address_;
    uint16_t port_;
    bool enable_control_;
    // only ips in the white list can access
    std::vector<boost::asio::ip::address_v4> whitelist_;
};

class Node;
class NodeRpcHandler : public RpcHandler
{
public:
    NodeRpcHandler(rai::Node&, rai::Rpc&, const std::string&, 
                   const boost::asio::ip::address_v4&,
                   const std::function<void(const rai::Ptree&)>&);
    virtual ~NodeRpcHandler() = default;

    void ProcessImpl() override;

    void AccountCount();
    void AccountForks();
    void AccountHeads();
    void AccountInfo();
    void AccountSubscribe();
    void AccountUnsubscribe();
    void ActiveAccountHeads();
    void BindingQuery();
    void BindingEntries();
    void BindingCount();
    void BlockConfirm();
    void BlockCount();
    void BlockDump();
    void BlockDumpOff();
    void BlockDumpOn();
    void BlockProcessorStatus();
    void BlockPublish();
    void BlockQuery();
    void BlockQueryByPrevious();
    void BlockQueryByHash();
    void BlockQueryByHeight();
    void BlocksQuery();
    void BootstrapStatus();
    void ConfirmManagerStatus();
    void DelegatorList();
    void ElectionCount();
    void ElectionInfo();
    void ElectionWeights();
    void ElectionStats();
    void ElectionActiveForks();
    void Elections();
    void EventSubscribe();
    void EventUnsubscribe();
    void Forks();
    void FullPeerCount();
    void MessageDump();
    void MessageDumpOff();
    void MessageDumpOn();
    void NodeAccount();
    void PeerCount();
    void Peers();
    void PeersGroupByVersion();
    void PeersVerbose();
    void QuerierStatus();
    void ReceivableCount();
    void Receivables();
    void Rewardable();
    void Rewardables();
    void RewarderStatus();
    void RichList();
    void Stats();
    void StatsVerbose();
    void StatsClear();
    void Stop();
    void Subscribers();
    void SubscriberCount();
    void Supply();
    void SyncerStatus();

    rai::Node& node_;

private:
    void AppendBlockAmount_(rai::Transaction&, const rai::Block&,
                            const std::string& = "");
};

}  // namespace rai
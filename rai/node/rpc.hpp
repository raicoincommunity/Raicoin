#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>

namespace rai
{
class RpcConfig
{
public:
    RpcConfig();
    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;

    bool enable_;
    boost::asio::ip::address_v4 address_;
    uint16_t port_;
    bool enable_control_;
    // only ips in the white list can access
    std::vector<boost::asio::ip::address_v4> whitelist_;
};

class Node;
class Rpc
{
public:
    Rpc(boost::asio::io_service&, rai::Node&, const rai::RpcConfig&);
    void Start();
    void Stop();
    virtual void Accept();
    bool CheckWhitelist(const boost::asio::ip::address_v4&) const;

    static uint16_t constexpr DEFAULT_PORT =
        rai::RAI_NETWORK == rai::RaiNetworks::LIVE ? 7078 : 54301;

    boost::asio::ip::tcp::acceptor acceptor_;
    rai::RpcConfig config_;
    rai::Node& node_;
    std::atomic_flag stopped_;
};

class RpcConnection : public std::enable_shared_from_this<rai::RpcConnection>
{
public:
    RpcConnection(rai::Node&, rai::Rpc&);
    virtual void Parse();
    virtual void Read();
    virtual void Write(const std::string&, unsigned);

    rai::Node& node_;
    rai::Rpc& rpc_;
    boost::asio::ip::tcp::socket socket_;
    boost::beast::flat_buffer buffer_;
    boost::beast::http::request<boost::beast::http::string_body> request_;
    boost::beast::http::response<boost::beast::http::string_body> response_;
    std::atomic_flag responded_;
};

class RpcHandler
{
public:
    RpcHandler(rai::Node&, rai::Rpc&, const std::string&, const std::string&,
               const boost::asio::ip::address_v4&,
               const std::function<void(const rai::Ptree&)>&);
    void Check();
    void Process();
    void Response();

    void AccountCount();
    void AccountInfo();
    void AccountSubscribe();
    void AccountUnsubscribe();
    void BlockCount();
    void BlockPublish();
    void BlockQuery();
    void BlockQueryByPrevious();
    void BlockQueryByHash();
    void BootstrapStatus();
    void ElectionInfo();
    void Elections();
    void MessageDump();
    void MessageDumpOff();
    void MessageDumpOn();
    void Peers();
    void PeersVerbose();
    void QuerierStatus();
    void ReceivableCount();
    void Receivables();
    void Rewardables();
    void Stats();
    void StatsVerbose();
    void StatsClear();
    void Stop();
    void SyncerStatus();

    static int constexpr MAX_JSON_DEPTH = 20;
    static uint32_t constexpr MAX_BODY_SIZE = 64 * 1024;

    rai::Node& node_;
    rai::Rpc& rpc_;
    std::string body_;
    std::string request_id_;
    boost::asio::ip::address_v4 ip_;
    std::function<void(const rai::Ptree&)> send_response_;
    rai::ErrorCode error_code_;
    rai::Ptree request_;
    rai::Ptree response_;

private:
    bool CheckControl_();
    bool GetAccount_(rai::Account&);
    bool GetCount_(uint64_t&);
    bool GetHash_(rai::BlockHash&);
    bool GetHeight_(uint64_t&);
    bool GetPrevious_(rai::BlockHash&);
    bool GetSignature_(rai::Signature&);
    bool GetTimestamp_(uint64_t&);
};

std::unique_ptr<rai::Rpc> MakeRpc(boost::asio::io_service&, rai::Node&,
                                  const rai::RpcConfig&);
}  // namespace rai
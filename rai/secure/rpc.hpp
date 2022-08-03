#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/common/blocks.hpp>

namespace rai
{
class RpcConfig
{
public:
    boost::asio::ip::address_v4 address_;
    uint16_t port_;
    bool enable_control_;
    // only ips in the white list can access
    std::vector<boost::asio::ip::address_v4> whitelist_;
};

class Rpc;
class RpcHandler;
typedef std::function<std::unique_ptr<RpcHandler>(
    rai::Rpc&, const std::string&,
    const boost::asio::ip::address_v4&,
    const std::function<void(const rai::Ptree&)>&)>
    RpcHandlerMaker;

class Rpc
{
public:
    Rpc(boost::asio::io_service&, const rai::RpcConfig&,
        const rai::RpcHandlerMaker&);
    void Start();
    void Stop();
    virtual void Accept();
    bool CheckWhitelist(const boost::asio::ip::address_v4&) const;

    boost::asio::io_service& service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    rai::RpcConfig config_;
    std::atomic_flag stopped_;
    rai::RpcHandlerMaker make_handler_;
};

class RpcConnection : public std::enable_shared_from_this<rai::RpcConnection>
{
public:
    RpcConnection(rai::Rpc&);
    virtual void Parse();
    virtual void Read();
    virtual void Write(const std::string&, unsigned);

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
    RpcHandler(rai::Rpc&, const std::string&,
               const boost::asio::ip::address_v4&,
               const std::function<void(const rai::Ptree&)>&);
    virtual ~RpcHandler() = default;
    void Check();
    void Process();
    void Response();
    void Stop();

    virtual void ProcessImpl() = 0;
    virtual void ExtraCheck(const std::string&);

    static int constexpr MAX_JSON_DEPTH = 20;
    static uint32_t constexpr MAX_BODY_SIZE = 64 * 1024;

    rai::Rpc& rpc_;
    std::string body_;
    std::string header_api_key_;
    boost::asio::ip::address_v4 ip_;
    std::function<void(const rai::Ptree&)> send_response_;
    rai::ErrorCode error_code_;
    rai::Ptree request_;
    rai::Ptree response_;

protected:
    bool CheckControl_();
    bool CheckLocal_();
    bool GetAccount_(rai::Account&);
    bool GetCount_(uint64_t&);
    bool GetHash_(rai::BlockHash&);
    bool GetHeight_(uint64_t&);
    bool GetPrevious_(rai::BlockHash&);
    bool GetSignature_(rai::Signature&);
    bool GetTimestamp_(uint64_t&);
    bool GetNext_(rai::Account&);
    bool GetAccountTypes_(std::vector<rai::BlockType>&);
    bool GetChain_(rai::Chain&);
    bool GetChainById_(rai::Chain&);
    bool GetChainOrId_(rai::Chain&);
};

std::unique_ptr<rai::Rpc> MakeRpc(boost::asio::io_service&,
                                  const rai::RpcConfig&,
                                  const rai::RpcHandlerMaker&);

}  // namespace rai
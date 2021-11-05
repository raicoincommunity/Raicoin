#pragma once

#include <boost/asio.hpp>
#include <xxhash/xxhash.h>
#include <rai/common/parameters.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/common/numbers.hpp>

namespace rai
{
using UdpEndpoint = boost::asio::ip::udp::endpoint;
using Endpoint = UdpEndpoint;

std::string ToString(const rai::Endpoint&);

class Node;
class UdpNetwork
{
public:
    UdpNetwork(rai::Node&, const rai::IP&, uint16_t);
    void Receive();
    void Start();
    void Stop();
    void Process(const boost::system::error_code&, size_t);
    void Send(const uint8_t*, size_t, const rai::Endpoint&,
              std::function<void(const boost::system::error_code&, size_t)>);
    void Resolve(const std::string&, const std::string&,
                 std::function<void(const boost::system::error_code&,
                                    boost::asio::ip::udp::resolver::iterator)>);

    static uint16_t constexpr DEFAULT_PORT =
        rai::RAI_NETWORK == rai::RaiNetworks::LIVE ? 7175 : 54300;

    typedef std::function<void(const rai::Endpoint&, rai::Stream&)> Handler;
    static void RegisterHandler(rai::Node&, const Handler&);

private:
    rai::Endpoint remote_;
    std::array<uint8_t, 1024> buffer_;
    boost::asio::ip::udp::socket socket_;
    std::mutex socket_mutex_;
    boost::asio::ip::udp::resolver resolver_;
    rai::Node& node_;
    std::atomic<bool> on_;
    Handler handler_;
};
using Network = UdpNetwork;

class UdpProxy
{
public:
    UdpProxy(const rai::Endpoint&, const rai::Account&, const rai::Account&);
    rai::Endpoint Endpoint() const;
    rai::IP Ip() const;
    uint32_t Priority() const;
    bool operator==(const rai::UdpProxy&) const;
    bool operator!=(const rai::UdpProxy&) const;

private:
    uint32_t Priority_(const rai::Account&, const rai::Account&) const;
    rai::UdpEndpoint endpoint_;
    uint32_t priority_;
};
using Proxy = UdpProxy;

class Route
{
public:
    bool use_proxy_;
    rai::Endpoint peer_endpoint_;
    rai::Endpoint proxy_endpoint_;
};

using TcpEndpoint = boost::asio::ip::tcp::endpoint;
class TcpSocket : public std::enable_shared_from_this<rai::TcpSocket>
{
public:
    TcpSocket(const std::shared_ptr<rai::Node>&);
    void AsyncConnect(
        const rai::TcpEndpoint&,
        const std::function<void(const boost::system::error_code&)>&);
    void AsyncRead(
        std::vector<uint8_t>&, size_t,
        const std::function<void(const boost::system::error_code&, size_t)>&);
    void AsyncWrite(
        std::vector<uint8_t>&,
        const std::function<void(const boost::system::error_code&, size_t)>&);
    void Start();
    void Stop();
    void Close();
    rai::TcpEndpoint Remote();

    static std::chrono::seconds constexpr TIMEOUT = std::chrono::seconds(16);

    std::atomic<uint32_t> ticket_;
    std::shared_ptr<rai::Node> node_;
    boost::asio::ip::tcp::socket socket_;
};
using Socket = TcpSocket;

bool IsReservedIp(const rai::IP&);
} // namespace rai

namespace boost
{
template <>
struct hash<rai::UdpEndpoint>
{
    size_t operator()(const rai::UdpEndpoint& data) const
    {
        XXH64_state_t hash;
        XXH64_reset(&hash, 0);
        auto bytes = data.address().to_v4().to_bytes();
        XXH64_update(&hash, bytes.data(), bytes.size());
        auto port = data.port();
        XXH64_update(&hash, &port, sizeof(port));
        auto result = XXH64_digest(&hash);
        return result;
    }
};
}  // namespace boost

namespace std
{
template <>
struct hash<rai::IP>
{
    size_t operator()(const rai::IP& data) const
    {
        return data.to_ulong();
    }
};
}

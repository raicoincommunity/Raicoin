#include <rai/node/network.hpp>

#include <boost/format.hpp>
#include <rai/node/node.hpp>

std::chrono::seconds constexpr rai::Socket::TIMEOUT;

rai::UdpNetwork::UdpNetwork(rai::Node& node, uint16_t port)
    : socket_(node.service_,
              rai::Endpoint(boost::asio::ip::address_v4::any(), port)),
      resolver_(node.service_),
      node_(node),
      on_(true)
{
}

void rai::UdpNetwork::Receive()
{
    if (!on_)
    {
        rai::Log::NetworkReceive(node_, "Receiving packet stopped");
        return;
    }
    rai::Log::NetworkReceive(node_, "Receiving packet");

    std::unique_lock<std::mutex> lock(socket_mutex_);
    socket_.async_receive_from(
        boost::asio::buffer(buffer_.data(), buffer_.size()), remote_,
        [this](const boost::system::error_code& error, size_t size) {
            Process(error, size);
        });
}

void rai::UdpNetwork::Start()
{
    Receive();
}

void rai::UdpNetwork::Stop()
{
    on_ = false;
    std::unique_lock<std::mutex> lock(socket_mutex_);
    socket_.close();
    resolver_.cancel();
}

void rai::UdpNetwork::Process(const boost::system::error_code& error, size_t size)
{
    if (!on_)
    {
        return;
    }

    if (error)
    {
        rai::Log::Network(node_,
                          boost::str(boost::format("UDP Receive error: %1%")
                                     % error.message()));
        Receive();
        return;
    }

    if (size == 0 || size > 1024)
    {
        // TODO: stat
        Receive();
        return;
    }

    if (rai::IsReservedIp(remote_.address().to_v4()))
    {
        // TODO: stat
        Receive();
        return;
    }

    std::cout << "<<< Receive message from " << remote_ << ", size " << size << std::endl;
    std::cout << "----------message begin-------------" << std::endl;
    rai::DumpBytes(buffer_.data(), size);
    std::cout << "----------message end---------------" << std::endl;

    if (handler_)
    {
        rai::BufferStream stream(buffer_.data(), size);
        handler_(remote_, stream);
    }

    Receive();
}

void rai::UdpNetwork::Send(
    const uint8_t* data, size_t size, const rai::Endpoint& remote,
    std::function<void(const boost::system::error_code&, size_t)> callback)
{
    std::unique_lock<std::mutex> lock(socket_mutex_);

    std::cout << ">>> Send message to " << remote << ", size " << size << std::endl;
    std::cout << "----------message begin:-------------" << std::endl;
    rai::DumpBytes(data, size);
    std::cout << "----------message end----------------" << std::endl;

    rai::Log::NetworkSend(
        node_, boost::str(boost::format("Sending packet, size %1%") % size));
    socket_.async_send_to(
        boost::asio::buffer(data, size), remote,
        [this, callback](const boost::system::error_code& ec, size_t size) {
            callback(ec, size);
            rai::Log::NetworkSend(node_, "Packet sent");
            // TODO: stat
        });
}

void rai::UdpNetwork::Resolve(
    const std::string& address, const std::string& port,
    std::function<void(const boost::system::error_code&,
                       boost::asio::ip::udp::resolver::iterator)>
        callback)
{
    resolver_.async_resolve(
        boost::asio::ip::udp::resolver::query(address, port),
        [callback](const boost::system::error_code& ec,
                   boost::asio::ip::udp::resolver::iterator it) {
            callback(ec, it);
        });
}

void rai::UdpNetwork::RegisterHandler(rai::Node& node, const Handler& handler)
{
    node.network_.handler_ = handler;
}

rai::UdpProxy::UdpProxy(const rai::Endpoint& proxy,
                        const rai::Account& node_account,
                        const rai::Account& target_account)
    : endpoint_(proxy),
      priority_(Priority_(node_account, target_account))
{
}

rai::Endpoint rai::UdpProxy::Endpoint() const
{
    return endpoint_;
}

rai::IP rai::UdpProxy::Ip() const
{
    return endpoint_.address().to_v4();
}

uint32_t rai::UdpProxy::Priority() const
{
    return priority_;
}

bool rai::UdpProxy::operator==(const rai::UdpProxy& other) const
{
    return endpoint_ == other.endpoint_;
}

bool rai::UdpProxy::operator!=(const rai::UdpProxy& other) const
{
    return !(*this == other);
}

uint32_t rai::UdpProxy::Priority_(const rai::Account& node_account,
                                  const rai::Account& target_account) const
{
    uint32_t result = 0;
    result ^= endpoint_.address().to_v4().to_uint();
    for (const auto& i : node_account.dwords)
    {
        result ^= i;
    }
    for (const auto& i : target_account.dwords)
    {
        result ^= i;
    }
    return result;
}

rai::TcpSocket::TcpSocket(const std::shared_ptr<rai::Node>& node)
    : ticket_(0), node_(node), socket_(node->service_)
{
}

void rai::TcpSocket::AsyncConnect(
    const rai::TcpEndpoint& remote,
    std::function<void(const boost::system::error_code&)>& callback)
{
    std::shared_ptr<rai::TcpSocket> this_s = shared_from_this();
    Start();
    socket_.async_connect(
        remote, [this_s, callback](const boost::system::error_code& ec) {
            this_s->Stop();
            callback(ec);
        });
}

void rai::TcpSocket::AsyncRead(
    std::shared_ptr<std::vector<uint8_t>>& buffer, size_t size,
    std::function<void(const boost::system::error_code&, size_t)>& callback)
{
    assert(size <= buffer->size());
    std::shared_ptr<rai::TcpSocket> this_s = shared_from_this();
    Start();
    boost::asio::async_read(
        socket_, boost::asio::buffer(buffer->data(), size),
        [this_s, callback](const boost::system::error_code& ec, size_t size) {
            this_s->Stop();
            callback(ec, size);
        });
}

void rai::TcpSocket::AsyncWrite(
    std::shared_ptr<std::vector<uint8_t>>& buffer,
    std::function<void(const boost::system::error_code&, size_t)>& callback)
{
    std::shared_ptr<rai::TcpSocket> this_s = shared_from_this();
    Start();
    boost::asio::async_write(
        socket_, boost::asio::buffer(buffer->data(), buffer->size()),
        [this_s, callback](const boost::system::error_code& ec, size_t size) {
            this_s->Stop();
            callback(ec, size);
        });
}

void rai::TcpSocket::Start()
{
    uint32_t ticket = ++ticket_;
    std::weak_ptr<rai::TcpSocket> this_w(shared_from_this());
    node_->alarm_.Add(
        std::chrono::steady_clock::now() + rai::TcpSocket::TIMEOUT,
        [this_w, ticket]() {
            std::shared_ptr<rai::TcpSocket> this_l = this_w.lock();
            if (!this_l || this_l->ticket_ != ticket)
            {
                return;
            }
            this_l->socket_.close();
            //TODO:log
        });
}

void rai::TcpSocket::Stop()
{
    ++ticket_;
}

void rai::TcpSocket::Close()
{
    socket_.close();
}

rai::TcpEndpoint rai::TcpSocket::Remote()
{
    try
    {
        return socket_.remote_endpoint();
    }
    catch(...)
    {
        return rai::TcpEndpoint(boost::asio::ip::make_address_v4("0.0.0.0"),0);
    }
}


bool rai::IsReservedIp(const rai::IP& ip)
{
    bool result = false;
    static const rai::IP rfc1700_min(0x00000000ul);
    static const rai::IP rfc1700_max(0x00fffffful);
    static const rai::IP loopback_min(0x7f000000ul);
    static const rai::IP loopback_max(0x7ffffffful);
    static const rai::IP rfc1918_1_min(0x0a000000ul);
    static const rai::IP rfc1918_1_max(0x0afffffful);
    static const rai::IP rfc1918_2_min(0xac100000ul);
    static const rai::IP rfc1918_2_max(0xac1ffffful);
    static const rai::IP rfc1918_3_min(0xc0a80000ul);
    static const rai::IP rfc1918_3_max(0xc0a8fffful);
    static const rai::IP rfc5737_1_min(0xc0000200ul);
    static const rai::IP rfc5737_1_max(0xc00002fful);
    static const rai::IP rfc5737_2_min(0xc6336400ul);
    static const rai::IP rfc5737_2_max(0xc63364fful);
    static const rai::IP rfc5737_3_min(0xcb007100ul);
    static const rai::IP rfc5737_3_max(0xcb0071fful);
    static const rai::IP rfc6598_min(0x64400000ul);
    static const rai::IP rfc6598_max(0x647ffffful);
    static const rai::IP multicast_min(0xe0000000ul);
    static const rai::IP multicast_max(0xeffffffful);
    static const rai::IP rfc6890_min(0xf0000000ul);
    static const rai::IP rfc6890_max(0xfffffffful);

    if ((ip >= rfc1700_min) && (ip <= rfc1700_max))
    {
        result = true;
    }
    else if ((ip >= loopback_min) && (ip <= loopback_max))
    {
        result = true;
    }
    else if ((ip >= rfc1918_1_min) && (ip <= rfc1918_1_max))
    {
        result = true;
    }
    else if ((ip >= rfc1918_2_min) && (ip <= rfc1918_2_max))
    {
        result = true;
    }
    else if ((ip >= rfc1918_3_min) && (ip <= rfc1918_3_max))
    {
        result = true;
    }
    else if ((ip >= rfc5737_1_min) && (ip <= rfc5737_1_max))
    {
        result = true;
    }
    else if ((ip >= rfc5737_2_min) && (ip <= rfc5737_2_max))
    {
        result = true;
    }
    else if ((ip >= rfc5737_3_min) && (ip <= rfc5737_3_max))
    {
        result = true;
    }
    else if ((ip >= rfc6598_min) && (ip <= rfc6598_max))
    {
        result = true;
    }
    else if ((ip >= multicast_min) && (ip <= multicast_max))
    {
        result = true;
    }
    else if ((ip >= rfc6890_min) && (ip <= rfc6890_max))
    {
        result = true;
    }
    else
    {
    }

    return result;
}

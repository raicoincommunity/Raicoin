#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <mutex>
#include <deque>
#include <rai/common/util.hpp>
#include <rai/common/numbers.hpp>


namespace rai
{
class Wallets;

enum class WebsocketStatus
{
    DISCONNECTED = 0,
    CONNECTING   = 1,
    CONNECTED    = 2,
};

using WssStream = boost::beast::websocket::stream<
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>;

using WsStream = boost::beast::websocket::stream<boost::asio::ip::tcp::socket>;

class WebsocketClient
    : public std::enable_shared_from_this<rai::WebsocketClient>
{
public:
    WebsocketClient(boost::asio::io_service&, const std::string&, uint16_t,
                    const std::string&, bool = true);
    ~WebsocketClient();

    void OnResolve(const boost::system::error_code&,
                   boost::asio::ip::tcp::resolver::iterator);
    void OnConnect(const boost::system::error_code&);
    void OnSslHandshake(const boost::system::error_code&);
    void OnWebsocketHandshake(const boost::system::error_code&);
    void Run();
    void Send(const std::string&);
    void Send(const rai::Ptree&);
    std::shared_ptr<rai::WebsocketClient> Shared();
    void OnSend(uint32_t, const boost::system::error_code&, size_t);
    void OnReceive(uint32_t, const boost::system::error_code&, size_t);
    void Close();
    size_t Size() const;
    bool Busy() const;
    rai::WebsocketStatus Status() const;

    std::function<void(rai::WebsocketStatus)> status_observer_;
    std::function<void(const std::shared_ptr<rai::Ptree>&)> message_processor_;
private:
    void CloseStream_();
    void Send_();
    void Receive_();
    void ChangeStatus_(rai::WebsocketStatus);

    static size_t constexpr MAX_QUEUE_SIZE = 2048;

    boost::asio::io_service& service_;
    bool ssl_;
    std::string host_;
    uint16_t port_;
    std::string path_;
    mutable std::mutex mutex_;
    bool stopped_;
    rai::WebsocketStatus status_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ssl::context ctx_;
    uint32_t session_id_;
    std::shared_ptr<rai::WssStream> wss_stream_;
    std::shared_ptr<rai::WsStream> ws_stream_;
    std::shared_ptr<boost::beast::multi_buffer> receive_buffer_;
    bool sending_;
    std::deque<std::string> send_queue_;
    std::chrono::steady_clock::time_point connect_at_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
};


class WebsocketServer;

class WebsocketSession
    : public std::enable_shared_from_this<rai::WebsocketSession>
{
public:
    WebsocketSession(const std::shared_ptr<rai::WebsocketServer>&,
                     const rai::UniqueId&,
                     boost::asio::ip::tcp::socket&& socket);
    ~WebsocketSession();

    std::shared_ptr<rai::WebsocketSession> Shared();

    void Start();
    void Stop();
    void OnAccept(const boost::system::error_code&);
    void OnSend(const boost::system::error_code&, size_t);
    void OnReceive(const boost::system::error_code&, size_t);
    void Send(const std::string&);
    void Send(const rai::Ptree&);
    rai::UniqueId Uid() const;

    std::shared_ptr<rai::WebsocketServer> server_;

private:
    void Close_(std::unique_lock<std::mutex>&);
    void Receive_(std::unique_lock<std::mutex>&);
    void Send_(std::unique_lock<std::mutex>&);

    static size_t constexpr MAX_QUEUE_SIZE = 64;

    rai::UniqueId id_;
    mutable std::mutex mutex_;
    bool stopped_;
    bool closed_;
    bool sending_;
    bool connected_;
    rai::WsStream ws_;
    std::deque<std::string> send_queue_;
    boost::beast::multi_buffer receive_buffer_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
};

class WebsocketServer
    : public std::enable_shared_from_this<rai::WebsocketServer>
{
public:
    WebsocketServer(boost::asio::io_service&, const rai::IP&, uint16_t);

    std::shared_ptr<rai::WebsocketServer> Shared();

    void Accept();
    void OnAccept(const boost::system::error_code&,
                  const std::shared_ptr<boost::asio::ip::tcp::socket>&);
    void Erase(const rai::UniqueId&);
    void Start();
    void Stop();
    std::shared_ptr<rai::WebsocketSession> Session(const rai::UniqueId&);
    void Send(const rai::UniqueId&, const std::string&);
    void Send(const rai::UniqueId&, const rai::Ptree&);
    std::vector<rai::UniqueId> Clients() const;

    std::function<void(const std::string&, const rai::UniqueId&)>
        message_observer_;
    std::function<void(const rai::UniqueId&, bool)> session_observer_;

    static size_t constexpr MAX_SESSIONS = 1024;

private:
    boost::asio::io_service& service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::endpoint local_;

    mutable std::mutex mutex_;
    bool stopped_;
    std::unordered_map<rai::UniqueId, std::weak_ptr<rai::WebsocketSession>>
        sessions_;
};

}  // namespace rai
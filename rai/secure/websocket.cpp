#include <rai/secure/websocket.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <rai/common/util.hpp>
#include <rai/common/stat.hpp>
#include <rai/secure/plat.hpp>

rai::WebsocketClient::WebsocketClient(boost::asio::io_service& service,
                                      const std::string& host, uint16_t port,
                                      const std::string& path, bool ssl,
                                      bool keeplive)
    : service_(service),
      ssl_(ssl),
      keeplive_(keeplive),
      host_(host),
      port_(port),
      path_(path),
      stopped_(false),
      status_(rai::WebsocketStatus::DISCONNECTED),
      resolver_(service_),
      ctx_(boost::asio::ssl::context::tlsv12_client),
      session_id_(0),
      sending_(false),
      connect_at_(),
      keeplive_sent_at_(),
      keeplive_received_at_(),
      strand_(service_.get_executor())
{
    if (ssl)
    {
        try
        {
            std::string pem_path = rai::PemPath();
            ctx_.load_verify_file(pem_path);
        }
        catch (...)
        {
            throw std::runtime_error("Failed to load cacert.pem");
        }
    }
}

rai::WebsocketClient::~WebsocketClient()
{
    Close();
}

void rai::WebsocketClient::OnResolve(
    uint32_t session_id,
    const boost::system::error_code& ec,
    boost::asio::ip::tcp::resolver::iterator results)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_ || session_id != session_id_)
    {
        return;
    }

    if (ec)
    {
        // log
        std::cout << "Failed to resolve: " << ec.message() << std::endl;
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
        CloseStream_();
        return;
    }

    std::weak_ptr<rai::WebsocketClient> client_w(Shared());
    if (ssl_)
    {
        std::shared_ptr<rai::WssStream> stream(wss_stream_);
        boost::asio::async_connect(
            wss_stream_->lowest_layer(), results,
            [client_w, stream, session_id](
                const boost::system::error_code& ec,
                boost::asio::ip::tcp::resolver::iterator iterator) {
                auto client = client_w.lock();
                if (!client)
                {
                    return;
                }

                client->OnConnect(session_id, ec);
            });
    }
    else
    {
        std::shared_ptr<rai::WsStream> stream(ws_stream_);
        boost::asio::async_connect(
            ws_stream_->lowest_layer(), results,
            [client_w, stream, session_id](
                const boost::system::error_code& ec,
                boost::asio::ip::tcp::resolver::iterator iterator) {
                auto client = client_w.lock();
                if (!client)
                {
                    return;
                }

                client->OnConnect(session_id, ec);
            });
    }
}

void rai::WebsocketClient::OnConnect(uint32_t session_id,
                                     const boost::system::error_code& ec)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_ || session_id != session_id_)
    {
        return;
    }

    if (ec)
    {
        // log
        std::cout << "Failed to connect: " << ec.message() << std::endl;
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
        CloseStream_();
        return;
    }

    std::weak_ptr<rai::WebsocketClient> client_w(Shared());
    if (ssl_)
    {
        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (!SSL_set_tlsext_host_name(wss_stream_->next_layer().native_handle(),
                                      host_.c_str()))
        {
            // log
            std::cout << "Failed to set SNI: " << host_ << std::endl;
            ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
            CloseStream_();
            return;
        }

        wss_stream_->next_layer().set_verify_mode(
            boost::asio::ssl::verify_peer
            | boost::asio::ssl::verify_fail_if_no_peer_cert);
        wss_stream_->next_layer().set_verify_callback(
            boost::asio::ssl::rfc2818_verification(host_));

        std::shared_ptr<rai::WssStream> stream(wss_stream_);
        wss_stream_->next_layer().async_handshake(
            boost::asio::ssl::stream_base::client,
            [client_w, stream,
             session_id](const boost::system::error_code& ec) {
                auto client = client_w.lock();
                if (!client)
                {
                    return;
                }

                client->OnSslHandshake(session_id, ec);
            });
    }
    else
    {
        std::shared_ptr<rai::WsStream> stream(ws_stream_);
        ws_stream_->async_handshake(
            host_, path_,
            [client_w, stream,
             session_id](const boost::system::error_code& ec) {
                auto client = client_w.lock();
                if (!client)
                {
                    return;
                }

                client->OnWebsocketHandshake(session_id, ec);
            });
    }
}

void rai::WebsocketClient::OnSslHandshake(uint32_t session_id,
                                          const boost::system::error_code& ec)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_ || session_id != session_id_)
    {
        return;
    }

    if (ec)
    {
        // log
        std::cout << "SSL handshake failed: " << ec.message() << std::endl;
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
        CloseStream_();
        return;
    }

    std::weak_ptr<rai::WebsocketClient> client_w(Shared());
    std::shared_ptr<rai::WssStream> stream(wss_stream_);
    wss_stream_->async_handshake(
        host_, path_,
        [client_w, stream, session_id](const boost::system::error_code& ec) {
            auto client = client_w.lock();
            if (!client)
            {
                return;
            }

            client->OnWebsocketHandshake(session_id, ec);
        });
}

void rai::WebsocketClient::OnWebsocketHandshake(
    uint32_t session_id, const boost::system::error_code& ec)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_ || session_id != session_id_)
    {
        return;
    }

    if (ec)
    {
        // log
        std::cout << "Websocket handshake failed: " << ec.message()
                  << std::endl;
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
        CloseStream_();
        return;
    }
    ChangeStatus_(rai::WebsocketStatus::CONNECTED);
    if (keeplive_)
    {
        keeplive_sent_at_ = std::chrono::steady_clock::time_point();
        keeplive_received_at_ = std::chrono::steady_clock::time_point();
    }
    std::cout << "Websocket connected: " << Url_() << std::endl;

    TrySend_();
    Receive_();
}

void rai::WebsocketClient::Send(const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_ || message.empty())
    {
        return;
    }

    if (send_queue_.size() >= rai::WebsocketClient::MAX_QUEUE_SIZE)
    {
        std::cout << "Send queue overflow, the message has been dropped:"
                  << std::endl;
        std::cout << message << std::endl;
        return;
    }

    send_queue_.push_back(message);
    TrySend_();
}

void rai::WebsocketClient::Send(const rai::Ptree& message)
{
    std::stringstream ostream;
    boost::property_tree::write_json(ostream, message);
    ostream.flush();
    Send(ostream.str());
}

std::shared_ptr<rai::WebsocketClient> rai::WebsocketClient::Shared()
{
    return shared_from_this();
}

void rai::WebsocketClient::OnSend(uint32_t session_id,
                                  const boost::system::error_code& ec,
                                  size_t size)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_ || session_id != session_id_
        || status_ != rai::WebsocketStatus::CONNECTED)
    {
        return;
    }

    if (ec || size == 0)
    {
        // log
        std::cout << "OnSend ec:" << ec.message() << std::endl;
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
        CloseStream_();
        return;
    }

    send_queue_.pop_front();
    Send_();
}

void rai::WebsocketClient::OnReceive(uint32_t session_id,
                                     const boost::system::error_code& ec,
                                     size_t size)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (stopped_ || session_id != session_id_
        || status_ != rai::WebsocketStatus::CONNECTED)
    {
        std::cout << "OnReceive stopped"<< std::endl;
        return;
    }

    if (ec || size == 0)
    {
        // log
        std::cout << "OnReceive ec:" << ec.message() << std::endl;
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
        CloseStream_();
        return;
    }

    std::stringstream os;
    os << boost::beast::buffers(receive_buffer_->data());
    receive_buffer_->consume(receive_buffer_->size());


    auto message = std::make_shared<rai::Ptree>();
    try
    {
        boost::property_tree::read_json(os, *message);
        auto ack_o = message->get_optional<std::string>("ack");
        if (ack_o && *ack_o == "keeplive")
        {
            keeplive_received_at_ = std::chrono::steady_clock::now();
        }
        else if (message_processor_)
        {
            lock.unlock();
            message_processor_(message);
            lock.lock();
        }
    }
    catch(const std::exception& e)
    {
        std::cout << e.what() << '\n';
        // log
    }

    if (stopped_ || session_id != session_id_
        || status_ != rai::WebsocketStatus::CONNECTED)
    {
        std::cout << "OnReceive stopped"<< std::endl;
        return;
    }

    Receive_();
}

void rai::WebsocketClient::Close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_)
    {
        return;
    }
    stopped_ = true;
    if ((!wss_stream_ && !ws_stream_)
        || status_ == rai::WebsocketStatus::DISCONNECTED)
    {
        return;
    }
    ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
    CloseStream_();
}

size_t rai::WebsocketClient::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return send_queue_.size();
}

bool rai::WebsocketClient::Busy() const
{
    return Size() >= rai::WebsocketClient::MAX_QUEUE_SIZE / 2;
}

rai::WebsocketStatus rai::WebsocketClient::Status() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

void rai::WebsocketClient::Run()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_)
    {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (status_ == rai::WebsocketStatus::CONNECTED)
    {
        if (!keeplive_) return;
        if (keeplive_sent_at_ == std::chrono::steady_clock::time_point()
            || keeplive_sent_at_ + std::chrono::seconds(60) < now)
        {
            SendKeeplive_();
            keeplive_sent_at_ = now;
        }
        if (keeplive_received_at_ == std::chrono::steady_clock::time_point())
        {
            keeplive_received_at_ = now;
        }
        if (keeplive_received_at_ + std::chrono::seconds(300) >= now)
        {
            return;
        }
    }

    auto cutoff = now - std::chrono::seconds(10);
    if (status_ == rai::WebsocketStatus::CONNECTING && connect_at_ >= cutoff)
    {
        return;
    }
    connect_at_ = now;
    ChangeStatus_(rai::WebsocketStatus::CONNECTING);

    CloseStream_();
    if (ssl_)
    {
        wss_stream_ = std::make_shared<rai::WssStream>(service_, ctx_);
    }
    else
    {
        ws_stream_ = std::make_shared<rai::WsStream>(service_);
    }
    receive_buffer_ = std::make_shared<boost::beast::multi_buffer>();
    session_id_++;
    sending_ = false;

    uint32_t session_id(session_id_);
    std::weak_ptr<rai::WebsocketClient> client_w(Shared());
    boost::asio::ip::tcp::resolver::query query(host_, std::to_string(port_));
    resolver_.async_resolve(
        query, [client_w, session_id](const boost::system::error_code& ec,
                          boost::asio::ip::tcp::resolver::iterator results) {
            auto client = client_w.lock();
            if (!client)
            {
                return;
            }

            client->OnResolve(session_id, ec, results);
        });
}

void rai::WebsocketClient::CloseStream_()
{
    if (ssl_)
    {
        if (wss_stream_ == nullptr)
        {
            return;
        }

        std::shared_ptr<rai::WssStream> stream(std::move(wss_stream_));
        wss_stream_ = nullptr;
        try
        {
            stream->async_close(
                boost::beast::websocket::close_code::normal,
                boost::asio::bind_executor(
                    strand_,
                    [stream](const boost::system::error_code& ec) {
                        std::cout << "ws stream closed:" << ec.message()
                                  << std::endl;
                    }));
        }
        catch (...)
        {
        }
    }
    else
    {
        if (ws_stream_ == nullptr)
        {
            return;
        }

        std::shared_ptr<rai::WsStream> stream(std::move(ws_stream_));
        ws_stream_ = nullptr;
        try
        {
            stream->async_close(
                boost::beast::websocket::close_code::normal,
                boost::asio::bind_executor(
                    strand_, [stream](const boost::system::error_code& ec) {
                        std::cout << "ws stream closed:" << ec.message()
                                  << std::endl;
                    }));
        }
        catch (...)
        {
        }
    }
}

void rai::WebsocketClient::TrySend_()
{
    if (!sending_)
    {
        sending_ = true;
        Send_();
    }
}

void rai::WebsocketClient::Send_()
{
    if (send_queue_.empty() || status_ != rai::WebsocketStatus::CONNECTED)
    {
        sending_ = false;
        return;
    }

    std::string message = send_queue_.front();
    auto message_s = std::make_shared<std::string>(std::move(message));
    std::weak_ptr<rai::WebsocketClient> client_w(Shared());
    if (ssl_)
    {
        if (wss_stream_ == nullptr)
        {
            sending_ = false;
            return;
        }
        std::shared_ptr<rai::WssStream> stream(wss_stream_);
        auto session_id = session_id_;
        wss_stream_->async_write(
            boost::asio::buffer(*message_s),
            boost::asio::bind_executor(
                strand_, [client_w, stream, session_id, message_s](
                             const boost::system::error_code& ec, size_t size) {
                    auto client = client_w.lock();
                    if (!client)
                    {
                        return;
                    }

                    client->OnSend(session_id, ec, size);
                }));
    }
    else
    {
        if (ws_stream_ == nullptr)
        {
            sending_ = false;
            return;
        }
        std::shared_ptr<rai::WsStream> stream(ws_stream_);
        auto session_id = session_id_;
        ws_stream_->async_write(
            boost::asio::buffer(*message_s),
            boost::asio::bind_executor(
                strand_, [client_w, stream, session_id, message_s](
                             const boost::system::error_code& ec, size_t size) {
                    auto client = client_w.lock();
                    if (!client)
                    {
                        return;
                    }

                    client->OnSend(session_id, ec, size);
                }));
    }

    #if 0
    std::cout << "send message=====>>:\n";
    std::cout << send_queue_.front() << std::endl;
    #endif
}

void rai::WebsocketClient::ChangeStatus_(rai::WebsocketStatus status)
{
    if (status == status_)
    {
        return;
    }
    status_ = status;

    if (status_observer_)
    {
        status_observer_(status_);
    }
}

std::string rai::WebsocketClient::Url_() const
{
    std::string url = ssl_ ? "wss://" : "ws://";
    url += host_;
    if ((ssl_ && port_ != 443) || (!ssl_ && port_ != 80))
    {
        url += ":" + std::to_string(port_);
    }
    if (!path_.empty() && !rai::StringStartsWith(path_, "/", false))
    {
        url += "/";
    }
    url += path_;
    return url;
}

void rai::WebsocketClient::SendKeeplive_()
{
    rai::Ptree ptree;
    ptree.put("action", "keeplive");
    ptree.put("timestamp", std::to_string(rai::CurrentTimestamp()));
    std::stringstream ostream;
    boost::property_tree::write_json(ostream, ptree);
    ostream.flush();
    send_queue_.push_front(ostream.str());
    TrySend_();
}

void rai::WebsocketClient::Receive_()
{
    std::weak_ptr<rai::WebsocketClient> client_w(Shared());
    if (ssl_)
    {
        std::shared_ptr<rai::WssStream> stream(wss_stream_);
        std::shared_ptr<boost::beast::multi_buffer> receive_buffer(receive_buffer_);
        auto session_id = session_id_;
        wss_stream_->async_read(
            *receive_buffer_,
            boost::asio::bind_executor(
                strand_, [client_w, stream, receive_buffer, session_id](
                             const boost::system::error_code& ec, size_t size) {
                    auto client = client_w.lock();
                    if (!client)
                    {
                        return;
                    }

                    client->OnReceive(session_id, ec, size);
                }));
    }
    else
    {
        std::shared_ptr<rai::WsStream> stream(ws_stream_);
        std::shared_ptr<boost::beast::multi_buffer> receive_buffer(
            receive_buffer_);
        auto session_id = session_id_;
        ws_stream_->async_read(
            *receive_buffer_,
            boost::asio::bind_executor(
                strand_, [client_w, stream, receive_buffer, session_id](
                             const boost::system::error_code& ec, size_t size) {
                    auto client = client_w.lock();
                    if (!client)
                    {
                        return;
                    }

                    client->OnReceive(session_id, ec, size);
                }));
    }
}

rai::WebsocketSession::WebsocketSession(
    const std::shared_ptr<rai::WebsocketServer>& server,
    const rai::UniqueId& id, boost::asio::ip::tcp::socket&& socket)
    : server_(server),
      id_(id),
      stopped_(false),
      closed_(false),
      sending_(false),
      connected_(false),
      ws_(std::move(socket)),
      strand_(ws_.get_executor())
{
    using boost::beast::websocket::stream;
    ws_.text(true);
}

rai::WebsocketSession::~WebsocketSession()
{
    server_->Erase(id_);
}

std::shared_ptr<rai::WebsocketSession> rai::WebsocketSession::Shared()
{
    return shared_from_this();
}

void rai::WebsocketSession::Start()
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (stopped_)
    {
        return;
    }

    std::shared_ptr<rai::WebsocketSession> session(Shared());
    ws_.async_accept([session](const boost::system::error_code& ec) {
        session->OnAccept(ec);
    });
}

void rai::WebsocketSession::Stop()
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (stopped_)
    {
        return;
    }
    stopped_ = true;

    Close_(lock);
}

void rai::WebsocketSession::OnAccept(const boost::system::error_code& ec)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (ec)
    {
        rai::Stats::Add(rai::ErrorCode::WEBSOCKET_ACCEPT,
                        "WebsocketSession::OnAccept: ", ec.message());
        return;
    }

    if (stopped_)
    {
        return;
    }

    connected_ = true;
    Receive_(lock);
    Send_(lock);
}

void rai::WebsocketSession::OnSend(const boost::system::error_code& ec,
                                   size_t size)
{
    std::unique_lock<std::mutex> lock(mutex_);
    sending_ = false;
    send_queue_.pop_front();

    if (ec || size == 0)
    {
        if (ec == boost::beast::websocket::error::closed)
        {
            return;
        }

        Close_(lock);
        rai::Stats::Add(rai::ErrorCode::WEBSOCKET_SEND,
                        "WebsocketSession::OnSend: ", ec.message());
        return;
    }
    
    if (stopped_)
    {
        Close_(lock);
        return;
    }

    Send_(lock);
}

void rai::WebsocketSession::OnReceive(const boost::system::error_code& ec,
                                      size_t size)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (ec || size == 0)
    {
        if (ec == boost::beast::websocket::error::closed)
        {
            return;
        }
        rai::Stats::Add(rai::ErrorCode::WEBSOCKET_RECEIVE,
                        "WebsocketSession::OnReceive: ", ec.message());
        return;
    }

    if (stopped_)
    {
        return;
    }

    std::stringstream os;
    os << boost::beast::buffers(receive_buffer_.data());
    os.flush();
    receive_buffer_.consume(receive_buffer_.size());

    lock.unlock();

    try
    {
        if (server_->message_observer_)
        {
            server_->message_observer_(os.str(), id_);
        }
    }
    catch(const std::exception& e)
    {
        std::cout << "WebsocketSession::OnReceive: " << e.what() << '\n';
        // log
    }

    lock.lock();

    Receive_(lock);
}

void rai::WebsocketSession::Send(const std::string& message)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (stopped_ ||  closed_ || message.empty())
    {
        return;
    }

    if (send_queue_.size() >= rai::WebsocketSession::MAX_QUEUE_SIZE)
    {
        rai::Stats::Add(rai::ErrorCode::WEBSOCKET_QUEUE_OVERFLOW);
        return;
    }

    send_queue_.push_back(message);
    Send_(lock);
}

void rai::WebsocketSession::Send(const rai::Ptree& message)
{
    std::stringstream ostream;
    boost::property_tree::write_json(ostream, message);
    ostream.flush();
    Send(ostream.str());
}

rai::UniqueId rai::WebsocketSession::Uid() const
{
    return id_;
}

void rai::WebsocketSession::Close_(std::unique_lock<std::mutex>& lock)
{
    if (closed_ || !connected_ || sending_)
    {
        return;
    }

    closed_ = true;

    std::shared_ptr<rai::WebsocketSession> session(Shared());
    boost::beast::websocket::close_reason reason;
    reason.code = boost::beast::websocket::close_code::normal;
    reason.reason = "Shutting down";
    ws_.async_close(reason,
                    boost::asio::bind_executor(
                        strand_, [session](const boost::system::error_code ec) {
                            if (ec)
                            {
                                rai::Stats::Add(rai::ErrorCode::WEBSOCKET_CLOSE,
                                                ec.message());
                            }
                        }));
}

void rai::WebsocketSession::Receive_(std::unique_lock<std::mutex>& lock)
{
    std::shared_ptr<rai::WebsocketSession> session(Shared());
    ws_.async_read(
        receive_buffer_,
        boost::asio::bind_executor(
            strand_, [session](const boost::system::error_code& ec,
                               size_t size) { session->OnReceive(ec, size); }));
}

void rai::WebsocketSession::Send_(std::unique_lock<std::mutex>& lock)
{
    if (!connected_ || closed_ || sending_)
    {
        return;
    }

    if (send_queue_.empty())
    {
        return;
    }

    sending_ = true;

    auto message = std::make_shared<std::string>(send_queue_.front());
    std::shared_ptr<rai::WebsocketSession> session(Shared());
    ws_.async_write(
        boost::asio::buffer(*message),
        boost::asio::bind_executor(
            strand_,
            [session, message](const boost::system::error_code& ec,
                               size_t size) { session->OnSend(ec, size); }));
}

rai::WebsocketServer::WebsocketServer(boost::asio::io_service& service,
                                      const rai::IP& ip, uint16_t port)
    : service_(service), acceptor_(service), local_(ip, port), stopped_(false)
{
}

std::shared_ptr<rai::WebsocketServer> rai::WebsocketServer::Shared()
{
    return shared_from_this();
}

void rai::WebsocketServer::Accept()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_)
    {
        return;
    }

    if (!acceptor_.is_open())
    {
        std::cout << "WebsocketServer::Accept: acceptor is not opened"
                  << std::endl;
        return;
    }

    std::shared_ptr<rai::WebsocketServer> server(Shared());
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(service_);
    acceptor_.async_accept(
        *socket, [server, socket](const boost::system::error_code& ec) {
            server->OnAccept(ec, socket);
        });
}

void rai::WebsocketServer::OnAccept(
    const boost::system::error_code& ec,
    const std::shared_ptr<boost::asio::ip::tcp::socket>& socket)
{
    std::shared_ptr<rai::WebsocketSession> session(nullptr);
    do
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }

        if (ec)
        {
            // log
            rai::Stats::Add(rai::ErrorCode::WEBSOCKET_ACCEPT, ec.message());
            break;
        }

        if (sessions_.size() >= rai::WebsocketServer::MAX_SESSIONS)
        {
            break;
        }

        rai::UniqueId uid;
        uid.Random();
        session = std::make_shared<rai::WebsocketSession>(Shared(), uid,
                                                          std::move(*socket));
        sessions_[uid] = session;
    } while (0);

    if (session != nullptr)
    {
        session->Start();
        if (session_observer_)
        {
            session_observer_(session->Uid(), true);
        }
    }

    Accept();
}

void rai::WebsocketServer::Erase(const rai::UniqueId& uid)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(uid);
        if (it == sessions_.end())
        {
            return;
        }
        sessions_.erase(it);
    }

    if (session_observer_)
    {
        session_observer_(uid, false);
    }
}

void rai::WebsocketServer::Start()
{
    acceptor_.open(local_.protocol());
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    boost::system::error_code ec;
    acceptor_.bind(local_, ec);
    if (ec)
    {
        std::cout << "[ERROR] WebsocketServer::Start: failed to bind ip:port = "
                  << local_ << std::endl;
        throw std::runtime_error(ec.message());
    }

    acceptor_.listen(static_cast<int>(rai::WebsocketServer::MAX_SESSIONS));
    Accept();
}

void rai::WebsocketServer::Stop()
{
    std::unordered_map<rai::UniqueId, std::weak_ptr<rai::WebsocketSession>>
        sessions;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
        sessions.swap(sessions_);
    }

    if (acceptor_.is_open())
    {
        boost::system::error_code ec;
        acceptor_.close(ec);
    }

    for (auto& i : sessions)
    {
        auto session = i.second.lock();
        if (session)
        {
            session->Stop();
        }
    }
}

std::shared_ptr<rai::WebsocketSession> rai::WebsocketServer::Session(
    const rai::UniqueId& uid)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::shared_ptr<rai::WebsocketSession> session(nullptr);

    if (stopped_)
    {
        return session;
    }

    auto it = sessions_.find(uid);
    if (it != sessions_.end())
    {
        session = it->second.lock();
    }

    return session;
}

void rai::WebsocketServer::Send(const rai::UniqueId& uid,
                                const std::string& message)
{
    auto session(Session(uid));
    if (session)
    {
        session->Send(message);
    }
}

void rai::WebsocketServer::Send(const rai::UniqueId& uid,
                                const rai::Ptree& message)
{
    auto session(Session(uid));
    if (session)
    {
        session->Send(message);
    }
}

std::vector<rai::UniqueId> rai::WebsocketServer::Clients() const
{
    std::vector<rai::UniqueId> result;
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& i : sessions_)
    {
        result.push_back(i.first);
    }
    return result;
}

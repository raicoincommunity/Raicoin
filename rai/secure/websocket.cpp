#include <rai/secure/websocket.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <rai/common/util.hpp>
#include <rai/secure/plat.hpp>

rai::WebsocketClient::WebsocketClient(boost::asio::io_service& service,
                                      const std::string& host, uint16_t port,
                                      const std::string& path, bool ssl)
    : service_(service),
      ssl_(ssl),
      host_(host),
      port_(port),
      path_(path),
      stopped_(false),
      status_(rai::WebsocketStatus::DISCONNECTED),
      resolver_(service_),
      ctx_(boost::asio::ssl::context::tlsv12_client),
      session_id_(0),
      sending_(false)

{
    if (ssl)
    {
        try
        {
            std::string pem_path = rai::PemPath();
            ctx_.load_verify_file(pem_path);
        }
        catch(...)
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
    const boost::system::error_code& ec,
    boost::asio::ip::tcp::resolver::iterator results)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_)
    {
        return;
    }

    if (ec)
    {
        // log
        std::cout << "Failed to resolve: " << ec.message() << std::endl;
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
        return;
    }
    std::cout << "Websocket resolve success " << std::endl;

    std::weak_ptr<rai::WebsocketClient> client_w(Shared());
    if (ssl_)
    {
        std::shared_ptr<rai::WssStream> stream(wss_stream_);
        boost::asio::async_connect(
            wss_stream_->lowest_layer(), results,
            [client_w, stream](
                const boost::system::error_code& ec,
                boost::asio::ip::tcp::resolver::iterator iterator) {
                auto client = client_w.lock();
                if (!client)
                {
                    return;
                }

                client->OnConnect(ec);
            });
    }
    else
    {
        std::shared_ptr<rai::WsStream> stream(ws_stream_);
        boost::asio::async_connect(
            ws_stream_->lowest_layer(), results,
            [client_w, stream](
                const boost::system::error_code& ec,
                boost::asio::ip::tcp::resolver::iterator iterator) {
                auto client = client_w.lock();
                if (!client)
                {
                    return;
                }

                client->OnConnect(ec);
            });
    }
}

void rai::WebsocketClient::OnConnect(const boost::system::error_code& ec)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_)
    {
        return;
    }

    if (ec)
    {
        // log
        std::cout << "Failed to connect: " << ec.message() << std::endl;
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
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
            [client_w, stream](const boost::system::error_code& ec) {
                auto client = client_w.lock();
                if (!client)
                {
                    return;
                }

                client->OnSslHandshake(ec);
            });
    }
    else
    {
        std::shared_ptr<rai::WsStream> stream(ws_stream_);
        ws_stream_->async_handshake(
            host_, path_,
            [client_w, stream](const boost::system::error_code& ec) {
                auto client = client_w.lock();
                if (!client)
                {
                    return;
                }

                client->OnWebsocketHandshake(ec);
            });
    }
}

void rai::WebsocketClient::OnSslHandshake(const boost::system::error_code& ec)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_)
    {
        return;
    }

    if (ec)
    {
        // log
        std::cout << "SSL handshake failed: " << ec.message() << std::endl;
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
        return;
    }
    std::cout << "SSL handshake success " << std::endl;

    std::weak_ptr<rai::WebsocketClient> client_w(Shared());
    std::shared_ptr<rai::WssStream> stream(wss_stream_);
    wss_stream_->async_handshake(
        host_, path_, [client_w, stream](const boost::system::error_code& ec) {
            auto client = client_w.lock();
            if (!client)
            {
                return;
            }

            client->OnWebsocketHandshake(ec);
        });
}

void rai::WebsocketClient::OnWebsocketHandshake(
    const boost::system::error_code& ec)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_)
    {
        return;
    }

    if (ec)
    {
        // log
        std::cout << "Websocket handshake failed: " << ec.message()
                  << std::endl;
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
        return;
    }
    ChangeStatus_(rai::WebsocketStatus::CONNECTED);
    std::cout << "Websocket handshake success" << std::endl;

    if (!sending_)
    {
        sending_ = true;
        Send_();
    }

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
    if (!sending_)
    {
        sending_ = true;
        Send_();
    }
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
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
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
        return;
    }

    std::stringstream os;
    os << boost::beast::buffers(receive_buffer_->data());
    receive_buffer_->consume(receive_buffer_->size());

    lock.unlock();

    auto message = std::make_shared<rai::Ptree>();
    try
    {
        boost::property_tree::read_json(os, *message);
        if (message_processor_)
        {
            message_processor_(message);
        }
    }
    catch(const std::exception& e)
    {
        std::cout << e.what() << '\n';
        // log
    }

    lock.lock();

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

    if (status_ != rai::WebsocketStatus::DISCONNECTED)
    {
        return;
    }
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

    std::weak_ptr<rai::WebsocketClient> client_w(Shared());
    boost::asio::ip::tcp::resolver::query query(host_, std::to_string(port_));
    resolver_.async_resolve(
        query, [client_w](const boost::system::error_code& ec,
                          boost::asio::ip::tcp::resolver::iterator results) {
            auto client = client_w.lock();
            if (!client)
            {
                return;
            }

            client->OnResolve(ec, results);
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
        try
        {
            stream->async_close(boost::beast::websocket::close_code::normal,
                                [stream](const boost::system::error_code& ec) {
                                    std::cout
                                        << "ws stream closed:" << ec.message()
                                        << std::endl;
                                });
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
        try
        {
            stream->async_close(boost::beast::websocket::close_code::normal,
                                [stream](const boost::system::error_code& ec) {
                                    std::cout
                                        << "ws stream closed:" << ec.message()
                                        << std::endl;
                                });
        }
        catch (...)
        {
        }
    }
}

void rai::WebsocketClient::Send_()
{
    if (send_queue_.empty() || status_ != rai::WebsocketStatus::CONNECTED)
    {
        sending_ = false;
        return;
    }

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
            boost::asio::buffer(send_queue_.front()),
            [client_w, stream, session_id](const boost::system::error_code& ec,
                                           size_t size) {
                auto client = client_w.lock();
                if (!client)
                {
                    return;
                }

                client->OnSend(session_id, ec, size);
            });
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
            boost::asio::buffer(send_queue_.front()),
            [client_w, stream, session_id](const boost::system::error_code& ec,
                                           size_t size) {
                auto client = client_w.lock();
                if (!client)
                {
                    return;
                }

                client->OnSend(session_id, ec, size);
            });
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

void rai::WebsocketClient::Receive_()
{
    std::weak_ptr<rai::WebsocketClient> client_w(Shared());
    if (ssl_)
    {
        std::shared_ptr<rai::WssStream> stream(wss_stream_);
        std::shared_ptr<boost::beast::multi_buffer> receive_buffer(receive_buffer_);
        auto session_id = session_id_;
        wss_stream_->async_read(
            *receive_buffer_, [client_w, stream, receive_buffer, session_id](
                                const boost::system::error_code& ec, size_t size) {
                auto client = client_w.lock();
                if (!client)
                {
                    return;
                }

                client->OnReceive(session_id, ec, size);
            });
    }
    else
    {
        std::shared_ptr<rai::WsStream> stream(ws_stream_);
        std::shared_ptr<boost::beast::multi_buffer> receive_buffer(
            receive_buffer_);
        auto session_id = session_id_;
        ws_stream_->async_read(
            *receive_buffer_,
            [client_w, stream, receive_buffer, session_id](
                const boost::system::error_code& ec, size_t size) {
                auto client = client_w.lock();
                if (!client)
                {
                    return;
                }

                client->OnReceive(session_id, ec, size);
            });
    }
}

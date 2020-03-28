#include <rai/wallet/websocket.hpp>
#include <rai/wallet/wallet.hpp>

rai::WebsocketClient::WebsocketClient(rai::Wallets& wallets,
                                      const std::string& host, uint16_t port,
                                      const std::string& path)
    : wallets_(wallets),
      host_(host),
      port_(port),
      path_(path),
      stopped_(false),
      status_(rai::WebsocketStatus::DISCONNECTED),
      resolver_(wallets.service_),
      ctx_(boost::asio::ssl::context::tlsv12_client),
      session_id_(0),
      sending_(false) 

{
    ctx_.load_verify_file("cacert.pem");
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

    std::weak_ptr<rai::Wallets> wallets(wallets_.Shared());
    std::shared_ptr<rai::WebsocketStream> stream(stream_);
    boost::asio::async_connect(
        stream_->lowest_layer(), results,
        [wallets, stream](const boost::system::error_code& ec,
                          boost::asio::ip::tcp::resolver::iterator iterator) {
            std::shared_ptr<rai::Wallets> wallets_l = wallets.lock();
            if (!wallets_l)
            {
                return;
            }

            wallets_l->websocket_.OnConnect(ec);
        });
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

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(stream_->next_layer().native_handle(),
                                  host_.c_str()))
    {
        // log
        std::cout << "Failed to set SNI: " << host_ << std::endl;
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
        return;
    }

    stream_->next_layer().set_verify_mode(
        boost::asio::ssl::verify_peer
        | boost::asio::ssl::verify_fail_if_no_peer_cert);
    stream_->next_layer().set_verify_callback(
        boost::asio::ssl::rfc2818_verification(host_));

    std::weak_ptr<rai::Wallets> wallets(wallets_.Shared());
    std::shared_ptr<rai::WebsocketStream> stream(stream_);
    stream_->next_layer().async_handshake(
        boost::asio::ssl::stream_base::client,
        [wallets, stream](const boost::system::error_code& ec) {
            std::shared_ptr<rai::Wallets> wallets_l = wallets.lock();
            if (!wallets_l)
            {
                return;
            }

            wallets_l->websocket_.OnSslHandshake(ec);
        });
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
        std::cout << "Ssl handshake failed: " << ec.message() << std::endl;
        ChangeStatus_(rai::WebsocketStatus::DISCONNECTED);
        return;
    }

    std::weak_ptr<rai::Wallets> wallets(wallets_.Shared());
    std::shared_ptr<rai::WebsocketStream> stream(stream_);
    stream_->async_handshake(
        host_, path_, [wallets, stream](const boost::system::error_code& ec) {
            std::shared_ptr<rai::Wallets> wallet_l = wallets.lock();
            if (!wallet_l)
            {
                return;
            }

            wallet_l->websocket_.OnWebsocketHandshake(ec);
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

    Send_();
    Receive_();
}

void rai::WebsocketClient::Send(const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_ || status_ != rai::WebsocketStatus::CONNECTED
        || message.empty())
    {
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
    std::lock_guard<std::mutex> lock(mutex_);
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
    if (!stream_ || status_ == rai::WebsocketStatus::DISCONNECTED)
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
    stream_ = std::make_shared<rai::WebsocketStream>(wallets_.service_, ctx_);
    receive_buffer_ = std::make_shared<boost::beast::multi_buffer>();
    session_id_++;
    sending_ = false;

    std::weak_ptr<rai::Wallets> wallets(wallets_.Shared());
    boost::asio::ip::tcp::resolver::query query(host_, std::to_string(port_));
    resolver_.async_resolve(
        query, [wallets](const boost::system::error_code& ec,
                        boost::asio::ip::tcp::resolver::iterator results) {
            std::shared_ptr<rai::Wallets> wallets_l = wallets.lock();
            if (!wallets_l)
            {
                return;
            }

            wallets_l->websocket_.OnResolve(ec, results);
        });
    wallets_.service_runner_.Notify();
}

void rai::WebsocketClient::CloseStream_()
{
    if (stream_ == nullptr)
    {
        return;
    }

    std::shared_ptr<rai::WebsocketStream> stream(std::move(stream_));
    try
    {
        stream->async_close(boost::beast::websocket::close_code::normal,
                             [stream](const boost::system::error_code& ec) {
                                 std::cout << "ws stream closed:" << ec.message() << std::endl;
                             });
    }
    catch (...)
    {
    }
}

void rai::WebsocketClient::Send_()
{
    if (send_queue_.empty())
    {
        sending_ = false;
        return;
    }

    std::cout << "send message=====>>:\n";
    std::cout << send_queue_.front() << std::endl;

    std::weak_ptr<rai::Wallets> wallets(wallets_.Shared());
    std::shared_ptr<rai::WebsocketStream> stream(stream_);
    auto session_id = session_id_;
    stream_->async_write(boost::asio::buffer(send_queue_.front()),
                         [wallets, stream, session_id](
                             const boost::system::error_code& ec, size_t size) {
                             std::shared_ptr<rai::Wallets> wallets_l =
                                 wallets.lock();
                             if (!wallets_l)
                             {
                                 return;
                             }

                             wallets_l->websocket_.OnSend(session_id, ec, size);
                         });
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
    std::weak_ptr<rai::Wallets> wallets(wallets_.Shared());
    std::shared_ptr<rai::WebsocketStream> stream(stream_);
    std::shared_ptr<boost::beast::multi_buffer> receive_buffer(receive_buffer_);
    auto session_id = session_id_;
    stream_->async_read(
        *receive_buffer_, [wallets, stream, receive_buffer, session_id](
                             const boost::system::error_code& ec, size_t size) {
            std::shared_ptr<rai::Wallets> wallets_l = wallets.lock();
            if (!wallets_l)
            {
                return;
            }

            wallets_l->websocket_.OnReceive(session_id, ec, size);
        });
}

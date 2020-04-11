#include <rai/secure/http.hpp>
#include <boost/property_tree/json_parser.hpp>

rai::HttpClient::HttpClient(boost::asio::io_service& service)
    : service_(service),
      used_(false),
      resolver_(service),
      ctx_(boost::asio::ssl::context::tlsv12_client)
{
}

std::shared_ptr<rai::HttpClient> rai::HttpClient::Shared()
{
    return shared_from_this();
}

bool rai::HttpClient::CheckUpdateUsed()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (used_)
    {
        return true;
    }
    used_ = true;
    return false;
}

rai::ErrorCode rai::HttpClient::Get(const rai::Url& url,
                                    const rai::HttpCallback& callback)
{
    IF_ERROR_RETURN(CheckUpdateUsed(), rai::ErrorCode::HTTP_CLIENT_USED);

    url_ = url; 
    callback_ = callback;
    IF_ERROR_RETURN(CheckUrl_(), rai::ErrorCode::INVALID_URL);
    if (url_.protocol_ == "https")
    {
        IF_ERROR_RETURN(LoadCert_(), rai::ErrorCode::LOAD_CERT);
    }
    PrepareGetReq_();

    Resolve();  
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::HttpClient::Post(const rai::Url& url,
                                     const rai::Ptree& ptree,
                                     const rai::HttpCallback& callback)
{
    IF_ERROR_RETURN(CheckUpdateUsed(), rai::ErrorCode::HTTP_CLIENT_USED);

    url_ = url; 
    callback_ = callback;
    IF_ERROR_RETURN(CheckUrl_(), rai::ErrorCode::INVALID_URL);
    if (url_.protocol_ == "https")
    {
        IF_ERROR_RETURN(LoadCert_(), rai::ErrorCode::LOAD_CERT);
    }
    PreparePostReq_(ptree);

    Resolve();
    return rai::ErrorCode::SUCCESS;
}

void rai::HttpClient::Resolve()
{
    std::shared_ptr<rai::HttpClient> client(Shared());
    boost::asio::ip::tcp::resolver::query query(url_.host_,
                                                std::to_string(url_.port_));
    resolver_.async_resolve(
        query, [client](const boost::system::error_code& ec,
                        boost::asio::ip::tcp::resolver::iterator results) {
            client->OnResolve(ec, results);
        });
}

void rai::HttpClient::OnResolve(
    const boost::system::error_code& ec,
    boost::asio::ip::tcp::resolver::iterator results)
{
    if (ec)
    {
        // log
        std::cout << "Failed to resolve: " << ec.message() << std::endl;
        Callback_(rai::ErrorCode::DNS_RESOLVE);
        return;
    }

    std::shared_ptr<rai::HttpClient> client(Shared());
    if (url_.protocol_ == "http")
    {
        socket_ = std::make_shared<boost::asio::ip::tcp::socket>(service_);
        boost::asio::async_connect(
            *socket_, results,
            [client](const boost::system::error_code& ec,
                     boost::asio::ip::tcp::resolver::iterator iterator) {
                client->OnConnect(ec);
            });
    }
    else
    {
        ssl_stream_ = std::make_shared<
            boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(service_,
                                                                    ctx_);
        boost::asio::async_connect(
            ssl_stream_->next_layer(), results,
            [client](const boost::system::error_code& ec,
                     boost::asio::ip::tcp::resolver::iterator iterator) {
                client->OnConnect(ec);
            });
    }
}

void rai::HttpClient::OnConnect(const boost::system::error_code& ec)
{
    if (ec)
    {
        // log
        std::cout << "Failed to connect: " << ec.message() << std::endl;
        Callback_(rai::ErrorCode::TCP_CONNECT);
        return;
    }

    if (url_.protocol_ == "https")
    {
        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (!SSL_set_tlsext_host_name(ssl_stream_->native_handle(),
                                      url_.host_.c_str()))
        {
            // log
            std::cout << "Failed to set SNI: " << url_.host_ << std::endl;
            Callback_(rai::ErrorCode::SET_SSL_SNI);
            return;
        }
        ssl_stream_->set_verify_mode(
            boost::asio::ssl::verify_peer
            | boost::asio::ssl::verify_fail_if_no_peer_cert);
        ssl_stream_->set_verify_callback(
            boost::asio::ssl::rfc2818_verification(url_.host_));
        std::shared_ptr<rai::HttpClient> client(Shared());
        ssl_stream_->async_handshake(
            boost::asio::ssl::stream_base::client,
            [client](const boost::system::error_code& ec) {
                client->OnSslHandshake(ec);
            });
    }
    else
    {
        Write();
    }
}

void rai::HttpClient::OnSslHandshake(const boost::system::error_code& ec)
{
    if (ec)
    {
        // log
        std::cout << "Ssl handshake failed: " << ec.message() << std::endl;
        Callback_(rai::ErrorCode::SSL_HANDSHAKE);
        return;
    }

    Write();
}

void rai::HttpClient::Onwrite(const boost::system::error_code& ec, size_t size)
{
    if (ec || size == 0)
    {
        // log
        std::cout << "Failed to write stream: " << ec.message() << std::endl;
        Callback_(rai::ErrorCode::WRITE_STREAM);
        return;
    }
    Read();
}

void rai::HttpClient::OnRead(const boost::system::error_code& ec, size_t size)
{
    if (ec)
    {
        // log
        std::cout << "Failed to read stream: " << ec.message() << std::endl;
        Callback_(rai::ErrorCode::STREAM);
        return;
    }

    if (res_.result() != boost::beast::http::status::ok)
    {
        if (post_req_)
        {
            // log
            std::cout << "Http post failed, status: "
                      << static_cast<uint32_t>(res_.result()) << std::endl;
            Callback_(rai::ErrorCode::HTTP_POST);
        }
        else
        {
            // log
            std::cout << "Http get failed, status: "
                      << static_cast<uint32_t>(res_.result()) << std::endl;
            Callback_(rai::ErrorCode::HTTP_GET);
        }
        return;
    }

    Callback_(rai::ErrorCode::SUCCESS, res_.body());

    if (url_.protocol_ == "https")
    {
        std::shared_ptr<rai::HttpClient> client(Shared());
        ssl_stream_->async_shutdown(
            [client](const boost::system::error_code&) {});
    }
    else
    {
        boost::system::error_code ignore;
        socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore);
    }
}

void rai::HttpClient::Write()
{
    std::shared_ptr<rai::HttpClient> client(Shared());
    if (url_.protocol_ == "https")
    {
        if (post_req_)
        {
            boost::beast::http::async_write(
                *ssl_stream_, *post_req_,
                [client](const boost::system::error_code& ec, size_t size) {
                    client->Onwrite(ec, size);
                });
        }
        else
        {
            boost::beast::http::async_write(
                *ssl_stream_, *get_req_,
                [client](const boost::system::error_code& ec, size_t size) {
                    client->Onwrite(ec, size);
                });
        }
    }
    else
    {
        if (post_req_)
        {
            boost::beast::http::async_write(
                *socket_, *post_req_,
                [client](const boost::system::error_code& ec, size_t size) {
                    client->Onwrite(ec, size);
                });
        }
        else
        {
            boost::beast::http::async_write(
                *socket_, *get_req_,
                [client](const boost::system::error_code& ec, size_t size) {
                    client->Onwrite(ec, size);
                });
        }
    }
}

void rai::HttpClient::Read()
{
    std::shared_ptr<rai::HttpClient> client(Shared());
    if (url_.protocol_ == "https")
    {
        boost::beast::http::async_read(
            *ssl_stream_, buffer_, res_,
            [client](const boost::system::error_code& ec, size_t size) {
                client->OnRead(ec, size);
            });
    }
    else
    {
        boost::beast::http::async_read(
            *socket_, buffer_, res_,
            [client](const boost::system::error_code& ec, size_t size) {
                client->OnRead(ec, size);
            });
    }
}

bool rai::HttpClient::CheckUrl_() const
{
    if (!url_ || (url_.protocol_ != "http" && url_.protocol_ != "https"))
    {
        return true;
    }
    return false;
}

void rai::HttpClient::Callback_(rai::ErrorCode error_code,
                                const std::string& response)
{
    if (callback_)
    {
        callback_(error_code, response);
    }
}

bool rai::HttpClient::LoadCert_()
{
    try
    {
        ctx_.load_verify_file("cacert.pem");
        return false;
    }
    catch(...)
    {
        return true;
    }
}

void rai::HttpClient::PrepareGetReq_()
{
    get_req_ = std::make_shared<
        boost::beast::http::request<boost::beast::http::empty_body>>();
    get_req_->version(11);
    get_req_->method(boost::beast::http::verb::get);
    get_req_->target(url_.path_);
    get_req_->insert(boost::beast::http::field::host, url_.host_);
}

void rai::HttpClient::PreparePostReq_(const rai::Ptree& ptree)
{
    std::stringstream stream;
    boost::property_tree::write_json(stream, ptree);
    stream.flush();

    post_req_ = std::make_shared<
        boost::beast::http::request<boost::beast::http::string_body>>();
    post_req_->method(boost::beast::http::verb::post);
    post_req_->target(url_.path_);
    post_req_->version(11);
    post_req_->insert(boost::beast::http::field::host, url_.host_);
    post_req_->insert(boost::beast::http::field::content_type,
                "application/json");
    post_req_->body() = stream.str();
    post_req_->prepare_payload();
}

#pragma once

#include <mutex>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast.hpp>

#include <rai/common/util.hpp>
#include <rai/common/errors.hpp>

namespace rai
{

using HttpCallback = std::function<void(rai::ErrorCode, const std::string&)>;

class HttpClient : public std::enable_shared_from_this<rai::HttpClient>
{
public:
    HttpClient(boost::asio::io_service&);
    HttpClient(const HttpClient&) = delete;
    std::shared_ptr<rai::HttpClient> Shared();

    bool CheckUpdateUsed();
    rai::ErrorCode Get(const rai::Url&, const rai::HttpCallback&);
    rai::ErrorCode Post(const rai::Url&, const rai::Ptree&,
                        const rai::HttpCallback&);

    void Resolve();
    void OnResolve(const boost::system::error_code&,
                   boost::asio::ip::tcp::resolver::iterator);
    void OnConnect(const boost::system::error_code&);
    void OnSslHandshake(const boost::system::error_code&);
    void Onwrite(const boost::system::error_code&, size_t);
    void OnRead(const boost::system::error_code&, size_t);
    void Write();
    void Read();

    boost::asio::io_service& service_;
private:
    bool CheckUrl_() const;
    void Callback_(rai::ErrorCode, const std::string& = "");
    bool LoadCert_();
    void PrepareGetReq_();
    void PreparePostReq_(const rai::Ptree&);

    std::mutex mutex_;
    bool used_;
    rai::Url url_;
    rai::HttpCallback callback_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ssl::context ctx_;
    boost::beast::flat_buffer buffer_;
    std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
        ssl_stream_;

    std::shared_ptr<
        boost::beast::http::request<boost::beast::http::string_body>>
        post_req_;
    std::shared_ptr<boost::beast::http::request<boost::beast::http::empty_body>>
        get_req_;
    boost::beast::http::response<boost::beast::http::string_body> res_;
};
}
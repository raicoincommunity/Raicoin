#include <rai/secure/rpc.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <rai/common/log.hpp>
#include <rai/common/stat.hpp>

rai::Rpc::Rpc(boost::asio::io_service& service, const rai::RpcConfig& config,
              const rai::RpcHandlerMaker& maker)
    : service_(service),
      acceptor_(service),
      config_(config),
      make_handler_(maker),
      stopped_{ATOMIC_FLAG_INIT}
{
}

void rai::Rpc::Start()
{
    boost::asio::ip::tcp::endpoint endpoint(config_.address_, config_.port_);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

    boost::system::error_code ec;
    acceptor_.bind(endpoint, ec);
    if (ec)
    {
        rai::Log::Network(boost::str(
            boost::format("Error while binding for RPC on port %1%: %2%")
            % endpoint.port() % ec.message()));
        throw std::runtime_error(ec.message());
    }

    acceptor_.listen();

    Accept();
}

void rai::Rpc::Stop()
{
    if (stopped_.test_and_set())
    {
        return;
    }
    acceptor_.close();
}


void rai::Rpc::Accept()
{
    auto connection = std::make_shared<rai::RpcConnection>(*this);
    acceptor_.async_accept(
        connection->socket_,
        [this, connection](const boost::system::error_code& ec) {
            if (boost::asio::error::operation_aborted != ec
                && acceptor_.is_open())
            {
                Accept();
            }

            if (!ec)
            {
                connection->Parse();
            }
            else
            {
                rai::Log::Network(boost::str(
                    boost::format("Error accepting RPC connections: %1%")
                    % ec.message()));
            }
        });
}

bool rai::Rpc::CheckWhitelist(const boost::asio::ip::address_v4& ip) const
{
    bool error = true;
    for (const auto& i : config_.whitelist_)
    {
        if (ip == i)
        {
            error = false;
            break;
        }
    }
    return error;
}

rai::RpcConnection::RpcConnection(rai::Rpc& rpc)
    : rpc_(rpc), socket_(rpc.service_)
{
    responded_.clear();
}


void rai::RpcConnection::Parse()
{
    boost::system::error_code ec;
    auto remote = socket_.remote_endpoint(ec);
    if (ec)
    {
        return;
    }
    if (rpc_.CheckWhitelist(remote.address().to_v4()))
    {
        rai::Log::Rpc(
            boost::str(boost::format("RPC request from %1% denied") % remote));
        return;
    }

    Read();
}

void rai::RpcConnection::Read()
{
    auto connection = shared_from_this();
    boost::beast::http::async_read(
        socket_, buffer_, request_,
        [connection](const boost::system::error_code& ec, size_t size) {
            if (ec)
            {
                rai::Log::Rpc(boost::str(boost::format("RPC read error:%1%")
                                         % ec.message()));
                return;
            }

            connection->rpc_.service_.post([connection]() {
                auto start = std::chrono::steady_clock::now();
                auto version = connection->request_.version();
                std::string request_id = boost::str(
                    boost::format("%1%")
                    % boost::io::group(
                          std::hex, std::showbase,
                          reinterpret_cast<uintptr_t>(connection.get())));

                auto response_handler =
                    [connection, version, start,
                     request_id](const boost::property_tree::ptree& ptree) {
                        std::stringstream ostream;
                        boost::property_tree::write_json(ostream, ptree);
                        ostream.flush();
                        std::string body = ostream.str();
                        connection->Write(body, version);
                        boost::beast::http::async_write(
                            connection->socket_, connection->response_,
                            [connection](const boost::system::error_code& ec,
                                         size_t size) {
                                if (ec)
                                {
                                    rai::Log::Rpc(boost::str(
                                        boost::format("RPC write error:%1%")
                                        % ec.message()));
                                    return;
                                }
                            });
                        rai::Log::Rpc(boost::str(
                            boost::format("RPC request %1% completed in: "
                                          "%2% microseconds")
                            % request_id
                            % std::chrono::duration_cast<
                                  std::chrono::microseconds>(
                                  std::chrono::steady_clock::now() - start)
                                  .count()));
                    };

                if (connection->request_.method()
                    == boost::beast::http::verb::post)
                {
                    auto rpc_handler = connection->rpc_.make_handler_(
                        connection->rpc_, connection->request_.body(),
                        request_id,
                        connection->socket_.remote_endpoint().address().to_v4(),
                        response_handler);
                    rpc_handler->Process();
                }
                else
                {
                    rai::Ptree response;
                    response.put("error", "Only POST requests are allowed");
                    response_handler(response);
                }
            });
        });
}

void rai::RpcConnection::Write(const std::string& body, unsigned version)
{
    if (responded_.test_and_set())
    {
        rai::Log::Error("RPC already responded");
        return;
    }

    response_.set("Content-Type", "application/json");
    response_.set("Access-Control-Allow-Origin", "*");
    response_.set("Access-Control-Allow-Headers",
                  "Accept, Accept-Language, Content-Language, Content-Type");
    response_.set("Connection", "close");
    response_.result(boost::beast::http::status::ok);
    response_.body() = body;
    response_.version(version);
    response_.prepare_payload();
}

rai::RpcHandler::RpcHandler(
    rai::Rpc& rpc, const std::string& body, const std::string& request_id,
    const boost::asio::ip::address_v4& ip,
    const std::function<void(const rai::Ptree&)>& send_response)
    : rpc_(rpc),
      body_(body),
      request_id_(request_id),
      ip_(ip),
      send_response_(send_response),
      error_code_(rai::ErrorCode::SUCCESS)
{
}

void rai::RpcHandler::Check()
{
    if (body_.size() > rai::RpcHandler::MAX_BODY_SIZE)
    {
        error_code_ = rai::ErrorCode::RPC_HTTP_BODY_SIZE;
        return;
    }

    int depth = 0;
    for (auto ch : body_)
    {
        if (ch == '[' || ch == '{')
        {
            ++depth;
        }
        else if (ch == ']' || ch == '}')
        {
            --depth;
        }
        else
        {
            continue;
        }

        if (depth < 0)
        {
            error_code_ = rai::ErrorCode::RPC_JSON;
            return;
        }

        if (depth > rai::RpcHandler::MAX_JSON_DEPTH)
        {
            error_code_ = rai::ErrorCode::RPC_JSON_DEPTH;
            return;
        }
    }
}

void rai::RpcHandler::Process()
{
    try
    {
        Check();
        if (error_code_ != rai::ErrorCode::SUCCESS)
        {
            Response();
            return;
        }

        std::stringstream stream(body_);
        boost::property_tree::read_json(stream, request_);
        std::string action = request_.get<std::string>("action");
        if (action == "stop")
        {
            if (!CheckLocal_())
            {
                Stop();
            }
        }

        ProcessImpl();

        response_.put("ack", action);
        auto request_id = request_.get_optional<std::string>("request_id");
        if (request_id)
        {
            response_.put("request_id", *request_id);
        }
    }
    catch (const std::exception& e)
    {
        response_.put("exception", e.what());
    }
    catch (...)
    {
        error_code_ = rai::ErrorCode::RPC_GENERIC;
    }

    Response();
}

void rai::RpcHandler::Response()
{
    if (error_code_ != rai::ErrorCode::SUCCESS || response_.empty())
    {
        rai::Ptree ptree;
        std::string error = "Empty response";
        if (error_code_ != rai::ErrorCode::SUCCESS)
        {
            error = rai::ErrorString(error_code_);
        }
        ptree.put("error", error);
        ptree.put("error_code", static_cast<uint32_t>(error_code_));
        send_response_(ptree);
    }
    else
    {
        send_response_(response_);
    }

    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        rai::Stats::Add(error_code_);
    }
}

void rai::RpcHandler::Stop()
{
    rpc_.Stop();
}


bool rai::RpcHandler::CheckControl_()
{
    if (ip_ == boost::asio::ip::address_v4::loopback())
    {
        return false;
    }

    if (rpc_.config_.enable_control_)
    {
        return false;
    }

    error_code_ = rai::ErrorCode::RPC_ENABLE_CONTROL;
    return true;
}

bool rai::RpcHandler::CheckLocal_()
{
    if (ip_ == boost::asio::ip::address_v4::loopback())
    {
        return false;
    }

    error_code_ = rai::ErrorCode::RPC_NOT_LOCALHOST;
    return true;
}

bool rai::RpcHandler::GetAccount_(rai::Account& account)
{
    auto account_o = request_.get_optional<std::string>("account");
    if (!account_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_ACCOUNT;
        return true;
    }

    if (account.DecodeAccount(*account_o))
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_ACCOUNT;
        return true;
    }

    return false;
}

bool rai::RpcHandler::GetCount_(uint64_t& count)
{
    auto count_o = request_.get_optional<std::string>("count");
    if (!count_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_COUNT;
        return true;
    }

    bool error = rai::StringToUint(*count_o, count);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_COUNT;
        return true;
    }

    return false;
}

bool rai::RpcHandler::GetHash_(rai::BlockHash& hash)
{
    auto hash_o = request_.get_optional<std::string>("hash");
    if (!hash_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_HASH;
        return true;
    }

    bool error = hash.DecodeHex(*hash_o);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_HASH;
        return true;
    }

    return false;
}

bool rai::RpcHandler::GetHeight_(uint64_t& height)
{
    auto height_o = request_.get_optional<std::string>("height");
    if (!height_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_HEIGHT;
        return true;
    }

    bool error = rai::StringToUint(*height_o, height);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_HEIGHT;
        return  true;
    }

    return false;
}

bool rai::RpcHandler::GetPrevious_(rai::BlockHash& previous)
{
    auto previous_o = request_.get_optional<std::string>("previous");
    if (!previous_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_PREVIOUS;
        return true;
    }

    bool error = previous.DecodeHex(*previous_o);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_PREVIOUS;
        return true;
    }

    return false;
}

bool rai::RpcHandler::GetSignature_(rai::Signature& signature)
{
    auto signature_o = request_.get_optional<std::string>("signature");
    if (!signature_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_SIGNATURE;
        return true;
    }

    bool error = signature.DecodeHex(*signature_o);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_SIGNATURE;
        return true;
    }

    return false;
}

bool rai::RpcHandler::GetTimestamp_(uint64_t& timestamp)
{
    auto timestamp_o = request_.get_optional<std::string>("timestamp");
    if (!timestamp_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_TIMESTAMP;
        return true;
    }

    bool error = rai::StringToUint(*timestamp_o, timestamp);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_TIMESTAMP;
        return true;
    }

    return false;
}

std::unique_ptr<rai::Rpc> rai::MakeRpc(boost::asio::io_service& service,
                                       const rai::RpcConfig& config,
                                       const rai::RpcHandlerMaker& maker)
{
    std::unique_ptr<rai::Rpc> impl;

    impl.reset(new rai::Rpc(service, config, maker));

    return impl;
}
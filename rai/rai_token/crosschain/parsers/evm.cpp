#include <rai/rai_token/crosschain/parsers/evm.hpp>
#include <rai/common/log.hpp>
#include <rai/common/stat.hpp>
#include <rai/secure/http.hpp>
#include <rai/rai_token/token.hpp>

bool rai::GetEvmChainId(rai::Chain chain, rai::EvmChainId& evm_chain)
{
    switch (chain)
    {
        case rai::Chain::ETHEREUM:
        {
            evm_chain = 1;
            break;
        }
        case rai::Chain::BINANCE_SMART_CHAIN:
        {
            evm_chain = 56;
            break;
        }
        case rai::Chain::ETHEREUM_TEST_ROPSTEN:
        {
            evm_chain = 3;
            break;
        }
        case rai::Chain::ETHEREUM_TEST_KOVAN:
        {
            evm_chain = 42;
            break;
        }
        case rai::Chain::ETHEREUM_TEST_RINKEBY:
        {
            evm_chain = 4;
            break;
        }
        case rai::Chain::ETHEREUM_TEST_GOERLI:
        {
            evm_chain = 5;
            break;
        }
        case rai::Chain::ETHEREUM_TEST_SEPOLIA:
        {
            evm_chain = 11155111;
            break;
        }
        case rai::Chain::BINANCE_SMART_CHAIN_TEST:
        {
            evm_chain = 97;
            break;
        }
        default:
        {
            return true;
        }
    }

    return false;
}


rai::EvmEndpoint::EvmEndpoint(uint64_t index, const rai::Url& url)
    : index_(index),
      url_(url),
      batch_(rai::EvmEndpoint::BATCH_MAX),
      request_id_(0),
      consecutive_errors_(0),
      checked_(false),
      valid_(false),
      sending_(false),
      timeout_count_(0),
      min_time_span_(0),
      max_time_span_(0)
{
}

void rai::EvmEndpoint::RecordTimeSpan()
{
    if (send_at_ == std::chrono::steady_clock::time_point())
    {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (now < send_at_)
    {
        return;
    }

    uint64_t time_span =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - send_at_)
            .count();
    if (time_spans_.size() >= rai::EvmEndpoint::MAX_TIME_SPAN_SIZE)
    {
        size_t index = request_id_ % rai::EvmEndpoint::MAX_TIME_SPAN_SIZE;
        time_spans_[index] = time_span;
    }
    else
    {
        time_spans_.push_back(time_span);
    }

    if (time_span < min_time_span_ || min_time_span_ == 0)
    {
        min_time_span_ = time_span;
    }

    if (time_span > max_time_span_)
    {
        max_time_span_ = time_span;
    }
}

uint64_t rai::EvmEndpoint::AverageTimeSpan() const
{
    if (time_spans_.size() == 0)
    {
        return 0;
    }

    uint64_t sum = 0;
    for (auto time_span : time_spans_)
    {
        sum += time_span;
    }

    return sum / time_spans_.size();
}

bool rai::EvmEndpoint::Timeout() const
{
    auto now = std::chrono::steady_clock::now();
    if (now < send_at_)
    {
        return false;
    }
    return (now - send_at_) > std::chrono::seconds(30);
}

rai::EvmParser::EvmParser(rai::Token& token, const std::vector<rai::Url>& urls,
                          rai::Chain chain, uint64_t sync_interval)
    : token_(token), session_id_(0), chain_(chain), sync_interval_(sync_interval)
{
    uint64_t index = 0;
    for (const auto& url : urls)
    {
        endpoints_.emplace_back(index, url);
        ++index;
    }

    bool error = rai::GetEvmChainId(chain_, evm_chain_id_);
    if (error)
    {
        throw std::runtime_error("GetEvmChainId failed");
    }
}

void rai::EvmParser::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (CheckEndpoints_())
    {
        return;
    }

    // todo:
}

uint64_t rai::EvmParser::Delay() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return sync_interval_;
}

void rai::EvmParser::Status(rai::Ptree& ptree) const
{
    std::unique_lock<std::mutex> lock(mutex_);

    ptree.put("type", "evm_parser");
    ptree.put("chain", rai::ChainToString(chain_));
    ptree.put("chain_id", std::to_string(static_cast<uint32_t>(chain_)));
    ptree.put("evm_chain_id", evm_chain_id_.StringDec());
    ptree.put("session_id", std::to_string(session_id_));
    ptree.put("sync_interval", std::to_string(sync_interval_));

    rai::Ptree endpoints;
    for (const auto& i : endpoints_)
    {
        rai::Ptree entry;
        entry.put("index", std::to_string(i.index_));
        entry.put("url", i.url_.String());
        entry.put("checked", rai::BoolToString(i.checked_));
        entry.put("valid", rai::BoolToString(i.valid_));
        entry.put("sending", rai::BoolToString(i.sending_));
        entry.put("request_id", std::to_string(i.request_id_));
        entry.put("batch_size", std::to_string(i.batch_));
        entry.put("consecutive_errors", std::to_string(i.consecutive_errors_));
        entry.put("timeout_count", std::to_string(i.timeout_count_));
        entry.put("min_time_span", rai::MilliToSecondString(i.min_time_span_));
        entry.put("average_time_span",
                  rai::MilliToSecondString(i.AverageTimeSpan()));
        entry.put("max_time_span", rai::MilliToSecondString(i.max_time_span_));
        endpoints.push_back(std::make_pair("", entry));
    }
    ptree.put_child("endpoints", endpoints);
}

void rai::EvmParser::Receive(uint64_t index, uint64_t request_id,
                             rai::ErrorCode error_code,
                             const std::string& response,
                             const EvmRequestCallback& callback)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (index > endpoints_.size())
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::Receive: index=", index,
            " is out of range, endpoints_.size()=", endpoints_.size()));
        return;
    }

    rai::EvmEndpoint& endpoint = endpoints_[index];
    if (endpoint.request_id_ != request_id)
    {
        rai::Stats::Add(
            rai::ErrorCode::TOKEN_CROSS_CHAIN_REQUEST_ID,
            "EvmParser::Receive: endpoint=", endpoint.url_.String());
        return;
    }
    endpoint.sending_ = false;
    endpoint.RecordTimeSpan();

    rai::Ptree ptree;
    if (error_code == rai::ErrorCode::SUCCESS)
    {
        bool error = rai::StringToPtree(response, ptree);
        if (error)
        {
            error_code = rai::ErrorCode::TOKEN_CROSS_CHAIN_RESPONSE_JSON;
        }
    }

    if (error_code != rai::ErrorCode::SUCCESS)
    {
        std::string error_info = rai::ToString("EvmParser::Receive: endpoint=",
                                               endpoint.url_.String());
        rai::Stats::Add(error_code, error_info);

        ++endpoint.consecutive_errors_;
    }
    else if (ptree.get_child_optional("error"))
    {

        error_code = rai::ErrorCode::TOKEN_CROSS_CHAIN_RESPONSE_ERROR;
        auto error = ptree.get_child("error");
        if (error.get_optional<std::string>("message"))
        {
            auto error_info = error.get<std::string>("message");
            if (error_info.size() > 100)
            {
                error_info = error_info.substr(0, 100);
            }
            rai::Stats::Add(error_code, error_info);
        }
        else
        {
            rai::Stats::Add(error_code);
        }
        ++endpoint.consecutive_errors_;
    }
    else
    {
        endpoint.consecutive_errors_ = 0;
    }

    callback(index, request_id, error_code, ptree);
}

void rai::EvmParser::Send_(rai::EvmEndpoint& endpoint,
                           const std::string& method, const rai::Ptree& params,
                           const EvmRequestCallback& callback)
{
    uint64_t index = endpoint.index_;
    uint64_t request_id = ++endpoint.request_id_;
    endpoint.sending_ = true;
    endpoint.send_at_ = std::chrono::steady_clock::now();

    rai::Ptree ptree;
    ptree.put("jsonrpc", "2.0");
    ptree.put("method", method);
    if (!params.empty())
    {
        ptree.put_child("params", params);
    }
    ptree.put("id", std::to_string(request_id));

    rai::HttpCallback handler = [this, index, request_id, callback](
                                    rai::ErrorCode error_code,
                                    const std::string& response) {
        Receive(index, request_id, error_code, response, callback);
    };

    auto http = std::make_shared<rai::HttpClient>(token_.service_);
    rai::ErrorCode error_code = http->Post(endpoint.url_, ptree, handler);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        token_.service_.post(
            [error_code, handler]() { handler(error_code, ""); });
    }
}

bool rai::EvmParser::CheckEndpoints_()
{
    bool checked = true;
    for (auto& endpoint : endpoints_)
    {
        if (endpoint.checked_)
        {
            continue;
        }
        if (endpoint.sending_ && endpoint.Timeout())
        {
            ++endpoint.request_id_;
            ++endpoint.consecutive_errors_;
            ++endpoint.timeout_count_;
            endpoint.sending_ = false;
            rai::Stats::Add(
                rai::ErrorCode::TOKEN_CROSS_CHAIN_REQUEST_TIMEOUT,
                rai::ToString("EvmParser::Run: endpoint=",
                                endpoint.url_.String()));
        }
        if (endpoint.consecutive_errors_
            >= rai::EvmEndpoint::CONSECUTIVE_ERRORS_MAX)
        {
            endpoint.checked_ = true;
            endpoint.valid_ = false;
            std::string error_info =
                rai::ToString("EvmParser::Run: check chainId failed, endpoint=",
                              endpoint.url_.String());
            rai::Log::Error(error_info);
            std::cout << error_info << std::endl;
            continue;
        }
        if (!endpoint.sending_)
        {
            CheckEvmChainId_(endpoint);
        }
        checked = false;
    }

    return !checked;
}

uint64_t rai::EvmParser::MostConsecutiveErrors_() const
{
    uint64_t most = 0;
    for (const auto& endpoint : endpoints_)
    {
        if (endpoint.consecutive_errors_ > most)
        {
            most = endpoint.consecutive_errors_;
        }
    }
    return most;
}

void rai::EvmParser::CheckEvmChainId_(EvmEndpoint& endpoint)
{
    rai::Ptree params;
    Send_(endpoint, "eth_chainId", params,
          std::bind(&EvmParser::EvmChainIdCallback_, this,
                    std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4));
}

void rai::EvmParser::EvmChainIdCallback_(uint64_t index, uint64_t request_id,
                                         rai::ErrorCode error_code,
                                         const rai::Ptree& ptree)
{
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    auto result = ptree.get_optional<std::string>("result");
    if (!result)
    {
        rai::Stats::Add(rai::ErrorCode::TOKEN_CROSS_CHAIN_UNEXPECTED,
                        "EvmParser::EvmChainIdCallback_: no result");
        return;
    }

    rai::EvmChainId chain_id;
    bool error = chain_id.DecodeEvmHex(*result);
    if (error)
    {
        rai::Stats::Add(rai::ErrorCode::TOKEN_CROSS_CHAIN_UNEXPECTED,
                        "EvmParser::EvmChainIdCallback_: decode error");
        return;
    }

    auto& endpoint = endpoints_[index];
    if (chain_id != evm_chain_id_)
    {
        endpoint.checked_ = true;
        endpoint.valid_ = false;
        std::string error_info = rai::ToString(
            "EvmParser::EvmChainIdCallback_: chainId mismatch, endpoint=",
            endpoint.url_.String(),
            ", expected chainId=", evm_chain_id_.StringDec(),
            ", actual chainId=", chain_id.StringDec());
        rai::Log::Error(error_info);
        std::cout << error_info << std::endl;
        return;
    }
    
    endpoint.checked_ = true;
    endpoint.valid_ = true;
}

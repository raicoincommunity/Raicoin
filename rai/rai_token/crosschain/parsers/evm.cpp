#include <rai/common/log.hpp>
#include <rai/common/stat.hpp>
#include <rai/rai_token/crosschain/parsers/evm.hpp>
#include <rai/rai_token/token.hpp>
#include <rai/secure/http.hpp>

const rai::Topic rai::EvmTopic::TOKEN_INFO_INITIALIZED = rai::uint256_t(
    "0x6326170d2855fe84187e23b0ebc28f2cc86021b6a04782bf4f3c79848279a809");
const rai::Topic rai::EvmTopic::WRAPPED_ERC20_TOKEN_CREATED = rai::uint256_t(
    "0x3d7925b6385b69f2ae4083ec472bd4a95e6aec23b2a364a9121708eddb4c034e");
const rai::Topic rai::EvmTopic::WRAPPED_ERC721_TOKEN_CREATED = rai::uint256_t(
    "0xc1b7eee9f857f4db122a8fee3d96594289a9d01ab8aa5d7fb092bbe984b5f8bb");
const rai::Topic rai::EvmTopic::ERC20_TOKEN_MAPPED = rai::uint256_t(
    "0x6b0f8dd2d77f906c303102b1ff97f89a569297c52ef817d5bee2ca7a383cfb52");
const rai::Topic rai::EvmTopic::ERC20_TOKEN_UNMAPPED = rai::uint256_t(
    "0x849b14e3d4ae9cfd864c45902ca262e46a3e8f900d2452f089e36a0eb23a8849");
const rai::Topic rai::EvmTopic::ERC20_TOKEN_WRAPPED = rai::uint256_t(
    "0x6c3e9ab4eca7a7594e25440adb82334b2a3541b57532ad125e07f50add6fefce");
const rai::Topic rai::EvmTopic::ERC20_TOKEN_UNWRAPPED = rai::uint256_t(
    "0x92c72190b7883da51e81bdc1a610b7c9c5eb165f361989d899ea368d5fad8aa7");
const rai::Topic rai::EvmTopic::ERC721_TOKEN_MAPPED = rai::uint256_t(
    "0x962047d98677685f6d2bc1997a70aee81897e9fa0a9dba1f776fe9a2809a4366");
const rai::Topic rai::EvmTopic::ERC721_TOKEN_UNMAPPED = rai::uint256_t(
    "0x46ef0e539f35e386d20e68ecfee4a020549df4e953604a7500bf5f9c114d65ad");
const rai::Topic rai::EvmTopic::ERC721_TOKEN_WRAPPED = rai::uint256_t(
    "0xf9d868318b716a02085623f17e6035dff573052aa13ae51abc36f008eb0862d7");
const rai::Topic rai::EvmTopic::ERC721_TOKEN_UNWRAPPED = rai::uint256_t(
    "0xe950f7023ac8fa0bcab60dc333dbcabccf498c1eae159fba3adffdde7fcd69cd");
const rai::Topic rai::EvmTopic::ETH_MAPPED = rai::uint256_t(
    "0x347efc6e408255a005c306922d5fc43199605c2a63e675cc31977d85a40d11a9");
const rai::Topic rai::EvmTopic::ETH_UNMAPPED = rai::uint256_t(
    "0x88c215d0b8443425f361092ff125bae37cf7aecf3c0d22f2ce9806d091c2c605");

const rai::TokenAddress rai::EvmParser::MAX_ADDRESS = rai::uint256_t(
    "0x000000000000000000000000ffffffffffffffffffffffffffffffffffffffff");

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
      batch_detected_(false),
      timeout_count_(0),
      min_time_span_(0),
      max_time_span_(0),
      last_error_code_(rai::ErrorCode::SUCCESS)
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

void rai::EvmEndpoint::ProcessTimeout()
{
    ++request_id_;
    ++timeout_count_;
    sending_ = false;
    ProcessError(rai::ErrorCode::TOKEN_CROSS_CHAIN_REQUEST_TIMEOUT, "timeout");
}

void rai::EvmEndpoint::ProcessError(rai::ErrorCode error_code,
                                    const std::string& message)
{
    last_error_code_ = error_code;
    last_error_message_ = message;
    ++consecutive_errors_;
    pause_until_ = std::chrono::steady_clock::now()
                   + std::chrono::seconds(consecutive_errors_
                                          * rai::BaseParser::MIN_DELAY);
    rai::Stats::Add(
        error_code,
        rai::ToString("EvmEndpoint::ProcessError: message=", message));
}

bool rai::EvmEndpoint::Paused() const
{
    return pause_until_ > std::chrono::steady_clock::now();
}

void rai::EvmEndpoint::DecreaseBatch()
{
    if (batch_detected_)
    {
        return;
    }

    if (batch_ >= rai::EvmEndpoint::BATCH_MIN + rai::EvmEndpoint::BATCH_STEP)
    {
        batch_ -= rai::EvmEndpoint::BATCH_STEP;
    }
}

void rai::EvmEndpoint::DetectBatch(uint64_t batch)
{
    if (batch_detected_)
    {
        return;
    }
    if (batch >= batch_)
    {
        batch_detected_ = true;
    }
}

rai::EvmBlockEvents::EvmBlockEvents(uint64_t height, const rai::BlockHash& hash)
    : height_(height), hash_(hash)
{
}

bool rai::EvmBlockEvents::operator==(const rai::EvmBlockEvents& other) const
{
    if (height_ != other.height_)
    {
        return false;
    }

    if (hash_ != other.hash_)
    {
        return false;
    }

    if (events_.size() != other.events_.size())
    {
        return false;
    }

    for (size_t i = 0; i < events_.size(); ++i)
    {
        if (*events_[i] != *other.events_[i])
        {
            return false;
        }
    }

    return true;
}

bool rai::EvmBlockEvents::operator!=(const rai::EvmBlockEvents& other) const
{
    return !(*this == other);
}

bool rai::EvmBlockEvents::AppendEvent(
    const std::shared_ptr<rai::CrossChainEvent>& event)
{
    if (event->BlockHeight() != height_)
    {
        rai::Log::Error(rai::ToString(
            "EvmSessionEntry::AppendEvent: invlaid block height, actual=",
            event->BlockHeight(), " expected=", height_));
        return true;
    }

    if (events_.empty() || events_.back()->Index() < event->Index())
    {
        events_.push_back(event);
        return false;
    }

    rai::Log::Error(rai::ToString(
        "EvmSessionEntry::AppendEvent: invlaid log index, current=",
        event->Index(), " previous=", events_.back()->Index()));
    return true;
}

rai::EvmSessionEntry::EvmSessionEntry(uint64_t index)
    : endpoint_index_(index),
      state_(rai::EvmSessionState::INIT),
      head_height_(0)
{
}

bool rai::EvmSessionEntry::operator==(const rai::EvmSessionEntry& other) const
{
    if (state_ != other.state_)
    {
        return false;
    }

    if (head_height_ != other.head_height_)
    {
        return false;
    }

    if (blocks_.size() != other.blocks_.size())
    {
        return false;
    }

    for (size_t i = 0; i < blocks_.size(); ++i)
    {
        if (blocks_[i] != other.blocks_[i])
        {
            return false;
        }
    }

    return true;
}

bool rai::EvmSessionEntry::operator!=(const rai::EvmSessionEntry& other) const
{
    return !(*this == other);
}

bool rai::EvmSessionEntry::AppendBlock(rai::EvmBlockEvents&& block)
{
    if (blocks_.empty() || blocks_.back().height_ < block.height_)
    {
        blocks_.push_back(std::move(block));
        return false;
    }

    rai::Log::Error(rai::ToString(
        "EvmSessionEntry::AppendBlockEvents: invlaid block height, block hash=",
        block.hash_.StringHex()));

    return true;
}

rai::EvmSession::EvmSession(uint64_t id, uint64_t tail,
                            const std::vector<uint64_t>& indices)
    : id_(id), tail_height_(tail)
{
    for (auto index : indices)
    {
        entries_.emplace_back(index);
    }
}

bool rai::EvmSession::Failed() const
{
    for (auto& entry : entries_)
    {
        if (entry.state_ == rai::EvmSessionState::FAILED)
        {
            return true;
        }
    }

    return false;
}

bool rai::EvmSession::Succeeded() const
{
    for (auto& entry : entries_)
    {
        if (entry.state_ != rai::EvmSessionState::SUCCESS)
        {
            return false;
        }
    }

    return true;
}

bool rai::EvmSession::Finished() const
{
    return Succeeded() || Failed();
}

bool rai::EvmSession::AllEntriesEqual() const
{
    for (size_t i = 1; i < entries_.size(); ++i)
    {
        if (entries_[i] != entries_[0])
        {
            return false;
        }
    }

    return true;
}

rai::EvmParser::EvmParser(rai::Token& token, const std::vector<rai::Url>& urls,
                          rai::Chain chain, uint64_t sync_interval,
                          uint64_t confirmations,
                          const std::string& core_contract)
    : token_(token),
      session_id_(0),
      confirmed_height_(0),
      submitted_height_(0),
      chain_(chain),
      sync_interval_(sync_interval),
      confirmations_(confirmations),
      since_height_(rai::ChainSinceHeight(chain)),
      core_contract_(core_contract),
      batch_(rai::EvmEndpoint::BATCH_MAX),
      waiting_(false)
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

    if (since_height_ == 0)
    {
        throw std::runtime_error("since_height_ is 0");
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, token_.ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        throw std::runtime_error("Construct transaction failed");
    }

    error = token_.ledger_.ChainHeadGet(transaction, chain_, confirmed_height_);
    if (error)
    {
        confirmed_height_ = since_height_ - 1;
    }
    submitted_height_ = confirmed_height_;

    if (core_contract_.size() != 42
        || rai::TokenAddress().DecodeEvmHex(core_contract_))
    {
        throw std::runtime_error("Invalid core contract address");
    }
}

void rai::EvmParser::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (CheckEndpoints_())
    {
        return;
    }

    if (waiting_)
    {
        return;
    }

    if (submitted_height_ < confirmed_height_)
    {
        SubmitBlocks_();
        return;
    }

    if (session_)
    {
        RunSession_(*session_);
    }

    if (!session_ || session_->Finished())
    {
        session_ = MakeSession_();
        if (session_)
        {
            RunSession_(*session_);
        }
    }
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
        entry.put("last_error_code",
                  std::to_string(static_cast<uint32_t>(i.last_error_code_)));
        entry.put("last_error_message", i.last_error_message_);
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
        endpoint.ProcessError(error_code, "receive");
    }
    else if (ptree.get_child_optional("error"))
    {
        error_code = rai::ErrorCode::TOKEN_CROSS_CHAIN_RESPONSE_ERROR;
        std::string error_info = "receive";
        auto error = ptree.get_child("error");
        if (error.get_optional<std::string>("message"))
        {
            error_info = error.get<std::string>("message");
            if (error_info.size() > 100)
            {
                error_info = error_info.substr(0, 100);
            }
        }
        endpoint.ProcessError(error_code, error_info);
    }
    else
    {
        endpoint.consecutive_errors_ = 0;
    }

    callback(index, request_id, error_code, ptree);
}

void rai::EvmParser::OnBlocksSubmitted(
    rai::ErrorCode error_code,
    const std::vector<rai::CrossChainBlockEvents>& blocks)
{
    std::unique_lock<std::mutex> lock(mutex_);
    waiting_ = false;

    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return;
    }

    for (const auto& i : blocks)
    {
        auto it = blocks_.begin();
        if (it == blocks_.end())
        {
            rai::Log::Error(
                rai::ToString("EvmParser::OnBlocksSubmitted: try to erase "
                              "unexisted block, chain=",
                              i.chain_, ", block_height=", i.block_height_));
            return;
        }

        if (i.block_height_ != it->first
            || i.block_height_ != it->second.height_)
        {
            rai::Log::Error(rai::ToString(
                "EvmParser::OnBlocksSubmitted: block mismatch, chain=",
                i.chain_, ", expected block_height=", it->second.height_,
                ", actual block_height=", i.block_height_));
            return;
        }

        blocks_.erase(it);
    }

    SubmitBlocks_();
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
            endpoint.ProcessTimeout();
        }
        if (endpoint.consecutive_errors_
            >= rai::EvmEndpoint::CONSECUTIVE_ERRORS_THRESHOLD)
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

void rai::EvmParser::CheckEvmChainId_(rai::EvmEndpoint& endpoint)
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

    rai::EvmEndpoint& endpoint = endpoints_[index];
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

void rai::EvmParser::QueryBlockHeight_(rai::EvmEndpoint& endpoint,
                                       uint64_t session_id)
{
    rai::Ptree params;
    Send_(endpoint, "eth_blockNumber", params,
          std::bind(&EvmParser::QueryBlockHeightCallback_, this, session_id,
                    std::placeholders::_1, std::placeholders::_3,
                    std::placeholders::_4));
}

void rai::EvmParser::QueryBlockHeightCallback_(uint64_t session_id,
                                               uint64_t endpoint_index,
                                               rai::ErrorCode error_code,
                                               const rai::Ptree& ptree)
{
    if (!session_)
    {
        return;
    }

    if (session_id != session_->id_)
    {
        rai::EvmEndpoint& endpoint = endpoints_[endpoint_index];
        rai::Stats::Add(
            rai::ErrorCode::TOKEN_CROSS_CHAIN_SESSION_ID,
            "EvmParser::Receive: endpoint=", endpoint.url_.String());
        return;
    }

    for (auto& entry : session_->entries_)
    {
        if (entry.endpoint_index_ != endpoint_index)
        {
            continue;
        }

        rai::EvmEndpoint& endpoint = endpoints_[endpoint_index];
        if (entry.state_ != rai::EvmSessionState::BLOCK_HEIGHT_QUERY)
        {
            std::string error_info = rai::ToString(
                "EvmParser::QueryBlockHeightCallback_: unexpected state=",
                entry.state_, ", endpoint=", endpoint.url_.String());
            rai::Log::Error(error_info);
            rai::Stats::Add(rai::ErrorCode::TOKEN_CROSS_CHAIN_UNEXPECTED,
                            error_info);
            return;
        }

        if (error_code != rai::ErrorCode::SUCCESS)
        {
            entry.state_ = rai::EvmSessionState::FAILED;
            return;
        }

        auto result = ptree.get_optional<std::string>("result");
        if (!result)
        {
            rai::Stats::Add(
                rai::ErrorCode::TOKEN_CROSS_CHAIN_UNEXPECTED,
                "EvmParser::QueryBlockHeightCallback_: no result, endpoint=",
                endpoint.url_.String());
            entry.state_ = rai::EvmSessionState::FAILED;
            return;
        }

        rai::uint256_union height;
        bool error = height.DecodeEvmHex(*result);
        if (error)
        {
            rai::Stats::Add(
                rai::ErrorCode::TOKEN_CROSS_CHAIN_UNEXPECTED,
                "EvmParser::QueryBlockHeightCallback_: decode error, endpoint=",
                endpoint.url_.String());
            entry.state_ = rai::EvmSessionState::FAILED;
            return;
        }

        if (height > std::numeric_limits<uint64_t>::max())
        {
            rai::Stats::Add(rai::ErrorCode::TOKEN_CROSS_CHAIN_UNEXPECTED,
                            "EvmParser::QueryBlockHeightCallback_: height "
                            "overflow, endpoint=",
                            endpoint.url_.String());
            entry.state_ = rai::EvmSessionState::FAILED;
            return;
        }
        entry.head_height_ = height.Uint64();

        if (entry.head_height_ < session_->tail_height_)
        {
            rai::Stats::Add(rai::ErrorCode::TOKEN_CROSS_CHAIN_BLOCK_HEIGHT,
                            "EvmParser::QueryBlockHeightCallback_: invalid "
                            "block height, endpoint=",
                            endpoint.url_.String());
            entry.state_ = rai::EvmSessionState::FAILED;
            return;
        }

        entry.state_ = rai::EvmSessionState::BLOCK_HEIGHT_RECEIVED;
        TryQueryLogs_();
        return;
    }
}

void rai::EvmParser::TryQueryLogs_()
{
    if (batch_ == 0)
    {
        rai::Log::Error("EvmParser::TryQueryLogs_: batch is 0");
        return;
    }

    if (!session_ || session_->entries_.empty())
    {
        rai::Log::Error("EvmParser::TryQueryLogs_: session is null or empty");
        return;
    }
    uint64_t tail_height = session_->tail_height_;

    uint64_t min_head_height = std::numeric_limits<uint64_t>::max();
    for (auto& entry : session_->entries_)
    {
        if (entry.state_ != rai::EvmSessionState::BLOCK_HEIGHT_RECEIVED)
        {
            return;
        }

        if (entry.head_height_ < tail_height)
        {
            entry.state_ = rai::EvmSessionState::FAILED;
            rai::Stats::Add(rai::ErrorCode::TOKEN_CROSS_CHAIN_UNEXPECTED,
                            "EvmParser::QueryBlockHeightCallback_: height "
                            "overflow");
            return;
        }

        uint64_t head_height = entry.head_height_;
        rai::EvmEndpoint& endpoint = endpoints_[entry.endpoint_index_];
        uint64_t batch_height = tail_height + endpoint.batch_ - 1;
        if (head_height > batch_height)
        {
            head_height = batch_height;
        }

        if (head_height < min_head_height)
        {
            min_head_height = head_height;
        }
    }

    if (min_head_height < tail_height)
    {
        rai::Log::Error("EvmParser::TryQueryLogs_: unexpected min head height");
        return;
    }

    if (tail_height + batch_ - 1 <= min_head_height)
    {
        min_head_height = tail_height + batch_ - 1;
    }
    else
    {
        batch_ = min_head_height - tail_height + 1;
    }

    for (auto& entry : session_->entries_)
    {
        rai::EvmEndpoint& endpoint = endpoints_[entry.endpoint_index_];
        entry.state_ = rai::EvmSessionState::LOGS_QUERY;
        QueryLogs_(endpoint, session_->id_, tail_height, min_head_height);
    }
}

void rai::EvmParser::QueryLogs_(rai::EvmEndpoint& endpoint, uint64_t session_id,
                                uint64_t tail_height, uint64_t head_height)
{
    rai::Ptree params;
    params.put("address", core_contract_);
    params.put("fromBlock",
               rai::uint256_union(tail_height).StringEvmShortHex());
    params.put("toBlock", rai::uint256_union(head_height).StringEvmShortHex());
    Send_(endpoint, "eth_getLogs", params,
          std::bind(&EvmParser::QueryLogsCallback_, this, session_id,
                    std::placeholders::_1, std::placeholders::_3,
                    std::placeholders::_4));
}

void rai::EvmParser::QueryLogsCallback_(uint64_t session_id,
                                        uint64_t endpoint_index,
                                        rai::ErrorCode error_code,
                                        const rai::Ptree& ptree)
{
    if (!session_)
    {
        return;
    }

    if (session_id != session_->id_)
    {
        rai::EvmEndpoint& endpoint = endpoints_[endpoint_index];
        rai::Stats::Add(
            rai::ErrorCode::TOKEN_CROSS_CHAIN_SESSION_ID,
            "EvmParser::Receive: endpoint=", endpoint.url_.String());
        return;
    }
    bool already_failed = session_->Failed();

    for (auto& entry : session_->entries_)
    {
        if (entry.endpoint_index_ != endpoint_index)
        {
            continue;
        }

        rai::EvmEndpoint& endpoint = endpoints_[endpoint_index];
        if (entry.state_ != rai::EvmSessionState::LOGS_QUERY)
        {
            std::string error_info = rai::ToString(
                "EvmParser::QueryLogsCallback_: unexpected state=",
                entry.state_, ", endpoint=", endpoint.url_.String());
            rai::Log::Error(error_info);
            rai::Stats::Add(rai::ErrorCode::TOKEN_CROSS_CHAIN_UNEXPECTED,
                            error_info);
            return;
        }

        bool error = true;
        do
        {
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                break;
            }

            auto result = ptree.get_child_optional("result");
            if (!result)
            {
                rai::Stats::Add(
                    rai::ErrorCode::TOKEN_CROSS_CHAIN_UNEXPECTED,
                    "EvmParser::QueryLogsCallback_: no result, endpoint=",
                    endpoint.url_.String());
                break;
            }

            error = ParseLogs_(*result, entry);
            IF_ERROR_BREAK(error);

            error = false;
        } while (0);

        if (error)
        {
            entry.state_ = rai::EvmSessionState::FAILED;
            endpoint.DecreaseBatch();
            if (!already_failed)
            {
                HalveBatch_();
            }
            return;
        }

        entry.state_ = rai::EvmSessionState::SUCCESS;
        endpoint.DetectBatch(batch_);
        TrySubmitSession_();
        return;
    }
}

std::vector<uint64_t> rai::EvmParser::RandomEndpoints_(size_t size) const
{
    std::vector<uint64_t> indices = ValidEndpoints_();
    if (indices.size() <= size)
    {
        return indices;
    }

    std::vector<uint64_t> result;
    while (result.size() < size)
    {
        uint64_t r = rai::Random(0, indices.size() - 1);
        result.push_back(indices[r]);
        indices.erase(indices.begin() + r);
    }

    return result;
}

std::vector<uint64_t> rai::EvmParser::ValidEndpoints_() const
{
    std::vector<uint64_t> result;
    for (const auto& i : endpoints_)
    {
        if (i.valid_ && !i.Paused())
        {
            result.push_back(i.index_);
        }
    }
    return result;
}

boost::optional<rai::EvmSession> rai::EvmParser::MakeSession_()
{
    std::vector<uint64_t> indices =
        RandomEndpoints_(rai::EvmParser::MIN_QUORUM);
    if (indices.size() < rai::EvmParser::MIN_QUORUM)
    {
        rai::Stats::Add(
            rai::ErrorCode::TOKEN_CROSS_CHAIN_QUORUM,
            "EvmParser::MakeSession_: chain=", rai::ChainToString(chain_));
        return boost::none;
    }

    return rai::EvmSession(++session_id_, confirmed_height_ + 1, indices);
}

void rai::EvmParser::TrySubmitSession_()
{
    if (!session_)
    {
        return;
    }

    if (!session_->Succeeded())
    {
        return;
    }

    if (session_->entries_.size() < rai::EvmParser::MIN_QUORUM)
    {
        return;
    }

    if (!session_->AllEntriesEqual())
    {
        return;
    }

    rai::EvmSessionEntry& entry = session_->entries_[0];
    if (session_->tail_height_ != confirmed_height_ + 1
        || session_->tail_height_ < entry.head_height_)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::TrySubmitSession_: unexpected height, tail_height=",
            session_->tail_height_, ", head_height=", entry.head_height_,
            ", confirmed_height_=", confirmed_height_));
        return;
    }

    if (entry.head_height_ > confirmed_height_ + confirmations_)
    {
        confirmed_height_ = entry.head_height_ - confirmations_;
    }

    size_t i = 0;
    for (uint64_t height = session_->tail_height_; height <= entry.head_height_;
         ++height)
    {
        rai::EvmBlockEvents block(height, rai::BlockHash(0));
        if (i < entry.blocks_.size() && entry.blocks_[i].height_ == height)
        {
            block = entry.blocks_[i];
            ++i;
        }

        auto it = blocks_.find(height);
        if (it == blocks_.end())
        {
            blocks_.insert(std::make_pair(height, std::move(block)));
        }
        else
        {
            it->second = std::move(block);
        }
    }

    SubmitBlocks_();
}

void rai::EvmParser::RunSession_(rai::EvmSession& session)
{
    if (session_id_ != session.id_) return;
    if (session.Finished()) return;

    for (auto& entry : session.entries_)
    {
        rai::EvmEndpoint& endpoint = endpoints_[entry.endpoint_index_];
        switch (entry.state_)
        {
            case rai::EvmSessionState::INIT:
            {
                QueryBlockHeight_(endpoint, session_id_);
                entry.state_ = rai::EvmSessionState::BLOCK_HEIGHT_QUERY;
                break;
            }
            case rai::EvmSessionState::FAILED:
            case rai::EvmSessionState::SUCCESS:
            case rai::EvmSessionState::BLOCK_HEIGHT_RECEIVED:
            {
                break;
            }
            case rai::EvmSessionState::BLOCK_HEIGHT_QUERY:
            case rai::EvmSessionState::LOGS_QUERY:
            {
                if (endpoint.sending_ && endpoint.Timeout())
                {
                    endpoint.ProcessTimeout();
                    entry.state_ = rai::EvmSessionState::FAILED;
                }
                break;
            }
            default:
            {
                rai::Log::Error("EvmParser::RunSession_: unexpected state");
                break;
            }
        }
    }
}

bool rai::EvmParser::ParseLogs_(const rai::Ptree& logs,
                                rai::EvmSessionEntry& entry)
{
    try
    {
        bool error = false;
        boost::optional<rai::EvmBlockEvents> block(boost::none);
        for (const auto& i : logs)
        {
            std::shared_ptr<rai::CrossChainEvent> event(nullptr);
            error = ParseLog_(i.second, event);
            IF_ERROR_RETURN(error, true);

            if (event == nullptr)
            {
                continue;
            }

            if (block && event->BlockHeight() != block->height_)
            {
                error = entry.AppendBlock(std::move(*block));
                IF_ERROR_RETURN(error, true);
                block = boost::none;
            }

            if (!block)
            {
                rai::BlockHash block_hash;
                error = ParseBlockHash_(i.second, block_hash);
                IF_ERROR_RETURN(error, true);
                block = rai::EvmBlockEvents(event->BlockHeight(), block_hash);
            }

            error = block->AppendEvent(event);
            IF_ERROR_RETURN(error, true);
        }

        if (block)
        {
            error = entry.AppendBlock(std::move(*block));
            IF_ERROR_RETURN(error, true);
            block = boost::none;
        }

        return false;
    }
    catch (const std::exception& e)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::ParseLogs_: exception=", e.what()));
        return true;
    }
}

bool rai::EvmParser::ParseLog_(const rai::Ptree& log,
                               std::shared_ptr<rai::CrossChainEvent>& event)
{
    uint64_t block_height = 0;
    bool error = ParseBlockHeight_(log, block_height);
    IF_ERROR_RETURN(error, true);

    uint64_t index = 0;
    error = ParseLogIndex_(log, index);
    IF_ERROR_RETURN(error, true);

    rai::BlockHash hash;
    error = ParseTxnHash_(log, hash);
    IF_ERROR_RETURN(error, true);

    std::vector<rai::uint256_union> data;
    error = ParseData_(log, data);
    IF_ERROR_RETURN(error, true);

    std::vector<rai::Topic> topics;
    error = ParseTopics_(log, topics);
    IF_ERROR_RETURN(error, true);
    if (topics.empty())
    {
        rai::Log::Error("EvmParser::ParseLog_: no topics");
        return true;
    }

    const rai::Topic& topic = topics[0];
    if (topic == rai::EvmTopic::TOKEN_INFO_INITIALIZED)
    {
        error =
            MakeTokenInitEvent_(block_height, index, hash, topics, data, event);
    }
    else if (topic == rai::EvmTopic::WRAPPED_ERC20_TOKEN_CREATED)
    {
        error = MakeWrappedErc20TokenCreateEvent_(block_height, index, hash,
                                                  topics, data, event);
    }
    else if (topic == rai::EvmTopic::WRAPPED_ERC721_TOKEN_CREATED)
    {
        error = MakeWrappedErc721TokenCreateEvent_(block_height, index, hash,
                                                   topics, data, event);
    }
    else if (topic == rai::EvmTopic::ERC20_TOKEN_MAPPED)
    {
        error = MakeErc20TokenMappedEvent_(block_height, index, hash, topics,
                                           data, event);
    }
    else if (topic == rai::EvmTopic::ERC721_TOKEN_MAPPED)
    {
        error = MakeErc721TokenMappedEvent_(block_height, index, hash, topics,
                                            data, event);
    }
    else if (topic == rai::EvmTopic::ETH_MAPPED)
    {
        error =
            MakeEthMappedEvent_(block_height, index, hash, topics, data, event);
    }
    else if (topic == rai::EvmTopic::ERC20_TOKEN_UNMAPPED)
    {
        error = MakeErc20TokenUnmappedEvent_(block_height, index, hash, topics,
                                             data, event);
    }
    else if (topic == rai::EvmTopic::ERC721_TOKEN_UNMAPPED)
    {
        error = MakeErc721TokenUnmappedEvent_(block_height, index, hash, topics,
                                              data, event);
    }
    else if (topic == rai::EvmTopic::ETH_UNMAPPED)
    {
        error = MakeEthUnmappedEvent_(block_height, index, hash, topics, data,
                                      event);
    }
    else if (topic == rai::EvmTopic::ERC20_TOKEN_WRAPPED)
    {
        error = MakeErc20TokenWrappedEvent_(block_height, index, hash, topics,
                                            data, event);
    }
    else if (topic == rai::EvmTopic::ERC721_TOKEN_WRAPPED)
    {
        error = MakeErc721TokenWrappedEvent_(block_height, index, hash, topics,
                                             data, event);
    }
    else if (topic == rai::EvmTopic::ERC20_TOKEN_UNWRAPPED)
    {
        error = MakeErc20TokenUnwrappedEvent_(block_height, index, hash, topics,
                                              data, event);
    }
    else if (topic == rai::EvmTopic::ERC721_TOKEN_UNWRAPPED)
    {
        error = MakeErc721TokenUnwrappedEvent_(block_height, index, hash,
                                               topics, data, event);
    }
    else
    {
        // pass
    }

    if (event != nullptr && event->Chain() != chain_)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::ParseLog_: invalid chain, expected=", chain_,
            "actual=", event->Chain()));
        return true;
    }

    return error;
}

bool rai::EvmParser::ParseBlockHash_(const rai::Ptree& log,
                                     rai::BlockHash& hash) const
{
    try
    {
        std::string hash_str = log.get<std::string>("blockHash");
        bool error = hash.DecodeEvmHex(hash_str);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "EvmParser::ParseBlockHash_: decode error, blockHash=",
                hash_str));
            return true;
        }

        return false;
    }
    catch (const std::exception& e)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::ParseBlockHash_: exception=", e.what()));
        return true;
    }
}

bool rai::EvmParser::ParseBlockHeight_(const rai::Ptree& log,
                                       uint64_t& height) const
{
    try
    {
        rai::uint256_union block_height;
        std::string height_str = log.get<std::string>("blockNumber");
        bool error = block_height.DecodeEvmHex(height_str);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "EvmParser::ParseBlockHeight_: decode error, blockNumber=",
                height_str));
            return true;
        }
        if (block_height > std::numeric_limits<uint64_t>::max())
        {
            rai::Log::Error(
                rai::ToString("EvmParser::ParseBlockHeight_: block height "
                              "overflow, blockNumber=",
                              height_str));
            return true;
        }
        height = block_height.Uint64();
        return false;
    }
    catch (const std::exception& e)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::ParseBlockHeight_: exception=", e.what()));
        return true;
    }
}

bool rai::EvmParser::ParseLogIndex_(const rai::Ptree& log,
                                    uint64_t& index) const
{
    try
    {
        rai::uint256_union log_index;
        std::string index_str = log.get<std::string>("logIndex");
        bool error = log_index.DecodeEvmHex(index_str);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "EvmParser::ParseLogIndex_: decode error, logIndex=",
                index_str));
            return true;
        }
        if (log_index > std::numeric_limits<uint64_t>::max())
        {
            rai::Log::Error(
                rai::ToString("EvmParser::ParseLogIndex_: log index "
                              "overflow, logIndex=",
                              index_str));
            return true;
        }
        index = log_index.Uint64();
        return false;
    }
    catch (const std::exception& e)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::ParseLogIndex_: exception=", e.what()));
        return true;
    }
}

bool rai::EvmParser::ParseTxnHash_(const rai::Ptree& log,
                                   rai::BlockHash& hash) const
{
    try
    {
        std::string hash_str = log.get<std::string>("transactionHash");
        bool error = hash.DecodeEvmHex(hash_str);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "EvmParser::ParseTxnHash_: decode error, transactionHash=",
                hash_str));
            return true;
        }

        return false;
    }
    catch (const std::exception& e)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::ParseTxnHash_: exception=", e.what()));
        return true;
    }
}

bool rai::EvmParser::ParseTopics_(const rai::Ptree& log,
                                  std::vector<rai::Topic>& topics) const
{
    try
    {
        const rai::Ptree& topics_ptree = log.get_child("topics");
        for (const auto& i : topics_ptree)
        {
            rai::Topic topic;
            bool error = topic.DecodeEvmHex(i.second.get<std::string>(""));
            if (error)
            {
                rai::Log::Error(rai::ToString(
                    "EvmParser::ParseTopics_: decode error, topic=",
                    i.second.get<std::string>("")));
                return true;
            }
            topics.push_back(topic);
        }
        return false;
    }
    catch (const std::exception& e)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::ParseTopics_: exception=", e.what()));
        return true;
    }
}

bool rai::EvmParser::ParseData_(const rai::Ptree& log,
                                std::vector<rai::uint256_union>& result) const
{
    try
    {
        std::string data = log.get<std::string>("data");
        if (!rai::StringStartsWith(data, "0x", true) || data.size() % 64 != 2)
        {
            rai::Log::Error(
                rai::ToString("EvmParser::ParseData_: data format error, "
                              "data=",
                              data));
            return true;
        }
        data = data.substr(2);

        size_t num = data.size() / 64;
        for (size_t i = 0; i < num; ++i)
        {
            rai::uint256_union item;
            bool error = item.DecodeHex(data.substr(i * 64, 64));
            if (error)
            {
                rai::Log::Error(
                    rai::ToString("EvmParser::ParseData_: decode error, data=",
                                  data.substr(i * 64, 64)));
                return true;
            }
            result.push_back(item);
        }

        return false;
    }
    catch (const std::exception& e)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::ParseData_: exception=", e.what()));
        return true;
    }
}

bool rai::EvmParser::MakeTokenInitEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 2)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeTokenInitEvent_: topics size error, "
                          "topics size=",
                          topics.size()));
        return true;
    }

    if (data.size() != 4)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeTokenInitEvent_: data size error, "
                          "data size=",
                          data.size()));
        return true;
    }

    rai::TokenAddress address;
    bool error = MakeUint160_(topics[1], address);
    IF_ERROR_RETURN(error, true);

    rai::TokenType token_type;
    error = MakeTokenType_(data[0], token_type);
    IF_ERROR_RETURN(error, true);

    bool wrapped;
    error = MakeBool_(data[1], wrapped);
    IF_ERROR_RETURN(error, true);

    uint8_t decimals;
    error = MakeUint8_(data[2], decimals);
    IF_ERROR_RETURN(error, true);

    rai::Chain chain;
    error = MakeChain_(data[3], chain);
    IF_ERROR_RETURN(error, true);

    if (!wrapped)
    {
        event = std::make_shared<rai::CrossChainTokenEvent>(
            height, index, chain, address, token_type, decimals, wrapped, hash);
    }
    return false;
}

bool rai::EvmParser::MakeWrappedErc20TokenCreateEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 4)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeWrappedErc20TokenCreateEvent_: topics size error, "
            "topics size=",
            topics.size()));
        return true;
    }

    if (data.size() < 8)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeWrappedErc20TokenCreateEvent_: data size error, "
            "data size=",
            data.size()));
        return true;
    }

    rai::Chain original_chain;
    bool error = MakeChain_(topics[1], original_chain);
    IF_ERROR_RETURN(error, true);

    rai::TokenAddress original_address = topics[2];

    rai::TokenAddress address;
    error = MakeUint160_(topics[3], address);
    IF_ERROR_RETURN(error, true);

    rai::Chain chain;
    error = MakeChain_(data[4], chain);
    IF_ERROR_RETURN(error, true);

    event = std::make_shared<rai::CrossChainCreateEvent>(
        height, index, chain, address, rai::TokenType::_20, original_chain,
        original_address, hash);

    return false;
}

bool rai::EvmParser::MakeWrappedErc721TokenCreateEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 4)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeWrappedErc721TokenCreateEvent_: topics size error, "
            "topics size=",
            topics.size()));
        return true;
    }

    if (data.size() < 7)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeWrappedErc721TokenCreateEvent_: data size error, "
            "data size=",
            data.size()));
        return true;
    }

    rai::Chain original_chain;
    bool error = MakeChain_(topics[1], original_chain);
    IF_ERROR_RETURN(error, true);

    rai::TokenAddress original_address = topics[2];

    rai::TokenAddress address;
    error = MakeUint160_(topics[3], address);
    IF_ERROR_RETURN(error, true);

    rai::Chain chain;
    error = MakeChain_(data[3], chain);
    IF_ERROR_RETURN(error, true);

    event = std::make_shared<rai::CrossChainCreateEvent>(
        height, index, chain, address, rai::TokenType::_721, original_chain,
        original_address, hash);

    return false;
}

bool rai::EvmParser::MakeErc20TokenMappedEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 4)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc20TokenMappedEvent_: topics size error, "
            "topics size=",
            topics.size()));
        return true;
    }

    if (data.size() != 3)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc20TokenMappedEvent_: data size error, "
            "data size=",
            data.size()));
        return true;
    }

    rai::TokenAddress address;
    bool error = MakeUint160_(topics[1], address);
    IF_ERROR_RETURN(error, true);

    rai::Account from;
    error = MakeUint160_(topics[2], from);
    IF_ERROR_RETURN(error, true);

    rai::Account to = topics[3];
    rai::TokenValue value = data[1];

    rai::Chain chain;
    error = MakeChain_(data[2], chain);
    IF_ERROR_RETURN(error, true);

    event = std::make_shared<rai::CrossChainMapEvent>(
        height, index, chain, address, rai::TokenType::_20, from, to, value,
        hash);
    return false;
}

bool rai::EvmParser::MakeErc721TokenMappedEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 4)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc721TokenMappedEvent_: topics size error, "
            "topics size=",
            topics.size()));
        return true;
    }

    if (data.size() != 2)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc721TokenMappedEvent_: data size error, "
            "data size=",
            data.size()));
        return true;
    }

    rai::TokenAddress address;
    bool error = MakeUint160_(topics[1], address);
    IF_ERROR_RETURN(error, true);

    rai::Account from;
    error = MakeUint160_(topics[2], from);
    IF_ERROR_RETURN(error, true);

    rai::Account to = topics[3];
    rai::TokenValue value = data[0];

    rai::Chain chain;
    error = MakeChain_(data[1], chain);
    IF_ERROR_RETURN(error, true);

    event = std::make_shared<rai::CrossChainMapEvent>(
        height, index, chain, address, rai::TokenType::_721, from, to, value,
        hash);
    return false;
}

bool rai::EvmParser::MakeEthMappedEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 3)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeEthMappedEvent_: topics size error, "
                          "topics size=",
                          topics.size()));
        return true;
    }

    if (data.size() != 2)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeEthMappedEvent_: data size error, "
                          "data size=",
                          data.size()));
        return true;
    }

    rai::Account from;
    bool error = MakeUint160_(topics[1], from);
    IF_ERROR_RETURN(error, true);

    rai::Account to = topics[2];
    rai::TokenValue value = data[0];

    rai::Chain chain;
    error = MakeChain_(data[1], chain);
    IF_ERROR_RETURN(error, true);

    event = std::make_shared<rai::CrossChainMapEvent>(
        height, index, chain, rai::NativeAddress(), rai::TokenType::_20, from,
        to, value, hash);
    return false;
}

bool rai::EvmParser::MakeErc20TokenUnmappedEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 4)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc20TokenUnmappedEvent_: topics size error, "
            "topics size=",
            topics.size()));
        return true;
    }

    if (data.size() != 5)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc20TokenUnmappedEvent_: data size error, "
            "data size=",
            data.size()));
        return true;
    }

    rai::TokenAddress address;
    bool error = MakeUint160_(topics[1], address);
    IF_ERROR_RETURN(error, true);

    rai::Account from = topics[2];

    rai::Account to;
    error = MakeUint160_(topics[3], to);
    IF_ERROR_RETURN(error, true);

    rai::BlockHash from_tx_hash = data[0];

    uint64_t from_height;
    error = MakeUint64_(data[1], from_height);
    IF_ERROR_RETURN(error, true);

    rai::TokenValue value = data[3];

    rai::Chain chain;
    error = MakeChain_(data[4], chain);
    IF_ERROR_RETURN(error, true);

    event = std::make_shared<rai::CrossChainUnmapEvent>(
        height, index, chain, address, rai::TokenType::_20, from, from_height,
        from_tx_hash, to, value, hash);
    return false;
}

bool rai::EvmParser::MakeErc721TokenUnmappedEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 4)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc721TokenUnmappedEvent_: topics size error, "
            "topics size=",
            topics.size()));
        return true;
    }

    if (data.size() != 4)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc721TokenUnmappedEvent_: data size error, "
            "data size=",
            data.size()));
        return true;
    }

    rai::TokenAddress address;
    bool error = MakeUint160_(topics[1], address);
    IF_ERROR_RETURN(error, true);

    rai::Account from = topics[2];

    rai::Account to;
    error = MakeUint160_(topics[3], to);
    IF_ERROR_RETURN(error, true);

    rai::BlockHash from_tx_hash = data[0];

    uint64_t from_height;
    error = MakeUint64_(data[1], from_height);
    IF_ERROR_RETURN(error, true);

    rai::TokenValue value = data[2];

    rai::Chain chain;
    error = MakeChain_(data[3], chain);
    IF_ERROR_RETURN(error, true);

    event = std::make_shared<rai::CrossChainUnmapEvent>(
        height, index, chain, address, rai::TokenType::_721, from, from_height,
        from_tx_hash, to, value, hash);
    return false;
}

bool rai::EvmParser::MakeEthUnmappedEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 3)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeEthUnmappedEvent_: topics size error, topics size=",
            topics.size()));
        return true;
    }

    if (data.size() != 4)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeEthUnmappedEvent_: data size error, "
                          "data size=",
                          data.size()));
        return true;
    }

    rai::Account from = topics[1];

    rai::Account to;
    bool error = MakeUint160_(topics[2], to);
    IF_ERROR_RETURN(error, true);

    rai::BlockHash from_tx_hash = data[0];

    uint64_t from_height;
    error = MakeUint64_(data[1], from_height);
    IF_ERROR_RETURN(error, true);

    rai::TokenValue value = data[2];

    rai::Chain chain;
    error = MakeChain_(data[3], chain);
    IF_ERROR_RETURN(error, true);

    event = std::make_shared<rai::CrossChainUnmapEvent>(
        height, index, chain, rai::NativeAddress(), rai::TokenType::_20, from,
        from_height, from_tx_hash, to, value, hash);
    return false;
}

bool rai::EvmParser::MakeErc20TokenWrappedEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 4)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc20TokenWrappedEvent_: topics size error, "
            "topics size=",
            topics.size()));
        return true;
    }

    if (data.size() != 6)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeErc20TokenWrappedEvent_: data size "
                          "error, data size=",
                          data.size()));
        return true;
    }

    rai::Chain original_chain;
    bool error = MakeChain_(topics[1], original_chain);
    IF_ERROR_RETURN(error, true);

    rai::TokenAddress original_address = topics[2];
    rai::Account from = topics[3];

    rai::Account to;
    error = MakeUint160_(data[0], to);
    IF_ERROR_RETURN(error, true);

    rai::BlockHash from_tx_hash = data[1];

    uint64_t from_height;
    error = MakeUint64_(data[2], from_height);
    IF_ERROR_RETURN(error, true);

    rai::TokenAddress address;
    error = MakeUint160_(data[3], address);
    IF_ERROR_RETURN(error, true);

    rai::TokenValue value = data[4];

    rai::Chain chain;
    error = MakeChain_(data[5], chain);
    IF_ERROR_RETURN(error, true);

    event = std::make_shared<rai::CrossChainWrapEvent>(
        height, index, chain, address, rai::TokenType::_20, original_chain,
        original_address, from, from_height, from_tx_hash, to, value, hash);
    return false;
}

bool rai::EvmParser::MakeErc721TokenWrappedEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 4)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc721TokenWrappedEvent_: topics size error, "
            "topics size=",
            topics.size()));
        return true;
    }

    if (data.size() != 6)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeErc721TokenWrappedEvent_: data size "
                          "error, data size=",
                          data.size()));
        return true;
    }

    rai::Chain original_chain;
    bool error = MakeChain_(topics[1], original_chain);
    IF_ERROR_RETURN(error, true);

    rai::TokenAddress original_address = topics[2];
    rai::Account from = topics[3];

    rai::Account to;
    error = MakeUint160_(data[0], to);
    IF_ERROR_RETURN(error, true);

    rai::BlockHash from_tx_hash = data[1];

    uint64_t from_height;
    error = MakeUint64_(data[2], from_height);
    IF_ERROR_RETURN(error, true);

    rai::TokenAddress address;
    error = MakeUint160_(data[3], address);
    IF_ERROR_RETURN(error, true);

    rai::TokenValue value = data[4];

    rai::Chain chain;
    error = MakeChain_(data[5], chain);
    IF_ERROR_RETURN(error, true);

    event = std::make_shared<rai::CrossChainWrapEvent>(
        height, index, chain, address, rai::TokenType::_721, original_chain,
        original_address, from, from_height, from_tx_hash, to, value, hash);
    return false;
}

bool rai::EvmParser::MakeErc20TokenUnwrappedEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 4)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc20TokenUnwrappedEvent_: topics size error, "
            "topics size=",
            topics.size()));
        return true;
    }

    if (data.size() != 4)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeErc20TokenUnwrappedEvent_: data size "
                          "error, data size=",
                          data.size()));
        return true;
    }

    rai::Chain original_chain;
    bool error = MakeChain_(topics[1], original_chain);
    IF_ERROR_RETURN(error, true);

    rai::TokenAddress original_address = topics[2];

    rai::Account from;
    error = MakeUint160_(topics[3], from);
    IF_ERROR_RETURN(error, true);

    rai::Account to = data[0];

    rai::TokenAddress address;
    error = MakeUint160_(data[1], address);
    IF_ERROR_RETURN(error, true);

    rai::TokenValue value = data[2];

    rai::Chain chain;
    error = MakeChain_(data[3], chain);
    IF_ERROR_RETURN(error, true);

    event = std::make_shared<rai::CrossChainUnwrapEvent>(
        height, index, chain, address, rai::TokenType::_20, original_chain,
        original_address, from, to, value, hash);

    return false;
}

bool rai::EvmParser::MakeErc721TokenUnwrappedEvent_(
    uint64_t height, uint64_t index, const rai::BlockHash& hash,
    const std::vector<rai::Topic>& topics,
    const std::vector<rai::uint256_union>& data,
    std::shared_ptr<rai::CrossChainEvent>& event) const
{
    if (topics.size() != 4)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc721TokenUnwrappedEvent_: topics size error, "
            "topics size=",
            topics.size()));
        return true;
    }

    if (data.size() != 4)
    {
        rai::Log::Error(rai::ToString(
            "EvmParser::MakeErc721TokenUnwrappedEvent_: data size "
            "error, data size=",
            data.size()));
        return true;
    }

    rai::Chain original_chain;
    bool error = MakeChain_(topics[1], original_chain);
    IF_ERROR_RETURN(error, true);

    rai::TokenAddress original_address = topics[2];

    rai::Account from;
    error = MakeUint160_(topics[3], from);
    IF_ERROR_RETURN(error, true);

    rai::Account to = data[0];

    rai::TokenAddress address;
    error = MakeUint160_(data[1], address);
    IF_ERROR_RETURN(error, true);

    rai::TokenValue value = data[2];

    rai::Chain chain;
    error = MakeChain_(data[3], chain);
    IF_ERROR_RETURN(error, true);

    event = std::make_shared<rai::CrossChainUnwrapEvent>(
        height, index, chain, address, rai::TokenType::_721, original_chain,
        original_address, from, to, value, hash);

    return false;
}

bool rai::EvmParser::MakeTokenType_(const rai::uint256_union& data,
                                    rai::TokenType& type) const
{
    if (data == static_cast<uint64_t>(rai::TokenType::_20))
    {
        type = rai::TokenType::_20;
    }
    else if (data == static_cast<uint64_t>(rai::TokenType::_721))
    {
        type = rai::TokenType::_721;
    }
    else
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeTokenType_: data format error, data=",
                          data.StringDec()));
        return true;
    }
    return false;
}

bool rai::EvmParser::MakeChain_(const rai::uint256_union& data,
                                rai::Chain& chain) const
{
    uint32_t u32;
    bool error = MakeUint32_(data, u32);
    IF_ERROR_RETURN(error, true);

    chain = static_cast<rai::Chain>(u32);
    if (chain == rai::Chain::INVALID)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeChain_: data format error, data=",
                          data.StringDec()));
        return true;
    }

    return false;
}

bool rai::EvmParser::MakeUint160_(const rai::uint256_union& data,
                                  rai::TokenAddress& address) const
{
    if (data > rai::EvmParser::MAX_ADDRESS)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeUint160_: data format error, data=",
                          data.StringHex()));
        return true;
    }

    address = data;
    return false;
}

bool rai::EvmParser::MakeUint64_(const rai::uint256_union& data,
                                 uint64_t& u64) const
{
    if (data > std::numeric_limits<uint64_t>::max())
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeUint64_: data format error, data=",
                          data.StringHex()));
        return true;
    }

    u64 = data.Uint64();
    return false;
}

bool rai::EvmParser::MakeUint32_(const rai::uint256_union& data,
                                 uint32_t& u32) const
{
    if (data > std::numeric_limits<uint32_t>::max())
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeUint32_: data format error, data=",
                          data.StringHex()));
        return true;
    }

    u32 = data.Uint32();
    return false;
}

bool rai::EvmParser::MakeUint8_(const rai::uint256_union& data,
                                uint8_t& u8) const
{
    if (data > std::numeric_limits<uint8_t>::max())
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeUint8_: data format error, data=",
                          data.StringHex()));
        return true;
    }

    u8 = data.Uint8();
    return false;
}

bool rai::EvmParser::MakeBool_(const rai::uint256_union& data, bool& b) const
{
    if (data != 0 && data != 1)
    {
        rai::Log::Error(
            rai::ToString("EvmParser::MakeBool_: data format error, data=",
                          data.StringHex()));
        return true;
    }

    b = data == 0;
    return false;
}

void rai::EvmParser::HalveBatch_()
{
    batch_ /= 2;
    if (batch_ == 0)
    {
        batch_ = 1;
    }
}

void rai::EvmParser::SubmitBlocks_()
{
    if (waiting_)
    {
        return;
    }

    if (submitted_height_ >= confirmed_height_)
    {
        return;
    }

    std::vector<rai::CrossChainBlockEvents> blocks;
    for (auto i = blocks_.begin(), n = blocks_.end(); i != n; ++i)
    {
        if (i->first > confirmed_height_)
        {
            break;
        }

        if (blocks.empty() || i->second.events_.empty())
        {
            blocks.emplace_back(chain_, i->first, i->second.events_);
        }

        if (!i->second.events_.empty())
        {
            break;
        }
    }

    if (blocks.empty())
    {
        return;
    }

    waiting_ = true;
    token_.SubmitCrossChainBlocks(
        blocks, std::bind(&EvmParser::OnBlocksSubmitted, this,
                          std::placeholders::_1, std::placeholders::_2));
}

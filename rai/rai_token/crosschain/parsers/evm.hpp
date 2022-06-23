#pragma once

#include <mutex>
#include <chrono>
#include <map>
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/util.hpp>
#include <rai/common/chain.hpp>
#include <rai/rai_token/crosschain/event.hpp>
#include <rai/rai_token/crosschain/parsers/base.hpp>

namespace rai
{

class Token;

bool GetEvmChainId(rai::Chain, rai::EvmChainId&);

using EvmRequestCallback =
    std::function<void(uint64_t, uint64_t, rai::ErrorCode, const rai::Ptree&)>;

class EvmEndpoint
{
public:
    EvmEndpoint(uint64_t, const rai::Url&);
    void RecordTimeSpan();
    uint64_t AverageTimeSpan() const;
    bool Timeout() const;
    void ProcessTimeout();
    void ProcessError(rai::ErrorCode, const std::string&);
    bool Paused() const;
    void DecreaseBatch();
    void DetectBatch(uint64_t);

    static constexpr uint64_t BATCH_MAX = 1000;
    static constexpr uint64_t BATCH_MIN = 100;
    static constexpr uint64_t BATCH_STEP = 100;
    static constexpr uint64_t CONSECUTIVE_ERRORS_THRESHOLD = 5;
    static constexpr size_t MAX_TIME_SPAN_SIZE = 10;

    uint64_t index_;
    rai::Url url_;
    uint64_t batch_;
    uint64_t request_id_;
    uint64_t consecutive_errors_;
    bool checked_;
    bool valid_;
    bool sending_;
    bool batch_detected_;
    std::chrono::steady_clock::time_point send_at_;
    uint64_t timeout_count_;
    uint64_t min_time_span_;
    uint64_t max_time_span_;
    std::vector<uint64_t> time_spans_;
    std::chrono::steady_clock::time_point pause_until_;
    rai::ErrorCode last_error_code_;
    std::string last_error_message_;
};

enum class EvmSessionState
{
    INIT                    = 0,
    BLOCK_HEIGHT_QUERY      = 1,
    BLOCK_HEIGHT_RECEIVED   = 2,
    LOGS_QUERY              = 3,
    SUCCESS                 = 4,
    FAILED                  = 5,
};

class EvmBlockEvents
{
public:
    EvmBlockEvents(uint64_t, const rai::BlockHash&);
    bool operator==(const rai::EvmBlockEvents&) const;
    bool operator!=(const rai::EvmBlockEvents&) const;
    bool AppendEvent(const std::shared_ptr<rai::CrossChainEvent>&);

    uint64_t height_;
    rai::BlockHash hash_;
    std::vector<std::shared_ptr<rai::CrossChainEvent>> events_;
};

class EvmSessionEntry
{
public:
    EvmSessionEntry(uint64_t);
    bool AppendBlock(rai::EvmBlockEvents&&);

    uint64_t endpoint_index_;
    rai::EvmSessionState state_;
    uint64_t head_height_;
    std::vector<rai::EvmBlockEvents> blocks_;
};

class EvmSession
{
public:
    EvmSession(uint64_t, uint64_t, const std::vector<uint64_t>&);
    bool Failed() const;
    bool Succeeded() const;
    bool Finished() const;

    uint64_t id_;
    uint64_t tail_height_;
    std::vector<rai::EvmSessionEntry> entries_;
};

class EvmTopic
{
public:
    static const rai::Topic TOKEN_INFO_INITIALIZED;
    static const rai::Topic WRAPPED_ERC20_TOKEN_CREATED;
    static const rai::Topic WRAPPED_ERC721_TOKEN_CREATED;
    static const rai::Topic ERC20_TOKEN_MAPPED;
    static const rai::Topic ERC20_TOKEN_UNMAPPED;
    static const rai::Topic ERC20_TOKEN_WRAPPED;
    static const rai::Topic ERC20_TOKEN_UNWRAPPED;
    static const rai::Topic ERC721_TOKEN_MAPPED;
    static const rai::Topic ERC721_TOKEN_UNMAPPED;
    static const rai::Topic ERC721_TOKEN_WRAPPED;
    static const rai::Topic ERC721_TOKEN_UNWRAPPED;
    static const rai::Topic ETH_MAPPED;
    static const rai::Topic ETH_UNMAPPED;
};

class EvmParser : public BaseParser
{
public:
    EvmParser(rai::Token&, const std::vector<rai::Url>&, rai::Chain, uint64_t,
              uint64_t, uint64_t, const std::string&);
    virtual ~EvmParser() = default;
    virtual void Run() override;
    virtual uint64_t Delay() const override;
    virtual void Status(rai::Ptree&) const override;

    void Receive(uint64_t, uint64_t, rai::ErrorCode, const std::string&,
                 const EvmRequestCallback&);
    rai::Token& token_;

    static constexpr size_t MIN_QUORUM = 2;
    static const rai::TokenAddress MAX_ADDRESS;

private:
    void Send_(rai::EvmEndpoint&, const std::string&, const rai::Ptree&,
               const EvmRequestCallback&);
    bool CheckEndpoints_();
    void CheckEvmChainId_(rai::EvmEndpoint&);
    void EvmChainIdCallback_(uint64_t, uint64_t, rai::ErrorCode,
                             const rai::Ptree&);
    void QueryBlockHeight_(rai::EvmEndpoint&, uint64_t);
    void QueryBlockHeightCallback_(uint64_t, uint64_t, rai::ErrorCode,
                                   const rai::Ptree&);
    void TryQueryLogs_();
    void QueryLogs_(rai::EvmEndpoint&, uint64_t, uint64_t, uint64_t);
    void QueryLogsCallback_(uint64_t, uint64_t, rai::ErrorCode,
                            const rai::Ptree&);
    std::vector<uint64_t> RandomEndpoints_(size_t) const;
    std::vector<uint64_t> ValidEndpoints_() const;
    boost::optional<rai::EvmSession> MakeSession_();
    void RunSession_(rai::EvmSession&);
    bool ParseLogs_(const rai::Ptree&, rai::EvmSessionEntry&);
    bool ParseLog_(const rai::Ptree&, std::shared_ptr<rai::CrossChainEvent>&);
    bool ParseBlockHash_(const rai::Ptree&, rai::BlockHash&) const;
    bool ParseBlockHeight_(const rai::Ptree&, uint64_t&) const;
    bool ParseLogIndex_(const rai::Ptree&, uint64_t&) const;
    bool ParseTxnHash_(const rai::Ptree&, rai::BlockHash&) const;
    bool ParseTopics_(const rai::Ptree&, std::vector<rai::Topic>&) const;
    bool ParseData_(const rai::Ptree&, std::vector<rai::uint256_union>&) const;
    bool MakeTokenInitEvent_(uint64_t, uint64_t, const rai::BlockHash&,
                             const std::vector<rai::Topic>&,
                             const std::vector<rai::uint256_union>&,
                             std::shared_ptr<rai::CrossChainEvent>&);
    bool MakeWrappedErc20TokenCreateEvent_(
        uint64_t, uint64_t, const rai::BlockHash&,
        const std::vector<rai::Topic>&, const std::vector<rai::uint256_union>&,
        std::shared_ptr<rai::CrossChainEvent>&);
    bool MakeWrappedErc721TokenCreateEvent_(
        uint64_t, uint64_t, const rai::BlockHash&,
        const std::vector<rai::Topic>&, const std::vector<rai::uint256_union>&,
        std::shared_ptr<rai::CrossChainEvent>&);
    bool MakeErc20TokenMappedEvent_(uint64_t, uint64_t, const rai::BlockHash&,
                                    const std::vector<rai::Topic>&,
                                    const std::vector<rai::uint256_union>&,
                                    std::shared_ptr<rai::CrossChainEvent>&);
    bool MakeErc721TokenMappedEvent_(uint64_t, uint64_t, const rai::BlockHash&,
                                     const std::vector<rai::Topic>&,
                                     const std::vector<rai::uint256_union>&,
                                     std::shared_ptr<rai::CrossChainEvent>&);
    bool MakeEthMappedEvent_(uint64_t, uint64_t, const rai::BlockHash&,
                             const std::vector<rai::Topic>&,
                             const std::vector<rai::uint256_union>&,
                             std::shared_ptr<rai::CrossChainEvent>&);
    bool MakeTokenType_(const rai::uint256_union&, rai::TokenType&) const;
    bool MakeChain_(const rai::uint256_union&, rai::Chain&) const;
    bool MakeUint160_(const rai::uint256_union&, rai::TokenAddress&) const;
    bool MakeUint64_(const rai::uint256_union&, uint64_t&) const;
    bool MakeUint32_(const rai::uint256_union&, uint32_t&) const;
    bool MakeUint8_(const rai::uint256_union&, uint8_t&) const;
    bool MakeBool_(const rai::uint256_union&, bool&) const;
    void SubmitBlocks_();

    mutable std::mutex mutex_;
    uint64_t session_id_;
    boost::optional<rai::EvmSession> session_;
    uint64_t confirmed_height_;
    uint64_t submitted_height_;
    std::map<uint64_t, rai::EvmBlockEvents> blocks_;
    std::vector<EvmEndpoint> endpoints_;
    rai::Chain chain_;
    rai::EvmChainId evm_chain_id_;
    uint64_t sync_interval_;
    uint64_t confirmations_;
    uint64_t since_height_;
    std::string core_contract_;
    uint64_t batch_;
    bool waiting_;
};

}  // namespace rai
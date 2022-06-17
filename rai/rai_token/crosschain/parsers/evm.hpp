#pragma once

#include <mutex>
#include <chrono>
#include <boost/asio.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/util.hpp>
#include <rai/common/chain.hpp>
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

    static constexpr uint64_t BATCH_MAX = 1000;
    static constexpr uint64_t BATCH_MIN = 100;
    static constexpr uint64_t BATCH_STEP = 100;
    static constexpr uint64_t CONSECUTIVE_ERRORS_MAX = 5;
    static constexpr size_t MAX_TIME_SPAN_SIZE = 10;

    uint64_t index_;
    rai::Url url_;
    uint64_t batch_;
    uint64_t request_id_;
    uint64_t consecutive_errors_;
    bool checked_;
    bool valid_;
    bool sending_;
    std::chrono::steady_clock::time_point send_at_;
    uint64_t timeout_count_;
    uint64_t min_time_span_;
    uint64_t max_time_span_;
    std::vector<uint64_t> time_spans_;
};

enum class EvmSessionState
{
    INIT                    = 0,
    GET_BLOCK_HEAD_HEIGHT   = 1,
    GET_LOGS                = 2,
    FAILED                  = 3,
    COMPLETED               = 4,
};

class EvmSessionEntry
{
public:
    EvmSessionEntry(const std::vector<uint64_t>&);

    EvmSessionState state_;
    rai::uint256_union head_height_;
    std::vector<>

};

class EvmSession
{
public:
    EvmSession(const std::vector<uint64_t>&);

    EvmSessionState state_;
    std::vector<rai::uint256_union> head_heights_;
};

class EvmParser : public BaseParser
{
public:
    EvmParser(rai::Token&, const std::vector<rai::Url>&, rai::Chain, uint64_t);
    virtual ~EvmParser() = default;
    virtual void Run() override;
    virtual uint64_t Delay() const override;
    virtual void Status(rai::Ptree&) const override;


    void Receive(uint64_t, uint64_t, rai::ErrorCode, const std::string&,
                 const EvmRequestCallback&);
    rai::Token& token_;

private:
    void Send_(EvmEndpoint&, const std::string&, const rai::Ptree&,
               const EvmRequestCallback&);
    bool CheckEndpoints_();
    uint64_t MostConsecutiveErrors_() const;
    void CheckEvmChainId_(EvmEndpoint&);
    void EvmChainIdCallback_(uint64_t, uint64_t, rai::ErrorCode,
                             const rai::Ptree&);

    mutable std::mutex mutex_;
    uint64_t session_id_;

    std::vector<EvmEndpoint> endpoints_;
    rai::Chain chain_;
    rai::EvmChainId evm_chain_id_;
    uint64_t sync_interval_;
};

}  // namespace rai
#pragma once

#include <mutex>
#include <rai/common/numbers.hpp>
#include <rai/common/util.hpp>
#include <rai/common/chain.hpp>
#include <rai/rai_token/parsers/base.hpp>

namespace rai
{

class Token;

bool GetEvmChainId(rai::Chain, rai::EvmChainId&);

class EvmEndpoint
{
public:
    EvmEndpoint(uint64_t, const rai::Url&);
    bool Valid() const;

    static constexpr uint64_t BATCH_MAX = 1000;
    static constexpr uint64_t BATCH_MIN = 100;
    static constexpr uint64_t BATCH_INTERVAL = 100;
    static constexpr uint64_t CONSECUTIVE_ERRORS_MAX = 5;

    uint64_t index_;
    rai::Url url_;
    uint64_t batch_;
    uint64_t consecutive_errors_;
    bool checked_;
};

class EvmParser : public BaseParser
{
public:
    virtual ~EvmParser() = default;
    virtual void Run() override;
    virtual uint64_t Delay() const override;

    virtual rai::uint256_union EvmChainId() const = 0;
    virtual rai::Chain ChainId() const = 0;

    rai::Token& token_;

private:
    void CheckNativeChainId_(const EvmEndpoint&);


    mutable std::mutex mutex_;
    uint64_t session_id_;
    std::vector<EvmEndpoint> endpoints_;

};

}  // namespace rai
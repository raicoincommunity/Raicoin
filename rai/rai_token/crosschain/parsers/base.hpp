#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <rai/common/util.hpp>
#include <rai/rai_token/crosschain/event.hpp>

namespace rai
{

class BaseParser
{
public:
    virtual ~BaseParser() = default;
    virtual void Run() = 0;
    virtual uint64_t Delay() const = 0;
    virtual void Status(rai::Ptree&) const = 0;
    virtual rai::Chain Chain() const = 0;
    virtual std::vector<std::shared_ptr<rai::CrossChainEvent>> Events(
        const std::function<bool(const rai::CrossChainEvent&)>&) const = 0;

    static constexpr uint64_t MIN_DELAY = 5;


};
}  // namespace rai
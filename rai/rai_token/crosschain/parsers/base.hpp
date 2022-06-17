#pragma once

#include <cstdint>
#include <rai/common/util.hpp>

namespace rai
{

class BaseParser
{
public:
    virtual ~BaseParser() = default;
    virtual void Run() = 0;
    virtual uint64_t Delay() const = 0;
    virtual void Status(rai::Ptree&) const = 0;

    static constexpr uint64_t MIN_DELAY = 5;


};
}  // namespace rai
#pragma once

#include <cstdint>

namespace rai
{

class BaseParser
{
public:
    virtual ~BaseParser() = default;
    virtual void Run() = 0;
    virtual uint64_t Delay() const = 0;
};
}  // namespace rai
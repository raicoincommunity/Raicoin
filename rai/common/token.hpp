#pragma once

#include <cstdint>
#include <string>

namespace rai
{
enum class TokenType : uint8_t
{
    INVALID = 0,
    _20     = 1,
    _721    = 2,
    MAX
};
std::string TokenTypeToString(TokenType);
TokenType StringToTokenType(const std::string&);

}
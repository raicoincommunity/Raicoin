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

enum class TokenSource : uint8_t
{
    INVALID = 0,
    LOCAL   = 1,
    MAP     = 2,
    UNWRAP  = 3,
    MAX
};
std::string TokenSourceToString(TokenSource);
TokenSource StringToTokenSource(const std::string&);

}
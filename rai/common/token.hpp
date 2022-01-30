#pragma once

#include <cstdint>
#include <string>
#include <rai/common/numbers.hpp>
#include <rai/common/chain.hpp>

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
    INVALID     = 0,
    SEND        = 1,
    MAP         = 2,
    UNWRAP      = 3,
    SWAP        = 4,
    MINT        = 5,
    REFUND      = 6,
    MAX
};
std::string TokenSourceToString(TokenSource);
TokenSource StringToTokenSource(const std::string&);
bool IsLocalSource(rai::TokenSource);

inline rai::TokenAddress NativeAddress()
{
    return rai::TokenAddress(1);
}

std::string TokenAddressToString(rai::Chain, const rai::TokenAddress&);
bool StringToTokenAddress(rai::Chain, const std::string&, rai::TokenAddress&);

}  // namespace rai
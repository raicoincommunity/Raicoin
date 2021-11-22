#include <rai/common/token.hpp>

std::string rai::TokenTypeToString(rai::TokenType type)
{
    switch (type)
    {
        case rai::TokenType::INVALID:
        {
            return "invalid";
        }
        case rai::TokenType::_20:
        {
            return "20";
        }
        case rai::TokenType::_721:
        {
            return "721";
        }
        default:
        {
            return "unknown";
        }
    }
}

rai::TokenType rai::StringToTokenType(const std::string& str)
{
    if ("20" == str)
    {
        return rai::TokenType::_20;
    }
    else if ("721" == str)
    {
        return rai::TokenType::_721;
    }
    else
    {
        return rai::TokenType::INVALID;
    }
}

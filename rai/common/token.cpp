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

std::string rai::TokenSourceToString(rai::TokenSource source)
{
    switch (source)
    {
        case rai::TokenSource::INVALID:
        {
            return "invalid";
        }
        case rai::TokenSource::SEND:
        {
            return "send";
        }
        case rai::TokenSource::MAP:
        {
            return "map";
        }
        case rai::TokenSource::UNWRAP:
        {
            return "unwrap";
        }
        case rai::TokenSource::SWAP:
        {
            return "swap";
        }
        case rai::TokenSource::MINT:
        {
            return "mint";
        }
        default:
        {
            return "unknown";
        }
    }
}

rai::TokenSource rai::StringToTokenSource(const std::string& str)
{
    if ("send" == str)
    {
        return rai::TokenSource::SEND;
    }
    else if ("map" == str)
    {
        return rai::TokenSource::MAP;
    }
    else if ("unwrap" == str)
    {
        return rai::TokenSource::UNWRAP;
    }
    else if ("swap" == str)
    {
        return rai::TokenSource::SWAP;
    }
    else if ("mint" == str)
    {
        return rai::TokenSource::MINT;
    }
    else
    {
        return rai::TokenSource::INVALID;
    }
}

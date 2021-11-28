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
        case rai::TokenSource::LOCAL:
        {
            return "local";
        }
        case rai::TokenSource::MAP:
        {
            return "map";
        }
        case rai::TokenSource::UNWRAP:
        {
            return "unwrap";
        }
        default:
        {
            return "unknown";
        }
    }
}

rai::TokenSource rai::StringToTokenSource(const std::string& str)
{
    if ("local" == str)
    {
        return rai::TokenSource::LOCAL;
    }
    else if ("map" == str)
    {
        return rai::TokenSource::MAP;
    }
    else if ("unwrap" == str)
    {
        return rai::TokenSource::UNWRAP;
    }
    else
    {
        return rai::TokenSource::INVALID;
    }
}

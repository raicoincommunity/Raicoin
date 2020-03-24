#include <rai/common/util.hpp>
#include <iomanip>
#include <sstream>
#include <string>
#include <iostream>

bool rai::Read(rai::Stream& stream, size_t size, std::vector<uint8_t>& data)
{
    uint8_t byte;

    for (size_t i = 0; i < size; ++i)
    {
        auto num(stream.sgetn(&byte, sizeof(byte)));
        if (num != sizeof(byte))
        {
            return true;
        }
        data.push_back(byte);
    }
    return false;
}

bool rai::StreamEnd(rai::Stream& stream)
{
    uint8_t junk;
    bool error = rai::Read(stream, junk);
    return true == error;
}

std::string rai::BytesToHex(const uint8_t* data, size_t size)
{
    std::stringstream stream;

    stream << std::hex << std::uppercase << std::noshowbase
           << std::setfill('0');
    for (size_t i = 0; i < size; ++i)
    {
        stream << std::setw(2) << static_cast<int>(data[i]);
    }

    stream.flush();
    return stream.str();
}

void rai::DumpBytes(const uint8_t* data, size_t size)
{
    const size_t chars_per_line = 32;
    std::string str(rai::BytesToHex(data, size));
    for (size_t i = 0; i < str.size(); ++i)
    {
        std::cout << str[i];
        if (i % chars_per_line == chars_per_line - 1)
        {
            std::cout << std::endl;
        }
    }
    if (str.size() % chars_per_line)
    {
        std::cout << std::endl;
    }
}

void rai::StringLeftTrim(std::string& str, const std::string& trim)
{
    while (true)
    {
        if (str.empty())
        {
            break;
        }

        auto it = str.begin();
        if (!StringContain(trim, *it))
        {
            break;
        }
        str.erase(it);
    }
}

void rai::StringRightTrim(std::string& str, const std::string& trim)
{
    while (true)
    {
        if (str.empty())
        {
            break;
        }

        auto it = str.rbegin();
        if (!StringContain(trim, *it))
        {
            break;
        }
        str.erase(--it.base());
    }
}

void rai::StringTrim(std::string& str, const std::string& trim)
{
    rai::StringLeftTrim(str, trim);
    rai::StringRightTrim(str, trim);
}

rai::Url::Url() : Url("http")
{
}

rai::Url::Url(const std::string& protocol) : protocol_(protocol), port_(0)
{
}

bool rai::Url::Parse(const std::string& url)
{
    std::string str(url);
    rai::StringLeftTrim(str, " \t\r\n");
    rai::StringRightTrim(str, " \t\r\n");
    if (str.find("://") != std::string::npos)
    {
        std::string prefix = protocol_ + "://";
        if (str.find(prefix) != 0)
        {
            return true;
        }
        str = str.substr(prefix.size());
    }

    size_t path_begin = str.find("/");
    path_ = path_begin == std::string::npos ? "/" : str.substr(path_begin);

    size_t port_begin = str.find(":");
    if (port_begin == std::string::npos)
    {
        port_ = DefaultPort();
    }
    else
    {
        if (port_begin + 1 >= str.size())
        {
            return true;
        }
        std::string port_str;
        if (path_begin == std::string::npos)
        {
            port_str = str.substr(port_begin + 1);
        }
        else
        {
            port_str = str.substr(port_begin + 1, path_begin - port_begin - 1);
        }
        bool error = rai::StringToUint(port_str, port_);
        IF_ERROR_RETURN(error, true);
    }
    
    if (port_begin != std::string::npos)
    {
        host_ = str.substr(0, port_begin);
    }
    else if (path_begin != std::string::npos)
    {
        host_ = str.substr(0, path_begin);
    }
    else
    {
        host_ = str;
    }

    if (host_.empty() || path_.empty())
    {
        return true;
    }

    return false;
}

std::string rai::Url::String() const
{
    std::string url;

    if (host_.empty() || port_ == 0 || path_.empty())
    {
        return url;
    }

    url += protocol_;
    url += "://";
    url += host_;
    if (port_ != DefaultPort())
    {
        url += ":";
        url += std::to_string(port_);
    }
    if (path_ != "/")
    {
        url += path_;
    }

    return url;
}

uint16_t rai::Url::DefaultPort() const
{
    if (protocol_ == "http" || protocol_ == "ws")
    {
        return 80;
    }
    else if (protocol_ == "https" || protocol_ == "wss")
    {
        return 443;
    }
    else
    {
        return 0;
    }
}

rai::Url::operator bool() const
{
    return !String().empty();
}

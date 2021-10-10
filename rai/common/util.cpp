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

void rai::Write(rai::Stream& stream, size_t size,
                const std::vector<uint8_t>& data)
{
    if (data.size() < size)
    {
        size = data.size();
    }

    for (size_t i = 0; i < size; ++i)
    {
        rai::Write(stream, data[i]);
    }
}

bool rai::StreamEnd(rai::Stream& stream)
{
    uint8_t junk;
    bool error = rai::Read(stream, junk);
    return true == error;
}

std::string rai::BytesToHex(const uint8_t* data, size_t size)
{
    if (size == 0)
    {
        return "";
    }

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

bool rai::HexToBytes(const std::string& hex, std::vector<uint8_t>& bytes)
{
    if (hex.size() % 2)
    {
        return true;
    }

    if (hex.find_first_not_of("0123456789ABCDEFabcdef") != std::string::npos)
    {
        return true;
    }

    for (size_t i = 0; i < hex.size(); i += 2)
    {
        uint32_t u32;
        std::stringstream stream(hex.substr(i, 2));
        stream << std::hex << std::noshowbase;
        try
        {
            stream >> u32;
        }
        catch (...)
        {
            return true;
        }
        bytes.push_back(static_cast<uint8_t>(u32));
    }

    return false;
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

void rai::Url::AddQuery(const std::string& key, const std::string& value)
{
    if (path_.find(key) != std::string::npos)
    {
        return;
    }

    if (path_.find("?") == std::string::npos)
    {
        path_ += "?" + key + "=" + value;
    }
    else
    {
        path_ += "&"+ key + "=" + value;
    }
}

bool rai::Url::Parse(const std::string& url)
{
    std::string str(url);
    rai::StringLeftTrim(str, " \t\r\n");
    rai::StringRightTrim(str, " \t\r\n");
    if (str.find("://") != std::string::npos)
    {
        protocol_ = str.substr(0, str.find("://"));
        if (CheckProtocol())
        {
            return true;
        }
        std::string prefix = protocol_ + "://";
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

bool rai::Url::CheckProtocol() const
{
    if (protocol_ != "http" && protocol_ != "https" && protocol_ != "ws"
        && protocol_ != "wss")
    {
        return true;
    }

    return false;
}

bool rai::Url::Ssl() const
{
    return protocol_ == "https" || protocol_ == "wss";
}

bool rai::CheckUtf8(const std::vector<uint8_t>& bytes, bool& ctrl)
{
    ctrl = false;

    uint8_t byte = 0;
    uint32_t units = 0;
    uint32_t code_point = 0;

    for (auto i = bytes.begin(), n = bytes.end(); i != n; ++i)
    {
        byte = *i;
        if ((byte & 0x80) == 0x00)
        {
            units = 1;
            code_point = byte & 0x7F;
        }
        else if ((byte & 0xE0) == 0xC0)
        {
            units = 2;
            code_point = byte & 0x1F;
        }
        else if ((byte & 0xF0) == 0xE0)
        {
            units = 3;
            code_point = byte & 0x0F;
        }
        else if ((byte & 0xF8) == 0xF0)
        {
            units = 4;
            code_point = byte & 0x07;
        }
        else
        {
            return true;
        }
        
        for (uint32_t j = 1; j < units; ++j)
        {
            if (++i == n) return true;
            byte = *i;
            if ((byte & 0xC0) != 0x80) return true;
            code_point = (code_point << 6) | (byte & 0x3F);
        }

        if ((code_point > 0x10FFFF)
            || (code_point >= 0xD800 && code_point <= 0xDFFF)
            || (code_point <= 0x007F && units != 1)
            || (code_point >= 0x0080 && code_point <= 0x07FF && units != 2)
            || (code_point >= 0x0800 && code_point <= 0xFFFF && units != 3)
            || (code_point >= 0x10000 && units != 4))
        {
            return true;
        }

        if (code_point < 0x20 || (code_point >= 0x7F && code_point <= 0x9F))
        {
            ctrl = true;
        }
    }

    return false;
}

bool rai::CheckUtf8(const std::string& str, bool& ctrl)
{
    std::vector<uint8_t> bytes(str.begin(), str.end());
    return rai::CheckUtf8(bytes, ctrl);
}

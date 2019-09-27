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

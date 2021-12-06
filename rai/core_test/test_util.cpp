#include <rai/core_test/test_util.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <boost/property_tree/json_parser.hpp>

using std::cout;
using std::string;
using std::endl;

void TestShowHex(const unsigned char* text, size_t size)
{
    cout << std::hex << std::uppercase << std::noshowbase << std::setfill('0');
    for (size_t i = 0; i < size; ++i)
    {
        cout << std::setw(2) << static_cast<int>(text[i]);
    }
    cout << endl;
}

bool TestDecodeHex(const string& in, unsigned char out[], size_t max_out)
{
    size_t size = in.size();
    if ((size > max_out * 2) || (size % 2 != 0))
    {
        return true;
    }

    uint32_t u32;
    size_t index = 0;
    for (size_t i = 0; i < size; i += 2)
    {
        std::stringstream stream (in.substr(i, 2));
        stream << std::hex << std::noshowbase;
        try
        {
            stream >> u32;
        }
        catch (std::runtime_error &)
        {
            return true;
        }
        out[index++] = static_cast<unsigned char>(u32);
    }

    return false;
}

bool TestDecodeHex(const string& in, std::vector<uint8_t>& out)
{
    if (in.size() % 2)
    {
        return true;
    }

    for (size_t i = 0; i < in.size(); i += 2)
    {
        uint32_t u32;
        std::stringstream stream (in.substr(i, 2));
        stream << std::hex << std::noshowbase;
        try
        {
            stream >> u32;
        }
        catch (std::runtime_error&)
        {
            return true;
        }
        out.push_back(static_cast<uint8_t>(u32));
    }

    return false;
}

void TestShowJson(const rai::Ptree& data, const std::string& key)
{
    rai::Ptree ptree;
    ptree.put_child(key, data);
    std::stringstream ostream;
    boost::property_tree::write_json(ostream, ptree);
    ostream.flush();
    std::cout << ostream.str() << std::endl;
}
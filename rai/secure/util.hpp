#pragma once

#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/secure/common.hpp>

namespace rai
{
boost::filesystem::path WorkingPath();

void OpenOrCreate(std::fstream&, const std::string&);
rai::ErrorCode CreateKey(const boost::filesystem::path&, const std::string&);
rai::ErrorCode DecryptKey(rai::Fan&, const boost::filesystem::path&);

// Reads a json object from the stream and if was changed, write the object back
// to the stream
template <typename T>
rai::ErrorCode FetchObject(T& object, const boost::filesystem::path& path,
                           std::fstream& stream)
{
    rai::ErrorCode error_code;
    rai::OpenOrCreate(stream, path.string());
    if (stream.fail())
    {
        return rai::ErrorCode::OPEN_OR_CREATE_FILE;
    }

    rai::Ptree ptree;
    try
    {
        boost::property_tree::read_json(stream, ptree);
    }
    catch (const std::runtime_error&)
    {
        auto pos(stream.tellg());
        if (pos != std::streampos(0))
        {
            return rai::ErrorCode::JSON_GENERIC;
        }
    }

    bool updated = false;
    error_code   = object.DeserializeJson(updated, ptree);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return error_code;
    }

    if (updated)
    {
        stream.close();
        stream.open(path.string(), std::ios_base::out | std::ios_base::trunc);
        try
        {
            boost::property_tree::write_json(stream, ptree);
        }
        catch (const std::runtime_error&)
        {
            error_code = rai::ErrorCode::WRITE_FILE;
        }
    }
    return error_code;
}
}  // namespace rai
#include <rai/common/extensions.hpp>

#include <rai/common/util.hpp>
#include <rai/common/parameters.hpp>

std::string rai::ExtensionTypeToString(rai::ExtensionType type)
{
    std::string result;

    switch (type)
    {
        case rai::ExtensionType::INVALID:
        {
            return "invalid";
        }
        case rai::ExtensionType::SUB_ACCOUNT:
        {
            return "sub_account";
        }
        case rai::ExtensionType::NOTE:
        {
            return "note";
        }
        case rai::ExtensionType::ALIAS:
        {
            return "alias";
        }
        default:
        {
            return std::to_string(static_cast<uint16_t>(type));
        }
    }
}

rai::ExtensionType rai::StringToExtensionType(const std::string& str)
{
    uint16_t type = 0;
    bool error = rai::StringToUint(str, type);
    if (!error)
    {
        return static_cast<rai::ExtensionType>(type);
    }

    if ("sub_account" == str)
    {
        return rai::ExtensionType::SUB_ACCOUNT;
    }
    else if ("note" == str)
    {
        return rai::ExtensionType::NOTE;
    }
    else if ("alias" == str)
    {
        return rai::ExtensionType::ALIAS;
    }
    else
    {
        return rai::ExtensionType::INVALID;
    }
}

rai::Extension::Extension() : type_(rai::ExtensionType::INVALID)
{
}

rai::Extension::Extension(ExtensionType type) : type_(type)
{
}

rai::Extension::Extension(ExtensionType type, const std::vector<uint8_t>& value)
    : type_(type), value_(value)
{
}

bool rai::Extension::Valid() const
{
    return type_ != rai::ExtensionType::INVALID;
}

bool rai::Extension::Is(ExtensionType type) const
{
    return type_ == type;
}

size_t rai::Extension::SerialSize() const
{
    return sizeof(type_) + sizeof(uint16_t) + value_.size();
}

rai::ErrorCode rai::Extension::FromJson(const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TYPE;
        std::string type_str = ptree.get<std::string>("type");
        rai::ExtensionType type = rai::StringToExtensionType(type_str);
        IF_ERROR_RETURN(type == rai::ExtensionType::INVALID, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_VALUE;
        std::string value_str = ptree.get<std::string>("value");
        std::vector<uint8_t> value;
        bool error = rai::HexToBytes(value_str, value);
        IF_ERROR_RETURN(error, error_code);
        type_ = type;
        value_ = std::move(value);
    }
    catch (...)
    {
        return error_code;
    }

    rai::ErrorCode::SUCCESS;
}

rai::Ptree rai::Extension::Json() const
{
    rai::Ptree ptree;
    ptree.put("type", rai::ExtensionTypeToString(type_));
    ptree.put("length", std::to_string(value_.size()));
    ptree.put("value", rai::BytesToHex(value_.data(), value_.size()));
    return ptree;
}

rai::ErrorCode rai::Extension::FromExtension(const rai::Extension& extension)
{
    *this = extension;
}

rai::ErrorCode rai::Extensions::Append(const rai::Extension& extension)
{
    if (extension.type_ == rai::ExtensionType::INVALID)
    {
        return rai::ErrorCode::EXTENSION_TYPE;
    }

    if (SerialSize() + extension.SerialSize() > MAX_EXTENSIONS_SIZE)
    {
        return rai::ErrorCode::EXTENSIONS_LENGTH;
    }

    extensions_.push_back(extension);
    return rai::ErrorCode::SUCCESS;
}

size_t rai::Extensions::Count(rai::ExtensionType type) const
{
    size_t count = 0;
    for (const auto& i : extensions_)
    {
        if (i.type_ == type)
        {
            ++count;
        }
    }
    return count;
}

size_t rai::Extensions::SerialSize() const
{
    size_t size = 0;
    for (const auto& i : extensions_)
    {
        size += i.SerialSize();
    }
    return size;
}

rai::ErrorCode rai::Extensions::FromBytes(const std::vector<uint8_t>& bytes)
{
    if (bytes.empty())
    {
        return rai::ErrorCode::SUCCESS;
    }

    if (bytes.size() > MAX_EXTENSIONS_SIZE)
    {
        return rai::ErrorCode::EXTENSIONS_LENGTH;
    }

    size_t count = 0;
    std::vector<rai::Extension> extensions;
    rai::BufferStream stream(bytes.data(), bytes.size());
    while (true)
    {
        rai::ExtensionType type;
        bool error = rai::Read(stream, type);
        if (error)
        {
            if (!rai::StreamEnd(stream))
            {
                return rai::ErrorCode::EXTENSIONS_BROKEN_STREAM;
            }
            break;
        }
        if (type == rai::ExtensionType::INVALID)
        {
            return rai::ErrorCode::EXTENSION_TYPE;
        }
        count += sizeof(type);

        uint16_t length;
        error = rai::Read(stream, length);
        IF_ERROR_RETURN(error, rai::ErrorCode::EXTENSIONS_BROKEN_STREAM);
        count += sizeof(length);

        std::vector<uint8_t> value;
        error = rai::Read(stream, length, value);
        IF_ERROR_RETURN(error, rai::ErrorCode::EXTENSIONS_BROKEN_STREAM);
        count += length;
        extensions.push_back({type, value});
    }

    if (count != bytes.size())
    {
        return rai::ErrorCode::EXTENSIONS_BROKEN_STREAM;
    }

    extensions_ = std::move(extensions);
    return rai::ErrorCode::SUCCESS;
}

std::vector<uint8_t> rai::Extensions::Bytes() const
{
    std::vector<uint8_t> bytes;
    rai::VectorStream stream(bytes);
    for (const auto& i : extensions_)
    {
        rai::Write(stream, i.type_);
        uint16_t length = static_cast<uint16_t>(i.value_.size());
        rai::Write(stream, length);
        rai::Write(stream,
                   std::vector<uint8_t>(i.value_.begin(), i.value_.end()));
    }
    return bytes;
}

rai::ErrorCode rai::Extensions::FromJson(const rai::Ptree& ptree)
{
    uint32_t count = 0;
    rai::ErrorCode error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_FORMAT;
    try
    {
        for (const auto& i : ptree)
        {
            std::shared_ptr<rai::Extension> extension(nullptr);
            error_code = rai::ParseExtensionJson(i, extension);
            IF_NOT_SUCCESS_RETURN(error_code);
            error_code = Append(*extension);
            IF_NOT_SUCCESS_RETURN(error_code);
            ++count;
        }
    }
    catch (...)
    {
        return error_code;
    }

    if (count == 0)
    {
        return rai::ErrorCode::JSON_BLOCK_EXTENSIONS_EMPTY;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::Ptree rai::Extensions::Json() const
{
    rai::Ptree result;
    rai::ErrorCode error_code;

    for (const auto& i : extensions_)
    {
        rai::Ptree entry;
        std::shared_ptr<rai::Extension> extension(nullptr);
        error_code = rai::ParseExtension(i, extension);
        if (error_code == rai::ErrorCode::SUCCESS)
        {
            entry = extension->Json();
        }
        else
        {
            entry = i.Json();
        }
        entry.put("type_raw", std::to_string(static_cast<uint16_t>(i.type_)));
        entry.put("value_raw",
                  rai::BytesToHex(i.value_.data(), i.value_.size()));
        result.push_back(std::make_pair("", entry));
    }

    return result;
}

rai::Extension rai::Extensions::Get(rai::ExtensionType type) const
{
    for (const auto& i : extensions_)
    {
        if (i.type_ == type)
        {
            return i;
        }
    }

    return rai::Extension();
}

rai::ExtensionSubAccount::ExtensionSubAccount()
    : Extension{rai::ExtensionType::SUB_ACCOUNT}
{
}

rai::ExtensionSubAccount::ExtensionSubAccount(const std::string& str)
    : Extension{rai::ExtensionType::SUB_ACCOUNT,
                std::vector<uint8_t>(str.begin(), str.end())}
{
}

rai::ErrorCode rai::ExtensionSubAccount::FromJson(const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TYPE;
        std::string type_str = ptree.get<std::string>("type");
        rai::ExtensionType type = rai::StringToExtensionType(type_str);
        IF_ERROR_RETURN(type != type_, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_VALUE;
        std::string value = ptree.get<std::string>("value");
        value_ = std::vector<uint8_t>(value.begin(), value.end());
    }
    catch (...)
    {
        return error_code;
    }

    rai::ErrorCode::SUCCESS;
}

rai::Ptree rai::ExtensionSubAccount::Json() const
{
    rai::Ptree ptree;
    ptree.put("type", rai::ExtensionTypeToString(type_));
    ptree.put("length", std::to_string(value_.size()));
    ptree.put("value", std::string(reinterpret_cast<const char*>(value_.data()),
                                   value_.size()));
    return ptree;
}

rai::ErrorCode rai::ExtensionSubAccount::FromExtension(
    const rai::Extension& extension)
{
    if (extension.type_ != type_)
    {
        return rai::ErrorCode::EXTENSION_TYPE;
    }

    value_ = extension.value_;

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionNote::ExtensionNote()
    : Extension{rai::ExtensionType::NOTE}
{
}

rai::ExtensionNote::ExtensionNote(const std::string& str)
    : Extension{rai::ExtensionType::NOTE,
                std::vector<uint8_t>(str.begin(), str.end())}
{
}

rai::ErrorCode rai::ExtensionNote::FromJson(const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TYPE;
        std::string type_str = ptree.get<std::string>("type");
        rai::ExtensionType type = rai::StringToExtensionType(type_str);
        IF_ERROR_RETURN(type != type_, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_VALUE;
        std::string value = ptree.get<std::string>("value");
        value_ = std::vector<uint8_t>(value.begin(), value.end());
    }
    catch (...)
    {
        return error_code;
    }

    rai::ErrorCode::SUCCESS;

}

rai::Ptree rai::ExtensionNote::Json() const
{
    rai::Ptree ptree;
    ptree.put("type", rai::ExtensionTypeToString(type_));
    ptree.put("length", std::to_string(value_.size()));
    ptree.put("value", std::string(reinterpret_cast<const char*>(value_.data()),
                                   value_.size()));
    return ptree;
}

rai::ErrorCode rai::ExtensionNote::FromExtension(const rai::Extension& extension)
{
    if (extension.type_ != type_)
    {
        return rai::ErrorCode::EXTENSION_TYPE;
    }

    value_ = extension.value_;

    return rai::ErrorCode::SUCCESS;

}

rai::ExtensionAlias::ExtensionAlias()
    : Extension{rai::ExtensionType::ALIAS}
{
}

rai::ExtensionAlias::ExtensionAlias(rai::ExtensionAlias::Op op,
                                   const std::string& str)
    : Extension{rai::ExtensionType::ALIAS}
{
    if (str.size() > std::numeric_limits<uint8_t>::max())
    {
        type_ = rai::ExtensionType::INVALID;
        return;
    }

    if (op == rai::ExtensionAlias::Op::NAME
        || op == rai::ExtensionAlias::Op::DNS)
    {
        op_ = op;
        op_value_ = std::vector<uint8_t>(str.begin(), str.end());

        std::vector<uint8_t> bytes;
        rai::VectorStream stream(bytes);
        {
            rai::Write(stream, op_);
            uint8_t length = static_cast<uint8_t>(op_value_.size());
            rai::Write(stream, length);
            rai::Write(stream, op_value_);
        }
        value_ = std::move(bytes);
    }
    else
    {
        type_ = rai::ExtensionType::INVALID;
    }
}

rai::ErrorCode rai::ExtensionAlias::FromJson(const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TYPE;
        std::string type_str = ptree.get<std::string>("type");
        rai::ExtensionType type = rai::StringToExtensionType(type_str);
        IF_ERROR_RETURN(type != type_, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_VALUE;
        rai::Ptree value = ptree.get_child("value");
        
    }
    catch (...)
    {
        return error_code;
    }

    rai::ErrorCode::SUCCESS;

}

rai::Ptree Json() const override;
rai::ErrorCode FromExtension(const rai::Extension&) override;


rai::ErrorCode rai::ParseExtension(const rai::Extension& in,
                                   std::shared_ptr<rai::Extension>& out)
{
    switch (in.type_)
    {
        case rai::ExtensionType::SUB_ACCOUNT:
        {
            out = std::make_shared<rai::ExtensionSubAccount>();
            break;
        }
        case rai::ExtensionType::NOTE:
        {
            out = std::make_shared<rai::ExtensionNote>();
            break;
        }
        default:
        {
            if (in.type_ > rai::ExtensionType::RESERVED_MAX)
            {
                out = std::make_shared<rai::Extension>();
            }
            else
            {
                return rai::ErrorCode::EXTENSION_PARSE_UNKNOWN;
            }
        }
    }

    if (out == nullptr)
    {
        return rai::ErrorCode::UNEXPECTED;
    }

    rai::ErrorCode error_code = out->FromExtension(in);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ParseExtensionJson(const rai::Ptree& in,
                                       std::shared_ptr<rai::Extension>& out)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TYPE;
        std::string type_str = in.get<std::string>("type");
        rai::ExtensionType type = rai::StringToExtensionType(type_str);
        if (type == rai::ExtensionType::INVALID)
        {
            return error_code;
        }

        switch (type)
        {
            case rai::ExtensionType::SUB_ACCOUNT:
            {
                out = std::make_shared<rai::ExtensionSubAccount>();
                break;
            }
            case rai::ExtensionType::NOTE:
            {
                out = std::make_shared<rai::ExtensionNote>();
                break;
            }
            default:
            {
                if (type > rai::ExtensionType::RESERVED_MAX)
                {
                    out = std::make_shared<rai::Extension>();
                }
                else
                {
                    return rai::ErrorCode::EXTENSION_PARSE_UNKNOWN;
                }
            }
        }
    }
    catch (...)
    {
        return error_code;
    }

    if (out == nullptr)
    {
        return rai::ErrorCode::UNEXPECTED;
    }

    rai::ErrorCode error_code = out->FromJson(in);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::ErrorCode::SUCCESS;
}

bool rai::ExtensionAppend(const rai::Extension& extension,
                          std::vector<uint8_t>& bytes)
{
    rai::Extensions extensions;
    rai::ErrorCode error_code = extensions.FromBytes(bytes);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    error_code = extensions.Append(extension);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    bytes = extensions.Bytes();
    return false;
}


bool rai::ExtensionsToPtree(const std::vector<uint8_t>& data, rai::Ptree& ptree)
{
    if (data.empty())
    {
        return false;
    }

    rai::Extensions extensions;
    rai::ErrorCode error_code = extensions.FromBytes(data);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    ptree = extensions.Json();

    return false;
}

rai::ErrorCode rai::PtreeToExtensions(const rai::Ptree& ptree,
                                      std::vector<uint8_t>& bytes)
{
    rai::Extensions extensions;

    rai::ErrorCode error_code = extensions.FromJson(ptree);
    IF_NOT_SUCCESS_RETURN(error_code);

    bytes = extensions.Bytes();
    return rai::ErrorCode::SUCCESS;
}
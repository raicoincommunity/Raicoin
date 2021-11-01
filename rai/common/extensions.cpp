#include <rai/common/extensions.hpp>

#include <rai/common/util.hpp>
#include <rai/common/parameters.hpp>

std::string rai::ExtensionTypeToString(rai::ExtensionType type)
{
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

std::shared_ptr<rai::Extension> rai::MakeExtension(rai::ExtensionType type)
{
    switch (type)
    {
        case rai::ExtensionType::SUB_ACCOUNT:
        {
            return std::make_shared<rai::ExtensionSubAccount>();
        }
        case rai::ExtensionType::NOTE:
        {
            return std::make_shared<rai::ExtensionNote>();
        }
        case rai::ExtensionType::ALIAS:
        {
            return std::make_shared<rai::ExtensionAlias>();
        }
        default:
        {
            if (type > rai::ExtensionType::RESERVED_MAX)
            {
                return std::make_shared<rai::Extension>();
            }
            else
            {
                return nullptr;
            }
        }
    }

    return nullptr;
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

    return rai::ErrorCode::SUCCESS;
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
    return rai::ErrorCode::SUCCESS;
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

    if (bytes.size() > rai::MAX_EXTENSIONS_SIZE)
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
            error_code = rai::ParseExtensionJson(i.second, extension);
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
        entry.put("type", rai::ExtensionTypeToString(i.type_));
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

        bool ctrl;
        bool error = rai::CheckUtf8(value_, ctrl);
        IF_ERROR_RETURN(error, rai::ErrorCode::UTF8_CHECK);
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::Ptree rai::ExtensionSubAccount::Json() const
{
    rai::Ptree ptree;
    ptree.put("type", rai::ExtensionTypeToString(type_));
    ptree.put("length", std::to_string(value_.size()));
    ptree.put("value_raw", rai::BytesToHex(value_.data(), value_.size()));

    bool ctrl;
    bool error = rai::CheckUtf8(value_, ctrl);
    if (error)
    {
        rai::ErrorCode error_code = rai::ErrorCode::UTF8_CHECK;
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
    }
    else
    {
        ptree.put("value",
                  std::string(reinterpret_cast<const char*>(value_.data()),
                              value_.size()));
    }

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

        bool ctrl;
        bool error = rai::CheckUtf8(value_, ctrl);
        IF_ERROR_RETURN(error, rai::ErrorCode::UTF8_CHECK);
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::Ptree rai::ExtensionNote::Json() const
{
    rai::Ptree ptree;
    ptree.put("type", rai::ExtensionTypeToString(type_));
    ptree.put("length", std::to_string(value_.size()));
    ptree.put("value_raw", rai::BytesToHex(value_.data(), value_.size()));

    bool ctrl;
    bool error = rai::CheckUtf8(value_, ctrl);
    if (error)
    {
        rai::ErrorCode error_code = rai::ErrorCode::UTF8_CHECK;
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
    }
    else
    {
        ptree.put("value",
                  std::string(reinterpret_cast<const char*>(value_.data()),
                              value_.size()));
    }

    return ptree;
}

rai::ErrorCode rai::ExtensionNote::FromExtension(
    const rai::Extension& extension)
{
    if (extension.type_ != type_)
    {
        return rai::ErrorCode::EXTENSION_TYPE;
    }

    value_ = extension.value_;

    return rai::ErrorCode::SUCCESS;
}

std::string rai::ExtensionAlias::OpToString(rai::ExtensionAlias::Op op)
{
    using Op  = rai::ExtensionAlias::Op;
    switch (op)
    {
        case Op::INVALID:
        {
            return "invalid";
        }
        case Op::NAME:
        {
            return "name";
        }
        case Op::DNS:
        {
            return "dns";
        }
        default:
        {
            return std::to_string(static_cast<uint8_t>(op));
        }
    }
}

rai::ExtensionAlias::Op rai::ExtensionAlias::StringToOp(const std::string& str)
{
    using Op = rai::ExtensionAlias::Op;
    uint8_t type = 0;
    bool error = rai::StringToUint(str, type);
    if (!error)
    {
        return static_cast<Op>(type);
    }

    if ("name" == str)
    {
        return Op::NAME;
    }
    else if ("dns" == str)
    {
        return Op::DNS;
    }
    else
    {
        return Op::INVALID;
    }
}

rai::ExtensionAlias::ExtensionAlias()
    : Extension{rai::ExtensionType::ALIAS}
{
}

rai::ExtensionAlias::ExtensionAlias(rai::ExtensionAlias::Op op,
                                   const std::string& str)
    : Extension{rai::ExtensionType::ALIAS}
{
    op_ = op;
    op_value_ = std::vector<uint8_t>(str.begin(), str.end());
    UpdateExtensionValue();
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

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_ALIAS_OP;
        std::string op = value.get<std::string>("op");
        op_ = rai::ExtensionAlias::StringToOp(op);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_ALIAS_OP_VALUE;
        std::string op_value = value.get<std::string>("op_value");
        op_value_ = std::vector<uint8_t>(op_value.begin(), op_value.end());

        error_code = CheckData();
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateExtensionValue();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;

}

rai::Ptree rai::ExtensionAlias::Json() const
{
    rai::Ptree ptree;
    ptree.put("type", rai::ExtensionTypeToString(type_));
    ptree.put("length", std::to_string(value_.size()));
    ptree.put("value_raw", rai::BytesToHex(value_.data(), value_.size()));

    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
    }
    else
    {
        rai::Ptree value;
        value.put("op", rai::ExtensionAlias::OpToString(op_));
        value.put("op_value",
                  std::string(reinterpret_cast<const char*>(op_value_.data()),
                              op_value_.size()));
        ptree.put_child("value", value);
    }

    return ptree;
}

rai::ErrorCode rai::ExtensionAlias::FromExtension(
    const rai::Extension& extension)
{
    if (extension.type_ != type_)
    {
        return rai::ErrorCode::EXTENSION_TYPE;
    }

    value_ = extension.value_;

    op_ = rai::ExtensionAlias::Op::INVALID;
    op_value_ = std::vector<uint8_t>();

    if (value_.size() > 0)
    {
        op_ = static_cast<rai::ExtensionAlias::Op>(value_[0]);
    }

    if (value_.size() > 1)
    {
        auto begin = value_.begin();
        ++begin;
        op_value_ = std::vector<uint8_t>(begin, value_.end());
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::ExtensionAlias::UpdateExtensionValue()
{
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, op_);
        rai::Write(stream, op_value_);
    }
    value_ = std::move(bytes);
}

rai::ErrorCode rai::ExtensionAlias::CheckData() const
{
    if (op_ == Op::INVALID)
    {
        return rai::ErrorCode::ALIAS_OP_INVALID;
    }
    else if (op_ == Op::NAME)
    {
        bool ctrl;
        bool error = rai::CheckUtf8(op_value_, ctrl);
        IF_ERROR_RETURN(error, rai::ErrorCode::UTF8_CHECK);
        IF_ERROR_RETURN(ctrl, rai::ErrorCode::UTF8_CONTROL_CHARACTER);

        if (rai::Contain(op_value_, static_cast<uint8_t>('@')))
        {
            return rai::ErrorCode::ALIAS_RESERVED_CHARACTOR_AT;
        }

    }
    else if (op_ == Op::DNS)
    {
        bool ctrl;
        bool error = rai::CheckUtf8(op_value_, ctrl);
        IF_ERROR_RETURN(error, rai::ErrorCode::UTF8_CHECK);
        IF_ERROR_RETURN(ctrl, rai::ErrorCode::UTF8_CONTROL_CHARACTER);

        if (rai::Contain(op_value_, static_cast<uint8_t>('@')))
        {
            return rai::ErrorCode::ALIAS_RESERVED_CHARACTOR_AT;
        }

        if (rai::Contain(op_value_, static_cast<uint8_t>('_')))
        {
            return rai::ErrorCode::ALIAS_RESERVED_CHARACTOR_UNDERSCORE;
        }
    }
    else
    {
        return rai::ErrorCode::ALIAS_OP_UNKNOWN;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ParseExtension(const rai::Extension& in,
                                   std::shared_ptr<rai::Extension>& out)
{
    out = rai::MakeExtension(in.type_);
    if (out == nullptr)
    {
        return rai::ErrorCode::EXTENSION_PARSE_UNKNOWN;
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

        out = rai::MakeExtension(type);
        if (out == nullptr)
        {
            return rai::ErrorCode::EXTENSION_PARSE_UNKNOWN;
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

    error_code = out->FromJson(in);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
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
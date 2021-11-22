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
        case rai::ExtensionType::TOKEN:
        {
            return "token";
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
    else if ("token" == str)
    {
        return rai::ExtensionType::TOKEN;
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
        case rai::ExtensionType::TOKEN:
        {
            return std::make_shared<rai::ExtensionToken>();
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
    ptree.put("value_raw", rai::BytesToHex(value_.data(), value_.size()));
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

std::string rai::ExtensionToken::OpToString(rai::ExtensionToken::Op op)
{
    using Op  = rai::ExtensionToken::Op;
    switch (op)
    {
        case Op::INVALID:
        {
            return "invalid";
        }
        case Op::CREATE:
        {
            return "create";
        }
        case Op::MINT:
        {
            return "mint";
        }
        case Op::BURN:
        {
            return "burn";
        }
        case Op::SEND:
        {
            return "send";
        }
        case Op::RECEIVE:
        {
            return "receive";
        }
        case Op::SWAP:
        {
            return "swap";
        }
        case Op::UNMAP:
        {
            return "unmap";
        }
        case Op::WRAP:
        {
            return "wrap";
        }
        default:
        {
            return std::to_string(static_cast<uint8_t>(op));
        }
    }
}

rai::ExtensionToken::Op rai::ExtensionToken::StringToOp(const std::string& str)
{
    using Op = rai::ExtensionToken::Op;
    uint8_t type = 0;
    bool error = rai::StringToUint(str, type);
    if (!error)
    {
        return static_cast<Op>(type);
    }

    if ("create" == str)
    {
        return Op::CREATE;
    }
    else if ("mint" == str)
    {
        return Op::MINT;
    }
    else if ("burn" == str)
    {
        return Op::BURN;
    }
    else if ("send" == str)
    {
        return Op::SEND;
    }
    else if ("receive" == str)
    {
        return Op::RECEIVE;
    }
    else if ("swap" == str)
    {
        return Op::SWAP;
    }
    else if ("unmap" == str)
    {
        return Op::UNMAP;
    }
    else if ("wrap" == str)
    {
        return Op::WRAP;
    }
    else
    {
        return Op::INVALID;
    }
}

rai::ExtensionToken::ExtensionToken()
    : Extension{rai::ExtensionType::TOKEN},
      op_(rai::ExtensionToken::Op::INVALID)
{
}

rai::ErrorCode rai::ExtensionToken::FromJson(const rai::Ptree& ptree)
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

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_OP;
        std::string op = value.get<std::string>("op");
        op_ = rai::ExtensionToken::StringToOp(op);
        if (op_ == Op::INVALID)
        {
            return rai::ErrorCode::TOKEN_OP_INVALID;
        }
        if (op_ >= Op::MAX)
        {
            return rai::ErrorCode::TOKEN_OP_UNKNOWN;
        }

        op_data_ = rai::ExtensionToken::MakeData(op_);
        if (op_data_ == nullptr)
        {
            return rai::ErrorCode::UNEXPECTED;
        }

        error_code = op_data_->DeserializeJson(value);
        IF_NOT_SUCCESS_RETURN(error_code);

        UpdateExtensionValue();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::Ptree rai::ExtensionToken::Json() const
{
    rai::Ptree ptree;
    ptree.put("type", rai::ExtensionTypeToString(type_));
    ptree.put("length", std::to_string(value_.size()));
    ptree.put("value_raw", rai::BytesToHex(value_.data(), value_.size()));

    rai::Ptree value;
    value.put("op", rai::ExtensionToken::OpToString(op_));
    if (op_data_)
    {
        op_data_->DeserializeJson(value);
    }
    ptree.put_child("value", value);

    return ptree;
}

rai::ErrorCode rai::ExtensionToken::FromExtension(
    const rai::Extension& extension)
{
    if (extension.type_ != type_)
    {
        return rai::ErrorCode::EXTENSION_TYPE;
    }

    value_ = extension.value_;

    using Op = rai::ExtensionToken::Op;
    op_ = Op::INVALID;
    op_data_ = nullptr;

    if (value_.size() == 0)
    {
        return rai::ErrorCode::EXTENSION_VALUE;
    }
    op_ = static_cast<Op>(value_[0]);

    if (op_ == Op::INVALID)
    {
        return rai::ErrorCode::TOKEN_OP_INVALID;
    }

    if (op_ >= Op::MAX)
    {
        return rai::ErrorCode::TOKEN_OP_UNKNOWN;
    }

    op_data_ = rai::ExtensionToken::MakeData(op_);
    if (op_data_ == nullptr)
    {
        return rai::ErrorCode::UNEXPECTED;
    }

    auto begin = value_.begin();
    ++begin;
    auto bytes = std::vector<uint8_t>(begin, value_.end());
    rai::BufferStream stream(bytes.data(), bytes.size());
    rai::ErrorCode error_code = op_data_->Deserialize(stream);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (!rai::StreamEnd(stream))
    {
        return rai::ErrorCode::STREAM;
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::ExtensionToken::UpdateExtensionValue()
{
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, op_);
        if (op_data_)
        {
            op_data_->Serialize(stream);
        }
    }
    value_ = std::move(bytes);
}

std::shared_ptr<rai::ExtensionToken::Data> rai::ExtensionToken::MakeData(
    rai::ExtensionToken::Op op)
{
    switch (op)
    {
        case Op::CREATE:
        {
            return std::make_shared<rai::ExtensionTokenCreate>();
        }
        //todo:
        default:
        {
            return nullptr;
        }
    }
}

rai::ExtensionTokenCreate::ExtensionTokenCreate()
    : type_(rai::TokenType::INVALID)
{
}

void rai::ExtensionTokenCreate::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, type_);
    if (creation_data_)
    {
        creation_data_->Serialize(stream);
    }
}

rai::ErrorCode rai::ExtensionTokenCreate::Deserialize(rai::Stream& stream)
{
    bool error = rai::Read(stream, type_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    if (type_ == rai::TokenType::INVALID)
    {
        return rai::ErrorCode::TOKEN_TYPE_INVALID;
    }

    if (type_ >= rai::TokenType::MAX)
    {
        return rai::ErrorCode::TOKEN_TYPE_UNKNOWN;
    }

    creation_data_ = rai::ExtensionTokenCreate::MakeData(type_);
    if (creation_data_ == nullptr)
    {
        return rai::ErrorCode::UNEXPECTED;
    }

    return creation_data_->Deserialize(stream);
}

void rai::ExtensionTokenCreate::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("type", rai::TokenTypeToString(type_));
    if (creation_data_)
    {
        creation_data_->SerializeJson(ptree);
    }
}

rai::ErrorCode rai::ExtensionTokenCreate::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TYPE;
        std::string type = ptree.get<std::string>("type");
        type_ = rai::StringToTokenType(type);
        IF_ERROR_RETURN(type_ == rai::TokenType::INVALID, error_code);

        creation_data_ = rai::ExtensionTokenCreate::MakeData(type_);
        if (creation_data_ == nullptr)
        {
            return rai::ErrorCode::UNEXPECTED;
        }

        return creation_data_->DeserializeJson(ptree);
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

std::shared_ptr<rai::ExtensionTokenCreate::Data>
rai::ExtensionTokenCreate::MakeData(rai::TokenType type)
{
    switch (type)
    {
        case rai::TokenType::_20:
        {
            return std::make_shared<rai::ExtensionToken20Create>();
        }
        // todo:
        default:
        {
            return nullptr;
        }
    }
}

rai::ExtensionToken20Create::ExtensionToken20Create()
    : decimals_(0), burnable_(false), mintable_(false), circulable_(false)
{
}

void rai::ExtensionToken20Create::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, name_);
    rai::Write(stream, symbol_);
    rai::Write(stream, init_supply_.bytes);
    rai::Write(stream, cap_supply_.bytes);
    rai::Write(stream, decimals_);
    rai::Write(stream, burnable_);
    rai::Write(stream, mintable_);
    rai::Write(stream, circulable_);
}

rai::ErrorCode rai::ExtensionToken20Create::Deserialize(rai::Stream& stream) 
{
    bool error = rai::Read(stream, name_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, symbol_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, init_supply_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, cap_supply_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, decimals_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, burnable_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, mintable_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, circulable_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionToken20Create::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    ptree.put("name", name_);
    ptree.put("symbol", symbol_);
    ptree.put("init_supply", init_supply_.StringDec());
    ptree.put("cap_supply", cap_supply_.StringDec());
    ptree.put("decimals", std::to_string(decimals_));
    ptree.put("burnable", rai::BoolToString(burnable_));
    ptree.put("mintable", rai::BoolToString(mintable_));
    ptree.put("circulable", rai::BoolToString(circulable_));
}

rai::ErrorCode rai::ExtensionToken20Create::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_NAME;
        name_ = ptree.get<std::string>("name");
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SYMBOL;
        symbol_ = ptree.get<std::string>("symbol");

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_INIT_SUPPLY;
        std::string init_supply = ptree.get<std::string>("init_supply");
        bool error = init_supply_.DecodeDec(init_supply);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_CAP_SUPPLY;
        std::string cap_supply = ptree.get<std::string>("cap_supply");
        error = cap_supply_.DecodeDec(cap_supply);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_DECIMALS;
        std::string decimals = ptree.get<std::string>("decimals");
        error = rai::StringToUint(decimals, decimals_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_BURNABLE;
        std::string burnable = ptree.get<std::string>("burnable");
        error = rai::StringToBool(burnable, burnable_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_MINTABLE;
        std::string mintable = ptree.get<std::string>("mintable");
        error = rai::StringToBool(mintable, mintable_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_CIRCULABLE;
        std::string circulable = ptree.get<std::string>("circulable");
        error = rai::StringToBool(circulable, circulable_);
        IF_ERROR_RETURN(error, error_code);
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionToken20Create::CheckData() const
{
    bool ctrl;
    bool error = rai::CheckUtf8(name_, ctrl);
    IF_ERROR_RETURN(error, rai::ErrorCode::TOKEN_NAME_UTF8_CHECK);
    IF_ERROR_RETURN(ctrl, rai::ErrorCode::TOKEN_NAME_UTF8_CHECK);

    error = rai::CheckUtf8(symbol_, ctrl);
    IF_ERROR_RETURN(error, rai::ErrorCode::TOKEN_SYMBOL_UTF8_CHECK);
    IF_ERROR_RETURN(ctrl, rai::ErrorCode::TOKEN_SYMBOL_UTF8_CHECK);

    if (!cap_supply_.IsZero() && init_supply_ > cap_supply_)
    {
        return rai::ErrorCode::TOKEN_CAP_SUPPLY_EXCEEDED;
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
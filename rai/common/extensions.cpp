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

size_t rai::Extensions::Size() const
{
    return extensions_.size();
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
        op_data_->SerializeJson(value);
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

    if (value_.size() <= 1)
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
        case Op::MINT:
        {
            return std::make_shared<rai::ExtensionTokenMint>();
        }
        case Op::BURN:
        {
            return std::make_shared<rai::ExtensionTokenBurn>();
        }
        case Op::SEND:
        {
            return std::make_shared<rai::ExtensionTokenSend>();
        }
        case Op::RECEIVE:
        {
            return std::make_shared<rai::ExtensionTokenReceive>();
        }
        case Op::SWAP:
        {
            return std::make_shared<rai::ExtensionTokenSwap>();
        }
        case Op::UNMAP:
        {
            return std::make_shared<rai::ExtensionTokenUnmap>();
        }
        case Op::WRAP:
        {
            return std::make_shared<rai::ExtensionTokenWrap>();
        }
        default:
        {
            return nullptr;
        }
    }
}

rai::ErrorCode rai::ExtensionToken::CheckType(rai::TokenType type)
{
    if (type == rai::TokenType::INVALID)
    {
        return rai::ErrorCode::TOKEN_TYPE_INVALID;
    }

    if (type >= rai::TokenType::MAX)
    {
        return rai::ErrorCode::TOKEN_TYPE_UNKNOWN;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionToken::CheckChain(rai::Chain chain)
{
    std::string str = rai::ChainToString(chain);
    if (str == "invalid")
    {
        return rai::ErrorCode::TOKEN_CHAIN_INVALID;
    }
    else if (str == "unknown")
    {
        return rai::ErrorCode::TOKEN_CHAIN_UNKNOWN;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionToken::CheckValue(rai::TokenType type,
                                               const rai::TokenValue& value)
{
    if (type == rai::TokenType::_20 && value.IsZero())
    {
        return rai::ErrorCode::TOKEN_VALUE;
    }
    return rai::ErrorCode::SUCCESS;
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

    rai::ErrorCode error_code = rai::ExtensionToken::CheckType(type_);
    IF_NOT_SUCCESS_RETURN(error_code);

    creation_data_ = rai::ExtensionTokenCreate::MakeData(type_);
    if (creation_data_ == nullptr)
    {
        return rai::ErrorCode::UNEXPECTED;
    }

    return creation_data_->Deserialize(stream);
}

void rai::ExtensionTokenCreate::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }
    
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

rai::ErrorCode rai::ExtensionTokenCreate::CheckData() const
{
    rai::ErrorCode error_code = rai::ExtensionToken::CheckType(type_);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (creation_data_ == nullptr)
    {
        return rai::ErrorCode::TOKEN_CREATION_DATA_MISS;
    }

    return creation_data_->CheckData();
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
        case rai::TokenType::_721:
        {
            return std::make_shared<rai::ExtensionToken721Create>();
        }
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

        return CheckData();
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

rai::ExtensionToken721Create::ExtensionToken721Create()
    : burnable_(false), circulable_(false)
{
}

void rai::ExtensionToken721Create::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, name_);
    rai::Write(stream, symbol_);
    rai::Write(stream, base_uri_);
    rai::Write(stream, cap_supply_.bytes);
    rai::Write(stream, burnable_);
    rai::Write(stream, circulable_);
}

rai::ErrorCode rai::ExtensionToken721Create::Deserialize(rai::Stream& stream) 
{
    bool error = rai::Read(stream, name_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, symbol_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, base_uri_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, cap_supply_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, burnable_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, circulable_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionToken721Create::SerializeJson(rai::Ptree& ptree) const
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
    ptree.put("base_uri", base_uri_);
    ptree.put("cap_supply", cap_supply_.StringDec());
    ptree.put("burnable", rai::BoolToString(burnable_));
    ptree.put("circulable", rai::BoolToString(circulable_));
}

rai::ErrorCode rai::ExtensionToken721Create::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_NAME;
        name_ = ptree.get<std::string>("name");
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SYMBOL;
        symbol_ = ptree.get<std::string>("symbol");
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_BASE_URI;
        base_uri_ = ptree.get<std::string>("base_uri");

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_CAP_SUPPLY;
        std::string cap_supply = ptree.get<std::string>("cap_supply");
        bool error = cap_supply_.DecodeDec(cap_supply);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_BURNABLE;
        std::string burnable = ptree.get<std::string>("burnable");
        error = rai::StringToBool(burnable, burnable_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_CIRCULABLE;
        std::string circulable = ptree.get<std::string>("circulable");
        error = rai::StringToBool(circulable, circulable_);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionToken721Create::CheckData() const
{
    bool ctrl;
    bool error = rai::CheckUtf8(name_, ctrl);
    IF_ERROR_RETURN(error, rai::ErrorCode::TOKEN_NAME_UTF8_CHECK);
    IF_ERROR_RETURN(ctrl, rai::ErrorCode::TOKEN_NAME_UTF8_CHECK);

    error = rai::CheckUtf8(symbol_, ctrl);
    IF_ERROR_RETURN(error, rai::ErrorCode::TOKEN_SYMBOL_UTF8_CHECK);
    IF_ERROR_RETURN(ctrl, rai::ErrorCode::TOKEN_SYMBOL_UTF8_CHECK);

    error = rai::CheckUtf8(base_uri_, ctrl);
    IF_ERROR_RETURN(error, rai::ErrorCode::TOKEN_BASE_URI_UTF8_CHECK);
    IF_ERROR_RETURN(ctrl, rai::ErrorCode::TOKEN_BASE_URI_UTF8_CHECK);

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenMint::ExtensionTokenMint()
    : type_(rai::TokenType::INVALID)
{
}

void rai::ExtensionTokenMint::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, type_);
    rai::Write(stream, to_.bytes);
    rai::Write(stream, value_.bytes);
    if (type_ == rai::TokenType::_721)
    {
        rai::Write(stream, uri_);
    }
}

rai::ErrorCode rai::ExtensionTokenMint::Deserialize(rai::Stream& stream)
{
    bool error = rai::Read(stream, type_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, to_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, value_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    if (type_ == rai::TokenType::_721)
    {
        error = rai::Read(stream, uri_);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    }

    return CheckData();
}

void rai::ExtensionTokenMint::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    ptree.put("type", rai::TokenTypeToString(type_));
    ptree.put("to", to_.StringAccount());
    ptree.put("value", value_.StringDec());

    if (type_ == rai::TokenType::_721)
    {
        ptree.put("uri", uri_);
    }
}

rai::ErrorCode rai::ExtensionTokenMint::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TYPE;
        std::string type = ptree.get<std::string>("type");
        type_ = rai::StringToTokenType(type);
        
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TO;
        std::string to = ptree.get<std::string>("to");
        bool error = to_.DecodeAccount(to);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_VALUE;
        std::string value = ptree.get<std::string>("value");
        error = value_.DecodeDec(value);
        IF_ERROR_RETURN(error, error_code);

        if (type_ == rai::TokenType::_721)
        {
            error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_URI;
            uri_ = ptree.get<std::string>("uri");
        }

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenMint::CheckData() const
{
    rai::ErrorCode error_code = rai::ExtensionToken::CheckType(type_);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (to_.IsZero())
    {
        return rai::ErrorCode::TOKEN_MINT_TO;
    }

    error_code = rai::ExtensionToken::CheckValue(type_, value_);
    IF_NOT_SUCCESS_RETURN(error_code);

    bool ctrl;
    bool error = rai::CheckUtf8(uri_, ctrl);
    IF_ERROR_RETURN(error, rai::ErrorCode::TOKEN_URI_UTF8_CHECK);
    IF_ERROR_RETURN(ctrl, rai::ErrorCode::TOKEN_URI_UTF8_CHECK);

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenBurn::ExtensionTokenBurn()
    : type_(rai::TokenType::INVALID)
{
}

void rai::ExtensionTokenBurn::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, type_);
    rai::Write(stream, value_.bytes);
}

rai::ErrorCode rai::ExtensionTokenBurn::Deserialize(rai::Stream& stream)
{
    bool error = rai::Read(stream, type_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, value_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenBurn::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    ptree.put("type", rai::TokenTypeToString(type_));
    ptree.put("value", value_.StringDec());
}

rai::ErrorCode rai::ExtensionTokenBurn::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TYPE;
        std::string type = ptree.get<std::string>("type");
        type_ = rai::StringToTokenType(type);
        
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_VALUE;
        std::string value = ptree.get<std::string>("value");
        bool error = value_.DecodeDec(value);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenBurn::CheckData() const
{
    rai::ErrorCode error_code = rai::ExtensionToken::CheckType(type_);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code = rai::ExtensionToken::CheckValue(type_, value_);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenInfo::ExtensionTokenInfo()
    : chain_(rai::Chain::INVALID), type_(rai::TokenType::INVALID)
{
}

void rai::ExtensionTokenInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, chain_);
    rai::Write(stream, type_);
    rai::Write(stream, address_.bytes);
}

rai::ErrorCode rai::ExtensionTokenInfo::Deserialize(rai::Stream& stream)
{
    bool error = rai::Read(stream, chain_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, type_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, address_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenInfo::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("chain", rai::ChainToString(chain_));
    ptree.put("type", rai::TokenTypeToString(type_));
    ptree.put("address", rai::TokenAddressToString(chain_, address_));
    ptree.put("address_raw", address_.StringHex());
}

rai::ErrorCode rai::ExtensionTokenInfo::DeserializeJson(const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_CHAIN;
        std::string chain = ptree.get<std::string>("chain");
        chain_ = rai::StringToChain(chain);
        
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TYPE;
        std::string type = ptree.get<std::string>("type");
        type_ = rai::StringToTokenType(type);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_ADDRESS;
        auto address_raw_o = ptree.get_optional<std::string>("address_raw");
        if (address_raw_o)
        {
            bool error = address_.DecodeHex(*address_raw_o);
            IF_ERROR_RETURN(error, error_code);
        }
        else
        {
            std::string address = ptree.get<std::string>("address");
            bool error = rai::StringToTokenAddress(chain_, address, address_);
            IF_ERROR_RETURN(error, error_code);
        }

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenInfo::CheckData() const
{
    rai::ErrorCode error_code = rai::ExtensionToken::CheckChain(chain_);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code = rai::ExtensionToken::CheckType(type_);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

bool rai::ExtensionTokenInfo::Fungible() const
{
    return type_ == rai::TokenType::_20;
}

bool rai::ExtensionTokenInfo::NonFungible() const
{
    return type_ == rai::TokenType::_721;
}

bool rai::ExtensionTokenInfo::operator==(
    const rai::ExtensionTokenInfo& other) const
{
    return chain_ == other.chain_ && type_ == other.type_
           && address_ == other.address_;
}

bool rai::ExtensionTokenInfo::IsNative() const
{
    return rai::NativeAddress() == address_;
}

void rai::ExtensionTokenSend::Serialize(rai::Stream& stream) const
{
    token_.Serialize(stream);
    rai::Write(stream, to_.bytes);
    rai::Write(stream, value_.bytes);
}

rai::ErrorCode rai::ExtensionTokenSend::Deserialize(rai::Stream& stream)
{
    rai::ErrorCode error_code = token_.Deserialize(stream);
    IF_NOT_SUCCESS_RETURN(error_code);

    bool error = rai::Read(stream, to_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, value_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenSend::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    token_.SerializeJson(ptree);
    ptree.put("to", to_.StringAccount());
    ptree.put("value", value_.StringDec());
}

rai::ErrorCode rai::ExtensionTokenSend::DeserializeJson(const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = token_.DeserializeJson(ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TO;
        std::string to = ptree.get<std::string>("to");
        bool error = to_.DecodeAccount(to);
        IF_ERROR_RETURN(error, error_code);
        
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_VALUE;
        std::string value = ptree.get<std::string>("value");
        error = value_.DecodeDec(value);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenSend::CheckData() const
{
    rai::ErrorCode error_code = token_.CheckData();
    IF_NOT_SUCCESS_RETURN(error_code);

    if (to_.IsZero())
    {
        return rai::ErrorCode::TOKEN_SEND_TO;
    }

    error_code = rai::ExtensionToken::CheckValue(token_.type_, value_);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenReceive::ExtensionTokenReceive()
    : source_(rai::TokenSource::INVALID),
      block_height_(std::numeric_limits<uint64_t>::max()),
      unwrap_chain_(rai::Chain::INVALID)
{
}

void rai::ExtensionTokenReceive::Serialize(rai::Stream& stream) const
{
    token_.Serialize(stream);
    rai::Write(stream, source_);
    rai::Write(stream, from_.bytes);
    rai::Write(stream, block_height_);
    rai::Write(stream, tx_hash_.bytes);
    rai::Write(stream, value_.bytes);
    if (source_ == rai::TokenSource::UNWRAP)
    {
        rai::Write(stream, unwrap_chain_);
    }
}

rai::ErrorCode rai::ExtensionTokenReceive::Deserialize(rai::Stream& stream)
{
    rai::ErrorCode error_code = token_.Deserialize(stream);
    IF_NOT_SUCCESS_RETURN(error_code);

    bool error = rai::Read(stream, source_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, from_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, block_height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, tx_hash_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, value_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    if (source_ == rai::TokenSource::UNWRAP)
    {
        error = rai::Read(stream, unwrap_chain_);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    }

    return CheckData();
}

void rai::ExtensionTokenReceive::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    token_.SerializeJson(ptree);
    ptree.put("source", rai::TokenSourceToString(source_));
    ptree.put("from", rai::TokenAddressToString(token_.chain_, from_));
    ptree.put("from_raw", from_.StringHex());
    ptree.put("block_height", std::to_string(block_height_));
    ptree.put("tx_hash", tx_hash_.StringHex());
    ptree.put("value", value_.StringDec());
    if (source_ == rai::TokenSource::UNWRAP)
    {
        ptree.put("unwrap_chain", rai::ChainToString(unwrap_chain_));
    }
}

rai::ErrorCode rai::ExtensionTokenReceive::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = token_.DeserializeJson(ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SOURCE;
        std::string source = ptree.get<std::string>("source");
        source_ = rai::StringToTokenSource(source);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_FROM;
        auto from_raw_o = ptree.get_optional<std::string>("from_raw");
        if (from_raw_o)
        {
            bool error = from_.DecodeHex(*from_raw_o);
            IF_ERROR_RETURN(error, error_code);
        }
        else
        {
            std::string from = ptree.get<std::string>("from");
            bool error = rai::StringToTokenAddress(token_.chain_, from, from_);
            IF_ERROR_RETURN(error, error_code);
        }

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_BLOCK_HEIGHT;
        std::string height = ptree.get<std::string>("block_height");
        bool error = rai::StringToUint(height, block_height_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TX_HASH;
        std::string hash = ptree.get<std::string>("tx_hash");
        error = tx_hash_.DecodeHex(hash);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_VALUE;
        std::string value = ptree.get<std::string>("value");
        error = value_.DecodeDec(value);
        IF_ERROR_RETURN(error, error_code);

        if (source_ == rai::TokenSource::UNWRAP)
        {
            error_code =
                rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_UNWRAP_CHAIN;
            std::string unwrap_chain = ptree.get<std::string>("unwrap_chain");
            unwrap_chain_ = rai::StringToChain(unwrap_chain);
        }

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenReceive::CheckData() const
{
    rai::ErrorCode error_code = token_.CheckData();
    IF_NOT_SUCCESS_RETURN(error_code);

    if (source_ == rai::TokenSource::INVALID)
    {
        return rai::ErrorCode::TOKEN_SOURCE_INVALID;
    }

    if (source_ >= rai::TokenSource::MAX)
    {
        return rai::ErrorCode::TOKEN_SOURCE_UNKNOWN;
    }

    if (block_height_ == std::numeric_limits<uint64_t>::max())
    {
        return rai::ErrorCode::TOKEN_BLOCK_HEIGHT;
    }

    error_code = rai::ExtensionToken::CheckValue(token_.type_, value_);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (source_ == rai::TokenSource::MAP)
    {
        if (rai::IsRaicoin(token_.chain_))
        {
            if (!token_.IsNative())
            {
                return rai::ErrorCode::TOKEN_SOURCE_INVALID;
            }
        }
    }

    if (source_ == rai::TokenSource::UNWRAP)
    {
        error_code = rai::ExtensionToken::CheckChain(unwrap_chain_);
        IF_NOT_SUCCESS_RETURN(error_code);

        if (token_.chain_ == unwrap_chain_)
        {
            return rai::ErrorCode::TOKEN_UNWRAP_CHAIN;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenSwap::ExtensionTokenSwap()
    : sub_op_(rai::ExtensionTokenSwap::SubOp::INVALID), sub_data_(nullptr)
{
}

void rai::ExtensionTokenSwap::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, sub_op_);
    if (sub_data_)
    {
        sub_data_->Serialize(stream);
    }
}

rai::ErrorCode rai::ExtensionTokenSwap::Deserialize(rai::Stream& stream)
{
    bool error = rai::Read(stream, sub_op_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    rai::ErrorCode error_code = rai::ExtensionTokenSwap::CheckSubOp(sub_op_);
    IF_NOT_SUCCESS_RETURN(error_code);

    sub_data_ = rai::ExtensionTokenSwap::MakeSubData(sub_op_);
    if (sub_data_ == nullptr)
    {
        return rai::ErrorCode::UNEXPECTED;
    }

    return sub_data_->Deserialize(stream);
}

void rai::ExtensionTokenSwap::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    ptree.put("sub_op", rai::ExtensionTokenSwap::SubOpToString(sub_op_));
    if (sub_data_)
    {
        sub_data_->SerializeJson(ptree);
    }
}

rai::ErrorCode rai::ExtensionTokenSwap::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_SUBOP;
        std::string sub_op = ptree.get<std::string>("sub_op");
        sub_op_ = rai::ExtensionTokenSwap::StringToSubOp(sub_op);
        error_code = rai::ExtensionTokenSwap::CheckSubOp(sub_op_);
        IF_NOT_SUCCESS_RETURN(error_code);

        sub_data_ = rai::ExtensionTokenSwap::MakeSubData(sub_op_);
        if (sub_data_ == nullptr)
        {
            return rai::ErrorCode::UNEXPECTED;
        }

        return sub_data_->DeserializeJson(ptree);
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenSwap::CheckData() const
{
    rai::ErrorCode error_code = rai::ExtensionTokenSwap::CheckSubOp(sub_op_);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (sub_data_ == nullptr)
    {
        return rai::ErrorCode::TOKEN_SWAP_SUB_DATA_MISS;
    }

    return sub_data_->CheckData();
}

std::string rai::ExtensionTokenSwap::SubOpToString(
    rai::ExtensionTokenSwap::SubOp sub_op)
{
    using SubOp = rai::ExtensionTokenSwap::SubOp;
    switch (sub_op)
    {
        case SubOp::INVALID:
        {
            return "invalid";
        }
        case SubOp::CONFIG:
        {
            return "config";
        }
        case SubOp::MAKE:
        {
            return "make";
        }
        case SubOp::INQUIRY:
        {
            return "inquiry";
        }
        case SubOp::INQUIRY_ACK:
        {
            return "inquiry_ack";
        }
        case SubOp::TAKE:
        {
            return "take";
        }
        case SubOp::TAKE_ACK:
        {
            return "take_ack";
        }
        case SubOp::TAKE_NACK:
        {
            return "take_nack";
        }
        case SubOp::CANCEL:
        {
            return "cancel";
        }
        case SubOp::PING:
        {
            return "ping";
        }
        case SubOp::PONG:
        {
            return "pong";
        }
        default:
        {
            return "unknown";
        }
    }
}

rai::ExtensionTokenSwap::SubOp rai::ExtensionTokenSwap::StringToSubOp(
    const std::string& str)
{
    using SubOp = rai::ExtensionTokenSwap::SubOp;
    if ("config" == str)
    {
        return SubOp::CONFIG;
    }
    else if ("make" == str)
    {
        return SubOp::MAKE;
    }
    else if ("inquiry" == str)
    {
        return SubOp::INQUIRY;
    }
    else if ("inquiry_ack" == str)
    {
        return SubOp::INQUIRY_ACK;
    }
    else if ("take" == str)
    {
        return SubOp::TAKE;
    }
    else if ("take_ack" == str)
    {
        return SubOp::TAKE_ACK;
    }
    else if ("take_nack" == str)
    {
        return SubOp::TAKE_NACK;
    }
    else if ("cancel" == str)
    {
        return SubOp::CANCEL;
    }
    else if ("ping" == str)
    {
        return SubOp::PING;
    }
    else if ("pong" == str)
    {
        return SubOp::PONG;
    }
    else
    {
        return SubOp::INVALID;
    }
}

rai::ErrorCode rai::ExtensionTokenSwap::CheckSubOp(
    rai::ExtensionTokenSwap::SubOp sub_op)
{
    using SubOp = rai::ExtensionTokenSwap::SubOp;
    if (sub_op == SubOp::INVALID)
    {
        return rai::ErrorCode::TOKEN_SWAP_SUB_OP_INVALID;
    }

    if (sub_op >= SubOp::MAX)
    {
        return rai::ErrorCode::TOKEN_SWAP_SUB_OP_UNKNOWN;
    }

    return rai::ErrorCode::SUCCESS;
}

std::shared_ptr<rai::ExtensionTokenSwap::Data>
rai::ExtensionTokenSwap::MakeSubData(rai::ExtensionTokenSwap::SubOp op)
{
    using SubOp = rai::ExtensionTokenSwap::SubOp;
    switch (op)
    {
        case SubOp::CONFIG:
        {
            return std::make_shared<rai::ExtensionTokenSwapConfig>();
        }
        case SubOp::MAKE:
        {
            return std::make_shared<rai::ExtensionTokenSwapMake>();
        }
        case SubOp::INQUIRY:
        {
            return std::make_shared<rai::ExtensionTokenSwapInquiry>();
        }
        case SubOp::INQUIRY_ACK:
        {
            return std::make_shared<rai::ExtensionTokenSwapInquiryAck>();
        }
        case SubOp::TAKE:
        {
            return std::make_shared<rai::ExtensionTokenSwapTake>();
        }
        case SubOp::TAKE_ACK:
        {
            return std::make_shared<rai::ExtensionTokenSwapTakeAck>();
        }
        case SubOp::TAKE_NACK:
        {
            return std::make_shared<rai::ExtensionTokenSwapTakeNack>();
        }
        case SubOp::CANCEL:
        {
            return std::make_shared<rai::ExtensionTokenSwapCancel>();
        }
        case SubOp::PING:
        {
            return std::make_shared<rai::ExtensionTokenSwapPing>();
        }
        case SubOp::PONG:
        {
            return std::make_shared<rai::ExtensionTokenSwapPong>();
        }
        default:
        {
            return nullptr;
        }
    }
}

void rai::ExtensionTokenSwapConfig::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, main_.bytes);
}

rai::ErrorCode rai::ExtensionTokenSwapConfig::Deserialize(rai::Stream& stream) 
{
    bool error = rai::Read(stream, main_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenSwapConfig::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    ptree.put("main_account", main_.StringAccount());
}


rai::ErrorCode rai::ExtensionTokenSwapConfig::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_MAIN_ACCOUNT;
        std::string main_account = ptree.get<std::string>("main_account");
        bool error = main_.DecodeAccount(main_account);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenSwapConfig::CheckData() const
{
    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenSwapMake::ExtensionTokenSwapMake() : timeout_(0)
{
}

void rai::ExtensionTokenSwapMake::Serialize(rai::Stream& stream) const
{
    token_offer_.Serialize(stream);
    token_want_.Serialize(stream);
    rai::Write(stream, value_offer_.bytes);
    rai::Write(stream, value_want_.bytes);
    if (FungiblePair())
    {
        rai::Write(stream, min_offer_.bytes);
        rai::Write(stream, max_offer_.bytes);
    }
    rai::Write(stream, timeout_);
}

rai::ErrorCode rai::ExtensionTokenSwapMake::Deserialize(rai::Stream& stream) 
{
    rai::ErrorCode error_code = token_offer_.Deserialize(stream);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code = token_want_.Deserialize(stream);
    IF_NOT_SUCCESS_RETURN(error_code);

    bool error = rai::Read(stream, value_offer_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, value_want_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    if (FungiblePair())
    {
        error = rai::Read(stream, min_offer_.bytes);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

        error = rai::Read(stream, max_offer_.bytes);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    }

    error = rai::Read(stream, timeout_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenSwapMake::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    rai::Ptree token_offer;
    token_offer_.SerializeJson(token_offer);
    ptree.put_child("token_offer", token_offer);

    rai::Ptree token_want;
    token_want_.SerializeJson(token_want);
    ptree.put_child("token_want", token_want);

    ptree.put("value_offer", value_offer_.StringDec());
    ptree.put("value_want", value_want_.StringDec());
    if (FungiblePair())
    {
        ptree.put("min_offer", min_offer_.StringDec());
        ptree.put("max_offer", max_offer_.StringDec());
    }
    ptree.put("timeout", std::to_string(timeout_));
}

rai::ErrorCode rai::ExtensionTokenSwapMake::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TOKEN_OFFER;
        rai::Ptree token_offer = ptree.get_child("token_offer");
        error_code = token_offer_.DeserializeJson(token_offer);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TOKEN_WANT;
        rai::Ptree token_want = ptree.get_child("token_want");
        error_code = token_want_.DeserializeJson(token_want);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_VALUE_OFFER;
        std::string value_offer = ptree.get<std::string>("value_offer");
        bool error = value_offer_.DecodeDec(value_offer);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_VALUE_WANT;
        std::string value_want = ptree.get<std::string>("value_want");
        error = value_want_.DecodeDec(value_want);
        IF_ERROR_RETURN(error, error_code);

        if (FungiblePair())
        {
            error_code =
                rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_MIN_OFFER;
            std::string min_offer = ptree.get<std::string>("min_offer");
            error = min_offer_.DecodeDec(min_offer);
            IF_ERROR_RETURN(error, error_code);

            error_code =
                rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_MAX_OFFER;
            std::string max_offer = ptree.get<std::string>("max_offer");
            error = max_offer_.DecodeDec(max_offer);
            IF_ERROR_RETURN(error, error_code);
        }

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TIMEOUT;
        std::string timeout = ptree.get<std::string>("timeout");
        error = rai::StringToUint(timeout, timeout_);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenSwapMake::CheckData() const
{
    rai::ErrorCode error_code = token_offer_.CheckData();
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code = token_want_.CheckData();
    IF_NOT_SUCCESS_RETURN(error_code);

    if (token_offer_ == token_want_)
    {
        if (token_offer_.Fungible())
        {
            return rai::ErrorCode::TOKEN_SWAP_PAIR_EQUAL;
        }

        if (token_offer_.NonFungible())
        {
            if (value_offer_ == value_want_)
            {
                return rai::ErrorCode::TOKEN_SWAP_PAIR_EQUAL;
            }
        }
    }

    if (token_offer_.Fungible())
    {
        if (value_offer_.IsZero())
        {
            return rai::ErrorCode::TOKEN_SWAP_VALUE_OFFER;
        }
    }

    if (token_want_.Fungible())
    {
        if (value_want_.IsZero())
        {
            return rai::ErrorCode::TOKEN_SWAP_VALUE_WANT;
        }
    }

    if (FungiblePair())
    {
        if (max_offer_.IsZero())
        {
            return rai::ErrorCode::TOKEN_SWAP_MAX_OFFER;
        }

        rai::uint512_t s(max_offer_.Number());
        if (s * value_want_.Number() % value_offer_.Number() != 0)
        {
            return rai::ErrorCode::TOKEN_SWAP_MAX_OFFER;
        }

        if (min_offer_ > max_offer_)
        {
            return rai::ErrorCode::TOKEN_SWAP_MIN_OFFER;
        }

        s = min_offer_.Number();
        if (s * value_want_.Number() % value_offer_.Number() != 0)
        {
            return rai::ErrorCode::TOKEN_SWAP_MIN_OFFER;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

bool rai::ExtensionTokenSwapMake::FungiblePair() const
{
    return token_offer_.Fungible() && token_want_.Fungible();
}

rai::ExtensionTokenSwapInquiry::ExtensionTokenSwapInquiry()
    : order_height_(0), ack_height_(0), timeout_(0)
{
}

void rai::ExtensionTokenSwapInquiry::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, maker_.bytes);
    rai::Write(stream, order_height_);
    rai::Write(stream, ack_height_);
    rai::Write(stream, timeout_);
    rai::Write(stream, value_.bytes);
    rai::Write(stream, share_.bytes);
}

rai::ErrorCode rai::ExtensionTokenSwapInquiry::Deserialize(rai::Stream& stream) 
{
    bool error = rai::Read(stream, maker_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, order_height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, ack_height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, timeout_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, value_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, share_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenSwapInquiry::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    ptree.put("maker", maker_.StringAccount());
    ptree.put("order_height", std::to_string(order_height_));
    ptree.put("ack_height", std::to_string(ack_height_));
    ptree.put("timeout", std::to_string(timeout_));
    ptree.put("value", value_.StringDec());
    ptree.put("share", share_.StringHex());
}

rai::ErrorCode rai::ExtensionTokenSwapInquiry::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_MAKER;
        std::string maker = ptree.get<std::string>("maker");
        bool error = maker_.DecodeAccount(maker);
        IF_ERROR_RETURN(error, error_code);

        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_ORDER_HEIGHT;
        std::string order_height = ptree.get<std::string>("order_height");
        error = rai::StringToUint(order_height, order_height_);
        IF_ERROR_RETURN(error, error_code);

        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_ACK_HEIGHT;
        std::string ack_height = ptree.get<std::string>("ack_height");
        error = rai::StringToUint(ack_height, ack_height_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TIMEOUT;
        std::string timeout = ptree.get<std::string>("timeout");
        error = rai::StringToUint(timeout, timeout_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_VALUE;
        std::string value = ptree.get<std::string>("value");
        error = value_.DecodeDec(value);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_SHARE;
        std::string share = ptree.get<std::string>("share");
        error = share_.DecodeHex(share);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenSwapInquiry::CheckData() const
{
    if (maker_.IsZero())
    {
        return rai::ErrorCode::TOKEN_SWAP_MAKER;
    }

    if (order_height_ == std::numeric_limits<uint64_t>::max())
    {
        return rai::ErrorCode::TOKEN_SWAP_ORDER_HEIGHT;
    }

    if (ack_height_ == std::numeric_limits<uint64_t>::max())
    {
        return rai::ErrorCode::TOKEN_SWAP_ACK_HEIGHT;
    }

    if (!share_.ValidPublicKey())
    {
        return rai::ErrorCode::TOKEN_SWAP_SHARE;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenSwapInquiryAck::ExtensionTokenSwapInquiryAck()
    : inquiry_height_(0), trade_height_(0)
{
}

void rai::ExtensionTokenSwapInquiryAck::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, taker_.bytes);
    rai::Write(stream, inquiry_height_);
    rai::Write(stream, trade_height_);
    rai::Write(stream, share_.bytes);
    rai::Write(stream, signature_.bytes);
}

rai::ErrorCode rai::ExtensionTokenSwapInquiryAck::Deserialize(
    rai::Stream& stream)
{
    bool error = rai::Read(stream, taker_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, inquiry_height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, trade_height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, share_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, signature_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenSwapInquiryAck::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    ptree.put("taker", taker_.StringAccount());
    ptree.put("inquiry_height", std::to_string(inquiry_height_));
    ptree.put("trade_height", std::to_string(trade_height_));
    ptree.put("share", share_.StringHex());
    ptree.put("signature", signature_.StringHex());
}

rai::ErrorCode rai::ExtensionTokenSwapInquiryAck::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TAKER;
        std::string taker = ptree.get<std::string>("taker");
        bool error = taker_.DecodeAccount(taker);
        IF_ERROR_RETURN(error, error_code);

        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_INQUIRY_HEIGHT;
        std::string inquiry_height = ptree.get<std::string>("inquiry_height");
        error = rai::StringToUint(inquiry_height, inquiry_height_);
        IF_ERROR_RETURN(error, error_code);

        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TRADE_HEIGHT;
        std::string trade_height = ptree.get<std::string>("trade_height");
        error = rai::StringToUint(trade_height, trade_height_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_SHARE;
        std::string share = ptree.get<std::string>("share");
        error = share_.DecodeHex(share);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_SIGNATURE;
        std::string signature = ptree.get<std::string>("signature");
        error = signature_.DecodeHex(signature);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenSwapInquiryAck::CheckData() const
{
    if (taker_.IsZero())
    {
        return rai::ErrorCode::TOKEN_SWAP_TAKER;
    }

    if (inquiry_height_ == std::numeric_limits<uint64_t>::max())
    {
        return rai::ErrorCode::TOKEN_SWAP_INQUIRY_HEIGHT;
    }

    if (trade_height_ == std::numeric_limits<uint64_t>::max()
        || trade_height_ == 0)
    {
        return rai::ErrorCode::TOKEN_SWAP_TRADE_HEIGHT;
    }

    if (!share_.ValidPublicKey())
    {
        return rai::ErrorCode::TOKEN_SWAP_SHARE;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenSwapTake::ExtensionTokenSwapTake() : inquiry_height_(0)
{
}

void rai::ExtensionTokenSwapTake::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, inquiry_height_);
}

rai::ErrorCode rai::ExtensionTokenSwapTake::Deserialize(
    rai::Stream& stream)
{
    bool error = rai::Read(stream, inquiry_height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenSwapTake::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    ptree.put("inquiry_height", std::to_string(inquiry_height_));
}


rai::ErrorCode rai::ExtensionTokenSwapTake::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_INQUIRY_HEIGHT;
        std::string inquiry_height = ptree.get<std::string>("inquiry_height");
        bool error = rai::StringToUint(inquiry_height, inquiry_height_);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenSwapTake::CheckData() const
{
    if (inquiry_height_ == std::numeric_limits<uint64_t>::max())
    {
        return rai::ErrorCode::TOKEN_SWAP_INQUIRY_HEIGHT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenSwapTakeAck::ExtensionTokenSwapTakeAck()
    : inquiry_height_(0), take_height_(0)
{
}

void rai::ExtensionTokenSwapTakeAck::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, taker_.bytes);
    rai::Write(stream, inquiry_height_);
    rai::Write(stream, take_height_);
    rai::Write(stream, value_.bytes);
}

rai::ErrorCode rai::ExtensionTokenSwapTakeAck::Deserialize(
    rai::Stream& stream)
{
    bool error = rai::Read(stream, taker_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, inquiry_height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, take_height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, value_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenSwapTakeAck::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    ptree.put("taker", taker_.StringAccount());
    ptree.put("inquiry_height", std::to_string(inquiry_height_));
    ptree.put("take_height", std::to_string(take_height_));
    ptree.put("value", value_.StringDec());
}

rai::ErrorCode rai::ExtensionTokenSwapTakeAck::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TAKER;
        std::string taker = ptree.get<std::string>("taker");
        bool error = taker_.DecodeAccount(taker);
        IF_ERROR_RETURN(error, error_code);

        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_INQUIRY_HEIGHT;
        std::string inquiry_height = ptree.get<std::string>("inquiry_height");
        error = rai::StringToUint(inquiry_height, inquiry_height_);
        IF_ERROR_RETURN(error, error_code);

        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TAKE_HEIGHT;
        std::string take_height = ptree.get<std::string>("take_height");
        error = rai::StringToUint(take_height, take_height_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_VALUE;
        std::string value = ptree.get<std::string>("value");
        error = value_.DecodeDec(value);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenSwapTakeAck::CheckData() const
{
    if (taker_.IsZero())
    {
        return rai::ErrorCode::TOKEN_SWAP_TAKER;
    }

    if (inquiry_height_ == std::numeric_limits<uint64_t>::max())
    {
        return rai::ErrorCode::TOKEN_SWAP_INQUIRY_HEIGHT;
    }

    if (take_height_ == std::numeric_limits<uint64_t>::max())
    {
        return rai::ErrorCode::TOKEN_SWAP_TAKE_HEIGHT;
    }

    if (take_height_ <= inquiry_height_)
    {
        return rai::ErrorCode::TOKEN_SWAP_TAKE_HEIGHT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenSwapTakeNack::ExtensionTokenSwapTakeNack()
    : inquiry_height_(0)
{
}

void rai::ExtensionTokenSwapTakeNack::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, taker_.bytes);
    rai::Write(stream, inquiry_height_);
}

rai::ErrorCode rai::ExtensionTokenSwapTakeNack::Deserialize(
    rai::Stream& stream)
{
    bool error = rai::Read(stream, taker_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, inquiry_height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenSwapTakeNack::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    ptree.put("taker", taker_.StringAccount());
    ptree.put("inquiry_height", std::to_string(inquiry_height_));
}

rai::ErrorCode rai::ExtensionTokenSwapTakeNack::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_TAKER;
        std::string taker = ptree.get<std::string>("taker");
        bool error = taker_.DecodeAccount(taker);
        IF_ERROR_RETURN(error, error_code);

        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_INQUIRY_HEIGHT;
        std::string inquiry_height = ptree.get<std::string>("inquiry_height");
        error = rai::StringToUint(inquiry_height, inquiry_height_);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenSwapTakeNack::CheckData() const
{
    if (taker_.IsZero())
    {
        return rai::ErrorCode::TOKEN_SWAP_TAKER;
    }

    if (inquiry_height_ == std::numeric_limits<uint64_t>::max())
    {
        return rai::ErrorCode::TOKEN_SWAP_INQUIRY_HEIGHT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenSwapCancel::ExtensionTokenSwapCancel()
    : order_height_(0)
{
}

void rai::ExtensionTokenSwapCancel::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, order_height_);
}

rai::ErrorCode rai::ExtensionTokenSwapCancel::Deserialize(
    rai::Stream& stream)
{
    bool error = rai::Read(stream, order_height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenSwapCancel::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    ptree.put("order_height", std::to_string(order_height_));
}

rai::ErrorCode rai::ExtensionTokenSwapCancel::CheckData() const
{
    if (order_height_ == std::numeric_limits<uint64_t>::max())
    {
        return rai::ErrorCode::TOKEN_SWAP_ORDER_HEIGHT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenSwapCancel::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_ORDER_HEIGHT;
        std::string order_height = ptree.get<std::string>("order_height");
        bool error = rai::StringToUint(order_height, order_height_);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenSwapPing::ExtensionTokenSwapPing() : maker_(0)
{
}

void rai::ExtensionTokenSwapPing::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, maker_.bytes);
}

rai::ErrorCode rai::ExtensionTokenSwapPing::Deserialize(rai::Stream& stream)
{
    bool error = rai::Read(stream, maker_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    return CheckData();
}

void rai::ExtensionTokenSwapPing::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    ptree.put("maker", maker_.StringAccount());
}

rai::ErrorCode rai::ExtensionTokenSwapPing::DeserializeJson(
    const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_SWAP_MAKER;
        std::string maker = ptree.get<std::string>("maker");
        bool error = maker_.DecodeAccount(maker);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenSwapPing::CheckData() const
{
    if (maker_.IsZero())
    {
        return rai::ErrorCode::TOKEN_SWAP_MAKER;
    }
    return rai::ErrorCode::SUCCESS;
}

void rai::ExtensionTokenSwapPong::Serialize(rai::Stream&) const
{
    return;
}

rai::ErrorCode rai::ExtensionTokenSwapPong::Deserialize(rai::Stream&)
{
    return rai::ErrorCode::SUCCESS;
}

void rai::ExtensionTokenSwapPong::SerializeJson(rai::Ptree&) const
{
    return;
}

rai::ErrorCode rai::ExtensionTokenSwapPong::DeserializeJson(const rai::Ptree&)
{
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenSwapPong::CheckData() const
{
    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenUnmap::ExtensionTokenUnmap() : extra_data_(0)
{
}

void rai::ExtensionTokenUnmap::Serialize(rai::Stream& stream) const
{
    token_.Serialize(stream);
    rai::Write(stream, to_.bytes);
    rai::Write(stream, value_.bytes);
    rai::Write(stream, extra_data_);
}

rai::ErrorCode rai::ExtensionTokenUnmap::Deserialize(rai::Stream& stream)
{
    rai::ErrorCode error_code = token_.Deserialize(stream);
    IF_NOT_SUCCESS_RETURN(error_code);

    bool error = rai::Read(stream, to_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, value_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, extra_data_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenUnmap::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    token_.SerializeJson(ptree);
    ptree.put("to", rai::TokenAddressToString(token_.chain_, to_));
    ptree.put("to_raw", to_.StringHex());
    ptree.put("value", value_.StringDec());
    ptree.put("extra_data", std::to_string(extra_data_));
}

rai::ErrorCode rai::ExtensionTokenUnmap::DeserializeJson(const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = token_.DeserializeJson(ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TO;
        auto to_raw_o = ptree.get_optional<std::string>("to_raw");
        if (to_raw_o)
        {
            bool error = to_.DecodeHex(*to_raw_o);
            IF_ERROR_RETURN(error, error_code);
        }
        else
        {
            std::string to = ptree.get<std::string>("to");
            bool error = rai::StringToTokenAddress(token_.chain_, to, to_);
            IF_ERROR_RETURN(error, error_code);
        }
        
        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_VALUE;
        std::string value = ptree.get<std::string>("value");
        bool error = value_.DecodeDec(value);
        IF_ERROR_RETURN(error, error_code);

        error_code =
            rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_UNMAP_EXTRA_DATA;
        std::string extra_data = ptree.get<std::string>("extra_data");
        error = rai::StringToUint(extra_data, extra_data_);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenUnmap::CheckData() const
{
    rai::ErrorCode error_code = token_.CheckData();
    IF_NOT_SUCCESS_RETURN(error_code);

    if (rai::IsRaicoin(token_.chain_))
    {
        if (!token_.IsNative())
        {
            return rai::ErrorCode::TOKEN_UNMAP_CHAIN;
        }
    }

    if (to_.IsZero())
    {
        return rai::ErrorCode::TOKEN_UNMAP_TO;
    }

    error_code = rai::ExtensionToken::CheckValue(token_.type_, value_);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::ExtensionTokenWrap::ExtensionTokenWrap() : to_chain_(rai::Chain::INVALID)
{
}

void rai::ExtensionTokenWrap::Serialize(rai::Stream& stream) const
{
    token_.Serialize(stream);
    rai::Write(stream, to_chain_);
    rai::Write(stream, to_account_.bytes);
    rai::Write(stream, value_.bytes);
}

rai::ErrorCode rai::ExtensionTokenWrap::Deserialize(rai::Stream& stream)
{
    rai::ErrorCode error_code = token_.Deserialize(stream);
    IF_NOT_SUCCESS_RETURN(error_code);

    bool error = rai::Read(stream, to_chain_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, to_account_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, value_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return CheckData();
}

void rai::ExtensionTokenWrap::SerializeJson(rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ptree.put("error", rai::ErrorString(error_code));
        ptree.put("error_code", static_cast<uint32_t>(error_code));
        return;
    }

    token_.SerializeJson(ptree);
    ptree.put("to_chain", rai::ChainToString(to_chain_));
    ptree.put("to_account", rai::TokenAddressToString(to_chain_, to_account_));
    ptree.put("to_account_raw", to_account_.StringHex());
    ptree.put("value", value_.StringDec());
}

rai::ErrorCode rai::ExtensionTokenWrap::DeserializeJson(const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::UNEXPECTED;
    try
    {
        error_code = token_.DeserializeJson(ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TO_CHAIN;
        std::string to_chain = ptree.get<std::string>("to_chain");
        to_chain_ = rai::StringToChain(to_chain);

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_TO_ACCOUNT;
        auto to_raw_o = ptree.get_optional<std::string>("to_account_raw");
        if (to_raw_o)
        {
            bool error = to_account_.DecodeHex(*to_raw_o);
            IF_ERROR_RETURN(error, error_code);
        }
        else
        {
            std::string to = ptree.get<std::string>("to_account");
            bool error = rai::StringToTokenAddress(to_chain_, to, to_account_);
            IF_ERROR_RETURN(error, error_code);
        }

        error_code = rai::ErrorCode::JSON_BLOCK_EXTENSION_TOKEN_VALUE;
        std::string value = ptree.get<std::string>("value");
        bool error = value_.DecodeDec(value);
        IF_ERROR_RETURN(error, error_code);

        return CheckData();
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::ExtensionTokenWrap::CheckData() const
{
    rai::ErrorCode error_code = token_.CheckData();
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code = rai::ExtensionToken::CheckChain(to_chain_);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (token_.chain_ == to_chain_)
    {
        return rai::ErrorCode::TOKEN_WRAP_CHAIN;
    }

    if (to_account_.IsZero())
    {
        return rai::ErrorCode::TOKEN_WRAP_TO;
    }

    error_code = rai::ExtensionToken::CheckValue(token_.type_, value_);
    IF_NOT_SUCCESS_RETURN(error_code);

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
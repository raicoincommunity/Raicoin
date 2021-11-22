#pragma once

#include <string>
#include <vector>
#include <memory>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/token.hpp>

namespace rai
{

enum class ExtensionType : uint16_t
{
    INVALID     = 0,
    SUB_ACCOUNT = 1, // UTF-8 encoding string
    NOTE        = 2, // UTF-8 encoding string
    ALIAS       = 3,
    TOKEN       = 4,

    RESERVED_MAX = 1023,
};
std::string ExtensionTypeToString(rai::ExtensionType);
rai::ExtensionType StringToExtensionType(const std::string&);

class Extension 
{
public:
    Extension();
    Extension(ExtensionType);
    Extension(ExtensionType, const std::vector<uint8_t>&);
    virtual ~Extension() = default;

    bool Valid() const;
    bool Is(ExtensionType) const;
    size_t SerialSize() const;
    virtual rai::ErrorCode FromJson(const rai::Ptree&);
    virtual rai::Ptree Json() const;
    virtual rai::ErrorCode FromExtension(const rai::Extension&);

    ExtensionType type_;
    std::vector<uint8_t> value_;
};

std::shared_ptr<rai::Extension> MakeExtension(rai::ExtensionType);

class Extensions
{
public:
    rai::ErrorCode Append(const rai::Extension&);
    size_t Count(rai::ExtensionType) const;
    size_t SerialSize() const;
    rai::ErrorCode FromBytes(const std::vector<uint8_t>&);
    std::vector<uint8_t> Bytes() const;
    rai::ErrorCode FromJson(const rai::Ptree&);
    rai::Ptree Json() const;
    rai::Extension Get(rai::ExtensionType) const;

private:
    std::vector<rai::Extension> extensions_;
};

class ExtensionSubAccount : public Extension
{
public:
    ExtensionSubAccount();
    ExtensionSubAccount(const std::string&);
    virtual ~ExtensionSubAccount() = default;
    rai::ErrorCode FromJson(const rai::Ptree&) override;
    rai::Ptree Json() const override;
    rai::ErrorCode FromExtension(const rai::Extension&) override;
};

class ExtensionNote : public Extension
{
public:
    ExtensionNote();
    ExtensionNote(const std::string&);
    virtual ~ExtensionNote() = default;
    rai::ErrorCode FromJson(const rai::Ptree&) override;
    rai::Ptree Json() const override;
    rai::ErrorCode FromExtension(const rai::Extension&) override;
};

class ExtensionAlias : public Extension
{
public:
    enum class Op : uint8_t 
    {
        INVALID         = 0,
        NAME            = 1,
        DNS             = 2,
        MAX
    };
    static std::string OpToString(Op);
    static Op StringToOp(const std::string&);

    ExtensionAlias();
    ExtensionAlias(Op, const std::string&);
    virtual ~ExtensionAlias() = default;
    rai::ErrorCode FromJson(const rai::Ptree&) override;
    rai::Ptree Json() const override;
    rai::ErrorCode FromExtension(const rai::Extension&) override;

    void UpdateExtensionValue();
    rai::ErrorCode CheckData() const;

    Op op_;
    std::vector<uint8_t> op_value_;
};

class ExtensionToken : public Extension
{
public:
    enum class Op : uint8_t 
    {
        INVALID         = 0,
        CREATE          = 1,
        MINT            = 2,
        BURN            = 3,
        SEND            = 4,
        RECEIVE         = 5,
        SWAP            = 6,
        UNMAP           = 7,
        WRAP            = 8,
        MAX
    };
    static std::string OpToString(Op);
    static Op StringToOp(const std::string&);

    class Data
    {
    public:
        virtual ~Data()                                           = default;
        virtual void Serialize(rai::Stream&) const                = 0;
        virtual rai::ErrorCode Deserialize(rai::Stream&)          = 0;
        virtual void SerializeJson(rai::Ptree&) const             = 0;
        virtual rai::ErrorCode DeserializeJson(const rai::Ptree&) = 0;
    };

    ExtensionToken();
    virtual ~ExtensionToken() = default;
    rai::ErrorCode FromJson(const rai::Ptree&) override;
    rai::Ptree Json() const override;
    rai::ErrorCode FromExtension(const rai::Extension&) override;

    void UpdateExtensionValue();
    static std::shared_ptr<Data> MakeData(Op);

    Op op_;
    std::shared_ptr<Data> op_data_;
};

class ExtensionTokenCreate : public rai::ExtensionToken::Data
{
public:
    class Data
    {
    public:
        virtual ~Data()                                           = default;
        virtual void Serialize(rai::Stream&) const                = 0;
        virtual rai::ErrorCode Deserialize(rai::Stream&)          = 0;
        virtual void SerializeJson(rai::Ptree&) const             = 0;
        virtual rai::ErrorCode DeserializeJson(const rai::Ptree&) = 0;
    };

    ExtensionTokenCreate();
    virtual ~ExtensionTokenCreate() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;

    static std::shared_ptr<Data> MakeData(rai::TokenType);
    
    rai::TokenType type_;
    std::shared_ptr<Data> creation_data_;
};

class ExtensionToken20Create : public rai::ExtensionTokenCreate::Data
{
public:
    ExtensionToken20Create();
    virtual ~ExtensionToken20Create() = default;

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;

    rai::ErrorCode CheckData() const;
    
    std::string name_;
    std::string symbol_;
    rai::TokenValue init_supply_;
    rai::TokenValue cap_supply_;
    uint8_t decimals_;
    bool burnable_;
    bool mintable_;
    bool circulable_;
};

class ExtensionToken721Create : public rai::ExtensionTokenCreate::Data
{
public:
    ExtensionToken721Create();
    virtual ~ExtensionToken721Create() = default;
    
    std::string name_;
    std::string symbol_;
    std::string base_uri_;
    rai::TokenValue cap_supply_;
    bool burnable_;
    bool circulable_;
};

rai::ErrorCode ParseExtension(const rai::Extension&,
                              std::shared_ptr<rai::Extension>&);

rai::ErrorCode ParseExtensionJson(const rai::Ptree&,
                                  std::shared_ptr<rai::Extension>&);

bool ExtensionAppend(const rai::Extension&, std::vector<uint8_t>&);
bool ExtensionsToPtree(const std::vector<uint8_t>&, rai::Ptree&);
rai::ErrorCode PtreeToExtensions(const rai::Ptree&, std::vector<uint8_t>&);

} // rai
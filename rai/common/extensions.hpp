#pragma once

#include <string>
#include <vector>

#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>

namespace rai
{

enum class ExtensionType : uint16_t
{
    INVALID     = 0,
    SUB_ACCOUNT = 1, // UTF-8 encoding string
    NOTE        = 2, // UTF-8 encoding string
    ALIAS       = 3,

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

rai::ErrorCode ParseExtension(const rai::Extension&,
                              std::shared_ptr<rai::Extension>&);

rai::ErrorCode ParseExtensionJson(const rai::Ptree&,
                                  std::shared_ptr<rai::Extension>&);

bool ExtensionAppend(const rai::Extension&, std::vector<uint8_t>&);
bool ExtensionsToPtree(const std::vector<uint8_t>&, rai::Ptree&);
rai::ErrorCode PtreeToExtensions(const rai::Ptree&, std::vector<uint8_t>&);

} // rai
#pragma once
#include <string>
#include <vector>
#include <blake2/blake2.h>
#include <boost/property_tree/json_parser.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/util.hpp>

namespace rai
{
enum class BlockType : uint8_t
{
    INVALID     = 0,
    TX_BLOCK    = 1,  // Transaction Block
    REP_BLOCK   = 2,  // Representive Block
    AD_BLOCK    = 3,  // Airdrop Block
};
std::string BlockTypeToString(rai::BlockType);

enum class BlockOpcode : uint8_t
{
    INVALID = 0,
    SEND    = 1,
    RECEIVE = 2,
    CHANGE  = 3,
    CREDIT  = 4,
    REWARD  = 5,
    DESTROY = 6,
};
std::string BlockOpcodeToString(rai::BlockOpcode);
rai::BlockOpcode StringToBlockOpcode(const std::string&);

class BlockVisitor;
class Block
{
public:
    Block();
    rai::BlockHash Hash() const;
    std::string Json() const;
    bool CheckSignature() const;
    bool operator!=(const rai::Block&) const;
    bool ForkWith(const rai::Block&) const;
    bool Limited() const;
    size_t Size() const;

    virtual ~Block()                                          = default;
    virtual bool operator==(const rai::Block&) const          = 0;
    virtual void Hash(blake2b_state&) const                   = 0;
    virtual rai::BlockHash Previous() const                   = 0;
    virtual void Serialize(rai::Stream&) const                = 0;
    virtual rai::ErrorCode Deserialize(rai::Stream&)          = 0;
    virtual void SerializeJson(rai::Ptree&) const             = 0;
    virtual rai::ErrorCode DeserializeJson(const rai::Ptree&) = 0;
    virtual void SetSignature(const rai::uint512_union&)      = 0;
    virtual rai::Signature Signature() const                  = 0;
    virtual rai::BlockType Type() const                       = 0;
    virtual rai::BlockOpcode Opcode() const                   = 0;
    virtual rai::ErrorCode Visit(rai::BlockVisitor&) const    = 0;
    virtual rai::Account Account() const                      = 0;
    virtual rai::Amount Balance() const                       = 0;
    virtual uint16_t Credit() const                           = 0;
    virtual uint32_t Counter() const                          = 0;
    virtual uint64_t Timestamp() const                        = 0;
    virtual uint64_t Height() const                           = 0;
    virtual rai::uint256_union Link() const                   = 0;
    virtual rai::Account Representative() const               = 0;
    virtual bool HasRepresentative() const                    = 0;

    static uint64_t constexpr INVALID_HEIGHT =
        std::numeric_limits<uint64_t>::max();

protected:
    bool CheckSignature_() const;

private:
    mutable bool signature_checked_;
    mutable bool signature_error_;
};

enum class ExtensionType : uint16_t
{
    INVALID     = 0,
    SUB_ACCOUNT = 1,
    NOTE        = 2,
    UNIQUE_ID   = 3,

    RESERVED_MAX = 1023,
};
std::string ExtensionTypeToString(rai::ExtensionType);
rai::ExtensionType StringToExtensionType(const std::string&);
bool ExtensionAppend(rai::ExtensionType, const std::string&,
                     std::vector<uint8_t>&);
bool ExtensionAppend(rai::ExtensionType, uint64_t,
                     std::vector<uint8_t>&);

bool ExtensionsToPtree(const std::vector<uint8_t>&, rai::Ptree&);
rai::ErrorCode PtreeToExtensions(const rai::Ptree&, std::vector<uint8_t>&);

// Transaction Block
class TxBlock : public Block
{
public:
    TxBlock(rai::BlockOpcode, uint16_t, uint32_t, uint64_t, uint64_t,
            const rai::Account&, const rai::BlockHash&, const rai::Account&,
            const rai::Amount&, const rai::uint256_union&, uint32_t,
            const std::vector<uint8_t>&, const rai::RawKey&,
            const rai::PublicKey&);
    TxBlock(rai::ErrorCode&, rai::Stream&);
    TxBlock(rai::ErrorCode&, const rai::Ptree&);
    virtual ~TxBlock() = default;

    bool operator==(const rai::Block&) const override;
    bool operator==(const rai::TxBlock&) const;
    using rai::Block::Hash;
    void Hash(blake2b_state&) const override;
    rai::BlockHash Previous() const override;
    // rai::BlockHash Root() const override;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    void SetSignature(const rai::uint512_union&) override;
    rai::Signature Signature() const override;
    rai::BlockType Type() const override;
    rai::BlockOpcode Opcode() const override;
    rai::ErrorCode Visit(rai::BlockVisitor&) const override;
    rai::Account Account() const override;
    rai::Amount Balance() const override;
    uint16_t Credit() const override;
    uint32_t Counter() const override;
    uint64_t Timestamp() const override;
    uint64_t Height() const override;
    rai::uint256_union Link() const override;
    rai::Account Representative() const override;
    bool HasRepresentative() const override;

    std::vector<uint8_t> Extensions() const;


    static bool CheckOpcode(rai::BlockOpcode);
    static bool CheckExtensionsLength(uint32_t);
    static uint32_t MaxExtensionsLength();

private:
    rai::BlockType type_;
    rai::BlockOpcode opcode_;
    uint16_t credit_;
    uint32_t counter_;
    uint64_t timestamp_;
    uint64_t height_;
    rai::Account account_;
    rai::BlockHash previous_;
    rai::Account representative_;
    rai::Amount balance_;
    rai::uint256_union link_;
    uint32_t extensions_length_;
    std::vector<uint8_t> extensions_;
    rai::Signature signature_;
};

// Representative Block
class RepBlock : public Block
{
public:
    RepBlock(rai::BlockOpcode, uint16_t, uint32_t, uint64_t, uint64_t,
             const rai::Account&, const rai::BlockHash&, const rai::Amount&,
             const rai::uint256_union&, const rai::RawKey&,
             const rai::PublicKey&);
    RepBlock(rai::ErrorCode&, rai::Stream&);
    RepBlock(rai::ErrorCode&, const rai::Ptree&);
    virtual ~RepBlock() = default;

    bool operator==(const rai::Block&) const override;
    bool operator==(const rai::RepBlock&) const;
    using rai::Block::Hash;
    void Hash(blake2b_state&) const override;
    rai::BlockHash Previous() const override;
    // rai::BlockHash Root() const override;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    void SetSignature(const rai::uint512_union&) override;
    rai::Signature Signature() const override;
    rai::BlockType Type() const override;
    rai::BlockOpcode Opcode() const override;
    rai::ErrorCode Visit(rai::BlockVisitor&) const override;
    rai::Account Account() const override;
    rai::Amount Balance() const override;
    uint16_t Credit() const override;
    uint32_t Counter() const override;
    uint64_t Timestamp() const override;
    uint64_t Height() const override;
    rai::uint256_union Link() const override;
    rai::Account Representative() const override;
    bool HasRepresentative() const override;

    static bool CheckOpcode(rai::BlockOpcode);

private:
    rai::BlockType type_;
    rai::BlockOpcode opcode_;
    uint16_t credit_;
    uint32_t counter_;
    uint64_t timestamp_;
    uint64_t height_;
    rai::Account account_;
    rai::BlockHash previous_;
    rai::Amount balance_;
    rai::uint256_union link_;
    rai::Signature signature_;
};

// Airdrop Block
class AdBlock : public Block
{
public:
    AdBlock(rai::BlockOpcode, uint16_t, uint32_t, uint64_t, uint64_t,
            const rai::Account&, const rai::BlockHash&, const rai::Account&,
            const rai::Amount&, const rai::uint256_union&, const rai::RawKey&,
            const rai::PublicKey&);
    AdBlock(rai::ErrorCode&, rai::Stream&);
    AdBlock(rai::ErrorCode&, const rai::Ptree&);
    virtual ~AdBlock() = default;

    bool operator==(const rai::Block&) const override;
    bool operator==(const rai::AdBlock&) const;
    using rai::Block::Hash;
    void Hash(blake2b_state&) const override;
    rai::BlockHash Previous() const override;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    void SetSignature(const rai::uint512_union&) override;
    rai::Signature Signature() const override;
    rai::BlockType Type() const override;
    rai::BlockOpcode Opcode() const override;
    rai::ErrorCode Visit(rai::BlockVisitor&) const override;
    rai::Account Account() const override;
    rai::Amount Balance() const override;
    uint16_t Credit() const override;
    uint32_t Counter() const override;
    uint64_t Timestamp() const override;
    uint64_t Height() const override;
    rai::uint256_union Link() const override;
    rai::Account Representative() const override;
    bool HasRepresentative() const override;

    static bool CheckOpcode(rai::BlockOpcode);

private:
    rai::BlockType type_;
    rai::BlockOpcode opcode_;
    uint16_t credit_;
    uint32_t counter_; 
    uint64_t timestamp_;
    uint64_t height_;
    rai::Account account_;
    rai::BlockHash previous_;
    rai::Account representative_;
    rai::Amount balance_;
    rai::uint256_union link_;
    rai::Signature signature_;
};

class BlockVisitor
{
public:
    virtual rai::ErrorCode Send(const rai::Block&)    = 0;
    virtual rai::ErrorCode Receive(const rai::Block&) = 0;
    virtual rai::ErrorCode Change(const rai::Block&)  = 0;
    virtual rai::ErrorCode Credit(const rai::Block&)  = 0;
    virtual rai::ErrorCode Reward(const rai::Block&)  = 0;
    virtual rai::ErrorCode Destroy(const rai::Block&) = 0;

    virtual ~BlockVisitor() = default;
};

std::unique_ptr<rai::Block> DeserializeBlockJson(rai::ErrorCode&,
                                                 const rai::Ptree&);

std::unique_ptr<rai::Block> DeserializeBlock(rai::ErrorCode&, rai::Stream&);
std::unique_ptr<rai::Block> DeserializeBlockUnverify(rai::ErrorCode&,
                                                     rai::Stream&);

}  // namespace rai
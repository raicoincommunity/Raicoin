#pragma once

#include <string>
#include <vector>
#include <memory>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/token.hpp>
#include <rai/common/chain.hpp>

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
    static rai::ErrorCode CheckType(rai::TokenType);
    static rai::ErrorCode CheckChain(rai::Chain);
    static rai::ErrorCode CheckValue(rai::TokenType, const rai::TokenValue&);

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
        virtual rai::ErrorCode CheckData() const                  = 0;

    };

    ExtensionTokenCreate();
    virtual ~ExtensionTokenCreate() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    rai::ErrorCode CheckData() const;

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

    rai::ErrorCode CheckData() const override;
    
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

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;

    rai::ErrorCode CheckData() const override;
    
    std::string name_;
    std::string symbol_;
    std::string base_uri_;
    rai::TokenValue cap_supply_;
    bool burnable_;
    bool circulable_;
};

class ExtensionTokenMint : public rai::ExtensionToken::Data
{
public:
    ExtensionTokenMint();
    virtual ~ExtensionTokenMint() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;

    rai::ErrorCode CheckData() const;

    rai::TokenType type_;
    rai::Account to_;
    rai::TokenValue value_;
    std::string uri_;
};

class ExtensionTokenBurn : public rai::ExtensionToken::Data
{
public:
    ExtensionTokenBurn();
    virtual ~ExtensionTokenBurn() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;

    rai::ErrorCode CheckData() const;

    rai::TokenType type_;
    rai::TokenValue value_;
};

class ExtensionTokenInfo
{
public:
    ExtensionTokenInfo();
    void Serialize(rai::Stream&) const;
    rai::ErrorCode Deserialize(rai::Stream&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode DeserializeJson(const rai::Ptree&);


    rai::ErrorCode CheckData() const;
    bool Fungible() const;
    bool NonFungible() const;
    bool operator==(const rai::ExtensionTokenInfo&) const;
    bool IsNative() const;

    rai::Chain chain_;
    rai::TokenType type_;
    rai::TokenAddress address_;
};

class ExtensionTokenSend : public rai::ExtensionToken::Data
{
public:
    ExtensionTokenSend() = default;
    virtual ~ExtensionTokenSend() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;

    rai::ErrorCode CheckData() const;

    rai::ExtensionTokenInfo token_;
    rai::Account to_;
    rai::TokenValue value_;
};

class ExtensionTokenReceive : public rai::ExtensionToken::Data
{
public:
    ExtensionTokenReceive();
    virtual ~ExtensionTokenReceive() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;

    rai::ErrorCode CheckData() const;

    rai::ExtensionTokenInfo token_;
    rai::TokenSource source_;
    rai::Account from_;
    uint64_t block_height_;
    rai::BlockHash tx_hash_;
    rai::TokenValue value_;
    rai::Chain unwrap_chain_;
};

class ExtensionTokenSwap : public rai::ExtensionToken::Data
{
public:
    ExtensionTokenSwap();
    virtual ~ExtensionTokenSwap() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;

    rai::ErrorCode CheckData() const;

    enum class SubOp : uint8_t
    {
        INVALID     = 0,
        CONFIG      = 1,
        MAKE        = 2,
        INQUIRY     = 3,
        INQUIRY_ACK = 4,
        TAKE        = 5,
        TAKE_ACK    = 6,
        TAKE_NACK   = 7,
        CANCEL      = 8,
        PING        = 9,
        PONG        = 10,
        MAX
    };
    static std::string SubOpToString(SubOp);
    static SubOp StringToSubOp(const std::string&);

    class Data
    {
    public:
        virtual ~Data()                                           = default;
        virtual void Serialize(rai::Stream&) const                = 0;
        virtual rai::ErrorCode Deserialize(rai::Stream&)          = 0;
        virtual void SerializeJson(rai::Ptree&) const             = 0;
        virtual rai::ErrorCode DeserializeJson(const rai::Ptree&) = 0;
        virtual rai::ErrorCode CheckData() const                  = 0;
    };

    static rai::ErrorCode CheckSubOp(SubOp);
    static std::shared_ptr<Data> MakeSubData(SubOp);

    SubOp sub_op_;
    std::shared_ptr<Data> sub_data_;
};

class ExtensionTokenSwapConfig : public rai::ExtensionTokenSwap::Data
{
public:
    ExtensionTokenSwapConfig() = default;
    virtual ~ExtensionTokenSwapConfig() = default;

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    rai::ErrorCode CheckData() const override;
    
    rai::Account main_;
};

class ExtensionTokenSwapMake : public rai::ExtensionTokenSwap::Data
{
public:
    ExtensionTokenSwapMake();
    virtual ~ExtensionTokenSwapMake() = default;

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    rai::ErrorCode CheckData() const override;
    bool FungiblePair() const;
    
    rai::ExtensionTokenInfo token_offer_;
    rai::ExtensionTokenInfo token_want_;
    rai::TokenValue value_offer_;
    rai::TokenValue value_want_;
    rai::TokenValue min_offer_;
    rai::TokenValue max_offer_;
    uint64_t timeout_; // a hint for the account it's self, not mandatory
};

class ExtensionTokenSwapInquiry : public rai::ExtensionTokenSwap::Data
{
public:
    ExtensionTokenSwapInquiry();
    virtual ~ExtensionTokenSwapInquiry() = default;

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    rai::ErrorCode CheckData() const override;

    rai::Account maker_;
    uint64_t order_height_;
    uint64_t ack_height_;
    uint64_t timeout_;
    rai::TokenValue value_;
    rai::PublicKey share_;
};

class ExtensionTokenSwapInquiryAck : public rai::ExtensionTokenSwap::Data
{
public:
    ExtensionTokenSwapInquiryAck();
    virtual ~ExtensionTokenSwapInquiryAck() = default;

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    rai::ErrorCode CheckData() const override;

    rai::Account taker_;
    uint64_t inquiry_height_;
    uint64_t trade_height_;
    rai::PublicKey share_;
    rai::Signature signature_;
};

class ExtensionTokenSwapTake : public rai::ExtensionTokenSwap::Data
{
public:
    ExtensionTokenSwapTake();
    virtual ~ExtensionTokenSwapTake() = default;

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    rai::ErrorCode CheckData() const override;

    uint64_t inquiry_height_;
};

class ExtensionTokenSwapTakeAck : public rai::ExtensionTokenSwap::Data
{
public:
    ExtensionTokenSwapTakeAck();
    virtual ~ExtensionTokenSwapTakeAck() = default;

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    rai::ErrorCode CheckData() const override;

    rai::Account taker_;
    uint64_t inquiry_height_;
    uint64_t take_height_;
    rai::TokenValue value_;
};

class ExtensionTokenSwapTakeNack : public rai::ExtensionTokenSwap::Data
{
public:
    ExtensionTokenSwapTakeNack();
    virtual ~ExtensionTokenSwapTakeNack() = default;

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    rai::ErrorCode CheckData() const override;

    rai::Account taker_;
    uint64_t inquiry_height_;
};

class ExtensionTokenSwapCancel : public rai::ExtensionTokenSwap::Data
{
public:
    ExtensionTokenSwapCancel();
    virtual ~ExtensionTokenSwapCancel() = default;

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    rai::ErrorCode CheckData() const override;

    uint64_t order_height_;
};

class ExtensionTokenSwapPing : public rai::ExtensionTokenSwap::Data
{
public:
    ExtensionTokenSwapPing();
    virtual ~ExtensionTokenSwapPing() = default;

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    rai::ErrorCode CheckData() const override;

    rai::Account maker_;
};

class ExtensionTokenSwapPong : public rai::ExtensionTokenSwap::Data
{
public:
    ExtensionTokenSwapPong() = default;
    virtual ~ExtensionTokenSwapPong() = default;

    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;
    rai::ErrorCode CheckData() const override;
};

class ExtensionTokenUnmap : public rai::ExtensionToken::Data
{
public:
    ExtensionTokenUnmap();
    virtual ~ExtensionTokenUnmap() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;

    rai::ErrorCode CheckData() const;

    rai::ExtensionTokenInfo token_;
    rai::Account to_;
    rai::TokenValue value_;
    uint64_t extra_data_;
};

class ExtensionTokenWrap : public rai::ExtensionToken::Data
{
public:
    ExtensionTokenWrap();
    virtual ~ExtensionTokenWrap() = default;
    void Serialize(rai::Stream&) const override;
    rai::ErrorCode Deserialize(rai::Stream&) override;
    void SerializeJson(rai::Ptree&) const override;
    rai::ErrorCode DeserializeJson(const rai::Ptree&) override;

    rai::ErrorCode CheckData() const;

    rai::ExtensionTokenInfo token_;
    rai::Chain to_chain_;
    rai::Account to_account_;
    rai::TokenValue value_;
};

rai::ErrorCode ParseExtension(const rai::Extension&,
                              std::shared_ptr<rai::Extension>&);

rai::ErrorCode ParseExtensionJson(const rai::Ptree&,
                                  std::shared_ptr<rai::Extension>&);

bool ExtensionAppend(const rai::Extension&, std::vector<uint8_t>&);
bool ExtensionsToPtree(const std::vector<uint8_t>&, rai::Ptree&);
rai::ErrorCode PtreeToExtensions(const rai::Ptree&, std::vector<uint8_t>&);

} // rai
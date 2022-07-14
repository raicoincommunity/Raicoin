#include <rai/common/blocks.hpp>

#include <algorithm>
#include <assert.h>
#include <sstream>
#include <string>
#include <vector>

#include <boost/endian/conversion.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/extensions.hpp>

namespace
{
template <typename T>
bool BlocksEqual(const T& first, const rai::Block& second)
{
    static_assert(std::is_base_of<rai::Block, T>::value,
                  "Input parameter is not a block type");
    return (first.Type() == second.Type())
           && (first.Opcode() == second.Opcode())
           && ((static_cast<const T&>(second)) == first);
}
}  // namespace

std::string rai::BlockTypeToString(rai::BlockType type)
{
    switch (type)
    {
        case rai::BlockType::INVALID:
        {
            return "invalid";
        }
        case rai::BlockType::TX_BLOCK:
        {
            return "transaction";
        }
        case rai::BlockType::REP_BLOCK:
        {
            return "representative";
        }
        case rai::BlockType::AD_BLOCK:
        {
            return "airdrop";
        }
        default:
        {
            return "unknow";
        }
    }
}

rai::BlockType rai::StringToBlockType(const std::string& str)
{
    if (str == "transaction")
    {
        return rai::BlockType::TX_BLOCK;
    }
    else if (str == "representative")
    {
        return rai::BlockType::REP_BLOCK;
    }
    else if (str == "airdrop")
    {
        return rai::BlockType::AD_BLOCK;
    }
    else
    {
        return rai::BlockType::INVALID;
    }
}

std::string rai::BlockOpcodeToString(rai::BlockOpcode opcode)
{
    switch (opcode)
    {
        case rai::BlockOpcode::INVALID:
        {
            return "invalid";
        }
        case rai::BlockOpcode::SEND:
        {
            return "send";
        }
        case rai::BlockOpcode::RECEIVE:
        {
            return "receive";
        }
        case rai::BlockOpcode::CHANGE:
        {
            return "change";
        }
        case rai::BlockOpcode::CREDIT:
        {
            return "credit";
        }
        case rai::BlockOpcode::REWARD:
        {
            return "reward";
        }
        case rai::BlockOpcode::DESTROY:
        {
            return "destroy";
        }
        case rai::BlockOpcode::BIND:
        {
            return "bind";
        }
        default:
        {
            return std::to_string(static_cast<uint8_t>(opcode));
        }
    }
}

rai::BlockOpcode rai::StringToBlockOpcode(const std::string& str)
{
    if (str == "send")
    {
        return rai::BlockOpcode::SEND;
    }
    else if (str == "receive")
    {
        return rai::BlockOpcode::RECEIVE;
    }
    else if (str == "change")
    {
        return rai::BlockOpcode::CHANGE;
    }
    else if (str == "credit")
    {
        return rai::BlockOpcode::CREDIT;
    }
    else if (str == "reward")
    {
        return rai::BlockOpcode::REWARD;
    }
    else if (str == "destroy")
    {
        return rai::BlockOpcode::DESTROY;
    }
    else if (str == "bind")
    {
        return rai::BlockOpcode::BIND;
    }
    else
    {
        return rai::BlockOpcode::INVALID;
    }
}

rai::Block::Block()
    : signature_checked_(false), signature_error_(false), hash_cache_(0)
{
}

rai::BlockHash rai::Block::Hash() const
{
    if (!hash_cache_.IsZero())
    {
        return hash_cache_;
    }

    int ret;
    rai::uint256_union result;
    blake2b_state hash;

    ret = blake2b_init(&hash, sizeof(result.bytes));
    assert(0 == ret);

    Hash(hash);

    ret = blake2b_final(&hash, result.bytes.data(), sizeof(result.bytes));
    assert(0 == ret);
    hash_cache_ = result;
    return result;
}

std::string rai::Block::Json() const
{
    rai::Ptree ptree;
    SerializeJson(ptree);

    std::stringstream ostream;
    boost::property_tree::write_json(ostream, ptree);
    return ostream.str();
}

bool rai::Block::CheckSignature() const
{
    if (signature_checked_)
    {
        return signature_error_;
    }

    return CheckSignature_();
}

bool rai::Block::operator!=(const rai::Block& other) const
{
    return !(*this == other);
}

bool rai::Block::ForkWith(const rai::Block& other) const
{
    if (Account() != other.Account())
    {
        return false;
    }

    if (Height() != other.Height())
    {
        return false;
    }

    if (*this == other)
    {
        return false;
    }

    return true;
}

bool rai::Block::Limited() const
{
    return rai::SameDay(Timestamp(), rai::CurrentTimestamp())
           && (Counter() >= Credit() * rai::TRANSACTIONS_PER_CREDIT);
}

bool rai::Block::Amount(rai::Amount& amount) const
{
    if (Height() != 0)
    {
        return true;
    }

    rai::uint256_t price_s(rai::CreditPrice(Timestamp()).Number());
    rai::uint256_t cost_s = price_s * Credit();
    rai::uint256_t amount_s = cost_s + Balance().Number();
    if (amount_s <= std::numeric_limits<rai::uint128_t>::max())
    {
        amount = rai::Amount(static_cast<rai::uint128_t>(amount_s));
    }
    else
    {
        return true;
    }

    return false;
}

bool rai::Block::Amount(const rai::Block& previous, rai::Amount& amount) const
{
    if (previous.Balance() > Balance())
    {
        amount = previous.Balance() - Balance();
    }
    else
    {
        amount = Balance() - previous.Balance();
    }
    return false;
}

size_t rai::Block::Size() const
{
    std::vector<uint8_t> bytes;
    bytes.reserve(1024);
    {
        rai::VectorStream stream(bytes);
        Serialize(stream);
    }
    return bytes.size();
}

bool rai::Block::CheckSignature_() const
{
    signature_error_ = rai::ValidateMessage(Account(), Hash(), Signature());
    signature_checked_ = true;
    return signature_error_;
}

void rai::Block::ClearHashCache_()
{
    hash_cache_.Clear();
}

rai::TxBlock::TxBlock(
    rai::BlockOpcode opcode, uint16_t credit, uint32_t counter,
    uint64_t timestamp, uint64_t height, const rai::Account& account,
    const rai::BlockHash& previous, const rai::Account& representative,
    const rai::Amount& balance, const rai::uint256_union& link,
    uint32_t extensions_length, const std::vector<uint8_t>& extensions)
    : type_(rai::BlockType::TX_BLOCK),
      opcode_(opcode),
      credit_(credit),
      counter_(counter),
      timestamp_(timestamp),
      height_(height),
      account_(account),
      previous_(previous),
      representative_(representative),
      balance_(balance),
      link_(link),
      extensions_length_(extensions_length),
      extensions_(extensions)
{
}

rai::TxBlock::TxBlock(
    rai::BlockOpcode opcode, uint16_t credit, uint32_t counter,
    uint64_t timestamp, uint64_t height, const rai::Account& account,
    const rai::BlockHash& previous, const rai::Account& representative,
    const rai::Amount& balance, const rai::uint256_union& link,
    uint32_t extensions_length, const std::vector<uint8_t>& extensions,
    const rai::RawKey& private_key, const rai::PublicKey& public_key)
    : type_(rai::BlockType::TX_BLOCK),
      opcode_(opcode),
      credit_(credit),
      counter_(counter),
      timestamp_(timestamp),
      height_(height),
      account_(account),
      previous_(previous),
      representative_(representative),
      balance_(balance),
      link_(link),
      extensions_length_(extensions_length),
      extensions_(extensions),
      signature_(rai::SignMessage(private_key, public_key, Hash()))
{
}

rai::TxBlock::TxBlock(rai::ErrorCode& error_code, rai::Stream& stream)
{
    error_code = Deserialize(stream);
}

rai::TxBlock::TxBlock(rai::ErrorCode& error_code, const rai::Ptree& ptree)
{
    error_code = DeserializeJson(ptree);
}

bool rai::TxBlock::operator==(const rai::Block& other) const
{
    return BlocksEqual(*this, other);
}

bool rai::TxBlock::operator==(const rai::TxBlock& other) const
{
    // type_ and opcode_ compared in BlocksEqual
    return (credit_ == other.credit_) && (counter_ == other.counter_)
           && (timestamp_ == other.timestamp_) && (height_ == other.height_)
           && (account_ == other.account_) && (previous_ == other.previous_)
           && (representative_ == other.representative_)
           && (balance_ == other.balance_) && (link_ == other.link_)
           && (extensions_length_ == other.extensions_length_)
           && (extensions_ == other.extensions_)
           && (signature_ == other.signature_);
}

void rai::TxBlock::Hash(blake2b_state& state) const
{
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, type_);
        rai::Write(stream, opcode_);
        rai::Write(stream, credit_);
        rai::Write(stream, counter_);
        rai::Write(stream, timestamp_);
        rai::Write(stream, height_);
        rai::Write(stream, account_.bytes);
        rai::Write(stream, previous_.bytes);
        rai::Write(stream, representative_.bytes);
        rai::Write(stream, balance_.bytes);
        rai::Write(stream, link_.bytes);
        rai::Write(stream, extensions_length_);
        rai::Write(stream, extensions_);
    }

    blake2b_update(&state, bytes.data(), bytes.size());
}

rai::BlockHash rai::TxBlock::Previous() const
{
    return previous_;
}

void rai::TxBlock::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, type_);
    rai::Write(stream, opcode_);
    rai::Write(stream, credit_);
    rai::Write(stream, counter_);
    rai::Write(stream, timestamp_);
    rai::Write(stream, height_);
    rai::Write(stream, account_.bytes);
    rai::Write(stream, previous_.bytes);
    rai::Write(stream, representative_.bytes);
    rai::Write(stream, balance_.bytes);
    rai::Write(stream, link_.bytes);
    rai::Write(stream, extensions_length_);
    rai::Write(stream, extensions_);
    rai::Write(stream, signature_.bytes);
}

rai::ErrorCode rai::TxBlock::Deserialize(rai::Stream& stream)
{
    ClearHashCache_();
    bool error = false;
    
    type_ = rai::BlockType::TX_BLOCK;

    error = rai::Read(stream, opcode_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    error = rai::TxBlock::CheckOpcode(opcode_);
    IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_OPCODE);

    error =  rai::Read(stream, credit_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    if (credit_ == 0)
    {
        return rai::ErrorCode::BLOCK_CREDIT;
    }

    error = rai::Read(stream, counter_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    if (counter_ == 0)
    {
        return rai::ErrorCode::BLOCK_COUNTER;
    }

    error = rai::Read(stream, timestamp_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, account_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, previous_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, representative_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, balance_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, link_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, extensions_length_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    error = rai::TxBlock::CheckExtensionsLength(extensions_length_);
    IF_ERROR_RETURN(error, rai::ErrorCode::EXTENSIONS_LENGTH);

    error = rai::Read(stream, extensions_length_, extensions_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, signature_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return rai::ErrorCode::SUCCESS;
}

void rai::TxBlock::SerializeJson(rai::Ptree& ptree) const
{

    ptree.put("type", rai::BlockTypeToString(type_));
    ptree.put("opcode", rai::BlockOpcodeToString(opcode_));
    ptree.put("credit", std::to_string(credit_));
    ptree.put("counter", std::to_string(counter_));
    ptree.put("timestamp", std::to_string(timestamp_));
    ptree.put("height", std::to_string(height_));
    ptree.put("account", account_.StringAccount());
    ptree.put("previous", previous_.StringHex());
    ptree.put("representative", representative_.StringAccount());
    ptree.put("balance", balance_.StringDec());
    if (rai::BlockOpcode::SEND == opcode_)
    {
        ptree.put("link", link_.StringAccount());
    }
    else
    {
        ptree.put("link", link_.StringHex());
    }
    ptree.put("extensions_length", std::to_string(extensions_length_));
    rai::Ptree extensions;
    rai::ExtensionsToPtree(extensions_, extensions);
    ptree.add_child("extensions", extensions);
    ptree.put("extensions_raw",
              rai::BytesToHex(extensions_.data(), extensions_.size()));
    ptree.put("signature", signature_.StringHex());
}

rai::ErrorCode rai::TxBlock::DeserializeJson(const rai::Ptree& ptree)
{
    ClearHashCache_();
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;

    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_TYPE;
        std::string type = ptree.get<std::string>("type");
        if (type != rai::BlockTypeToString(rai::BlockType::TX_BLOCK))
        {
            return rai::ErrorCode::BLOCK_TYPE;
        }
        type_ = rai::BlockType::TX_BLOCK;

        error_code = rai::ErrorCode::JSON_BLOCK_OPCODE;
        std::string opcode = ptree.get<std::string>("opcode");
        opcode_ = rai::StringToBlockOpcode(opcode);
        bool error = rai::TxBlock::CheckOpcode(opcode_);
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_OPCODE);

        error_code = rai::ErrorCode::JSON_BLOCK_CREDIT;
        std::string credit = ptree.get<std::string>("credit");
        error = rai::StringToUint(credit, credit_);
        IF_ERROR_RETURN(error, error_code);
        if (credit_ == 0)
        {
            return rai::ErrorCode::BLOCK_CREDIT;
        }

        error_code = rai::ErrorCode::JSON_BLOCK_COUNTER;
        std::string counter = ptree.get<std::string>("counter");
        error = rai::StringToUint(counter, counter_);
        IF_ERROR_RETURN(error, error_code);
        if (counter_ == 0)
        {
            return rai::ErrorCode::BLOCK_COUNTER;
        }

        error_code = rai::ErrorCode::JSON_BLOCK_TIMESTAMP;
        std::string timestamp = ptree.get<std::string>("timestamp");
        error = rai::StringToUint(timestamp, timestamp_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_HEIGHT;
        std::string height = ptree.get<std::string>("height");
        error = rai::StringToUint(height, height_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_ACCOUNT;
        std::string account = ptree.get<std::string>("account");
        error = account_.DecodeAccount(account);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_PREVIOUS;
        std::string previous = ptree.get<std::string>("previous");
        error = previous_.DecodeHex(previous);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_REPRESENTATIVE;
        std::string representative = ptree.get<std::string>("representative");
        error = representative_.DecodeAccount(representative);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_BALANCE;
        std::string balance = ptree.get<std::string>("balance");
        error = balance_.DecodeDec(balance);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_LINK;
        std::string link = ptree.get<std::string>("link");
        if (rai::BlockOpcode::SEND == opcode_)
        {
            error = link_.DecodeAccount(link);
        }
        else
        {
            error = link_.DecodeHex(link);
        }
        IF_ERROR_RETURN(error, error_code);

        auto extensions_raw = ptree.get_optional<std::string>("extensions_raw");
        if (extensions_raw)
        {
            error = rai::HexToBytes(*extensions_raw, extensions_);
            IF_ERROR_RETURN(error, rai::ErrorCode::JSON_BLOCK_EXTENSIONS_RAW);
        }
        else
        {
            auto extensions  = ptree.get_child_optional("extensions");
            if (extensions && !extensions->empty())
            {
                error_code = rai::PtreeToExtensions(*extensions, extensions_);
                IF_NOT_SUCCESS_RETURN(error_code);
            }
        }

        extensions_length_ = static_cast<uint32_t>(extensions_.size());
        error = rai::TxBlock::CheckExtensionsLength(extensions_length_);
        IF_ERROR_RETURN(error, rai::ErrorCode::JSON_BLOCK_EXTENSIONS_LENGTH);

        error_code = rai::ErrorCode::JSON_BLOCK_SIGNATURE;
        std::string signature = ptree.get<std::string>("signature");
        error = signature_.DecodeHex(signature);
        IF_ERROR_RETURN(error, error_code);

        error =  CheckSignature_();
        IF_ERROR_RETURN(error, rai::ErrorCode::SIGNATURE);
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::TxBlock::SetSignature(const rai::uint512_union& signature)
{
    signature_ = signature;
    CheckSignature_();
}

rai::Signature rai::TxBlock::Signature() const
{
    return signature_;
}

rai::BlockType rai::TxBlock::Type() const
{
    return type_;
}

rai::BlockOpcode rai::TxBlock::Opcode() const
{
    return opcode_;
}

rai::ErrorCode rai::TxBlock::Visit(rai::BlockVisitor& visitor) const
{
    if (opcode_ == rai::BlockOpcode::SEND)
    {
        return visitor.Send(*this);
    }
    else if (opcode_ == rai::BlockOpcode::RECEIVE)
    {
        return visitor.Receive(*this);
    }
    else if (opcode_ == rai::BlockOpcode::CHANGE)
    {
        return visitor.Change(*this);
    }
    else if (opcode_ == rai::BlockOpcode::CREDIT)
    {
        return visitor.Credit(*this);
    }
    else
    {
        assert(0);
        return rai::ErrorCode::BLOCK_OPCODE;
    }
}

rai::Account rai::TxBlock::Account() const
{
    return account_;
}

rai::Amount rai::TxBlock::Balance() const
{
    return balance_;
}

uint16_t rai::TxBlock::Credit() const
{
    return credit_;
}

uint32_t rai::TxBlock::Counter() const
{
    return counter_;
}

uint64_t rai::TxBlock::Timestamp() const
{
    return timestamp_;
}

uint64_t rai::TxBlock::Height() const
{
    return height_;
}

rai::uint256_union rai::TxBlock::Link() const
{
    return link_;
}

rai::Account rai::TxBlock::Representative() const
{
    return representative_;
}

bool rai::TxBlock::HasRepresentative() const
{
    return true;
}

std::vector<uint8_t> rai::TxBlock::Extensions() const
{
    return extensions_;
}

bool rai::TxBlock::HasChain() const
{
    return false;
}

rai::Chain rai::TxBlock::Chain() const
{
    return rai::Chain::INVALID;
}

bool rai::TxBlock::CheckOpcode(rai::BlockOpcode opcode)
{
    std::vector<rai::BlockOpcode> opcodes = {
        rai::BlockOpcode::SEND, rai::BlockOpcode::RECEIVE,
        rai::BlockOpcode::CHANGE, rai::BlockOpcode::CREDIT};

    return std::find(opcodes.begin(), opcodes.end(), opcode) == opcodes.end();
}

bool rai::TxBlock::CheckExtensionsLength(uint32_t length)
{
    return length > rai::TxBlock::MaxExtensionsLength();
}

uint32_t rai::TxBlock::MaxExtensionsLength()
{
    return static_cast<uint32_t>(MAX_EXTENSIONS_SIZE);
}

rai::RepBlock::RepBlock(rai::BlockOpcode opcode, uint16_t credit,
                        uint32_t counter, uint64_t timestamp, uint64_t height,
                        const rai::Account& account,
                        const rai::BlockHash& previous,
                        const rai::Amount& balance,
                        const rai::uint256_union& link,
                        rai::Chain chain)
    : type_(rai::BlockType::REP_BLOCK),
      opcode_(opcode),
      credit_(credit),
      counter_(counter),
      timestamp_(timestamp),
      height_(height),
      account_(account),
      previous_(previous),
      balance_(balance),
      link_(link),
      chain_(chain)
{
}

rai::RepBlock::RepBlock(rai::BlockOpcode opcode, uint16_t credit,
                        uint32_t counter, uint64_t timestamp, uint64_t height,
                        const rai::Account& account,
                        const rai::BlockHash& previous,
                        const rai::Amount& balance,
                        const rai::uint256_union& link,
                        const rai::RawKey& private_key,
                        const rai::PublicKey& public_key,
                        rai::Chain chain)
    : type_(rai::BlockType::REP_BLOCK),
      opcode_(opcode),
      credit_(credit),
      counter_(counter),
      timestamp_(timestamp),
      height_(height),
      account_(account),
      previous_(previous),
      balance_(balance),
      link_(link),
      chain_(chain),
      signature_(rai::SignMessage(private_key, public_key, Hash()))
{
}

rai::RepBlock::RepBlock(rai::ErrorCode& error_code, rai::Stream& stream)
{
    error_code = Deserialize(stream);
}

rai::RepBlock::RepBlock(rai::ErrorCode& error_code, const rai::Ptree& ptree)
{
    error_code = DeserializeJson(ptree);
}

bool rai::RepBlock::operator==(const rai::Block& other) const
{
    return BlocksEqual(*this, other);
}

bool rai::RepBlock::operator==(const rai::RepBlock& other) const
{
    // type_ and opcode_ compared in BlocksEqual
    return (credit_ == other.credit_) && (counter_ == other.counter_)
           && (timestamp_ == other.timestamp_) && (height_ == other.height_)
           && (account_ == other.account_) && (previous_ == other.previous_)
           && (balance_ == other.balance_) && (link_ == other.link_)
           && (chain_ == other.chain_) && (signature_ == other.signature_);
}

void rai::RepBlock::Hash(blake2b_state& state) const
{
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, type_);
        rai::Write(stream, opcode_);
        rai::Write(stream, credit_);
        rai::Write(stream, counter_);
        rai::Write(stream, timestamp_);
        rai::Write(stream, height_);
        rai::Write(stream, account_.bytes);
        rai::Write(stream, previous_.bytes);
        rai::Write(stream, balance_.bytes);
        rai::Write(stream, link_.bytes);
        if (HasChain())
        {
            rai::Write(stream, chain_);
        }
    }

    blake2b_update(&state, bytes.data(), bytes.size());
}

rai::BlockHash rai::RepBlock::Previous() const
{
    return previous_;
}

void rai::RepBlock::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, type_);
    rai::Write(stream, opcode_);
    rai::Write(stream, credit_);
    rai::Write(stream, counter_);
    rai::Write(stream, timestamp_);
    rai::Write(stream, height_);
    rai::Write(stream, account_.bytes);
    rai::Write(stream, previous_.bytes);
    rai::Write(stream, balance_.bytes);
    rai::Write(stream, link_.bytes);
    if (HasChain())
    {
        rai::Write(stream, chain_);
    }
    rai::Write(stream, signature_.bytes);
}

rai::ErrorCode rai::RepBlock::Deserialize(rai::Stream& stream)
{
    ClearHashCache_();
    bool error = false;
    
    type_ = rai::BlockType::REP_BLOCK;

    error = rai::Read(stream, opcode_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    error = rai::RepBlock::CheckOpcode(opcode_);
    IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_OPCODE);

    error = rai::Read(stream, credit_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    if (credit_ == 0)
    {
        return rai::ErrorCode::BLOCK_CREDIT;
    }

    error = rai::Read(stream, counter_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, timestamp_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, account_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, previous_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, balance_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, link_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    if (HasChain())
    {
        error = rai::Read(stream, chain_);
        IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
        if (chain_ == rai::Chain::INVALID)
        {
            return rai::ErrorCode::BLOCK_CHAIN;
        }
    }
    else
    {
        chain_ = rai::Chain::INVALID;
    }

    error = rai::Read(stream, signature_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return rai::ErrorCode::SUCCESS;
}

void rai::RepBlock::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("type", rai::BlockTypeToString(rai::BlockType::REP_BLOCK));
    ptree.put("opcode", rai::BlockOpcodeToString(opcode_));
    ptree.put("credit", std::to_string(credit_));
    ptree.put("counter", std::to_string(counter_));
    ptree.put("timestamp", std::to_string(timestamp_));
    ptree.put("height", std::to_string(height_));
    ptree.put("account", account_.StringAccount());
    ptree.put("previous", previous_.StringHex());
    ptree.put("balance", balance_.StringDec());
    if (rai::BlockOpcode::SEND == opcode_)
    {
        ptree.put("link", link_.StringAccount());
    }
    else
    {
        ptree.put("link", link_.StringHex());
    }
    if (HasChain())
    {
        ptree.put("chain", rai::ChainToString(chain_));
        ptree.put("chain_id", std::to_string(static_cast<uint32_t>(chain_)));
    }
    ptree.put("signature", signature_.StringHex());
}

rai::ErrorCode rai::RepBlock::DeserializeJson(const rai::Ptree& ptree)
{
    ClearHashCache_();
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;

    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_TYPE;
        std::string type = ptree.get<std::string>("type");
        if (type != rai::BlockTypeToString(rai::BlockType::REP_BLOCK))
        {
            return rai::ErrorCode::BLOCK_TYPE;
        }
        type_ = rai::BlockType::REP_BLOCK;

        error_code = rai::ErrorCode::JSON_BLOCK_OPCODE;
        std::string opcode = ptree.get<std::string>("opcode");
        opcode_ = rai::StringToBlockOpcode(opcode);
        bool error = rai::RepBlock::CheckOpcode(opcode_);
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_OPCODE);

        error_code = rai::ErrorCode::JSON_BLOCK_CREDIT;
        std::string credit = ptree.get<std::string>("credit");
        error = rai::StringToUint(credit, credit_);
        IF_ERROR_RETURN(error, error_code);
        if (credit_ == 0)
        {
            return rai::ErrorCode::BLOCK_CREDIT;
        }

        error_code          = rai::ErrorCode::JSON_BLOCK_COUNTER;
        std::string counter = ptree.get<std::string>("counter");
        error               = rai::StringToUint(counter, counter_);
        IF_ERROR_RETURN(error, error_code);

        error_code            = rai::ErrorCode::JSON_BLOCK_TIMESTAMP;
        std::string timestamp = ptree.get<std::string>("timestamp");
        error = rai::StringToUint(timestamp, timestamp_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_HEIGHT;
        std::string height = ptree.get<std::string>("height");
        error = rai::StringToUint(height, height_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_ACCOUNT;
        std::string account = ptree.get<std::string>("account");
        error = account_.DecodeAccount(account);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_PREVIOUS;
        std::string previous = ptree.get<std::string>("previous");
        error = previous_.DecodeHex(previous);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_BALANCE;
        std::string balance = ptree.get<std::string>("balance");
        error = balance_.DecodeDec(balance);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_LINK;
        std::string link = ptree.get<std::string>("link");
        if (rai::BlockOpcode::SEND == opcode_)
        {
            error = link_.DecodeAccount(link);
        }
        else
        {
            error = link_.DecodeHex(link);
        }
        IF_ERROR_RETURN(error, error_code);

        if (HasChain())
        {
            error_code = rai::ErrorCode::JSON_BLOCK_CHAIN_ID;
            std::string chain_str = ptree.get<std::string>("chain_id");
            uint32_t chain;
            error = rai::StringToUint(chain_str, chain);
            IF_ERROR_RETURN(error, error_code);
            chain_ = static_cast<rai::Chain>(chain);
            if (chain_ == rai::Chain::INVALID)
            {
                return error_code;
            }
        }
        else
        {
            chain_ = rai::Chain::INVALID;
        }

        error_code = rai::ErrorCode::JSON_BLOCK_SIGNATURE;
        std::string signature = ptree.get<std::string>("signature");
        error = signature_.DecodeHex(signature);
        IF_ERROR_RETURN(error, error_code);

        error =  CheckSignature_();
        IF_ERROR_RETURN(error, rai::ErrorCode::SIGNATURE);
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::RepBlock::SetSignature(const rai::uint512_union& signature)
{
    signature_ = signature;
    CheckSignature_();
}

rai::Signature rai::RepBlock::Signature() const
{
    return signature_;
}

rai::BlockType rai::RepBlock::Type() const
{
    return type_;
}

rai::BlockOpcode rai::RepBlock::Opcode() const
{
    return opcode_;
}

rai::ErrorCode rai::RepBlock::Visit(rai::BlockVisitor& visitor) const
{
    if (opcode_ == rai::BlockOpcode::SEND)
    {
        return visitor.Send(*this);
    }
    else if (opcode_ == rai::BlockOpcode::RECEIVE)
    {
        return visitor.Receive(*this);
    }
    else if (opcode_ == rai::BlockOpcode::CREDIT)
    {
        return visitor.Credit(*this);
    }
    else if (opcode_ == rai::BlockOpcode::REWARD)
    {
        return visitor.Reward(*this);
    }
    else if (opcode_ == rai::BlockOpcode::BIND)
    {
        return visitor.Bind(*this);
    }
    else
    {
        assert(0);
        return rai::ErrorCode::BLOCK_OPCODE;
    }
}

rai::Account rai::RepBlock::Account() const
{
    return account_;
}

rai::Amount rai::RepBlock::Balance() const
{
    return balance_;
}

uint16_t rai::RepBlock::Credit() const
{
    return credit_;
}

uint32_t rai::RepBlock::Counter() const
{
    return counter_;
}

uint64_t rai::RepBlock::Timestamp() const
{
    return timestamp_;
}

uint64_t rai::RepBlock::Height() const
{
    return height_;
}

rai::uint256_union rai::RepBlock::Link() const
{
    return link_;
}

rai::Account rai::RepBlock::Representative() const
{
    return rai::Account(0);
}

bool rai::RepBlock::HasRepresentative() const
{
    return false;
}

std::vector<uint8_t> rai::RepBlock::Extensions() const
{
    return std::vector<uint8_t>();
}

bool rai::RepBlock::HasChain() const
{
    return opcode_ == rai::BlockOpcode::BIND;
}

rai::Chain rai::RepBlock::Chain() const
{
    return chain_;
}

bool rai::RepBlock::CheckOpcode(rai::BlockOpcode opcode)
{
    std::vector<rai::BlockOpcode> opcodes = {
        rai::BlockOpcode::SEND, rai::BlockOpcode::RECEIVE,
        rai::BlockOpcode::REWARD, rai::BlockOpcode::CREDIT,
        rai::BlockOpcode::BIND};

    return std::find(opcodes.begin(), opcodes.end(), opcode) == opcodes.end();
}


rai::AdBlock::AdBlock(
    rai::BlockOpcode opcode, uint16_t credit, uint32_t counter,
    uint64_t timestamp, uint64_t height, const rai::Account& account,
    const rai::BlockHash& previous, const rai::Account& representative,
    const rai::Amount& balance, const rai::uint256_union& link,
    const rai::RawKey& private_key, const rai::PublicKey& public_key)
    : type_(rai::BlockType::AD_BLOCK),
      opcode_(opcode),
      credit_(credit),
      counter_(counter),
      timestamp_(timestamp),
      height_(height),
      account_(account),
      previous_(previous),
      representative_(representative),
      balance_(balance),
      link_(link),
      signature_(rai::SignMessage(private_key, public_key, Hash()))
{
}

rai::AdBlock::AdBlock(rai::ErrorCode& error_code, rai::Stream& stream)
{
    error_code = Deserialize(stream);
}

rai::AdBlock::AdBlock(rai::ErrorCode& error_code, const rai::Ptree& ptree)
{
    error_code = DeserializeJson(ptree);
}


bool rai::AdBlock::operator==(const rai::Block& other) const
{
    return BlocksEqual(*this, other);
}

bool rai::AdBlock::operator==(const rai::AdBlock& other) const
{
    // type_ and opcode_ compared in BlocksEqual
    return (credit_ == other.credit_) && (counter_ == other.counter_)
           && (timestamp_ == other.timestamp_) && (height_ == other.height_)
           && (account_ == other.account_) && (previous_ == other.previous_)
           && (representative_ == other.representative_)
           && (balance_ == other.balance_) && (link_ == other.link_)
           && (signature_ == other.signature_);
}

void rai::AdBlock::Hash(blake2b_state& state) const
{
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, type_);
        rai::Write(stream, opcode_);
        rai::Write(stream, credit_);
        rai::Write(stream, counter_);
        rai::Write(stream, timestamp_);
        rai::Write(stream, height_);
        rai::Write(stream, account_.bytes);
        rai::Write(stream, previous_.bytes);
        rai::Write(stream, representative_.bytes);
        rai::Write(stream, balance_.bytes);
        rai::Write(stream, link_.bytes);
    }

    blake2b_update(&state, bytes.data(), bytes.size());
}

rai::BlockHash rai::AdBlock::Previous() const
{
    return previous_;
}


void rai::AdBlock::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, type_);
    rai::Write(stream, opcode_);
    rai::Write(stream, credit_);
    rai::Write(stream, counter_);
    rai::Write(stream, timestamp_);
    rai::Write(stream, height_);
    rai::Write(stream, account_.bytes);
    rai::Write(stream, previous_.bytes);
    rai::Write(stream, representative_.bytes);
    rai::Write(stream, balance_.bytes);
    rai::Write(stream, link_.bytes);
    rai::Write(stream, signature_.bytes);
}

rai::ErrorCode rai::AdBlock::Deserialize(rai::Stream& stream)
{
    ClearHashCache_();
    bool error = false;
    
    type_ = rai::BlockType::AD_BLOCK;

    error = rai::Read(stream, opcode_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    error = rai::AdBlock::CheckOpcode(opcode_);
    IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_OPCODE);

    error =  rai::Read(stream, credit_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    if (credit_ == 0)
    {
        return rai::ErrorCode::BLOCK_CREDIT;
    }

    error = rai::Read(stream, counter_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    if (counter_ == 0)
    {
        return rai::ErrorCode::BLOCK_COUNTER;
    }

    error = rai::Read(stream, timestamp_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, height_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, account_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, previous_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, representative_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, balance_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, link_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, signature_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    return rai::ErrorCode::SUCCESS;
}

void rai::AdBlock::SerializeJson(rai::Ptree& ptree) const
{

    ptree.put("type", rai::BlockTypeToString(type_));
    ptree.put("opcode", rai::BlockOpcodeToString(opcode_));
    ptree.put("credit", std::to_string(credit_));
    ptree.put("counter", std::to_string(counter_));
    ptree.put("timestamp", std::to_string(timestamp_));
    ptree.put("height", std::to_string(height_));
    ptree.put("account", account_.StringAccount());
    ptree.put("previous", previous_.StringHex());
    ptree.put("representative", representative_.StringAccount());
    ptree.put("balance", balance_.StringDec());
    ptree.put("link", link_.StringHex());
    ptree.put("signature", signature_.StringHex());
}

rai::ErrorCode rai::AdBlock::DeserializeJson(const rai::Ptree& ptree)
{
    ClearHashCache_();
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;

    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_TYPE;
        std::string type = ptree.get<std::string>("type");
        if (type != rai::BlockTypeToString(rai::BlockType::AD_BLOCK))
        {
            return rai::ErrorCode::BLOCK_TYPE;
        }
        type_ = rai::BlockType::AD_BLOCK;

        error_code = rai::ErrorCode::JSON_BLOCK_OPCODE;
        std::string opcode = ptree.get<std::string>("opcode");
        opcode_ = rai::StringToBlockOpcode(opcode);
        bool error = rai::AdBlock::CheckOpcode(opcode_);
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_OPCODE);

        error_code = rai::ErrorCode::JSON_BLOCK_CREDIT;
        std::string credit = ptree.get<std::string>("credit");
        error = rai::StringToUint(credit, credit_);
        IF_ERROR_RETURN(error, error_code);
        if (credit_ == 0)
        {
            return rai::ErrorCode::BLOCK_CREDIT;
        }

        error_code = rai::ErrorCode::JSON_BLOCK_COUNTER;
        std::string counter = ptree.get<std::string>("counter");
        error = rai::StringToUint(counter, counter_);
        IF_ERROR_RETURN(error, error_code);
        if (counter_ == 0)
        {
            return rai::ErrorCode::BLOCK_COUNTER;
        }

        error_code = rai::ErrorCode::JSON_BLOCK_TIMESTAMP;
        std::string timestamp = ptree.get<std::string>("timestamp");
        error = rai::StringToUint(timestamp, timestamp_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_HEIGHT;
        std::string height = ptree.get<std::string>("height");
        error = rai::StringToUint(height, height_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_ACCOUNT;
        std::string account = ptree.get<std::string>("account");
        error = account_.DecodeAccount(account);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_PREVIOUS;
        std::string previous = ptree.get<std::string>("previous");
        error = previous_.DecodeHex(previous);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_REPRESENTATIVE;
        std::string representative = ptree.get<std::string>("representative");
        error = representative_.DecodeAccount(representative);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_BALANCE;
        std::string balance = ptree.get<std::string>("balance");
        error = balance_.DecodeDec(balance);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_LINK;
        std::string link = ptree.get<std::string>("link");
        error = link_.DecodeHex(link);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_SIGNATURE;
        std::string signature = ptree.get<std::string>("signature");
        error = signature_.DecodeHex(signature);
        IF_ERROR_RETURN(error, error_code);

        error =  CheckSignature_();
        IF_ERROR_RETURN(error, rai::ErrorCode::SIGNATURE);
    }
    catch (...)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::AdBlock::SetSignature(const rai::uint512_union& signature)
{
    signature_ = signature;
    CheckSignature_();
}

rai::Signature rai::AdBlock::Signature() const
{
    return signature_;
}

rai::BlockType rai::AdBlock::Type() const
{
    return type_;
}

rai::BlockOpcode rai::AdBlock::Opcode() const
{
    return opcode_;
}

rai::ErrorCode rai::AdBlock::Visit(rai::BlockVisitor& visitor) const
{
    if (opcode_ == rai::BlockOpcode::RECEIVE)
    {
        return visitor.Receive(*this);
    }
    else if (opcode_ == rai::BlockOpcode::CHANGE)
    {
        return visitor.Change(*this);
    }
    else if (opcode_ == rai::BlockOpcode::DESTROY)
    {
        return visitor.Destroy(*this);
    }
    else
    {
        assert(0);
        return rai::ErrorCode::BLOCK_OPCODE;
    }
}

rai::Account rai::AdBlock::Account() const
{
    return account_;
}

rai::Amount rai::AdBlock::Balance() const
{
    return balance_;
}

uint16_t rai::AdBlock::Credit() const
{
    return credit_;
}

uint32_t rai::AdBlock::Counter() const
{
    return counter_;
}

uint64_t rai::AdBlock::Timestamp() const
{
    return timestamp_;
}

uint64_t rai::AdBlock::Height() const
{
    return height_;
}

rai::uint256_union rai::AdBlock::Link() const
{
    return link_;
}

rai::Account rai::AdBlock::Representative() const
{
    return representative_;
}

bool rai::AdBlock::HasRepresentative() const
{
    return true;
}

std::vector<uint8_t> rai::AdBlock::Extensions() const
{
    return std::vector<uint8_t>();
}

bool rai::AdBlock::HasChain() const
{
    return false;
}

rai::Chain rai::AdBlock::Chain() const
{
    return rai::Chain::INVALID;
}

bool rai::AdBlock::CheckOpcode(rai::BlockOpcode opcode)
{
    std::vector<rai::BlockOpcode> opcodes = {rai::BlockOpcode::RECEIVE,
                                             rai::BlockOpcode::CHANGE,
                                             rai::BlockOpcode::DESTROY};

    return std::find(opcodes.begin(), opcodes.end(), opcode) == opcodes.end();
}

rai::TxBlockBuilder::TxBlockBuilder(const rai::Account& account,
                                    const rai::Account& rep,
                                    const std::shared_ptr<rai::Block>& previous)
    : has_key_(false), account_(account), rep_(rep), previous_(previous)
{
}

rai::TxBlockBuilder::TxBlockBuilder(const rai::Account& account,
                                    const rai::Account& rep,
                                    const rai::RawKey& private_key,
                                    const std::shared_ptr<rai::Block>& previous)
    : has_key_(true),
      account_(account),
      rep_(rep),
      private_key_(private_key),
      previous_(previous)
{
}

rai::ErrorCode rai::TxBlockBuilder::Change(
    std::shared_ptr<rai::Block>& block, const rai::Account& rep, uint64_t now,
    const std::vector<uint8_t>& extensions)
{
    if (!previous_)
    {
        return rai::ErrorCode::BLOCK_OPCODE;
    }

    if (previous_->Type() != rai::BlockType::TX_BLOCK)
    {
        return rai::ErrorCode::BLOCK_TYPE;
    }

    if (extensions.size() > rai::MAX_EXTENSIONS_SIZE)
    {
        return rai::ErrorCode::EXTENSIONS_LENGTH;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::CHANGE;
    uint16_t credit = previous_->Credit();
    uint64_t timestamp =
        now > previous_->Timestamp() ? now : previous_->Timestamp();
    if (timestamp > now + 60)
    {
        return rai::ErrorCode::BLOCK_TIMESTAMP;
    }

    uint32_t counter = rai::SameDay(timestamp, previous_->Timestamp())
                           ? previous_->Counter() + 1
                           : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        return rai::ErrorCode::ACCOUNT_ACTION_CREDIT;
    }

    uint64_t height = previous_->Height() + 1;
    rai::BlockHash previousHash = previous_->Hash();
    rai::Amount balance = previous_->Balance();
    rai::uint256_union link(0);
    if (has_key_)
    {
        block = std::make_shared<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account_, previousHash,
            rep, balance, link, extensions.size(), extensions, private_key_,
            account_);
    }
    else
    {
        block = std::make_shared<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account_, previousHash,
            rep, balance, link, extensions.size(), extensions);
    }

    previous_ = block;
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::TxBlockBuilder::Receive(
    std::shared_ptr<rai::Block>& block, const rai::BlockHash& source,
    const rai::Amount& amount, uint64_t sent_at, uint64_t now,
    const std::vector<uint8_t>& extensions)
{
    if (extensions.size() > rai::MAX_EXTENSIONS_SIZE)
    {
        return rai::ErrorCode::EXTENSIONS_LENGTH;
    }

    if (amount.IsZero())
    {
        return rai::ErrorCode::RECEIVABLE_AMOUNT;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::RECEIVE;
    uint16_t credit = 0;
    uint64_t timestamp = 0;
    uint32_t counter = 0;
    uint64_t height = 0;
    rai::BlockHash previous;
    rai::Amount balance;
    rai::uint256_union link;
    rai::Account rep;

    if (!previous_)
    {
        credit = 1;
        timestamp = now >= sent_at ? now : sent_at;
        if (timestamp > now + 60)
        {
            return rai::ErrorCode::BLOCK_TIMESTAMP;
        }
        counter = 1;
        height = 0;
        previous = 0;
        if (amount < rai::CreditPrice(timestamp))
        {
            return rai::ErrorCode::RECEIVABLE_AMOUNT;
        }
        balance = amount - rai::CreditPrice(timestamp);
        link = source;
        rep = rep_;
    }
    else
    {
        if (previous_->Type() != rai::BlockType::TX_BLOCK)
        {
            return rai::ErrorCode::BLOCK_TYPE;
        }

        credit = previous_->Credit();
        timestamp =
            now > previous_->Timestamp() ? now : previous_->Timestamp();
        if (timestamp < sent_at)
        {
            timestamp = sent_at;
        }
        if (timestamp > now + 60)
        {
            return rai::ErrorCode::BLOCK_TIMESTAMP;
        }

        counter = rai::SameDay(timestamp, previous_->Timestamp())
                      ? previous_->Counter() + 1
                      : 1;
        if (counter
            > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
        {
            return rai::ErrorCode::BLOCK_COUNTER;
        }
        height = previous_->Height() + 1;
        previous = previous_->Hash();
        balance = previous_->Balance() + amount;
        if (balance < amount)
        {
            return rai::ErrorCode::RECEIVABLE_AMOUNT;
        }
        link = source;
        rep = previous_->Representative();
    }

    if (has_key_)
    {
        block = std::make_shared<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account_, previous,
            rep, balance, link, extensions.size(), extensions, private_key_,
            account_);
    }
    else
    {
        block = std::make_shared<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account_, previous,
            rep, balance, link, extensions.size(), extensions);
    }

    previous_ = block;
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::TxBlockBuilder::Send(std::shared_ptr<rai::Block>& block,
                                         const rai::Account& destination,
                                         const rai::Amount& amount,
                                         uint64_t now,
                                         const std::vector<uint8_t>& extensions)
{
    if (!previous_)
    {
        return rai::ErrorCode::BLOCK_OPCODE;
    }

    if (previous_->Type() != rai::BlockType::TX_BLOCK)
    {
        return rai::ErrorCode::BLOCK_TYPE;
    }

    if (amount.IsZero())
    {
        return rai::ErrorCode::SEND_AMOUNT;
    }

    if (extensions.size() > rai::MAX_EXTENSIONS_SIZE)
    {
        return rai::ErrorCode::EXTENSIONS_LENGTH;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::SEND;
    uint16_t credit = previous_->Credit();
    uint64_t timestamp =
        now > previous_->Timestamp() ? now : previous_->Timestamp();
    if (timestamp > now + 60)
    {
        return rai::ErrorCode::BLOCK_TIMESTAMP;
    }
    uint32_t counter = rai::SameDay(timestamp, previous_->Timestamp())
                           ? previous_->Counter() + 1
                           : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        return rai::ErrorCode::BLOCK_COUNTER;
    }
    uint64_t height = previous_->Height() + 1;
    rai::BlockHash previous = previous_->Hash();
    if (previous_->Balance() < amount)
    {
        return rai::ErrorCode::SEND_AMOUNT;
    }
    rai::Amount balance = previous_->Balance() - amount;
    rai::uint256_union link = destination;
    rai::Account rep = previous_->Representative();

    if (has_key_)
    {
        block = std::make_shared<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account_, previous,
            rep, balance, link, extensions.size(), extensions, private_key_,
            account_);
    }
    else
    {
        block = std::make_shared<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account_, previous,
            rep, balance, link, extensions.size(), extensions);
    }

    previous_ = block;
    return rai::ErrorCode::SUCCESS;
}

rai::RepBlockBuilder::RepBlockBuilder(
    const rai::Account& account, const std::shared_ptr<rai::Block>& previous)
    : has_key_(false), account_(account), previous_(previous)
{
}

rai::RepBlockBuilder::RepBlockBuilder(
    const rai::Account& account, const rai::RawKey& private_key,
    const std::shared_ptr<rai::Block>& previous)
    : has_key_(true),
      account_(account),
      private_key_(private_key),
      previous_(previous)
{
}

rai::ErrorCode rai::RepBlockBuilder::Bind(std::shared_ptr<rai::Block>& block,
                                          rai::Chain chain,
                                          const rai::SignerAddress& signer,
                                          uint64_t now)
{
    if (!previous_)
    {
        return rai::ErrorCode::BLOCK_OPCODE;
    }

    if (previous_->Type() != rai::BlockType::REP_BLOCK)
    {
        return rai::ErrorCode::BLOCK_TYPE;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::BIND;
    uint16_t credit = previous_->Credit();
    uint64_t timestamp =
        now > previous_->Timestamp() ? now : previous_->Timestamp();
    if (timestamp > now + 60)
    {
        return rai::ErrorCode::BLOCK_TIMESTAMP;
    }

    uint32_t counter = rai::SameDay(timestamp, previous_->Timestamp())
                           ? previous_->Counter() + 1
                           : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        return rai::ErrorCode::BLOCK_COUNTER;
    }

    uint64_t height = previous_->Height() + 1;
    rai::BlockHash previous = previous_->Hash();
    rai::Amount balance = previous_->Balance();
    rai::uint256_union link(signer);

    if (has_key_)
    {
        block = std::make_shared<rai::RepBlock>(
            opcode, credit, counter, timestamp, height, account_, previous,
            balance, link, private_key_, account_, chain);
    }
    else
    {
        block = std::make_shared<rai::RepBlock>(opcode, credit, counter,
                                                timestamp, height, account_,
                                                previous, balance, link, chain);
    }

    previous_ = block;
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::RepBlockBuilder::Credit(std::shared_ptr<rai::Block>& block,
                                            uint16_t credit_inc, uint64_t now)
{
    if (!previous_)
    {
        return rai::ErrorCode::BLOCK_OPCODE;
    }

    if (previous_->Type() != rai::BlockType::REP_BLOCK)
    {
        return rai::ErrorCode::BLOCK_TYPE;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::CREDIT;
    uint16_t credit = previous_->Credit() + credit_inc;
    if (credit <= previous_->Credit())
    {
        return rai::ErrorCode::BLOCK_CREDIT;
    }
    uint64_t timestamp =
        now > previous_->Timestamp() ? now : previous_->Timestamp();
    if (timestamp > now + 60)
    {
        return rai::ErrorCode::BLOCK_TIMESTAMP;
    }

    uint32_t counter = rai::SameDay(timestamp, previous_->Timestamp())
                           ? previous_->Counter() + 1
                           : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        return rai::ErrorCode::BLOCK_COUNTER;
    }

    uint64_t height = previous_->Height() + 1;
    rai::BlockHash previous = previous_->Hash();

    rai::Amount cost = rai::CreditPrice(timestamp) * credit_inc;
    if (cost > previous_->Balance())
    {
        return rai::ErrorCode::ACCOUNT_ACTION_BALANCE;
    }
    rai::Amount balance = previous_->Balance() - cost;

    rai::uint256_union link(0);

    if (has_key_)
    {
        block = std::make_shared<rai::RepBlock>(
            opcode, credit, counter, timestamp, height, account_, previous,
            balance, link, private_key_, account_);
    }
    else
    {
        block = std::make_shared<rai::RepBlock>(opcode, credit, counter,
                                                timestamp, height, account_,
                                                previous, balance, link);
    }

    previous_ = block;
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::RepBlockBuilder::Receive(
    std::shared_ptr<rai::Block>& block, const rai::BlockHash& source,
    const rai::Amount& amount, uint64_t sent_at, uint64_t now)
{
    if (amount.IsZero())
    {
        return rai::ErrorCode::RECEIVABLE_AMOUNT;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::RECEIVE;
    uint16_t credit = 0;
    uint64_t timestamp = 0;
    uint32_t counter = 0;
    uint64_t height = 0;
    rai::BlockHash previous;
    rai::Amount balance;
    rai::uint256_union link;

    if (!previous_)
    {
        credit = 1;
        timestamp = now >= sent_at ? now : sent_at;
        if (timestamp > now + 60)
        {
            return rai::ErrorCode::BLOCK_TIMESTAMP;
        }
        counter = 1;
        height = 0;
        previous = 0;
        if (amount < rai::CreditPrice(timestamp))
        {
            return rai::ErrorCode::RECEIVABLE_AMOUNT;
        }
        balance = amount - rai::CreditPrice(timestamp);
        link = source;
    }
    else
    {
        if (previous_->Type() != rai::BlockType::REP_BLOCK)
        {
            return rai::ErrorCode::BLOCK_TYPE;
        }

        credit = previous_->Credit();
        timestamp =
            now > previous_->Timestamp() ? now : previous_->Timestamp();
        if (timestamp < sent_at)
        {
            timestamp = sent_at;
        }
        if (timestamp > now + 60)
        {
            return rai::ErrorCode::BLOCK_TIMESTAMP;
        }

        counter = rai::SameDay(timestamp, previous_->Timestamp())
                      ? previous_->Counter() + 1
                      : 1;
        if (counter
            > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
        {
            return rai::ErrorCode::BLOCK_COUNTER;
        }
        height = previous_->Height() + 1;
        previous = previous_->Hash();
        balance = previous_->Balance() + amount;
        if (balance < amount)
        {
            return rai::ErrorCode::RECEIVABLE_AMOUNT;
        }
        link = source;
    }

    if (has_key_)
    {
        block = std::make_shared<rai::RepBlock>(
            opcode, credit, counter, timestamp, height, account_, previous,
            balance, link, private_key_, account_);
    }
    else
    {
        block = std::make_shared<rai::RepBlock>(opcode, credit, counter,
                                                timestamp, height, account_,
                                                previous, balance, link);
    }

    previous_ = block;
    return rai::ErrorCode::SUCCESS;
}

std::unique_ptr<rai::Block> rai::DeserializeBlockJson(
    rai::ErrorCode& error_code, const rai::Ptree& ptree)
{
    std::unique_ptr<rai::Block> result;

    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_TYPE;
        std::string type = ptree.get<std::string>("type");
        if (type == rai::BlockTypeToString(rai::BlockType::TX_BLOCK))
        {
            std::unique_ptr<rai::TxBlock> ptr(
                new rai::TxBlock(error_code, ptree));
            if (rai::ErrorCode::SUCCESS == error_code)
            {
                result = std::move(ptr);
            }
        }
        else if (type == rai::BlockTypeToString(rai::BlockType::REP_BLOCK))
        {
            std::unique_ptr<rai::RepBlock> ptr(
                new rai::RepBlock(error_code, ptree));
            if (rai::ErrorCode::SUCCESS == error_code)
            {
                result = std::move(ptr);
            }
        }
        else if (type == rai::BlockTypeToString(rai::BlockType::AD_BLOCK))
        {
            std::unique_ptr<rai::AdBlock> ptr(
                new rai::AdBlock(error_code, ptree));
            if (rai::ErrorCode::SUCCESS == error_code)
            {
                result = std::move(ptr);
            }
        }
        else
        {
            return result;
        }
    }
    catch (...)
    {
    }

    return result;
}

std::unique_ptr<rai::Block> rai::DeserializeBlock(rai::ErrorCode& error_code,
                                                  rai::Stream& stream)
{
    std::unique_ptr<rai::Block> result =
        rai::DeserializeBlockUnverify(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS || result == nullptr)
    {
        return nullptr;
    }

    bool error = result->CheckSignature();
    if (error)
    {
        error_code = rai::ErrorCode::SIGNATURE;
        return nullptr;
    }

    return result;
}

std::unique_ptr<rai::Block> rai::DeserializeBlockUnverify(
    rai::ErrorCode& error_code, rai::Stream& stream)
{
    std::unique_ptr<rai::Block> result(nullptr);

    rai::BlockType type;
    bool error = rai::Read(stream, type);
    if (error)
    {
        error_code = rai::ErrorCode::STREAM;
        return result;
    }

    switch (type)
    {
        case rai::BlockType::TX_BLOCK:
        {
            std::unique_ptr<rai::TxBlock> ptr(
                new rai::TxBlock(error_code, stream));
            if (rai::ErrorCode::SUCCESS == error_code)
            {
                result = std::move(ptr);
            }
            break;
        }
        case rai::BlockType::REP_BLOCK:
        {
            std::unique_ptr<rai::RepBlock> ptr(
                new rai::RepBlock(error_code, stream));
            if (rai::ErrorCode::SUCCESS == error_code)
            {
                result = std::move(ptr);
            }
            break;
        }
        case rai::BlockType::AD_BLOCK:
        {
            std::unique_ptr<rai::AdBlock> ptr(
                new rai::AdBlock(error_code, stream));
            if (rai::ErrorCode::SUCCESS == error_code)
            {
                result = std::move(ptr);
            }
            break;
        }
        default:
        {
            error_code = rai::ErrorCode::BLOCK_TYPE;
        }
    }

    return result;
}

#include <rai/common/blocks.hpp>

#include <algorithm>
#include <assert.h>
#include <sstream>
#include <string>
#include <vector>

#include <boost/endian/conversion.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <rai/common/parameters.hpp>

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

    if (str == "receive")
    {
        return rai::BlockOpcode::RECEIVE;
    }

    if (str == "change")
    {
        return rai::BlockOpcode::CHANGE;
    }

    if (str == "credit")
    {
        return rai::BlockOpcode::CREDIT;
    }

    if (str == "reward")
    {
        return rai::BlockOpcode::REWARD;
    }

    if (str == "destroy")
    {
        return rai::BlockOpcode::DESTROY;
    }

    return rai::BlockOpcode::INVALID;
}

rai::Block::Block() : signature_checked_(false), signature_error_(false)
{
}

rai::BlockHash rai::Block::Hash() const
{
    int ret;
    rai::uint256_union result;
    blake2b_state hash;

    ret = blake2b_init(&hash, sizeof(result.bytes));
    assert(0 == ret);

    Hash(hash);

    ret = blake2b_final(&hash, result.bytes.data(), sizeof(result.bytes));
    assert(0 == ret);
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

rai::TxBlock::TxBlock(
    rai::BlockOpcode opcode, uint16_t credit, uint32_t counter,
    uint64_t timestamp, uint64_t height, const rai::Account& account,
    const rai::BlockHash& previous, const rai::Account& representative,
    const rai::Amount& balance, const rai::uint256_union& link,
    uint32_t note_length, const std::vector<uint8_t>& note,
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
      note_length_(note_length),
      note_(note),
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

std::string rai::NoteTypeToString(rai::NoteType type)
{
    std::string result;

    switch (type)
    {
        case rai::NoteType::INVALID:
        {
            result = "invalid";
            break;
        }
        case rai::NoteType::TEXT:
        {
            result = "text";
            break;
        }
        default:
        {
            result = std::to_string(static_cast<uint8_t>(type));
            break;
        }
    }

    return result;
}

rai::NoteType rai::StringToNoteType(const std::string& str)
{
    if ("text" == str)
    {
        return rai::NoteType::TEXT;
    }

    return rai::NoteType::INVALID;
}

std::string rai::NoteEncodeToString(rai::NoteEncode encode)
{
    std::string result;

    switch (encode)
    {
        case rai::NoteEncode::INVALID:
        {
            result = "invalid";
            break;
        }
        case rai::NoteEncode::UTF8:
        {
            result = "utf8";
            break;
        }
        default:
        {
            result = std::to_string(static_cast<uint8_t>(encode));
            break;
        }
    }

    return result;
}

rai::NoteEncode rai::StringToNoteEncode(const std::string& str)
{
    if ("utf8" == str)
    {
        return rai::NoteEncode::UTF8;
    }

    return rai::NoteEncode::INVALID;
}

rai::Ptree rai::NoteDataToPtree(const std::vector<uint8_t>& data)
{
    boost::property_tree::ptree tree;
    const uint8_t* ptr = data.data();
    size_t size        = data.size();

    if (size < 1)
    {
        return tree;
    }
    rai::NoteType type = static_cast<rai::NoteType>(ptr[0]);
    tree.put("type", NoteTypeToString(type));

    if (size < 2)
    {
        return tree;
    }
    rai::NoteEncode encode = static_cast<rai::NoteEncode>(ptr[1]);
    tree.put("encode", NoteEncodeToString(encode));

    if (size < 3)
    {
        return tree;
    }

    if ((rai::NoteType::TEXT == type) && (rai::NoteEncode::UTF8 == encode))
    {
        tree.put("data",
                 std::string(reinterpret_cast<const char*>(&ptr[2]), size - 2));
    }
    else
    {
        tree.put("data", rai::BytesToHex(&ptr[2], size - 2));
    }

    return tree;
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
           && (note_length_ == other.note_length_) && (note_ == other.note_)
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
        rai::Write(stream, note_length_);
        rai::Write(stream, note_);
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
    rai::Write(stream, note_length_);
    rai::Write(stream, note_);
    rai::Write(stream, signature_.bytes);
}

rai::ErrorCode rai::TxBlock::Deserialize(rai::Stream& stream)
{
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

    error = rai::Read(stream, note_length_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    error = rai::TxBlock::CheckNoteLength(note_length_);
    IF_ERROR_RETURN(error, rai::ErrorCode::NOTE_LENGTH);

    error = rai::Read(stream, note_length_, note_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, signature_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = CheckSignature_();
    IF_ERROR_RETURN(error, rai::ErrorCode::SIGNATURE);

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
    ptree.put("note_length", std::to_string(note_length_));
    ptree.add_child("note", rai::NoteDataToPtree(note_));
    ptree.put("signature", signature_.StringHex());
}

rai::ErrorCode rai::TxBlock::DeserializeJson(const rai::Ptree& ptree)
{
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

        error_code = rai::ErrorCode::JSON_BLOCK_NOTE_LENGTH;
        std::string note_length = ptree.get<std::string>("note_length");
        error = rai::StringToUint(note_length, note_length_);
        IF_ERROR_RETURN(error, error_code);
        error = rai::TxBlock::CheckNoteLength(note_length_);
        IF_ERROR_RETURN(error, rai::ErrorCode::NOTE_LENGTH);
        if ((note_length_ > 0) && (note_length_ <= 2))
        {
            return error_code;
        }

        if (note_length_ > 2)
        {
            error_code = rai::ErrorCode::JSON_BLOCK_NOTE_TYPE;
            const rai::Ptree& note  = ptree.get_child("note");
            std::string note_type = note.get<std::string>("type");
            rai::NoteType note_type_ = rai::StringToNoteType(note_type);
            if (rai::NoteType::INVALID == note_type_)
            {
                return error_code;
            }
            note_.push_back(static_cast<uint8_t>(note_type_));

            error_code = rai::ErrorCode::JSON_BLOCK_NOTE_ENCODE;
            std::string note_encode = note.get<std::string>("encode");
            rai::NoteEncode note_encode_ = rai::StringToNoteEncode(note_encode);
            if (rai::NoteEncode::INVALID == note_encode_)
            {
                return error_code;
            }
            note_.push_back(static_cast<uint8_t>(note_encode_));

            error_code = rai::ErrorCode::JSON_BLOCK_NOTE_DATA;
            std::string note_data = note.get<std::string>("data");
            if ((note_data.size() + 2) != note_length_)
            {
                return error_code;
            }
            note_.insert(note_.end(), note_data.begin(), note_data.end());
        }

        error_code = rai::ErrorCode::JSON_BLOCK_SIGNATURE;
        std::string signature = ptree.get<std::string>("signature");
        error = signature_.DecodeHex(signature);
        IF_ERROR_RETURN(error, error_code);

        error =  CheckSignature_();
        IF_ERROR_RETURN(error, rai::ErrorCode::SIGNATURE);
    }
    catch (const std::exception&)
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


bool rai::TxBlock::CheckOpcode(rai::BlockOpcode opcode)
{
    std::vector<rai::BlockOpcode> opcodes = {
        rai::BlockOpcode::SEND, rai::BlockOpcode::RECEIVE,
        rai::BlockOpcode::CHANGE, rai::BlockOpcode::CREDIT};

    return std::find(opcodes.begin(), opcodes.end(), opcode) == opcodes.end();
}

bool rai::TxBlock::CheckNoteLength(uint32_t length)
{
    return length > rai::TxBlock::MaxNoteLength();
}

uint32_t rai::TxBlock::MaxNoteLength()
{
    return 130; // 128 bytes raw data
}

rai::RepBlock::RepBlock(rai::BlockOpcode opcode, uint16_t credit,
                        uint32_t counter, uint64_t timestamp, uint64_t height,
                        const rai::Account& account,
                        const rai::BlockHash& previous,
                        const rai::Amount& balance,
                        const rai::uint256_union& link,
                        const rai::RawKey& private_key,
                        const rai::PublicKey& public_key)
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
           && (signature_ == other.signature_);
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
    rai::Write(stream, signature_.bytes);
}

rai::ErrorCode rai::RepBlock::Deserialize(rai::Stream& stream)
{
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

    error = rai::Read(stream, signature_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error =  CheckSignature_();
    IF_ERROR_RETURN(error, rai::ErrorCode::SIGNATURE);

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
    ptree.put("signature", signature_.StringHex());
}

rai::ErrorCode rai::RepBlock::DeserializeJson(const rai::Ptree& ptree)
{
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

        error_code = rai::ErrorCode::JSON_BLOCK_SIGNATURE;
        std::string signature = ptree.get<std::string>("signature");
        error = signature_.DecodeHex(signature);
        IF_ERROR_RETURN(error, error_code);

        error =  CheckSignature_();
        IF_ERROR_RETURN(error, rai::ErrorCode::SIGNATURE);
    }
    catch (const std::exception&)
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

bool rai::RepBlock::CheckOpcode(rai::BlockOpcode opcode)
{
    std::vector<rai::BlockOpcode> opcodes = {
        rai::BlockOpcode::SEND, rai::BlockOpcode::RECEIVE,
        rai::BlockOpcode::REWARD, rai::BlockOpcode::CREDIT};

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

    error = CheckSignature_();
    IF_ERROR_RETURN(error, rai::ErrorCode::SIGNATURE);

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
    catch (const std::exception&)
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

bool rai::AdBlock::CheckOpcode(rai::BlockOpcode opcode)
{
    std::vector<rai::BlockOpcode> opcodes = {rai::BlockOpcode::RECEIVE,
                                             rai::BlockOpcode::CHANGE,
                                             rai::BlockOpcode::DESTROY};

    return std::find(opcodes.begin(), opcodes.end(), opcode) == opcodes.end();
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
    catch (const std::runtime_error&)
    {
    }

    return result;
}

std::unique_ptr<rai::Block> rai::DeserializeBlock(rai::ErrorCode& error_code,
                                                  rai::Stream& stream)
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

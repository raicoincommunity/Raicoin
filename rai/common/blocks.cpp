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
    blake2b_update(&state, &type_, sizeof(type_));
    blake2b_update(&state, &opcode_, sizeof(opcode_));

    uint16_t credit = boost::endian::native_to_big(credit_);
    blake2b_update(&state, &credit, sizeof(credit));

    uint32_t counter = boost::endian::native_to_big(counter_);
    blake2b_update(&state, &counter, sizeof(counter));

    uint64_t timestamp = boost::endian::native_to_big(timestamp_);
    blake2b_update(&state, &timestamp, sizeof(timestamp));

    uint64_t height = boost::endian::native_to_big(height_);
    blake2b_update(&state, &height, sizeof(height));

    blake2b_update(&state, account_.bytes.data(), account_.bytes.size());
    blake2b_update(&state, previous_.bytes.data(), previous_.bytes.size());
    blake2b_update(&state, representative_.bytes.data(),
                  representative_.bytes.size());
    blake2b_update(&state, balance_.bytes.data(), balance_.bytes.size());
    blake2b_update(&state, link_.bytes.data(), link_.bytes.size());

    uint32_t length = boost::endian::native_to_big(note_length_);
    blake2b_update(&state, &length, sizeof(length));
    blake2b_update(&state, note_.data(), note_.size());
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

    ptree.put("type", "transaction");
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
        if (type != "transaction")
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

bool rai::TxBlock::CheckType(rai::BlockType type)
{
    return type != rai::BlockType::TX_BLOCK;
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
    blake2b_update(&state, &type_, sizeof(type_));
    blake2b_update(&state, &opcode_, sizeof(opcode_));

    uint16_t credit = boost::endian::native_to_big(credit_);
    blake2b_update(&state, &credit, sizeof(credit));

    uint32_t counter = boost::endian::native_to_big(counter_);
    blake2b_update(&state, &counter, sizeof(counter));

    uint64_t timestamp = boost::endian::native_to_big(timestamp_);
    blake2b_update(&state, &timestamp, sizeof(timestamp));

    uint64_t height = boost::endian::native_to_big(height_);
    blake2b_update(&state, &height, sizeof(height));

    blake2b_update(&state, account_.bytes.data(), account_.bytes.size());
    blake2b_update(&state, previous_.bytes.data(), previous_.bytes.size());
    blake2b_update(&state, balance_.bytes.data(), balance_.bytes.size());
    blake2b_update(&state, link_.bytes.data(), link_.bytes.size());
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
    ptree.put("type", "representative");
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
        if (type != "representative")
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
        if (counter_ == 0)
        {
            return rai::ErrorCode::BLOCK_COUNTER;
        }

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

bool rai::RepBlock::CheckType(rai::BlockType type)
{
    return type != rai::BlockType::REP_BLOCK;
}

bool rai::RepBlock::CheckOpcode(rai::BlockOpcode opcode)
{
    std::vector<rai::BlockOpcode> opcodes = {
        rai::BlockOpcode::SEND, rai::BlockOpcode::RECEIVE,
        rai::BlockOpcode::REWARD, rai::BlockOpcode::CREDIT};

    return std::find(opcodes.begin(), opcodes.end(), opcode) == opcodes.end();
}

#if 0
rai::AdminBlock::AdminBlock(rai::BlockOpcode opcode, uint16_t credit,
                        uint32_t counter, uint64_t timestamp, uint64_t height,
                        const rai::Account& account,
                        const rai::BlockHash& previous,
                        const rai::Amount& price,
                        uint64_t begin_time, uint64_t end_time,
                        const rai::RawKey& private_key,
                        const rai::PublicKey& public_key)
    : type_(rai::BlockType::ADMIN_BLOCK),
      opcode_(opcode),
      credit_(credit),
      counter_(counter),
      timestamp_(timestamp),
      height_(height),
      account_(account),
      previous_(previous),
      price_(price),
      begin_time_(begin_time),
      end_time_(end_time),
      signature_(rai::SignMessage(private_key, public_key, Hash()))
{
}


rai::AdminBlock::AdminBlock(rai::ErrorCode& error_code, rai::Stream& stream)
{
    error_code = Deserialize(stream);
}

rai::AdminBlock::AdminBlock(rai::ErrorCode& error_code, const rai::Ptree& ptree)
{
    error_code = DeserializeJson(ptree);
}

bool rai::AdminBlock::operator==(const rai::Block& other) const
{
    return BlocksEqual(*this, other);
}

bool rai::AdminBlock::operator==(const rai::AdminBlock& other) const
{
    // type_ and opcode_ compared in BlocksEqual
    return (credit_ == other.credit_) && (counter_ == other.counter_)
           && (timestamp_ == other.timestamp_) && (height_ == other.height_)
           && (account_ == other.account_) && (previous_ == other.previous_)
           && (price_ == other.price_) && (begin_time_ == other.begin_time_)
           && (end_time_ == other.end_time_)
           && (signature_ == other.signature_);
}

void rai::AdminBlock::Hash(blake2b_state& state) const
{
    blake2b_update(&state, &type_, sizeof(type_));
    blake2b_update(&state, &opcode_, sizeof(opcode_));

    uint16_t credit = boost::endian::native_to_big(credit_);
    blake2b_update(&state, &credit, sizeof(credit));

    uint32_t counter = boost::endian::native_to_big(counter_);
    blake2b_update(&state, &counter, sizeof(counter));

    uint64_t timestamp = boost::endian::native_to_big(timestamp_);
    blake2b_update(&state, &timestamp, sizeof(timestamp));

    uint64_t height = boost::endian::native_to_big(height_);
    blake2b_update(&state, &height, sizeof(height));

    blake2b_update(&state, account_.bytes.data(), account_.bytes.size());
    blake2b_update(&state, previous_.bytes.data(), previous_.bytes.size());
    blake2b_update(&state, price_.bytes.data(), price_.bytes.size());

    uint64_t begin_time = boost::endian::native_to_big(begin_time_);
    blake2b_update(&state, &begin_time, sizeof(begin_time));

    uint64_t end_time = boost::endian::native_to_big(end_time_);
    blake2b_update(&state, &end_time, sizeof(end_time));
}

rai::BlockHash rai::AdminBlock::Previous() const
{
    return previous_;
}


void rai::AdminBlock::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, type_);
    rai::Write(stream, opcode_);
    rai::Write(stream, credit_);
    rai::Write(stream, counter_);
    rai::Write(stream, timestamp_);
    rai::Write(stream, height_);
    rai::Write(stream, account_.bytes);
    rai::Write(stream, previous_.bytes);
    rai::Write(stream, price_.bytes);
    rai::Write(stream, begin_time_);
    rai::Write(stream, end_time_);
    rai::Write(stream, signature_.bytes);
}

rai::ErrorCode rai::AdminBlock::Deserialize(rai::Stream& stream)
{
    bool error = false;
    
    type_ = rai::BlockType::ADMIN_BLOCK;

    error = rai::Read(stream, opcode_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);
    error = rai::AdminBlock::CheckOpcode(opcode_);
    IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_OPCODE);

    error =  rai::Read(stream, credit_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

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

    error = rai::Read(stream, price_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, begin_time_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, end_time_);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = rai::Read(stream, signature_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::STREAM);

    error = CheckSignature_();
    IF_ERROR_RETURN(error, rai::ErrorCode::SIGNATURE);
    
    return rai::ErrorCode::SUCCESS;
}

void rai::AdminBlock::SerializeJson(std::string& str) const
{
    rai::Ptree tree;
    tree.put("type", "admin");
    tree.put("opcode", rai::BlockOpcodeToString(opcode_));
    tree.put("credit", std::to_string(credit_));
    tree.put("counter", std::to_string(counter_));
    tree.put("timestamp", std::to_string(timestamp_));
    tree.put("height", std::to_string(height_));
    tree.put("account", account_.StringAccount());
    tree.put("previous", previous_.StringHex());
    tree.put("price", price_.StringDec());
    tree.put("begin_time", std::to_string(begin_time_));
    tree.put("end_time", std::to_string(end_time_));
    tree.put("signature", signature_.StringHex());

    std::stringstream ostream;
    boost::property_tree::write_json(ostream, tree);
    str = ostream.str();
}

rai::ErrorCode rai::AdminBlock::DeserializeJson(const rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;

    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_TYPE;
        std::string type = ptree.get<std::string>("type");
        if (type != "admin")
        {
            return rai::ErrorCode::BLOCK_TYPE;
        }
        type_ = rai::BlockType::ADMIN_BLOCK;

        error_code = rai::ErrorCode::JSON_BLOCK_OPCODE;
        std::string opcode = ptree.get<std::string>("opcode");
        opcode_ = rai::StringToBlockOpcode(opcode);
        bool error = rai::AdminBlock::CheckOpcode(opcode_);
        IF_ERROR_RETURN(error, rai::ErrorCode::BLOCK_OPCODE);

        error_code = rai::ErrorCode::JSON_BLOCK_CREDIT;
        std::string credit = ptree.get<std::string>("credit");
        error = rai::StringToUint(credit, credit_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_COUNTER;
        std::string counter = ptree.get<std::string>("counter");
        error = rai::StringToUint(counter, counter_);
        IF_ERROR_RETURN(error, error_code);

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

        error_code = rai::ErrorCode::JSON_BLOCK_PRICE;
        std::string price = ptree.get<std::string>("price");
        error = price_.DecodeDec(price);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_BEGIN_TIME;
        std::string begin_time = ptree.get<std::string>("begin_time");
        error = rai::StringToUint(begin_time, begin_time_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_END_TIME;
        std::string end_time = ptree.get<std::string>("end_time");
        error = rai::StringToUint(end_time, end_time_);
        IF_ERROR_RETURN(error, error_code);

        error_code = rai::ErrorCode::JSON_BLOCK_SIGNATURE;
        std::string signature = ptree.get<std::string>("signature");
        error = signature_.DecodeHex(signature);
        IF_ERROR_RETURN(error, error_code);

        error = CheckSignature_();
        IF_ERROR_RETURN(error, rai::ErrorCode::SIGNATURE);
    }
    catch (const std::exception&)
    {
        return error_code;
    }

    return rai::ErrorCode::SUCCESS;
}

void rai::AdminBlock::SetSignature(const rai::uint512_union& signature)
{
    signature_ = signature;
    CheckSignature_();
}

rai::Signature rai::AdminBlock::Signature() const
{
    return signature_;
}

rai::BlockType rai::AdminBlock::Type() const
{
    return type_;
}

rai::BlockOpcode rai::AdminBlock::Opcode() const
{
    return opcode_;
}

void rai::AdminBlock::Visit(rai::BlockVisitor& visitor) const
{
    visitor.AdminBlock(*this);
}

rai::Account rai::AdminBlock::Account() const
{
    return account_;
}

uint16_t rai::AdminBlock::Credit() const
{
    return credit_;
}

uint32_t rai::AdminBlock::Counter() const
{
    return counter_;
}

uint64_t rai::AdminBlock::Timestamp() const
{
    return timestamp_;
}

bool rai::AdminBlock::CheckType(rai::BlockType type)
{
    return type != rai::BlockType::ADMIN_BLOCK;
}

bool rai::AdminBlock::CheckOpcode(rai::BlockOpcode opcode)
{
    std::vector<rai::BlockOpcode> opcodes = {
        rai::BlockOpcode::PRICE,
        // TODO: CHANGE
    };

    return std::find(opcodes.begin(), opcodes.end(), opcode) == opcodes.end();
}
#endif

std::unique_ptr<rai::Block> rai::DeserializeBlockJson(
    rai::ErrorCode& error_code, const rai::Ptree& ptree)
{
    std::unique_ptr<rai::Block> result;

    try
    {
        error_code = rai::ErrorCode::JSON_BLOCK_TYPE;
        std::string type = ptree.get<std::string>("type");
        if (type == "transaction")
        {
            std::unique_ptr<rai::TxBlock> ptr(
                new rai::TxBlock(error_code, ptree));
            if (rai::ErrorCode::SUCCESS == error_code)
            {
                result = std::move(ptr);
            }
        }
        else if (type == "representative")
        {
            std::unique_ptr<rai::RepBlock> ptr(
                new rai::RepBlock(error_code, ptree));
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
    std::unique_ptr<rai::Block> result;

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
        default:
        {
            error_code = rai::ErrorCode::BLOCK_TYPE;
        }
    }

    return result;
}

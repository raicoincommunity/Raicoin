#include <rai/common/numbers.hpp>

#include <iostream>
#include <string>


#include <blake2/blake2.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <ed25519-donna/ed25519.h>

using std::string;

thread_local CryptoPP::AutoSeededRandomPool rai::random_pool;

namespace
{
char const* account_lookup = "13456789abcdefghijkmnopqrstuwxyz";
char const* account_reverse =
    "~0~1234567~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~89:;<=>?@AB~CDEFGHIJK~"
    "LMNO~~~~~";

char AccountCharEncode(uint8_t value)
{
    char result = '~';

    if (value >= strlen(account_lookup))
    {
        return result;
    }

    result = account_lookup[value];
    return result;
}

uint8_t AccountCharDecode(char value)
{
    uint8_t result = 0xff;

    if (NULL == strchr(account_lookup, value))
    {
        return result;
    }

    result = account_reverse[value - 0x30] - 0x30;
    return result;
}
}  // namespace

rai::uint128_union::uint128_union()
{
    *this = rai::uint128_t(0);
}

rai::uint128_union::uint128_union(uint64_t value)
{
    *this = rai::uint128_t(value);
}

rai::uint128_union::uint128_union(const rai::uint128_t& value)
{
    rai::uint128_t value_l(value);

    for (auto i(bytes.rbegin()), n(bytes.rend()); i != n; ++i)
    {
        *i = static_cast<uint8_t>(value_l & static_cast<uint8_t>(0xff));
        value_l >>= 8;
    }
}

bool rai::uint128_union::operator==(const rai::uint128_union& other) const
{
    return qwords == other.qwords;
}

bool rai::uint128_union::operator!=(const rai::uint128_union& other) const
{
    return !(*this == other);
}

bool rai::uint128_union::operator<(const rai::uint128_union& other) const
{
    return Number() < other.Number();
}

bool rai::uint128_union::operator>(const rai::uint128_union& other) const
{
    return Number() > other.Number();
}

bool rai::uint128_union::operator<=(const rai::uint128_union& other) const
{
    return Number() <= other.Number();
}

bool rai::uint128_union::operator>=(const rai::uint128_union& other) const
{
    return Number() >= other.Number();
}

rai::uint128_union rai::uint128_union::operator+(
    const rai::uint128_union& other) const
{
    return rai::uint128_union(Number() + other.Number());
}

rai::uint128_union rai::uint128_union::operator-(
    const rai::uint128_union& other) const
{
    return rai::uint128_union(Number() - other.Number());
}

rai::uint128_union& rai::uint128_union::operator+=(
    const rai::uint128_union& other)
{
    *this = *this + other;
    return *this;
}

rai::uint128_union& rai::uint128_union::operator-=(
    const rai::uint128_union& other)
{
    *this = *this - other;
    return *this;
}

rai::uint128_t rai::uint128_union::Number() const
{
    rai::uint128_t result;
    int shift = 0;

    for (auto i(bytes.begin()), n(bytes.end()); i != n; ++i)
    {
        result <<= shift;
        result |= *i;
        shift = 8;
    }
    return result;
}

void rai::uint128_union::Clear()
{
    qwords.fill(0);
}

bool rai::uint128_union::IsZero() const
{
    return (qwords[0] == 0) && (qwords[1] == 0);
}

void rai::uint128_union::EncodeHex(std::string& text) const
{
    std::stringstream stream;
    stream << std::hex << std::noshowbase << std::setw(32) << std::setfill('0');
    stream << Number();
    text = stream.str();
}

bool rai::uint128_union::DecodeHex(const std::string& text)
{
    if (text.size() != 32)
    {
        return true;
    }

    try
    {
        rai::uint128_t number;
        std::stringstream stream(text);

        stream << std::hex << std::noshowbase;
        stream >> number;
        if (!stream.eof())
        {
            return true;
        }
        *this = number;
        return false;
    }
    catch (std::runtime_error&)
    {
        return true;
    }
}

void rai::uint128_union::EncodeDec(std::string& text) const
{
    std::stringstream stream;
    stream << std::dec << std::noshowbase;
    stream << Number();
    text = stream.str();
}

bool rai::uint128_union::DecodeDec(const std::string& text)
{
    size_t size = text.size();
    if (text.empty() || (size > 39) || ((size > 1) && ('0' == text[0]))
        || ((size > 0) && ('-' == text[0])))
    {
        return true;
    }

    try
    {
        std::stringstream stream(text);
        boost::multiprecision::checked_uint128_t number;

        stream << std::dec << std::noshowbase;
        stream >> number;
        if (!stream.eof())
        {
            return true;
        }
        *this = rai::uint128_t(number);
        return false;
    }
    catch (std::runtime_error&)
    {
        return true;
    }
}

std::string rai::uint128_union::StringHex() const
{
    string result;
    EncodeHex(result);
    return result;
}

std::string rai::uint128_union::StringDec() const
{
    string result;
    EncodeDec(result);
    return result;
}

rai::uint256_union::uint256_union()
{
    *this = rai::uint256_t(0);
}

rai::uint256_union::uint256_union(uint64_t value)
{
    *this = rai::uint256_t(value);
}

rai::uint256_union::uint256_union(const rai::uint256_t& value)
{
    rai::uint256_t value_l(value);

    for (auto i(bytes.rbegin()), n(bytes.rend()); i != n; ++i)
    {
        *i = static_cast<uint8_t>(value_l & static_cast<uint8_t>(0xff));
        value_l >>= 8;
    }
}

bool rai::uint256_union::operator==(const rai::uint256_union& other) const
{
    return qwords == other.qwords;
}

bool rai::uint256_union::operator!=(const rai::uint256_union& other) const
{
    return !(*this == other);
}

bool rai::uint256_union::operator<(const rai::uint256_union& other) const
{
    return Number() < other.Number();
}

bool rai::uint256_union::operator>(const rai::uint256_union& other) const
{
    return Number() > other.Number();
}

bool rai::uint256_union::operator<=(const rai::uint256_union& other) const
{
    return Number() <= other.Number();
}

bool rai::uint256_union::operator>=(const rai::uint256_union& other) const
{
    return Number() >= other.Number();
}

rai::uint256_union rai::uint256_union::operator^(
    const rai::uint256_union& other) const
{
    rai::uint256_union result;
    for (size_t i = 0; i < qwords.size(); ++i)
    {
        result.qwords[i] = qwords[i] ^ other.qwords[i];
    }
    return result;
}

rai::uint256_union& rai::uint256_union::operator^=(
    const rai::uint256_union& other)
{
    for (size_t i = 0; i < qwords.size(); ++i)
    {
        qwords[i] ^= other.qwords[i];
    }
    return *this;
}

rai::uint256_t rai::uint256_union::Number() const
{
    rai::uint256_t result;
    int shift = 0;

    for (auto i = bytes.begin(), n = bytes.end(); i != n; ++i)
    {
        result <<= shift;
        result |= *i;
        shift = 8;
    }
    return result;
}

void rai::uint256_union::Clear()
{
    qwords.fill(0);
}

void rai::uint256_union::SecureClear()
{
    for (size_t i = 0; i < qwords.size(); ++i)
    {
        volatile uint64_t& qword = qwords[i];
        qword                    = 0;
    }
}

bool rai::uint256_union::IsZero() const
{
    return Number() == 0;
}

void rai::uint256_union::EncodeHex(std::string& text) const
{
    std::stringstream stream;
    stream << std::hex << std::noshowbase << std::setw(64) << std::setfill('0');
    stream << Number();
    text = stream.str();
}

bool rai::uint256_union::DecodeHex(const std::string& text)
{
    if (text.size() != 64)
    {
        return true;
    }

    try
    {
        rai::uint256_t number;
        std::stringstream stream(text);

        stream << std::hex << std::noshowbase;
        stream >> number;
        if (!stream.eof())
        {
            return true;
        }
        *this = number;
        return false;
    }
    catch (std::runtime_error&)
    {
        return true;
    }
}

void rai::uint256_union::EncodeDec(std::string& text) const
{
    std::stringstream stream;
    stream << std::dec << std::noshowbase;
    stream << Number();
    text = stream.str();
}

bool rai::uint256_union::DecodeDec(const std::string& text)
{
    size_t size = text.size();
    if (text.empty() || (size > 78) || ((size > 1) && ('0' == text[0]))
        || ((size > 0) && ('-' == text[0])))
    {
        return true;
    }

    try
    {
        std::stringstream stream(text);
        boost::multiprecision::checked_uint256_t number;

        stream << std::dec << std::noshowbase;
        stream >> number;
        if (!stream.eof())
        {
            return true;
        }
        *this = rai::uint256_t(number);
        return false;
    }
    catch (std::runtime_error&)
    {
        return true;
    }
}

// only support Little Endian
void rai::uint256_union::EncodeAccount(std::string& text) const
{
    string text_l;
    uint64_t check = 0;
    blake2b_state hash;
    rai::uint512_t number = Number();

    blake2b_init(&hash, 5);
    blake2b_update(&hash, bytes.data(), bytes.size());
    blake2b_final(&hash, reinterpret_cast<uint8_t*>(&check), 5);

    number <<= 40;
    number |= rai::uint512_t(check);

    text_l.reserve(64);
    for (int i = 0; i < 60; ++i)
    {
        uint8_t r = static_cast<uint8_t>(number & static_cast<uint8_t>(0x1f));
        number >>= 5;
        text_l.push_back(AccountCharEncode(r));
    }
    text_l.append("_iar");  // rai_
    std::reverse(text_l.begin(), text_l.end());

    text = text_l;
}

// only support Little Endian
bool rai::uint256_union::DecodeAccount(const std::string& text)
{
    string prefix = "rai_";
    if ((text.size() != 64) || (text.find(prefix) != 0))
    {
        return true;
    }

    auto i = text.begin() + prefix.size();
    if ((*i != '1') && (*i != '3'))
    {
        return true;
    }

    rai::uint512_t number;
    for (; i != text.end(); ++i)
    {
        uint8_t byte = AccountCharDecode(*i);
        if (0xff == byte)
        {
            return true;
        }
        number <<= 5;
        number += byte;
    }

    uint64_t check =
        static_cast<uint64_t>(number & static_cast<uint64_t>(0xffffffffff));
    uint64_t validation  = 0;
    rai::uint256_union u = (number >> 40).convert_to<rai::uint256_t>();
    blake2b_state hash;
    blake2b_init(&hash, 5);
    blake2b_update(&hash, u.bytes.data(), u.bytes.size());
    blake2b_final(&hash, reinterpret_cast<uint8_t*>(&validation), 5);
    if (check != validation)
    {
        return true;
    }

    *this = u;
    return false;
}

std::string rai::uint256_union::StringHex() const
{
    string result;
    EncodeHex(result);
    return result;
}

std::string rai::uint256_union::StringDec() const
{
    string result;
    EncodeDec(result);
    return result;
}

std::string rai::uint256_union::StringAccount() const
{
    string result;
    EncodeAccount(result);
    return result;
}

rai::RawKey::~RawKey()
{
    data_.SecureClear();
}

bool rai::RawKey::operator==(const rai::RawKey& other_a) const
{
    return data_ == other_a.data_;
}

bool rai::RawKey::operator!=(const rai::RawKey& other_a) const
{
    return !(*this == other_a);
}

void rai::RawKey::Decrypt(const rai::uint256_union& ciphertext,
                          const rai::RawKey& key, const rai::uint128_union& iv)
{
    CryptoPP::AES::Encryption alg(key.data_.bytes.data(),
                                  key.data_.bytes.size());
    CryptoPP::CTR_Mode_ExternalCipher::Decryption dec(alg, iv.bytes.data());
    dec.ProcessData(data_.bytes.data(), ciphertext.bytes.data(),
                    ciphertext.bytes.size());
}

rai::uint512_union::uint512_union()
{
    *this = rai::uint512_t(0);
}

rai::uint512_union::uint512_union(const rai::uint512_t& value)
{
    rai::uint512_t value_l(value);

    for (auto i = bytes.rbegin(), n = bytes.rend(); i != n; ++i)
    {
        *i = static_cast<uint8_t>(value_l & static_cast<uint8_t>(0xff));
        value_l >>= 8;
    }
}

bool rai::uint512_union::operator==(const rai::uint512_union& other) const
{
    return qwords == other.qwords;
}

bool rai::uint512_union::operator!=(const rai::uint512_union& other) const
{
    return !(*this == other);
}

rai::uint512_t rai::uint512_union::Number() const
{
    rai::uint512_t result;
    int shift = 0;

    for (auto i = bytes.begin(), n = bytes.end(); i != n; ++i)
    {
        result <<= shift;
        result |= *i;
        shift = 8;
    }
    return result;
}

void rai::uint512_union::Clear()
{
    qwords.fill(0);
}

bool rai::uint512_union::IsZero() const
{
    return Number() == 0;
}

void rai::uint512_union::EncodeHex(std::string& text) const
{
    std::stringstream stream;
    stream << std::hex << std::noshowbase << std::setw(128)
           << std::setfill('0');
    stream << Number();
    text = stream.str();
}

bool rai::uint512_union::DecodeHex(const std::string& text)
{
    if (text.size() != 128)
    {
        return true;
    }

    try
    {
        rai::uint512_t number;
        std::stringstream stream(text);

        stream << std::hex << std::noshowbase;
        stream >> number;
        if (!stream.eof())
        {
            return true;
        }
        *this = number;
        return false;
    }
    catch (std::runtime_error&)
    {
        return true;
    }
}

std::string rai::uint512_union::StringHex() const
{
    string result;
    EncodeHex(result);
    return result;
}

rai::uint512_union rai::SignMessage(const rai::RawKey& private_key,
                                    const rai::PublicKey& public_key,
                                    const rai::uint256_union& message)
{
    rai::uint512_union result;

    ed25519_sign(message.bytes.data(), message.bytes.size(),
                 private_key.data_.bytes.data(), public_key.bytes.data(),
                 result.bytes.data());
    return result;
}

bool rai::ValidateMessage(const rai::PublicKey& public_key,
                          const rai::uint256_union& message,
                          const rai::uint512_union& signature)
{
    int ret = 0;

    ret = ed25519_sign_open(message.bytes.data(), message.bytes.size(),
                            public_key.bytes.data(), signature.bytes.data());

    return ret != 0;
}

rai::PublicKey rai::GeneratePublicKey(const rai::PrivateKey& private_key)
{
    rai::PublicKey result;
    ed25519_publickey(private_key.bytes.data(), result.bytes.data());
    return result;
}

#include <rai/common/numbers.hpp>
#include <rai/common/util.hpp>

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

size_t MaxFracLen(rai::uint128_t scale)
{
    if (scale == rai::RAI)
    {
        return 9;
    }
    else if (scale == rai::mRAI)
    {
        return 6;
    }
    else if (scale == rai::uRAI)
    {
        return 3;
    }
    else
    {
        return 0;
    }
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

void rai::uint128_union::EncodeBalance(const rai::uint128_t& scale,
                                       std::string& balance) const
{
    if (scale != rai::RAI && scale != rai::mRAI && scale != rai::uRAI)
    {
        return;
    }

    std::stringstream stream;
    rai::uint128_t int_part = Number() / scale;
    rai::uint128_t frac_part = Number() % scale;
    stream << int_part;
    if (frac_part > 0)
    {
        stream << ".";
        while (frac_part > 0)
        {
            frac_part *= 10;
            stream << frac_part / scale;
            frac_part %= scale;
        }
    }
    balance = stream.str();
}

bool rai::uint128_union::DecodeBalance(const rai::uint128_t& scale,
                                       const std::string& balance)
{
    size_t max_frac_len = MaxFracLen(scale);
    if (max_frac_len == 0)
    {
        return true;
    }

    std::string str = balance;
    if (str.empty() || str.size() > 40)
    {
        return true;
    }

    if (str.find_first_not_of("0123456789.") != std::string::npos)
    {
        return true;
    }

    if (rai::StringCount(str, '.') > 1 || *str.begin() == '.'
        || *str.rbegin() == '.')
    {
        return true;
    }

    if (str.find('.') != std::string::npos)
    {
        rai::StringRightTrim(str, "0");
        rai::StringRightTrim(str, ".");
    }
    std::string check = str;

    size_t frac_len = 0;
    if (str.find('.') != std::string::npos)
    {
        frac_len = str.size() - str.find('.') - 1;
        str.erase(str.find('.'), 1);
    }
    if (frac_len > max_frac_len)
    {
        return true;
    }
    str.append(max_frac_len - frac_len, '0');
    rai::StringLeftTrim(str, "0");

    bool error = DecodeDec(str);
    if (error)
    {
        return true;
    }

    if (check != StringBalance(scale))
    {
        return true;
    }

    return false;
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

std::string rai::uint128_union::StringBalance(const rai::uint128_t& scale) const
{
    string result;
    EncodeBalance(scale, result);
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

rai::uint256_union rai::uint256_union::operator+(
    const rai::uint256_union& other) const
{
    return rai::uint256_union(Number() + other.Number());
}

rai::uint256_union rai::uint256_union::operator-(
    const rai::uint256_union& other) const
{
    return rai::uint256_union(Number() - other.Number());
}

rai::uint256_union& rai::uint256_union::operator+=(
    const rai::uint256_union& other)
{
    *this = *this + other;
    return *this;
}

rai::uint256_union& rai::uint256_union::operator-=(
    const rai::uint256_union& other)
{
    *this = *this - other;
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

void rai::uint256_union::Encrypt(const rai::RawKey& cleartext,
                                 const rai::RawKey& key,
                                 const rai::uint128_union& iv)
{
    CryptoPP::AES::Encryption alg(key.data_.bytes.data(),
                                  key.data_.bytes.size());
    CryptoPP::CTR_Mode_ExternalCipher::Encryption enc(alg, iv.bytes.data());
    enc.ProcessData(bytes.data(), cleartext.data_.bytes.data(),
                    cleartext.data_.bytes.size());
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

void rai::uint256_union::Random()
{
    rai::random_pool.GenerateBlock(bytes.data(), bytes.size());
}

rai::AccountParser::AccountParser(const std::string& str)
{
    error_ = true;

    if (str.empty() || StringContain(str, ' ') || StringContain(str, '\r')
        || StringContain(str, '\t') || StringContain(str, '\n'))
    {
        return;
    }

    do
    {
        if (str.find("rai_") == 0)
        {
            size_t size = str.size();
            if (size < 64)
            {
                return;
            }

            bool error = account_.DecodeAccount(str.substr(0, 64));
            IF_ERROR_RETURN_VOID(error);
            if (size == 64)
            {
                break;
            }

            if (str[64] != '_' || size == 65)
            {
                return;
            }
            sub_account_ = str.substr(65);
        }
        else
        {
            // TODO: alias
            return;
        }
    } while (0);

    error_ = false;
}

rai::Account rai::AccountParser::Account() const
{
    return account_;
}

std::string rai::AccountParser::SubAccount() const
{
    return sub_account_;
}

bool rai::AccountParser::Error() const
{
    return error_;
}

bool rai::AccountHeight::operator<(const rai::AccountHeight& other) const
{
    if (account_ < other.account_)
    {
        return true;
    }
    if (height_ < other.height_)
    {
        return true;
    }
    return false;
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

rai::uint512_union::uint512_union(uint64_t value)
{
    *this = rai::uint512_t(value);
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

rai::uint512_union::uint512_union(const std::vector<uint8_t>& vec)
{
    Clear();

    auto i = bytes.begin();
    auto j = vec.begin();
    for (; i != bytes.end() && j != vec.end(); ++i, ++j)
    {
        *i = *j;
    }
}

rai::uint512_union::uint512_union(uint8_t byte, size_t offset)
{
    Clear();
    if (offset < 0)
    {
        offset = 0;
    }
    else if (offset >= bytes.size())
    {
        offset = bytes.size() - 1;
    }
    bytes[offset] = byte;
}

rai::uint512_union::uint512_union(const std::string& str, bool to_lower)
{
    Clear();

    auto i = bytes.begin();
    auto j = str.begin();
    for (; i != bytes.end() && j != str.end(); ++i, ++j)
    {
        *i = *j;
    }

    if (to_lower)
    {
        ToLower();
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

rai::uint512_union rai::uint512_union::operator+(
    const rai::uint512_union& other) const
{
    return rai::uint512_union(Number() + other.Number());
}

rai::uint512_union& rai::uint512_union::operator+=(
    const rai::uint512_union& other)
{
    *this = *this + other;
    return *this;
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

void rai::uint512_union::ToLower()
{
    for (auto i = bytes.begin(), n = bytes.end(); i != n; ++i)
    {
        if (*i >= 'A' && *i <= 'Z')
        {
            *i += 32;
        }
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

uint64_t rai::Random(uint64_t min, uint64_t max)
{
    if (min > max)
    {
        throw std::invalid_argument("Invalid ramdom parameters");
    }
    if (min == max)
    {
        return min;
    }

    uint64_t range = max - min;
    uint32_t low = 0;
    uint32_t high = 64;
    while (high - low > 1)
    {
        uint32_t middle = (low + high) / 2;
        if (range >> middle)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    uint32_t bits = high;

    uint64_t value = 0;
    do
    {
        rai::random_pool.GenerateBlock((uint8_t*)&value, sizeof(value));
        if (bits < 64)
        {
            value &= (uint64_t(1) << bits) - 1;
        }
    } while (value > range);

    return value + min;
}

#pragma once
#include <boost/multiprecision/cpp_int.hpp>
#include <cryptopp/osrng.h>

namespace rai
{
extern thread_local CryptoPP::AutoSeededRandomPool random_pool;

using uint128_t = boost::multiprecision::uint128_t;
using uint256_t = boost::multiprecision::uint256_t;
using uint512_t = boost::multiprecision::uint512_t;

const rai::uint128_t Colin = rai::uint128_t("1");
const rai::uint128_t uRAI = 1000 * Colin;      // 10^3 Colin
const rai::uint128_t mRAI = 1000000 * Colin;   // 10^6 Colin
const rai::uint128_t RAI = 1000000000 * Colin;  // 10^9 Colin

union uint128_union
{
public:
    uint128_union();
    uint128_union(const rai::uint128_union&) = default;
    uint128_union(uint64_t);
    uint128_union(const rai::uint128_t&);
    bool operator==(const rai::uint128_union&) const;
    bool operator!=(const rai::uint128_union&) const;
    bool operator<(const rai::uint128_union&) const;
    bool operator>(const rai::uint128_union&) const;
    bool operator<=(const rai::uint128_union&) const;
    bool operator>=(const rai::uint128_union&) const;
    rai::uint128_union operator+(const rai::uint128_union&) const;
    rai::uint128_union operator-(const rai::uint128_union&) const;
    rai::uint128_union& operator+=(const rai::uint128_union&);
    rai::uint128_union& operator-=(const rai::uint128_union&);
    rai::uint128_t Number() const;
    void Clear();
    bool IsZero() const;
    void EncodeHex(std::string&) const;
    bool DecodeHex(const std::string&);
    void EncodeDec(std::string&) const;
    bool DecodeDec(const std::string&);
    void EncodeBalance(const rai::uint128_t&, std::string&) const;
    bool DecodeBalance(const rai::uint128_t&, const std::string&);
    std::string StringHex() const;
    std::string StringDec() const;
    std::string StringBalance(const rai::uint128_t&) const;

    std::array<char, 16> chars;
    std::array<uint8_t, 16> bytes;
    std::array<uint32_t, 4> dwords;
    std::array<uint64_t, 2> qwords;
};
// Balances are 128 bit.
using Amount = uint128_union;

class RawKey;
union uint256_union
{
    uint256_union();
    uint256_union(const rai::uint256_union&) = default;
    uint256_union(uint64_t);
    uint256_union(const rai::uint256_t&);
    bool operator==(const rai::uint256_union&) const;
    bool operator!=(const rai::uint256_union&) const;
    bool operator<(const rai::uint256_union&) const;
    bool operator>(const rai::uint256_union&) const;
    bool operator<=(const rai::uint256_union&) const;
    bool operator>=(const rai::uint256_union&) const;
    rai::uint256_union operator^(const rai::uint256_union&) const;
    rai::uint256_union& operator^=(const rai::uint256_union&);
    rai::uint256_union operator+(const rai::uint256_union&) const;
    rai::uint256_union operator-(const rai::uint256_union&) const;
    rai::uint256_union& operator+=(const rai::uint256_union&);
    rai::uint256_union& operator-=(const rai::uint256_union&);
    rai::uint256_t Number() const;
    void Clear();
    void SecureClear();
    void Encrypt(const rai::RawKey&, const rai::RawKey&,
                 const rai::uint128_union&);
    bool IsZero() const;
    void EncodeHex(std::string&) const;
    bool DecodeHex(const std::string&);
    void EncodeDec(std::string&) const;
    bool DecodeDec(const std::string&);
    void EncodeAccount(std::string&) const;
    bool DecodeAccount(const std::string&);
    std::string StringHex() const;
    std::string StringDec() const;
    std::string StringAccount() const;

    std::array<uint8_t, 32> bytes;
    std::array<char, 32> chars;
    std::array<uint32_t, 8> dwords;
    std::array<uint64_t, 4> qwords;
    std::array<uint128_union, 2> owords;
};
using BlockHash  = uint256_union;
using Account    = uint256_union;
using PublicKey  = uint256_union;
using PrivateKey = uint256_union;

class RawKey
{
public:
    RawKey() = default;
    ~RawKey();
    bool operator==(const rai::RawKey&) const;
    bool operator!=(const rai::RawKey&) const;
    void Decrypt(const rai::uint256_union&, const rai::RawKey&,
                 const uint128_union&);

    rai::uint256_union data_;
};

union uint512_union
{
    uint512_union();
    uint512_union(const rai::uint512_union&) = default;
    uint512_union(const rai::uint512_t&);
    bool operator==(const rai::uint512_union&) const;
    bool operator!=(const rai::uint512_union&) const;
    rai::uint512_t Number() const;
    void Clear();
    bool IsZero() const;
    void EncodeHex(std::string&) const;
    bool DecodeHex(const std::string&);
    std::string StringHex() const;

    std::array<uint8_t, 64> bytes;
    std::array<uint32_t, 16> dwords;
    std::array<uint64_t, 8> qwords;
    std::array<uint256_union, 2> uint256s;
};
using Signature = uint512_union;

rai::uint512_union SignMessage(const rai::RawKey&, const rai::PublicKey&,
                               const rai::uint256_union&);

bool ValidateMessage(const rai::PublicKey&, const rai::uint256_union&,
                     const rai::uint512_union&);

rai::PublicKey GeneratePublicKey(const rai::PrivateKey&);
}  // namespace rai

namespace boost
{
template <>
struct hash<rai::uint256_union>
{
    size_t operator()(const rai::uint256_union& data) const
    {
        return *reinterpret_cast<const size_t*>(data.bytes.data());
    }
};
}  // namespace boost

namespace std
{
template <>
struct hash<rai::uint256_union>
{
    size_t operator()(const rai::uint256_union& data) const
    {
        return *reinterpret_cast<const size_t*>(data.bytes.data());
    }
};
}  // namespace std

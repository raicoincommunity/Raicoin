#include <rai/common/numbers.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <rai/core_test/test_util.hpp>

using std::string;

TEST(uint128_union, constructor)
{
    bool ret;

    // rai::uint128_union::uint128_union()
    rai::uint128_union value1;
    unsigned char* pvalue1 = reinterpret_cast<unsigned char*>(&value1);
    string expect1_hex = "00000000000000000000000000000000";
    unsigned char expect1[16] = "";

    ret = TestDecodeHex(expect1_hex, expect1, 16);
    ASSERT_EQ(false, ret);
    std::vector<unsigned char> v1(expect1, expect1 + sizeof(expect1));
    std::vector<unsigned char> v2(pvalue1, pvalue1 + sizeof(value1));
    ASSERT_EQ(v1, v2);

    // rai::uint128_union::uint128_union(uint64_t value)
    rai::uint128_union value2(static_cast<uint64_t>(1));
    unsigned char* pvalue2 = reinterpret_cast<unsigned char*>(&value2);
    string expect2_hex = "00000000000000000000000000000001";
    unsigned char expect2[16] = "";

    ret = TestDecodeHex(expect2_hex, expect2, 16);
    ASSERT_EQ(false, ret);
    std::vector<unsigned char> v3(expect2, expect2 + sizeof(expect2));
    std::vector<unsigned char> v4(pvalue2, pvalue2 + sizeof(value2));
    ASSERT_EQ(v3, v4);

    rai::uint128_union value3(static_cast<uint64_t>(0xffffffffffffffff));
    unsigned char* pvalue3 = reinterpret_cast<unsigned char*>(&value3);
    string expect3_hex = "0000000000000000ffffffffffffffff";
    unsigned char expect3[16] = "";

    ret = TestDecodeHex(expect3_hex, expect3, 16);
    ASSERT_EQ(false, ret);
    std::vector<unsigned char> v5(expect3, expect3 + sizeof(expect3));
    std::vector<unsigned char> v6(pvalue3, pvalue3 + sizeof(value3));
    ASSERT_EQ(v5, v6);

    ASSERT_EQ(false, value3.IsZero());
    value3.Clear();
    ASSERT_EQ(true, value3.IsZero());
}
 
TEST(uint128_union, relational_operator)
{
    bool ret;
    rai::uint128_t number("0x34F0A37AAD20F4A260F0A5B3CB3D7FB5");
    rai::uint128_union value1 = number;
    rai::uint128_union value2 = number;
    rai::uint128_union value3 = number + 1;
    
    ret = value1 == value2;
    ASSERT_EQ(true, ret);
    ret = value1 != value2;
    ASSERT_EQ(false, ret);
    ret = value1 < value2;
    ASSERT_EQ(false, ret);
    ret = value1 > value2;
    ASSERT_EQ(false, ret);

    ret = value1 == value3;
    ASSERT_EQ(false, ret);
    ret = value1 != value3;
    ASSERT_EQ(true, ret);
    ret = value1 < value3;
    ASSERT_EQ(true, ret);
    ret = value1 > value3;
    ASSERT_EQ(false, ret);
}

TEST(uint128_union, arithmetic_operator)
{
    rai::uint128_t number("0x34F0A37AAD20F4A260F0A5B3CB3D7FB5");
    rai::uint128_union value1 = number;
    rai::uint128_union value2 = number;
    rai::uint128_union value3 = number + 1;

    rai::uint128_union result = value2 - value1;
    ASSERT_EQ(0, result.Number());
    result = value3 - value1;
    ASSERT_EQ(1, result.Number());
    result = value1 - value3;
    ASSERT_EQ(rai::uint128_t(-1), result.Number());
    
    result = value1 + 1;
    ASSERT_EQ(value3, result);
    result = rai::uint128_union(rai::uint128_t(-1)) + 2;
    ASSERT_EQ(1, result.Number());

    result = value1;
    result += 1;
    ASSERT_EQ(value3, result);
    result = rai::uint128_t(-1);
    result += 2;
    ASSERT_EQ(1, result.Number());

    result = value3;
    result -= 1;
    ASSERT_EQ(value1, result);
    result = 1;
    result -= 2;
    ASSERT_EQ(rai::uint128_t(-1), result.Number());

    result = 2;
    ASSERT_EQ(6, (result * 3).Number());
    result = number;
    ASSERT_EQ(number * 2, (result * 2).Number());
}

TEST(uint128_union, hex)
{
    bool ret;
    rai::uint128_union value;

    ret = value.DecodeHex("");
    ASSERT_EQ(true, ret);

    ret = value.DecodeHex("Ff");
    ASSERT_EQ(true, ret);
    ASSERT_EQ(0, value.Number());

    ret = value.DecodeHex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    ASSERT_EQ(false, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint128_t>::max)(), value.Number());

    ret = value.DecodeHex("123456789012345678901234567890123");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint128_t>::max)(), value.Number());

    ret = value.DecodeHex("xFF");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint128_t>::max)(), value.Number());

    ret = value.DecodeHex("FFx");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint128_t>::max)(), value.Number());

    string str;
    value.EncodeHex(str);
    ASSERT_EQ("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", str);

    ret = value.DecodeDec("255");
    ASSERT_EQ(false, ret);
    value.EncodeHex(str);
    ASSERT_EQ("000000000000000000000000000000FF", str);
}

TEST(uint128_union, dec)
{
    bool ret;
    rai::uint128_union value;

    ret = value.DecodeDec("255");
    ASSERT_EQ(false, ret);
    ASSERT_EQ(255, value.Number());

    ret = value.DecodeDec("340282366920938463463374607431768211455");
    ASSERT_EQ(false, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint128_t>::max)(), value.Number());

    ret = value.DecodeDec("340282366920938463463374607431768211456");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint128_t>::max)(), value.Number());

    ret = value.DecodeDec("1234567890123456789012345678901234567890");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint128_t>::max)(), value.Number());

    ret = value.DecodeDec("FF");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint128_t>::max)(), value.Number());

    string str;
    value.EncodeDec(str);
    ASSERT_EQ("340282366920938463463374607431768211455", str);

    ret = value.DecodeDec("255");
    ASSERT_EQ(false, ret);
    value.EncodeDec(str);
    ASSERT_EQ("255", str);

    ASSERT_EQ(rai::uint128_t("1"), rai::Colin);
    ASSERT_EQ(rai::uint128_t("1000"), rai::uRAI);
    ASSERT_EQ(rai::uint128_t("1000000"), rai::mRAI);
    ASSERT_EQ(rai::uint128_t("1000000000"), rai::RAI);
}


TEST(uint256_union, constructor)
{
    bool ret;

    // rai::uint256_union::uint256_union()
    rai::uint256_union value1;
    unsigned char* pvalue1 = reinterpret_cast<unsigned char*>(&value1);
    string expect1_hex =
        "0000000000000000000000000000000000000000000000000000000000000000";
    unsigned char expect1[32] = "";

    ret = TestDecodeHex(expect1_hex, expect1, 32);
    ASSERT_EQ(false, ret);
    std::vector<unsigned char> v1(expect1, expect1 + sizeof(expect1));
    std::vector<unsigned char> v2(pvalue1, pvalue1 + sizeof(value1));
    ASSERT_EQ(v1, v2);

    // rai::uint256_union::uint256_union(uint64_t value)
    rai::uint256_union value2(static_cast<uint64_t>(1));
    unsigned char* pvalue2 = reinterpret_cast<unsigned char*>(&value2);
    string expect2_hex =
        "0000000000000000000000000000000000000000000000000000000000000001";
    unsigned char expect2[32] = "";

    ret = TestDecodeHex(expect2_hex, expect2, 32);
    ASSERT_EQ(false, ret);
    std::vector<unsigned char> v3(expect2, expect2 + sizeof(expect2));
    std::vector<unsigned char> v4(pvalue2, pvalue2 + sizeof(value2));
    ASSERT_EQ(v3, v4);

    rai::uint256_union value3(static_cast<uint64_t>(0xffffffffffffffff));
    unsigned char* pvalue3 = reinterpret_cast<unsigned char*>(&value3);
    string expect3_hex =
        "000000000000000000000000000000000000000000000000ffffffffffffffff";
    unsigned char expect3[32] = "";

    ret = TestDecodeHex(expect3_hex, expect3, 32);
    ASSERT_EQ(false, ret);
    std::vector<unsigned char> v5(expect3, expect3 + sizeof(expect3));
    std::vector<unsigned char> v6(pvalue3, pvalue3 + sizeof(value3));
    ASSERT_EQ(v5, v6);

    ASSERT_EQ(false, value3.IsZero());
    value3.Clear();
    ASSERT_EQ(true, value3.IsZero());

    rai::uint256_union value4;
    value4.DecodeHex(
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    ASSERT_EQ(value4, rai::uint256_union::Max());
}

TEST(uint256_union, relational_operator)
{
    bool ret;
    rai::uint256_t number(
        "0x34F0A37AAD20F4A260F0A5B3CB3D7FB534F0A37AAD20F4A260F0A5B3CB3D7FB5");
    rai::uint256_union value1 = number;
    rai::uint256_union value2 = number;
    rai::uint256_union value3 = number + 1;
    
    ret = value1 == value2;
    ASSERT_EQ(true, ret);
    ret = value1 != value2;
    ASSERT_EQ(false, ret);
    ret = value1 < value2;
    ASSERT_EQ(false, ret);
    ret = value1 > value2;
    ASSERT_EQ(false, ret);
    ret = value1 >= value2;
    ASSERT_EQ(true, ret);
    ret = value1 <= value2;
    ASSERT_EQ(true, ret);

    ret = value1 == value3;
    ASSERT_EQ(false, ret);
    ret = value1 != value3;
    ASSERT_EQ(true, ret);
    ret = value1 < value3;
    ASSERT_EQ(true, ret);
    ret = value1 > value3;
    ASSERT_EQ(false, ret);
    ret = value1 >= value3;
    ASSERT_EQ(false, ret);
    ret = value1 <= value3;
    ASSERT_EQ(true, ret);
}

TEST(uint256_union, bitwise_operator)
{
    rai::uint256_union value1;
    value1.DecodeHex(
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    rai::uint256_union value2(value1);
    rai::uint256_union value3(value1);

    value3 = value1 ^ value2;
    ASSERT_EQ(0, value3.Number());

    value2 ^= value1;
    ASSERT_EQ(0, value2.Number());

    value1 ^= value1;
    ASSERT_EQ(0, value1.Number());

    ASSERT_EQ(std::numeric_limits<rai::uint256_t>::max(), (~value1).Number());

    rai::uint256_union value4;
    value4.DecodeHex(
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0");
    ASSERT_EQ(0xF, (~value4).Number());
}

TEST(uint256_union, arithmetic_operator)
{
    rai::uint256_t number("0x34F0A37AAD20F4A260F0A5B3CB3D7FB534F0A37AAD20F4A260F0A5B3CB3D7FB5");
    rai::uint256_union value1 = number;
    rai::uint256_union value2 = number;
    rai::uint256_union value3 = number + 1;

    rai::uint256_union result = value2 - value1;
    ASSERT_EQ(0, result.Number());
    result = value3 - value1;
    ASSERT_EQ(1, result.Number());
    result = value1 - value3;
    ASSERT_EQ(rai::uint256_t(-1), result.Number());
    
    result = value1 + 1;
    ASSERT_EQ(value3, result);
    result = rai::uint256_union(rai::uint256_t(-1)) + 2;
    ASSERT_EQ(1, result.Number());

    result = value1;
    result += 1;
    ASSERT_EQ(value3, result);
    result = rai::uint256_t(-1);
    result += 2;
    ASSERT_EQ(1, result.Number());

    result = value3;
    result -= 1;
    ASSERT_EQ(value1, result);
    result = 1;
    result -= 2;
    ASSERT_EQ(rai::uint256_t(-1), result.Number());

    rai::uint256_union value4 = rai::uint256_t(
        "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0");
    rai::uint256_union value5 = rai::uint256_t("0x10");
    rai::uint256_union value6(value4);
    value6 += value5;
    ASSERT_EQ(0, value6.Number());
    ASSERT_EQ(true, value6 < value4);
    ASSERT_EQ(true, value6 <= value5);
}

TEST(uint256_union, dec)
{
    bool ret;
    rai::uint256_union value;

    ret = value.DecodeDec("255");
    ASSERT_EQ(false, ret);
    ASSERT_EQ(255, value.Number());

    ret = value.DecodeDec(
        "1157920892373161954235709850086879078532699846656405640394575840079131"
        "29639935");
    ASSERT_EQ(false, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint256_t>::max)(), value.Number());

    ret = value.DecodeDec(
        "1157920892373161954235709850086879078532699846656405640394575840079131"
        "29639936");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint256_t>::max)(), value.Number());

    ret = value.DecodeDec(
        "1234567890123456789012345678901234567890123456789012345678901234567890"
        "123456789");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint256_t>::max)(), value.Number());

    ret = value.DecodeDec("FF");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint256_t>::max)(), value.Number());

    string str;
    value.EncodeDec(str);
    ASSERT_EQ(
        "1157920892373161954235709850086879078532699846656405640394575840079131"
        "29639935",
        str);

    ret = value.DecodeDec("255");
    ASSERT_EQ(false, ret);
    value.EncodeDec(str);
    ASSERT_EQ("255", str);
}

TEST(uint256_union, hex)
{
    bool ret;
    rai::uint256_union value;

    ret = value.DecodeHex("");
    ASSERT_EQ(true, ret);

    ret = value.DecodeHex("Ff");
    ASSERT_EQ(true, ret);
    ASSERT_EQ(0, value.Number());

    ret = value.DecodeHex(
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    ASSERT_EQ(false, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint256_t>::max)(), value.Number());

    ret = value.DecodeHex(
        "12345678901234567890123456789012345678901234567890123456789012345");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint256_t>::max)(), value.Number());

    ret = value.DecodeHex("xFF");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint256_t>::max)(), value.Number());

    ret = value.DecodeHex("FFx");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint256_t>::max)(), value.Number());

    string str;
    value.EncodeHex(str);
    ASSERT_EQ(
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
        str);

    ret = value.DecodeDec("255");
    ASSERT_EQ(false, ret);
    value.EncodeHex(str);
    ASSERT_EQ(
        "00000000000000000000000000000000000000000000000000000000000000FF",
        str);
}

TEST(uint256_union, evm_hex)
{
    bool ret;
    rai::uint256_union value;

    ret = value.DecodeEvmHex("");
    ASSERT_EQ(true, ret);

    ret = value.DecodeEvmHex("Ff");
    ASSERT_EQ(true, ret);
    ASSERT_EQ(0, value.Number());

    ret = value.DecodeEvmHex(
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    ASSERT_EQ(true, ret);
    ASSERT_EQ(0, value.Number());

    ret = value.DecodeEvmHex(
        "0XFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    ASSERT_EQ(true, ret);
    ASSERT_EQ(0, value.Number());

    ret = value.DecodeEvmHex(
        "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    ASSERT_EQ(false, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint256_t>::max)(), value.Number());

    ret = value.DecodeEvmHex(
        "0x12345678901234567890123456789012345678901234567890123456789012345");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint256_t>::max)(), value.Number());

    ret = value.DecodeEvmHex("xFF");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint256_t>::max)(), value.Number());

    ret = value.DecodeEvmHex("FFx");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint256_t>::max)(), value.Number());

    string str;
    value.EncodeEvmShortHex(str);
    ASSERT_EQ(false,
        str == "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

    value.EncodeEvmShortHex(str);
    ASSERT_EQ(true,
        str == "0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    ret = value.DecodeEvmHex("0x12f");
    ASSERT_EQ(false, ret);
    ASSERT_EQ(0x12f, value.Number());
    value.EncodeEvmShortHex(str);
    ASSERT_EQ(true, str == "0x12f");

    ret = value.DecodeEvmHex("0x0");
    ASSERT_EQ(false, ret);
    ASSERT_EQ(0, value.Number());
    value.EncodeEvmShortHex(str);
    ASSERT_EQ(true, str == "0x0");
}

TEST(uint256_union, account)
{
    bool ret;
    rai::uint256_union value;

    ret = value.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    ASSERT_EQ(false, ret);

    ASSERT_EQ(
        "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
        value.StringAccount());

    value.Clear();
    ret = value.DecodeAccount(
        "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1");
    ASSERT_EQ(false, ret);
    rai::uint256_union value_expect;
    value_expect.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    ASSERT_EQ(value_expect, value);

    ret = value.DecodeAccount(
        "raj_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1");
    ASSERT_EQ(true, ret);
    ASSERT_EQ(value_expect, value);

    ret = value.DecodeAccount(
        "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn");
    ASSERT_EQ(true, ret);
    ASSERT_EQ(value_expect, value);

    ret = value.DecodeAccount(
        "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg5950cytdbkrxcbzekkcqkc3dn1");
    ASSERT_EQ(true, ret);
    ASSERT_EQ(value_expect, value);

    ret = value.DecodeAccount(
        "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn2");
    ASSERT_EQ(true, ret);
    ASSERT_EQ(value_expect, value);
}

TEST(uint512_union, constructor)
{
    bool ret;

    // rai::uint512_union::uint512_union()
    rai::uint512_union value1;
    unsigned char* pvalue1 = reinterpret_cast<unsigned char*>(&value1);
    string expect1_hex =
        "0000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000";
    unsigned char expect1[64] = "";

    ret = TestDecodeHex(expect1_hex, expect1, 64);
    ASSERT_EQ(false, ret);
    std::vector<unsigned char> v1(expect1, expect1 + sizeof(expect1));
    std::vector<unsigned char> v2(pvalue1, pvalue1 + sizeof(value1));
    ASSERT_EQ(v1, v2);

    // rai::uint512_union::uint512_union(const rai::uint512_t& value)
    rai::uint512_union value2 = rai::uint512_t(1);
    unsigned char* pvalue2 = reinterpret_cast<unsigned char*>(&value2);
    string expect2_hex =
        "0000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000001";
    unsigned char expect2[64] = "";

    ret = TestDecodeHex(expect2_hex, expect2, 64);
    ASSERT_EQ(false, ret);
    std::vector<unsigned char> v3(expect2, expect2 + sizeof(expect2));
    std::vector<unsigned char> v4(pvalue2, pvalue2 + sizeof(value2));
    ASSERT_EQ(v3, v4);

    rai::uint512_union value3 = rai::uint512_t(
        "0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    unsigned char* pvalue3 = reinterpret_cast<unsigned char*>(&value3);
    string expect3_hex =
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    unsigned char expect3[64] = "";

    ret = TestDecodeHex(expect3_hex, expect3, 64);
    ASSERT_EQ(false, ret);
    std::vector<unsigned char> v5(expect3, expect3 + sizeof(expect3));
    std::vector<unsigned char> v6(pvalue3, pvalue3 + sizeof(value3));
    ASSERT_EQ(v5, v6);

    ASSERT_EQ(false, value3.IsZero());
    value3.Clear();
    ASSERT_EQ(true, value3.IsZero());

    rai::uint256_union denominator(2);
    rai::uint256_union numerator(1);
    rai::uint512_union value4(denominator, numerator);
    rai::uint512_t value4_expect(
        "0x8000000000000000000000000000000000000000000000000000000000000000");
    ASSERT_EQ(value4_expect, value4.Number());

    rai::uint512_union value5(rai::uint256_union(0), numerator);
    rai::uint512_t value5_expect(
        "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    ASSERT_EQ(value5_expect, value5.Number());
}

TEST(uint512_union, relational_operator)
{
    bool ret;
    rai::uint512_t number(
        "0x34F0A37AAD20F4A260F0A5B3CB3D7FB534F0A37AAD20F4A260F0A5B3CB3D7FB534F0"
        "A37AAD20F4A260F0A5B3CB3D7FB534F0A37AAD20F4A260F0A5B3CB3D7FB5");
    rai::uint512_union value1 = number;
    rai::uint512_union value2 = number;
    rai::uint512_union value3 = number + 1;
    
    ret = value1 == value2;
    ASSERT_EQ(true, ret);
    ret = value1 != value2;
    ASSERT_EQ(false, ret);

    ret = value1 == value3;
    ASSERT_EQ(false, ret);
    ret = value1 != value3;
    ASSERT_EQ(true, ret);
}

TEST(uint512_union, hex)
{
    bool ret;
    rai::uint512_union value;

    ret = value.DecodeHex("");
    ASSERT_EQ(true, ret);

    ret = value.DecodeHex("Ff");
    ASSERT_EQ(true, ret);
    ASSERT_EQ(0, value.Number());

    ret = value.DecodeHex(
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    ASSERT_EQ(false, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint512_t>::max)(), value.Number());

    ret = value.DecodeHex(
        "1234567890123456789012345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890123456789");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint512_t>::max)(), value.Number());

    ret = value.DecodeHex("xFF");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint512_t>::max)(), value.Number());

    ret = value.DecodeHex("FFx");
    ASSERT_EQ(true, ret);
    ASSERT_EQ((std::numeric_limits<rai::uint512_t>::max)(), value.Number());

    string str;
    value.EncodeHex(str);
    ASSERT_EQ(
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
        str);
}

TEST(RawKey, Decrypt)
{
    rai::uint256_union u;
    rai::uint128_union iv;
    rai::RawKey key1;
    rai::RawKey key2;

    u.DecodeHex(
        "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
    key1.data_ = u;
    iv.DecodeHex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    u.DecodeHex(
        "6bc1bee22e409f96e93d7e117393172a6bc1bee22e409f96e93d7e117393172a");
    key2.Decrypt(u, key1, iv);

    rai::uint256_union expect;
    expect.DecodeHex(
        "601EC313775789A5B7A7F504BBF3D22831AFD77F7D218690BD0EF82DFCF66CBE");
    ASSERT_EQ(expect, key2.data_);

    key2.Decrypt(key2.data_, key1, iv);
    ASSERT_EQ(u, key2.data_);
}

TEST(PublicKey, generate)
{
    rai::PrivateKey pri;
    pri.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");

    rai::PublicKey pub = rai::GeneratePublicKey(pri);
    rai::PublicKey pub_expect;
    pub_expect.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    ASSERT_EQ(pub_expect, pub);
}

TEST(AccountParser, parse)
{
    rai::AccountParser parser(
        "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo_"
        "sub_account");
    ASSERT_EQ(false, parser.Error());

    rai::Account account;
    bool error = account.DecodeAccount(
        "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo");
    ASSERT_EQ(false, error);
    ASSERT_EQ(account, parser.Account());
    ASSERT_EQ("sub_account", parser.SubAccount());

    rai::AccountParser parser2(
        "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo");
    ASSERT_EQ(false, parser2.Error());
    ASSERT_EQ(account, parser.Account());
    ASSERT_EQ("", parser2.SubAccount());

    rai::AccountParser parser3(
        "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo ");
    ASSERT_EQ(true, parser3.Error());

    rai::AccountParser parser4(
        "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo_");
    ASSERT_EQ(true, parser4.Error());

    rai::AccountParser parser5(
        "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtd");
    ASSERT_EQ(true, parser5.Error());

    rai::AccountParser parser6(
        "Rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtd0");
    ASSERT_EQ(true, parser6.Error());
}

TEST(Prefix, ToLower)
{
    bool ret;
    rai::uint512_union value;

    ret = value.DecodeHex(
        "4041FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF5A5B");
    ASSERT_EQ(false, ret);
    value.ToLower();

    std::string expect =
        "4061FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7A5B";
    ASSERT_EQ(expect, value.StringHex());
}

TEST(SecureMath, u128)
{
    rai::uint128_t u128(1);
    rai::uint256_t u256(u128);
    EXPECT_EQ(1, u256);

    rai::uint128_t u128_max("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    EXPECT_EQ(std::numeric_limits<rai::uint128_t>::max(), u128_max);
    u256 = 1;
    u256 *= u128_max;
    EXPECT_EQ(std::numeric_limits<rai::uint128_t>::max(), u256);
    u256 *= u128_max;
    EXPECT_EQ(std::numeric_limits<rai::uint256_t>::max()
                  - std::numeric_limits<rai::uint128_t>::max()
                  - std::numeric_limits<rai::uint128_t>::max(),
              u256);
    u256 /= u128_max;
    EXPECT_EQ(std::numeric_limits<rai::uint128_t>::max(), u256);
}

TEST(SecureMath, u256)
{
    rai::uint256_t u256(1);
    rai::uint512_t u512(u256);
    EXPECT_EQ(1, u512);

    rai::uint256_t u256_max("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    EXPECT_EQ(std::numeric_limits<rai::uint256_t>::max(), u256_max);
    u512 = 1;
    u512 *= u256_max;
    EXPECT_EQ(std::numeric_limits<rai::uint256_t>::max(), u512);
    u512 *= u256_max;
    EXPECT_EQ(std::numeric_limits<rai::uint512_t>::max()
                  - std::numeric_limits<rai::uint256_t>::max()
                  - std::numeric_limits<rai::uint256_t>::max(),
              u512);
    u512 /= u256_max;
    EXPECT_EQ(std::numeric_limits<rai::uint256_t>::max(), u512);
    EXPECT_EQ(true,
              rai::uint512_t(10) * rai::uint512_t(2) % rai::uint512_t(3) != 0);
    EXPECT_EQ(false,
              rai::uint512_t(9) * rai::uint512_t(2) % rai::uint512_t(3) != 0);
}
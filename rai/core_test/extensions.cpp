#include <gtest/gtest.h>
#include <iostream>
#include <rai/core_test/test_util.hpp>
#include <rai/common/extensions.hpp>
#include <string>
#include <vector>
#include <memory>

TEST(ExtensionToken, Common)
{
    std::string hex;
    hex += "00040000";  // type + length
    std::vector<uint8_t> bytes;
    bool ret = TestDecodeHex(hex, bytes);
    EXPECT_EQ(false, ret);

    rai::Extensions extensions;
    rai::ErrorCode error_code = extensions.FromBytes(bytes);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    size_t count = extensions.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);

    rai::ExtensionToken token;
    error_code = token.FromExtension(extensions.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::EXTENSION_VALUE, error_code);

    hex = "000400020001";  // type + length + op
    bytes.clear();
    ret = TestDecodeHex(hex, bytes);
    EXPECT_EQ(false, ret);
    error_code = extensions.FromBytes(bytes);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token.FromExtension(extensions.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_OP_INVALID, error_code);

    bytes[4] = 9;
    error_code = extensions.FromBytes(bytes);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token.FromExtension(extensions.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_OP_UNKNOWN, error_code);

    bytes[4] = 1;
    error_code = extensions.FromBytes(bytes);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token.FromExtension(extensions.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::STREAM, error_code);

    hex = "0004005E";  // type + length
    hex += "0101";  // op_create + token type
    hex += "12526169636F696E205465737420546F6B656E";  // name
    hex += "03525454"; // symbol
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000";  // init_supply
    hex += "0000000000000000000000000000000000000000000DE0B6B3A7640000000000"; // cap_supply
    hex += "12"; // decimals
    hex += "00010100"; // burnable + mintable + circulable;
    bytes.clear();
    ret = TestDecodeHex(hex, bytes);
    EXPECT_EQ(false, ret);
    error_code = extensions.FromBytes(bytes);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token.FromExtension(extensions.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::STREAM, error_code);
}

TEST(ExtensionToken, Create20)
{
    std::string hex;
    hex += "0004005D";  // type + length
    hex += "0101";  // op_create + token type
    hex += "12526169636F696E205465737420546F6B656E";  // name
    hex += "03525454"; // symbol
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000";  // init_supply
    hex += "0000000000000000000000000000000000000000000DE0B6B3A7640000000000"; // cap_supply
    hex += "12"; // decimals
    hex += "000101"; // burnable + mintable + circulable;
    std::vector<uint8_t> bytes;
    bool ret = TestDecodeHex(hex, bytes);
    EXPECT_EQ(false, ret);

    rai::Extensions extensions;
    rai::ErrorCode error_code = extensions.FromBytes(bytes);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);

    size_t count = extensions.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);

    rai::ExtensionToken token;
    error_code = token.FromExtension(extensions.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(rai::ExtensionToken::Op::CREATE, token.op_);
    EXPECT_EQ(false, token.op_data_ == nullptr);

    auto create =
        std::static_pointer_cast<rai::ExtensionTokenCreate>(token.op_data_);
    EXPECT_EQ(rai::TokenType::_20, create->type_);
    EXPECT_EQ(false, create->creation_data_ == nullptr);
    auto create20 = std::static_pointer_cast<rai::ExtensionToken20Create>(
        create->creation_data_);
    EXPECT_EQ("Raicoin Test Token", create20->name_);
    EXPECT_EQ("RTT", create20->symbol_);
    EXPECT_EQ(rai::TokenValue(rai::uint256_t("65536000000000000000000")),
              create20->init_supply_);
    EXPECT_EQ(rai::TokenValue(rai::uint256_t("16777216000000000000000000")),
              create20->cap_supply_);
    EXPECT_EQ(false, create20->burnable_);
    EXPECT_EQ(true, create20->mintable_);
    EXPECT_EQ(true, create20->circulable_);

    EXPECT_EQ(bytes, extensions.Bytes());
    rai::Extensions extensions2;
    error_code = extensions2.FromJson(extensions.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes, extensions2.Bytes());

    std::vector<uint8_t> bytes2(bytes);
    bytes2[5] = 0;
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    rai::ExtensionToken token2;
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_TYPE_INVALID, error_code);
    
    bytes2[5] = 3;
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_TYPE_UNKNOWN, error_code);

    bytes2[5] = bytes[5];
    bytes2[8] = 0xFF;
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_NAME_UTF8_CHECK, error_code);
    bytes2[8] = bytes[8];

    bytes2[27] = 0xFF;
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SYMBOL_UTF8_CHECK, error_code);

    hex = "0004005D";  // type + length
    hex += "0101";  // op_create + token type
    hex += "12526169636F696E205465737420546F6B656E";  // name
    hex += "03525454"; // symbol
    hex += "0000000000000000000000000000000000000000000DE0B6B3A7640000000000"; // init_supply
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000";  // cap_supply
    hex += "12"; // decimals
    hex += "000101"; // burnable + mintable + circulable;
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_CAP_SUPPLY_EXCEEDED, error_code);
}

TEST(ExtensionToken, Create721)
{
    std::string hex;
    hex += "0004003C";  // type + length
    hex += "0102";  // op_create + token type
    hex += "12526169636F696E205465737420546F6B656E";  // name
    hex += "03525454"; // symbol
    hex += "00"; // base_uri
    hex += "0000000000000000000000000000000000000000000000000000000000001000"; // cap_supply
    hex += "0101"; // burnable + circulable;
    std::vector<uint8_t> bytes;
    bool ret = TestDecodeHex(hex, bytes);
    EXPECT_EQ(false, ret);

    rai::Extensions extensions;
    rai::ErrorCode error_code = extensions.FromBytes(bytes);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);

    size_t count = extensions.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);

    rai::ExtensionToken token;
    error_code = token.FromExtension(extensions.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(rai::ExtensionToken::Op::CREATE, token.op_);
    EXPECT_EQ(false, token.op_data_ == nullptr);

    auto create =
        std::static_pointer_cast<rai::ExtensionTokenCreate>(token.op_data_);
    EXPECT_EQ(rai::TokenType::_721, create->type_);
    EXPECT_EQ(false, create->creation_data_ == nullptr);
    auto create721 = std::static_pointer_cast<rai::ExtensionToken721Create>(
        create->creation_data_);
    EXPECT_EQ("Raicoin Test Token", create721->name_);
    EXPECT_EQ("RTT", create721->symbol_);
    EXPECT_EQ("", create721->base_uri_);
    EXPECT_EQ(rai::TokenValue(rai::uint256_t("4096")),
              create721->cap_supply_);
    EXPECT_EQ(true, create721->burnable_);
    EXPECT_EQ(true, create721->circulable_);

    EXPECT_EQ(bytes, extensions.Bytes());
    rai::Extensions extensions2;
    error_code = extensions2.FromJson(extensions.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes, extensions2.Bytes());

    std::vector<uint8_t> bytes2(bytes);
    bytes2[8] = 0xFF;
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    rai::ExtensionToken token2;
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_NAME_UTF8_CHECK, error_code);
    bytes2[8] = bytes[8];

    bytes2[27] = 0xFF;
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SYMBOL_UTF8_CHECK, error_code);

    hex = "00040043";  // type + length
    hex += "0102";  // op_create + token type
    hex += "12526169636F696E205465737420546F6B656E";  // name
    hex += "03525454"; // symbol
    hex += "07697066733A2F2F"; // base_uri
    hex += "0000000000000000000000000000000000000000000000000000000000000001"; // cap_supply
    hex += "0101"; // burnable + circulable;
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    
    hex = "00040043";  // type + length
    hex += "0102";  // op_create + token type
    hex += "12526169636F696E205465737420546F6B656E";  // name
    hex += "03525454"; // symbol
    hex += "07FF7066733A2F2F"; // base_uri
    hex += "0000000000000000000000000000000000000000000000000000000000000001"; // cap_supply
    hex += "0101"; // burnable + circulable;
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_BASE_URI_UTF8_CHECK, error_code);

    hex = "00040043";  // type + length
    hex += "0102";  // op_create + token type
    hex += "12526169636F696E205465737420546F6B656E";  // name
    hex += "03525454"; // symbol
    hex += "07697066733A2F2F"; // base_uri
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // cap_supply
    hex += "0101"; // burnable + circulable;
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_CAP_SUPPLY, error_code);
}

TEST(ExtensionToken, Mint20)
{
    std::string hex;
    hex += "00040042";  // type + length
    hex += "0201";  // op_mint + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    std::vector<uint8_t> bytes;
    bool ret = TestDecodeHex(hex, bytes);
    EXPECT_EQ(false, ret);

    rai::Extensions extensions;
    rai::ErrorCode error_code = extensions.FromBytes(bytes);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);

    size_t count = extensions.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);

    rai::ExtensionToken token;
    error_code = token.FromExtension(extensions.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(rai::ExtensionToken::Op::MINT, token.op_);
    EXPECT_EQ(false, token.op_data_ == nullptr);

    auto mint =
        std::static_pointer_cast<rai::ExtensionTokenMint>(token.op_data_);
    EXPECT_EQ(rai::TokenType::_20, mint->type_);
    EXPECT_EQ(
        "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
        mint->to_.StringAccount());
    EXPECT_EQ(rai::TokenValue(rai::uint256_t("65536000000000000000000")),
              mint->value_);

    EXPECT_EQ(bytes, extensions.Bytes());
    rai::Extensions extensions2;
    error_code = extensions2.FromJson(extensions.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes, extensions2.Bytes());


    rai::ExtensionToken token2;

    hex = "00040042";  // type + length
    hex += "0201";  // op_mint + token type
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // to
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value;
    std::vector<uint8_t> bytes2;
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_MINT_TO, error_code);

    hex = "00040042";  // type + length
    hex += "0201";  // op_mint + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value;
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_VALUE, error_code);

    hex = "00040044";  // type + length
    hex += "0201";  // op_mint + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "0000000000000000000000000000000000000000000000000000000000000001"; // value;
    hex += "0152"; // uri
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::STREAM, error_code);
}

TEST(ExtensionToken, Mint721)
{
    std::string hex;
    hex += "00040062";  // type + length
    hex += "0202";  // op_mint + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    hex += "1F303132333435363738396162636465666768696A4B4C4D4E4F50515253542F"; // uri
    std::vector<uint8_t> bytes;
    bool ret = TestDecodeHex(hex, bytes);
    EXPECT_EQ(false, ret);

    rai::Extensions extensions;
    rai::ErrorCode error_code = extensions.FromBytes(bytes);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);

    size_t count = extensions.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);

    rai::ExtensionToken token;
    error_code = token.FromExtension(extensions.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(rai::ExtensionToken::Op::MINT, token.op_);
    EXPECT_EQ(false, token.op_data_ == nullptr);

    auto mint =
        std::static_pointer_cast<rai::ExtensionTokenMint>(token.op_data_);
    EXPECT_EQ(rai::TokenType::_721, mint->type_);
    EXPECT_EQ(
        "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
        mint->to_.StringAccount());
    EXPECT_EQ(rai::TokenValue(rai::uint256_t("65536000000000000000000")),
              mint->value_);
    EXPECT_EQ("0123456789abcdefghijKLMNOPQRST/", mint->uri_);

    EXPECT_EQ(bytes, extensions.Bytes());
    rai::Extensions extensions2;
    error_code = extensions2.FromJson(extensions.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes, extensions2.Bytes());

    rai::ExtensionToken token2;
    hex = "00040042";  // type + length
    hex += "0202";  // op_mint + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    std::vector<uint8_t> bytes2;
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::STREAM, error_code);

    hex = "00040062";  // type + length
    hex += "0202";  // op_mint + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    hex += "1F303132333435363738396162636465666768696A4B4C4D4E4F50515253542F"; // uri
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);

    hex = "00040062";  // type + length
    hex += "0202";  // op_mint + token type
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // to
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    hex += "1F303132333435363738396162636465666768696A4B4C4D4E4F50515253542F"; // uri
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_MINT_TO, error_code);

    hex = "00040062";  // type + length
    hex += "0202";  // op_mint + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    hex += "1FFF3132333435363738396162636465666768696A4B4C4D4E4F50515253542F"; // uri
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_URI_UTF8_CHECK, error_code);
}

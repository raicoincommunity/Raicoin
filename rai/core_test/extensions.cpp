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

TEST(ExtensionToken, Burn20)
{
    std::string hex;
    hex += "00040022";  // type + length
    hex += "0301";  // op_burn + token type
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
    EXPECT_EQ(rai::ExtensionToken::Op::BURN, token.op_);
    EXPECT_EQ(false, token.op_data_ == nullptr);

    auto burn =
        std::static_pointer_cast<rai::ExtensionTokenBurn>(token.op_data_);
    EXPECT_EQ(rai::TokenType::_20, burn->type_);
    EXPECT_EQ(rai::TokenValue(rai::uint256_t("65536000000000000000000")),
              burn->value_);

    EXPECT_EQ(bytes, extensions.Bytes());
    rai::Extensions extensions2;
    error_code = extensions2.FromJson(extensions.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes, extensions2.Bytes());

    rai::ExtensionToken token2;

    hex = "00040022";  // type + length
    hex += "0300";  // op_burn + token type
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    std::vector<uint8_t> bytes2;
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_TYPE_INVALID, error_code);

    hex = "00040022";  // type + length
    hex += "0303";  // op_burn + token type
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_TYPE_UNKNOWN, error_code);

    hex = "00040022";  // type + length
    hex += "0301";  // op_burn + token type
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_VALUE, error_code);
}

TEST(ExtensionToken, Burn721)
{
    std::string hex;
    hex += "00040022";  // type + length
    hex += "0302";  // op_burn + token type
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
    EXPECT_EQ(rai::ExtensionToken::Op::BURN, token.op_);
    EXPECT_EQ(false, token.op_data_ == nullptr);

    auto burn =
        std::static_pointer_cast<rai::ExtensionTokenBurn>(token.op_data_);
    EXPECT_EQ(rai::TokenType::_721, burn->type_);
    EXPECT_EQ(rai::TokenValue(rai::uint256_t("65536000000000000000000")),
              burn->value_);

    EXPECT_EQ(bytes, extensions.Bytes());
    rai::Extensions extensions2;
    error_code = extensions2.FromJson(extensions.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes, extensions2.Bytes());

    rai::ExtensionToken token2;

    hex = "00040022";  // type + length
    hex += "0302";  // op_mint + token type
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    std::vector<uint8_t> bytes2;
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
}

TEST(ExtensionToken, Send20)
{
    std::string hex;
    hex = "00040066";  // type + length
    hex += "040000000101";  // op_send + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
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
    EXPECT_EQ(rai::ExtensionToken::Op::SEND, token.op_);
    EXPECT_EQ(false, token.op_data_ == nullptr);

    auto send =
        std::static_pointer_cast<rai::ExtensionTokenSend>(token.op_data_);
    EXPECT_EQ(rai::Chain::RAICOIN, send->token_.chain_);
    EXPECT_EQ(rai::TokenType::_20, send->token_.type_);
    EXPECT_EQ(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
        send->token_.address_.StringHex());
    EXPECT_EQ(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
        send->to_.StringHex());
    EXPECT_EQ(rai::TokenValue(rai::uint256_t("65536000000000000000000")),
              send->value_);

    EXPECT_EQ(bytes, extensions.Bytes());
    rai::Extensions extensions2;
    error_code = extensions2.FromJson(extensions.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes, extensions2.Bytes());

    rai::ExtensionToken token2;
    std::vector<uint8_t> bytes2;

    hex = "00040066";  // type + length
    hex += "040000000001";  // op_send + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_CHAIN_INVALID, error_code);

    hex = "00040066";  // type + length
    hex += "040000100001";  // op_send + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_CHAIN_UNKNOWN, error_code);

    hex = "00040066";  // type + length
    hex += "040000000100";  // op_send + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_TYPE_INVALID, error_code);

    hex = "00040066";  // type + length
    hex += "040000000103";  // op_send + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_TYPE_UNKNOWN, error_code);

    hex = "00040066";  // type + length
    hex += "040000000101";  // op_send + chain + token type
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // token address
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);

    hex = "00040066";  // type + length
    hex += "040000000101";  // op_send + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // to
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SEND_TO, error_code);

    hex = "00040066";  // type + length
    hex += "040000000101";  // op_send + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_VALUE, error_code);
}

TEST(ExtensionToken, Send721)
{
    std::string hex;
    hex = "00040066";  // type + length
    hex += "040000000202";  // op_send + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
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
    EXPECT_EQ(rai::ExtensionToken::Op::SEND, token.op_);
    EXPECT_EQ(false, token.op_data_ == nullptr);

    auto send =
        std::static_pointer_cast<rai::ExtensionTokenSend>(token.op_data_);
    EXPECT_EQ(rai::Chain::BITCOIN, send->token_.chain_);
    EXPECT_EQ(rai::TokenType::_721, send->token_.type_);
    EXPECT_EQ(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
        send->token_.address_.StringHex());
    EXPECT_EQ(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
        send->to_.StringHex());
    EXPECT_EQ(rai::TokenValue(rai::uint256_t("65536000000000000000000")),
              send->value_);

    EXPECT_EQ(bytes, extensions.Bytes());
    rai::Extensions extensions2;
    error_code = extensions2.FromJson(extensions.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes, extensions2.Bytes());

    rai::ExtensionToken token2;
    std::vector<uint8_t> bytes2;

    hex = "00040066";  // type + length
    hex += "040000000202";  // op_send + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // to
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
}

TEST(ExtensionToken, Receive)
{
    std::string hex;
    hex = "0004008F";  // type + length
    hex += "050000000101";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "01"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
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
    EXPECT_EQ(rai::ExtensionToken::Op::RECEIVE, token.op_);
    EXPECT_EQ(false, token.op_data_ == nullptr);

    auto receive =
        std::static_pointer_cast<rai::ExtensionTokenReceive>(token.op_data_);
    EXPECT_EQ(rai::Chain::RAICOIN, receive->token_.chain_);
    EXPECT_EQ(rai::TokenType::_20, receive->token_.type_);
    EXPECT_EQ(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
        receive->token_.address_.StringHex());
    EXPECT_EQ(rai::TokenSource::LOCAL, receive->source_);
    EXPECT_EQ(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
        receive->from_.StringHex());
    EXPECT_EQ(1, receive->block_height_);
    EXPECT_EQ(
        "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD",
        receive->tx_hash_.StringHex());
    EXPECT_EQ(rai::TokenValue(rai::uint256_t("65536000000000000000000")),
              receive->value_);

    EXPECT_EQ(bytes, extensions.Bytes());
    rai::Extensions extensions2;
    error_code = extensions2.FromJson(extensions.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes, extensions2.Bytes());

    rai::ExtensionToken token2;
    std::vector<uint8_t> bytes2;

    hex = "0004008F";  // type + length
    hex += "050000000201";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "02"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);

    hex = "0004008F";  // type + length
    hex += "050000000001";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "01"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_CHAIN_INVALID, error_code);

    hex = "0004008F";  // type + length
    hex += "050000100001";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "01"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_CHAIN_UNKNOWN, error_code);
    hex = "0004008F";  // type + length
    hex += "050000000101";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "02"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SOURCE_INVALID, error_code);

    hex = "0004008F";  // type + length
    hex += "050000000100";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "01"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_TYPE_INVALID, error_code);

    hex = "0004008F";  // type + length
    hex += "050000000104";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "01"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_TYPE_UNKNOWN, error_code);

    hex = "0004008F";  // type + length
    hex += "050000000101";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "00"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SOURCE_INVALID, error_code);

    hex = "0004008F";  // type + length
    hex += "050000000101";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "04"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SOURCE_UNKNOWN, error_code);

    hex = "0004008F";  // type + length
    hex += "050000000101";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "03"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::STREAM, error_code);

    hex = "0004008F";  // type + length
    hex += "050000000101";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "01"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "FFFFFFFFFFFFFFFF"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "000000000000000000000000000000000000000000000DE0B6B3A76400000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_BLOCK_HEIGHT, error_code);

    hex = "0004008F";  // type + length
    hex += "050000000101";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "02"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_VALUE, error_code);

    hex = "0004008F";  // type + length
    hex += "050000000102";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "01"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);

    hex = "00040093";  // type + length
    hex += "050000000102";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "03"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    hex += "00000003"; // unwrap_chain
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);

    hex = "00040093";  // type + length
    hex += "050000000102";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "03"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    hex += "00000000"; // unwrap_chain
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_CHAIN_INVALID, error_code);

    hex = "00040093";  // type + length
    hex += "050000000102";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "03"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    hex += "00001000"; // unwrap_chain
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_CHAIN_UNKNOWN, error_code);

    hex = "00040093";  // type + length
    hex += "050000000102";  // op_receive + chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "03"; // token source
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // from
    hex += "0000000000000001"; // block height
    hex += "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD"; // tx hash
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value
    hex += "00000001"; // unwrap_chain
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_UNWRAP_CHAIN, error_code);
}

TEST(ExtensionToken, SwapConfig)
{
    std::string hex;
    hex = "00040022";  // type + length
    hex += "0601";  // op_swap + subop
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // main account
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
    EXPECT_EQ(rai::ExtensionToken::Op::SWAP, token.op_);
    EXPECT_EQ(false, token.op_data_ == nullptr);

    auto swap =
        std::static_pointer_cast<rai::ExtensionTokenSwap>(token.op_data_);
    EXPECT_EQ(rai::ExtensionTokenSwap::SubOp::CONFIG, swap->sub_op_);
    EXPECT_EQ(false, swap->sub_data_ == nullptr);
    auto config = std::static_pointer_cast<rai::ExtensionTokenSwapConfig>(
        swap->sub_data_);
    EXPECT_EQ(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
        config->main_.StringHex());

    EXPECT_EQ(bytes, extensions.Bytes());
    rai::Extensions extensions2;
    error_code = extensions2.FromJson(extensions.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes, extensions2.Bytes());

    rai::ExtensionToken token2;
    std::vector<uint8_t> bytes2;

    hex = "00040022";  // type + length
    hex += "0601";  // op_swap + subop
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // main account
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_MAIN_ACCOUNT, error_code);
}

TEST(ExtensionToken, SwapMake)
{
    std::string hex;
    hex = "000400D4";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000000101";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000201";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D1"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010001"; // value_want
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // min_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // max_offer
    hex += "000000001234567F"; // timeout
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
    EXPECT_EQ(rai::ExtensionToken::Op::SWAP, token.op_);
    EXPECT_EQ(false, token.op_data_ == nullptr);

    auto swap =
        std::static_pointer_cast<rai::ExtensionTokenSwap>(token.op_data_);
    EXPECT_EQ(rai::ExtensionTokenSwap::SubOp::MAKE, swap->sub_op_);
    EXPECT_EQ(false, swap->sub_data_ == nullptr);
    auto make = std::static_pointer_cast<rai::ExtensionTokenSwapMake>(
        swap->sub_data_);
    EXPECT_EQ(rai::Chain::RAICOIN, make->token_offer_.chain_);
    EXPECT_EQ(rai::TokenType::_20, make->token_offer_.type_);
    EXPECT_EQ(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
        make->token_offer_.address_.StringHex());
    EXPECT_EQ(rai::Chain::BITCOIN, make->token_want_.chain_);
    EXPECT_EQ(rai::TokenType::_20, make->token_want_.type_);
    EXPECT_EQ(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D1",
        make->token_want_.address_.StringHex());
    EXPECT_EQ(rai::TokenValue(rai::uint256_t("0x10000")), make->value_offer_);
    EXPECT_EQ(rai::TokenValue(rai::uint256_t("0x10001")), make->value_want_);
    EXPECT_EQ(rai::TokenValue(0), make->min_offer_);
    EXPECT_EQ(rai::TokenValue(65536), make->max_offer_);
    EXPECT_EQ(0x1234567F, make->timeout_);

    EXPECT_EQ(bytes, extensions.Bytes());
    rai::Extensions extensions2;
    error_code = extensions2.FromJson(extensions.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes, extensions2.Bytes());

    rai::ExtensionToken token2;
    std::vector<uint8_t> bytes2;

    hex = "000400D4";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000000001";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000201";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D1"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010001"; // value_want
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // min_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // max_offer
    hex += "000000001234567F"; // timeout
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_CHAIN_INVALID, error_code);

    hex = "000400D4";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000100001";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000201";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D1"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010001"; // value_want
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // min_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // max_offer
    hex += "000000001234567F"; // timeout
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_CHAIN_UNKNOWN, error_code);

    hex = "000400D4";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000000101";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000200";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D1"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010001"; // value_want
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // min_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // max_offer
    hex += "000000001234567F"; // timeout
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_TYPE_INVALID, error_code);

    hex = "000400D4";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000000101";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000203";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D1"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010001"; // value_want
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // min_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // max_offer
    hex += "000000001234567F"; // timeout
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_TYPE_UNKNOWN, error_code);

    hex = "000400D4";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000000101";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000101";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010001"; // value_want
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // min_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // max_offer
    hex += "000000001234567F"; // timeout
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_PAIR_EQUAL, error_code);

    hex = "00040094";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000000102";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000102";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010001"; // value_want
    hex += "000000001234567F"; // timeout
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes2, extensions2.Bytes());
    rai::Extensions extensions3;
    error_code = extensions3.FromJson(extensions2.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes2, extensions3.Bytes());

    hex = "00040094";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000000102";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000102";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_want
    hex += "000000001234567F"; // timeout
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_PAIR_EQUAL, error_code);

    hex = "000400D4";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000000101";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000201";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D1"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010001"; // value_want
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // min_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // max_offer
    hex += "000000001234567F"; // timeout
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_VALUE_OFFER, error_code);

    hex = "000400D4";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000000101";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000201";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D1"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // value_want
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // min_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // max_offer
    hex += "000000001234567F"; // timeout
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_VALUE_WANT, error_code);

    hex = "000400D4";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000000101";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000201";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D1"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010001"; // value_want
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // min_offer
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // max_offer
    hex += "000000001234567F"; // timeout
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_MAX_OFFER, error_code);

    hex = "000400D4";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000000101";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000201";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D1"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010001"; // value_want
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // min_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // max_offer
    hex += "000000001234567F"; // timeout
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);

    hex = "000400D4";  // type + length
    hex += "0602";  // op_swap + subop
    hex += "0000000101";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // token address
    hex += "0000000201";  // chain + token type
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D1"; // token address
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // value_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010001"; // value_want
    hex += "0000000000000000000000000000000000000000000000000000000000010001"; // min_offer
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // max_offer
    hex += "000000001234567F"; // timeout
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_MIN_OFFER, error_code);
}

TEST(ExtensionToken, SwapInquiry)
{
    std::string hex;
    hex = "0004007A";  // type + length
    hex += "0603";  // op_swap + subop
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // maker
    hex += "0000000000000100";  // order height
    hex += "0000000000000101";  // trade height
    hex += "100000001234567F"; // timeout
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // token value
    hex += "76E483400A6440E31DE6494A9462EB768F032B9325B16A22E2BD09B812661FE3"; // share
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
    EXPECT_EQ(rai::ExtensionToken::Op::SWAP, token.op_);
    EXPECT_EQ(false, token.op_data_ == nullptr);

    auto swap =
        std::static_pointer_cast<rai::ExtensionTokenSwap>(token.op_data_);
    EXPECT_EQ(rai::ExtensionTokenSwap::SubOp::INQUIRY, swap->sub_op_);
    EXPECT_EQ(false, swap->sub_data_ == nullptr);
    auto inquiry = std::static_pointer_cast<rai::ExtensionTokenSwapInquiry>(
        swap->sub_data_);
    EXPECT_EQ(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
        inquiry->maker_.StringHex());
    EXPECT_EQ(256, inquiry->order_height_);
    EXPECT_EQ(257, inquiry->trade_height_);
    EXPECT_EQ(0x100000001234567F, inquiry->timeout_);
    EXPECT_EQ(rai::TokenValue(65536), inquiry->value_);
    EXPECT_EQ(
        "76E483400A6440E31DE6494A9462EB768F032B9325B16A22E2BD09B812661FE3",
        inquiry->share_.StringHex());

    EXPECT_EQ(bytes, extensions.Bytes());
    rai::Extensions extensions2;
    error_code = extensions2.FromJson(extensions.Json());
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    EXPECT_EQ(bytes, extensions2.Bytes());

    rai::ExtensionToken token2;
    std::vector<uint8_t> bytes2;

    hex = "0004007A";  // type + length
    hex += "0603";  // op_swap + subop
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // maker
    hex += "0000000000000100";  // order height
    hex += "0000000000000101";  // trade height
    hex += "100000001234567F"; // timeout
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // token value
    hex += "76E483400A6440E31DE6494A9462EB768F032B9325B16A22E2BD09B812661FE3"; // share
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_MAKER, error_code);


    hex = "0004007A";  // type + length
    hex += "0603";  // op_swap + subop
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // maker
    hex += "FFFFFFFFFFFFFFFF";  // order height
    hex += "0000000000000101";  // trade height
    hex += "100000001234567F"; // timeout
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // token value
    hex += "76E483400A6440E31DE6494A9462EB768F032B9325B16A22E2BD09B812661FE3"; // share
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_ORDER_HEIGHT, error_code);

    hex = "0004007A";  // type + length
    hex += "0603";  // op_swap + subop
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // maker
    hex += "0000000000000100";  // order height
    hex += "FFFFFFFFFFFFFFFF";  // trade height
    hex += "100000001234567F"; // timeout
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // token value
    hex += "76E483400A6440E31DE6494A9462EB768F032B9325B16A22E2BD09B812661FE3"; // share
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_TRADE_HEIGHT, error_code);

    hex = "0004007A";  // type + length
    hex += "0603";  // op_swap + subop
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // maker
    hex += "0000000000000100";  // order height
    hex += "0000000000000100";  // trade height
    hex += "100000001234567F"; // timeout
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // token value
    hex += "76E483400A6440E31DE6494A9462EB768F032B9325B16A22E2BD09B812661FE3"; // share
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_TRADE_HEIGHT, error_code);

    hex = "0004007A";  // type + length
    hex += "0603";  // op_swap + subop
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // maker
    hex += "0000000000000100";  // order height
    hex += "0000000000000101";  // trade height
    hex += "100000001234567F"; // timeout
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // token value
    hex += "0000000000000000000000000000000000000000000000000000000000000000"; // share
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_SHARE, error_code);

    hex = "0004007A";  // type + length
    hex += "0603";  // op_swap + subop
    hex += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // maker
    hex += "0000000000000100";  // order height
    hex += "0000000000000101";  // trade height
    hex += "100000001234567F"; // timeout
    hex += "0000000000000000000000000000000000000000000000000000000000010000"; // token value
    hex += "DBFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"; // share
    bytes2.clear();
    ret = TestDecodeHex(hex, bytes2);
    EXPECT_EQ(false, ret);
    error_code = extensions2.FromBytes(bytes2);
    EXPECT_EQ(rai::ErrorCode::SUCCESS, error_code);
    count = extensions2.Count(rai::ExtensionType::TOKEN);
    EXPECT_EQ(1, count);
    error_code = token2.FromExtension(extensions2.Get(rai::ExtensionType::TOKEN));
    EXPECT_EQ(rai::ErrorCode::TOKEN_SWAP_SHARE, error_code);
}
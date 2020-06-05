#include <ed25519-donna/ed25519.h>
#include <gtest/gtest.h>
#include <iostream>
#include <rai/core_test/test_util.hpp>
#include <rai/common/blocks.hpp>
#include <string>
#include <vector>

namespace
{
void DumpSignature(const rai::Block& block)
{
    rai::RawKey raw_key;
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    rai::PublicKey public_key;
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    rai::uint512_union signature;
    signature = rai::SignMessage(raw_key, public_key, block.Hash());
    std::cout << signature.StringHex() << std::endl;
}
}

TEST(TxBlock, constructor)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::TxBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, 11,
                       {0, 2, 0, 7, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                       public_key);

    std::string str("");
    str += "0101000100000001";  // type + opcode + credit + counter
    str += "000000005BDBC07E";  // timestamp
    str += "0000000000000001";  // height
                                // account
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    str += hash.StringHex();  // hash
    // representive
    str += "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252";
    str += "00000000000000000000000000000001";  // balance
    // link
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    str += "0000000B";            // extensions_length
    str += "00020007726169636F696E";  // extensions

    size_t size    = str.size() / 2;
    uint8_t* bytes = new uint8_t[size];
    bool ret       = TestDecodeHex(str, bytes, size);
    EXPECT_EQ(false, ret);

    unsigned char hash_expect[32] = "";
    blake2b_state state;
    blake2b_init(&state, sizeof(hash_expect));
    blake2b_update(&state, bytes, size);
    blake2b_final(&state, hash_expect, sizeof(hash_expect));
    delete[] bytes;

    hash = block.Hash();

    std::vector<unsigned char> v1(hash_expect,
                                  hash_expect + sizeof(hash_expect));
    std::vector<unsigned char> v2(hash.bytes.begin(), hash.bytes.end());
    ASSERT_EQ(v1, v2);

    ed25519_signature sign_expect;
    ed25519_sign(hash_expect, sizeof(hash_expect), raw_key.data_.bytes.data(),
                 public_key.bytes.data(), sign_expect);

    rai::Signature signature = block.Signature();

    std::vector<unsigned char> v3(sign_expect,
                                  sign_expect + sizeof(sign_expect));
    std::vector<unsigned char> v4(signature.bytes.begin(),
                                  signature.bytes.end());
    ASSERT_EQ(v3, v4);
}

TEST(TxBlock, equal_or_not)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;
    bool result = false;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    rai::TxBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, 9,
                       {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                       public_key);

    rai::TxBlock block1(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                        hash, representive, balance, link, 9,
                        {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                        public_key);
    result = block == block1;
    ASSERT_EQ(true, result);
    result = block != block1;
    ASSERT_EQ(false, result);

    rai::TxBlock block2(rai::BlockOpcode::RECEIVE, 1, 1, 1541128318, 1, account,
                        hash, representive, balance, link, 9,
                        {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                        public_key);
    result = block == block2;
    ASSERT_EQ(false, result);
    result = block != block2;
    ASSERT_EQ(true, result);

    rai::TxBlock block3(rai::BlockOpcode::SEND, 2, 1, 1541128318, 1, account,
                        hash, representive, balance, link, 9,
                        {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                        public_key);
    result = block == block3;
    ASSERT_EQ(false, result);
    result = block != block3;
    ASSERT_EQ(true, result);

    rai::TxBlock block4(rai::BlockOpcode::SEND, 1, 2, 1541128318, 1, account,
                        hash, representive, balance, link, 9,
                        {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                        public_key);
    result = block == block4;
    ASSERT_EQ(false, result);
    result = block != block4;
    ASSERT_EQ(true, result);

    rai::TxBlock block5(rai::BlockOpcode::SEND, 1, 1, 1541128319, 1, account,
                        hash, representive, balance, link, 9,
                        {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                        public_key);
    result = block == block5;
    ASSERT_EQ(false, result);
    result = block != block5;
    ASSERT_EQ(true, result);

    rai::TxBlock block6(rai::BlockOpcode::SEND, 1, 1, 1541128318, 2, account,
                        hash, representive, balance, link, 9,
                        {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                        public_key);
    result = block == block6;
    ASSERT_EQ(false, result);
    result = block != block6;
    ASSERT_EQ(true, result);

    rai::TxBlock block7(
        rai::BlockOpcode::SEND, 1, 1, 1541128318, 1,
        rai::Account(account.Number() + 1), hash, representive, balance, link,
        9, {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key, public_key);
    result = block == block7;
    ASSERT_EQ(false, result);
    result = block != block7;
    ASSERT_EQ(true, result);

    rai::TxBlock block8(
        rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
        rai::BlockHash(hash.Number() + 1), representive, balance, link, 9,
        {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key, public_key);
    result = block == block8;
    ASSERT_EQ(false, result);
    result = block != block8;
    ASSERT_EQ(true, result);

    rai::TxBlock block9(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                        hash, rai::Account(representive.Number() - 1), balance,
                        link, 9, {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'},
                        raw_key, public_key);
    result = block == block9;
    ASSERT_EQ(false, result);
    result = block != block9;
    ASSERT_EQ(true, result);

    rai::TxBlock block10(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                         hash, representive, rai::Amount(balance.Number() + 1),
                         link, 9, {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'},
                         raw_key, public_key);
    result = block == block10;
    ASSERT_EQ(false, result);
    result = block != block10;
    ASSERT_EQ(true, result);

    rai::TxBlock block11(
        rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account, hash,
        representive, balance, rai::uint256_union(link.Number() + 1), 9,
        {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key, public_key);
    result = block == block11;
    ASSERT_EQ(false, result);
    result = block != block11;
    ASSERT_EQ(true, result);

    rai::TxBlock block12(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                         hash, representive, balance, link, 10,
                         {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                         public_key);
    result = block == block12;
    ASSERT_EQ(false, result);
    result = block != block12;
    ASSERT_EQ(true, result);

    rai::TxBlock block13(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                         hash, representive, balance, link, 9,
                         {1, 1, 'R', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                         public_key);
    result = block == block13;
    ASSERT_EQ(false, result);
    result = block != block13;
    ASSERT_EQ(true, result);

    rai::Block& block_ref = block;
    rai::Block& block1_ref = block1;
    rai::Block& block10_ref = block10;
    result = block_ref == block1_ref;
    ASSERT_EQ(true, result);
    result = block_ref != block1_ref;
    ASSERT_EQ(false, result);
    result = block_ref == block10_ref;
    ASSERT_EQ(false, result);
    result = block_ref != block10_ref;
    ASSERT_EQ(true, result);
}

TEST(TxBlock, serialize_json)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;
    bool result = false;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    rai::TxBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, 11,
                       {0, 1, 0, 7, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                       public_key);

    std::stringstream stream(block.Json());
    rai::Ptree ptree;
    boost::property_tree::read_json(stream, ptree);
    rai::ErrorCode error_code;
    rai::TxBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(block, block2);
}

TEST(TxBlock, deserialize_json_opcode)
{
    std::string str = R"%%%({
    "type": "transaction",
    "opcode": "receive",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "15E23A2EF0D6E0F5B5095F04BE0D157943921927FF0603D435E72D5B333DFEBD956799A292100F759343C1829B98CCAD2D700C7C29299866E89E270EC4BACA00"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockOpcode::RECEIVE, block1.Opcode());

    str.replace(str.find("receive"), std::string("receive").size(), "change");
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    ptree.put<std::string>("signature", "52F671F408E505C72150860F19EB3D66398BB24103DB5A89EF4B94D04F7C29099B6F1A52342C79AEB07C6E2DD3DCC4D81F9735459EBD741757BE09D7ABAD9406");
    rai::TxBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockOpcode::CHANGE, block4.Opcode());

    str.replace(str.find("change"), std::string("change").size(), "reward");
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::BLOCK_OPCODE, error_code);
}

TEST(TxBlock, deserialize_json_credit)
{
    std::string str = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "87C56758DE961356A6E2F7F5571CF38316C77325564FB0D76B62F47C517A2B9561EB7228DE97908086F85DC31529E45880FB807AEF66F77776646E142306590C"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65536",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "0xFFFF",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "01",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "-1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);
}

TEST(TxBlock, deserialize_json_counter)
{
    std::string str = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "1B52157E93D35620E223DD4D08AD5A76142665D10ABE48E0F37E5505A6BDD39630248D70ED09BEE9A500DE467C57E181BFA59E4243B567732DD619CB1254A80A"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967296",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_COUNTER, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "1",
    "counter": "0x1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_COUNTER, error_code);
}

TEST(TxBlock, deserialize_json_timestamp)
{
    std::string str = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "8D33EEDF0B7FED1E4AEED5BDC34A4EAD9D8AE1752FB64B478431F5517F193D04F781E511534FC7DB0C10A8FBE1B4100A640932FB4B5BD6ED53DEE928CE876603"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551616",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_TIMESTAMP, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "0x1",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_TIMESTAMP, error_code);
}

TEST(TxBlock, deserialize_json_height)
{
    std::string str = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "05EEF8D5FD5C6CEBFE3CECE34716D52ACF00E35B5803FCA37D0642D79799E2E9CCB801B0543F23E5CF895134343AC556BAA2AF122C3225EFCB1356C060F29409"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551616",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1",
    "height": "0X1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "01",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "-1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);
}

TEST(TxBlock, deserialize_json_account)
{
    std::string str = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "0000000000000000000000000000000000000000000000000000000000000000",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_ACCOUNT, error_code);
}

TEST(TxBlock, deserialize_json_representative)
{
    std::string str = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_REPRESENTATIVE, error_code);
}

TEST(TxBlock, deserialize_json_balance)
{
    std::string str = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "340282366920938463463374607431768211455",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "C8CA8B6B228B072A33119C4EA50D7135E25369D0D1408234BF29DAA5C6C5E0DFCDFCCA359E941D05DD6612435B310E37716D47B12026344579C5352052F4D401"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "340282366920938463463374607431768211456",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "0x1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "01",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "-1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);
}

TEST(TxBlock, deserialize_json_link)
{
    std::string str = R"%%%({
    "type": "transaction",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_LINK, error_code);
}

TEST(TxBlock, deserialize_json_extensions)
{
    std::string str = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "raicoin"
        },
        {
            "type": "sub_account",
            "value": "unique user id in third party system"
        },
        {
            "type": "1024",
            "value": "74686973206973206120637573746F6D20746C76"
        },
        {
            "type": "65535",
            "value": "7468697320697320616e6f7468657220637573746f6d20746c76"
        }
    ],
    "signature": "EE4C28048CDB9AC4B9B326C60F120676B17CC86B1A2BEBD6ABE19E3054E1BF883007DD1BD435B9319FE94956F93B9559A9CCF19B3FD10362C83032656CB8150A"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "note",
            "value": "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
        },
        {
            "type": "sub_account",
            "value": "unique user id in third party system"
        },
        {
            "type": "1024",
            "value": "74686973206973206120637573746F6D20746C76"
        },
        {
            "type": "65535",
            "value": "7468697320697320616e6f7468657220637573746f6d20746c76"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_EXTENSIONS_LENGTH, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "65536",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_EXTENSION_TYPE, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "65535",
            "value": "raicoin"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_EXTENSION_VALUE, error_code);

    str    = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "65535",
            "value": "0xFFFF0101"
        }
    ],
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_EXTENSION_VALUE, error_code);
}

TEST(TxBlock, deserialize_json_signature)
{
    std::string str = R"%%%({
    "type": "transaction",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "extensions": [
        {
            "type": "sub_account",
            "value": "raicoin"
        }
    ],
    "signature": ""
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_SIGNATURE, error_code);
}

TEST(TxBlock, deserialize)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::TxBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, 11,
                       {0, 1, 0, 7, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                       public_key);
    std::string str("");
    str += "01000100000001";  // opcode + credit + counter
    str += "000000005BDBC07E";  // timestamp
    str += "0000000000000001";  // height
                                // account
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    str += hash.StringHex();  // hash
    // representive
    str += "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252";
    str += "00000000000000000000000000000001";  // balance
    // link
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    str += "0000000B";            // extensions_length
    str += "00010007726169636F696E";  // extensions
    // signature
    str +=
        "1EFFFBDE0313E2F5360B7C50496B2C48A7EEA6B39B3C2F4A21BC051F9F7283E3CAB57A"
        "F79C48C9BE9CDAC8F462A23C57843FC51FF70D04F7AA528244EE68A70C";

    std::vector<uint8_t> bytes;
    bool error = TestDecodeHex(str, bytes);
    ASSERT_FALSE(error);
    rai::BufferStream stream(bytes.data(), bytes.size());
    rai::ErrorCode error_code;
    rai::TxBlock block2(error_code, stream);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(block, block2);

    rai::BufferStream stream3(bytes.data(), bytes.size() - 1);
    rai::TxBlock block3(error_code, stream3);
    ASSERT_EQ(rai::ErrorCode::STREAM, error_code);
}

TEST(TxBlock, serialize)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::TxBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, 11,
                       {0, 1, 0, 7, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                       public_key);
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
    }

    std::string str("");
    str += "0101000100000001";  // type + opcode + credit + counter
    str += "000000005BDBC07E";  // timestamp
    str += "0000000000000001";  // height
                                // account
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    str += hash.StringHex();  // hash
    // representive
    str += "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252";
    str += "00000000000000000000000000000001";  // balance
    // link
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    str += "0000000B";            // extensions_length
    str += "00010007726169636F696E";  // extensions
    // signature
    str +=
        "1EFFFBDE0313E2F5360B7C50496B2C48A7EEA6B39B3C2F4A21BC051F9F7283E3CAB57A"
        "F79C48C9BE9CDAC8F462A23C57843FC51FF70D04F7AA528244EE68A70C";

    std::vector<uint8_t> bytes_expect;
    bool error = TestDecodeHex(str, bytes_expect);
    ASSERT_FALSE(error);
    ASSERT_EQ(bytes_expect, bytes);
    ASSERT_EQ(247, block.Size());
}

TEST(TxBlock, hash)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::TxBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, 9,
                       {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                       public_key);

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
    }
    int ret;
    rai::uint256_union expect_hash;
    blake2b_state state;

    ret = blake2b_init(&state, sizeof(expect_hash.bytes));
    ASSERT_EQ(0, ret);

    blake2b_update(&state, bytes.data(), bytes.size() - 64);

    ret = blake2b_final(&state, expect_hash.bytes.data(),
                        expect_hash.bytes.size());
    ASSERT_EQ(0, ret);

    ASSERT_EQ(expect_hash, block.Hash());
}

TEST(RepBlock, constructor)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    balance.DecodeDec("1");
    link.DecodeHex(
        "0000000000000000000000000000000000000000000000000000000000000000");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::RepBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                        hash, balance, link, raw_key, public_key);

    std::string str("");
    str += "0201000100000001";  // type + opcode + credit + counter
    str += "000000005BDBC07E";  // timestamp
    str += "0000000000000001";  // height
                                // account
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    // previous
    str += "0000000000000000000000000000000000000000000000000000000000000000";
    str += "00000000000000000000000000000001";  // balance
    // link
    str += "0000000000000000000000000000000000000000000000000000000000000000";

    size_t size    = str.size() / 2;
    uint8_t* bytes = new uint8_t[size];
    bool ret       = TestDecodeHex(str, bytes, size);
    EXPECT_EQ(false, ret);

    unsigned char hash_expect[32] = "";
    blake2b_state state;
    blake2b_init(&state, sizeof(hash_expect));
    blake2b_update(&state, bytes, size);
    blake2b_final(&state, hash_expect, sizeof(hash_expect));
    delete[] bytes;

    hash = block.Hash();

    std::vector<unsigned char> v1(hash_expect,
                                  hash_expect + sizeof(hash_expect));
    std::vector<unsigned char> v2(hash.bytes.begin(), hash.bytes.end());
    ASSERT_EQ(v1, v2);

    ed25519_signature sign_expect;
    ed25519_sign(hash_expect, sizeof(hash_expect), raw_key.data_.bytes.data(),
                 public_key.bytes.data(), sign_expect);

    rai::Signature signature = block.Signature();

    std::vector<unsigned char> v3(sign_expect,
                                  sign_expect + sizeof(sign_expect));
    std::vector<unsigned char> v4(signature.bytes.begin(),
                                  signature.bytes.end());
    ASSERT_EQ(v3, v4);
}

TEST(RepBlock, equal_or_not)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;
    bool result = false;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    rai::RepBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                        hash, balance, link, raw_key, public_key);

    rai::RepBlock block1(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                         hash, balance, link, raw_key, public_key);
    result = block == block1;
    ASSERT_EQ(true, result);
    result = block != block1;
    ASSERT_EQ(false, result);

    rai::RepBlock block2(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                         hash, balance, link, raw_key, public_key);
    result = block == block2;
    ASSERT_EQ(true, result);
    result = block != block2;
    ASSERT_EQ(false, result);

    rai::RepBlock block3(rai::BlockOpcode::SEND, 2, 1, 1541128318, 1, account,
                         hash, balance, link, raw_key, public_key);
    result = block == block3;
    ASSERT_EQ(false, result);
    result = block != block3;
    ASSERT_EQ(true, result);

    rai::RepBlock block4(rai::BlockOpcode::SEND, 1, 2, 1541128318, 1, account,
                         hash, balance, link, raw_key, public_key);
    result = block == block4;
    ASSERT_EQ(false, result);
    result = block != block4;
    ASSERT_EQ(true, result);

    rai::RepBlock block5(rai::BlockOpcode::SEND, 1, 1, 1541128319, 1, account,
                         hash, balance, link, raw_key, public_key);
    result = block == block5;
    ASSERT_EQ(false, result);
    result = block != block5;
    ASSERT_EQ(true, result);

    rai::RepBlock block6(rai::BlockOpcode::SEND, 1, 1, 1541128318, 2, account,
                         hash, balance, link, raw_key, public_key);
    result = block == block6;
    ASSERT_EQ(false, result);
    result = block != block6;
    ASSERT_EQ(true, result);

    rai::RepBlock block7(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1,
                         rai::Account(account.Number() + 1), hash, balance,
                         link, raw_key, public_key);
    result = block == block7;
    ASSERT_EQ(false, result);
    result = block != block7;
    ASSERT_EQ(true, result);

    rai::RepBlock block8(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                         rai::BlockHash(hash.Number() + 1), balance, link,
                         raw_key, public_key);
    result = block == block8;
    ASSERT_EQ(false, result);
    result = block != block8;
    ASSERT_EQ(true, result);

    rai::RepBlock block9(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                         hash, rai::Amount(balance.Number() + 1), link, raw_key,
                         public_key);
    result = block == block9;
    ASSERT_EQ(false, result);
    result = block != block9;
    ASSERT_EQ(true, result);

    rai::RepBlock block10(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                          hash, balance, rai::uint256_union(link.Number() + 1),
                          raw_key, public_key);
    result = block == block10;
    ASSERT_EQ(false, result);
    result = block != block10;
    ASSERT_EQ(true, result);

    rai::Block& block_ref = block;
    rai::Block& block1_ref = block1;
    rai::Block& block10_ref = block10;
    result = block_ref == block1_ref;
    ASSERT_EQ(true, result);
    result = block_ref != block1_ref;
    ASSERT_EQ(false, result);
    result = block_ref == block10_ref;
    ASSERT_EQ(false, result);
    result = block_ref != block10_ref;
    ASSERT_EQ(true, result);
}

TEST(RepBlock, serialize_json)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;
    bool result = false;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    rai::RepBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                        hash, balance, link, raw_key, public_key);

    std::stringstream stream(block.Json());
    rai::Ptree ptree;
    boost::property_tree::read_json(stream, ptree);
    rai::ErrorCode error_code;
    rai::RepBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(block, block2);
}

TEST(RepBlock, deserialize_json_opcode)
{
    std::string str = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6AF3097F33B3F5EF07590654DBC379186234CA59E70FE9135BB26DFFC87D3C78C86D96C6534EA9D923EE8248FCF1306790A732FC460B4A3760BECB4268A7F60C"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockOpcode::SEND, block1.Opcode());

    str    = R"%%%({
    "type": "representative",
    "opcode": "receive",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "0000000000000000000000000000000000000000000000000000000000000000",
    "signature": "EDCFA2CEB3D1CE4CB3350DDABB6A501BD756F236F675312DC1E9698AC8162AB210845ABA724FFDAA04F3F6AF1AB948590B8A52268BA92CFD9377DEADFCCAFF00"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockOpcode::RECEIVE, block2.Opcode());

    str.replace(str.find("receive"), std::string("receive").size(), "reward");
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    ptree.put<std::string>("signature", "D91FF6492BB9BC7878A86EF9B3C883A951D7C1ED20A8A76B96459E1C1BE0D02B4C1D0F4F6CA057F53283FD20C66918597549970E2E3454C95484DB78F9C43503");
    rai::RepBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockOpcode::REWARD, block4.Opcode());

    str.replace(str.find("reward"), std::string("reward").size(), "change");
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::BLOCK_OPCODE, error_code);
}

TEST(RepBlock, deserialize_json_credit)
{
    std::string str = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "65535",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "922E2FF9161480735CB46F211AE361C49C32AA3A86E9B3379DEFB4C61E78A5852DB4CA906707D70065A3C01D7B62BB33CF4D5813296E7FE1D05C87A389D4CC0A"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "65536",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "0xFFFF",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "01",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "-1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);
}

TEST(RepBlock, deserialize_json_counter)
{
    std::string str = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6AE94666C3134928B653AC921FAE9BD869CA2825CD1B1A6B39BA17D7B811401D1B96961075997ED8357844D5D5F0DFFB918EDA09AC429626102795881548600A"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967296",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_COUNTER, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "0x1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_COUNTER, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "-1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_COUNTER, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "01",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_COUNTER, error_code);
}

TEST(RepBlock, deserialize_json_timestamp)
{
    std::string str = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "71032C4EA079A24FB77E26B212008796400968864A98DC361D0914B3AB4DC7F4D5C2E63EA5CF6E5E924F3635F14C9B596B3D6224E2D9B11247A228ECF1963E04"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551616",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_TIMESTAMP, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "0x1",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_TIMESTAMP, error_code);
}

TEST(RepBlock, deserialize_json_height)
{
    std::string str = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "D5EB09C4E712462655F24489FE699B0A94C31EA9B5A0A9C13E167B523D75326BDD52E0DC5F2645871783D39182B63232E90F07143116D719BAD90FCA483EDE0D"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551616",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1",
    "height": "0X1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "01",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "-1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);
}

TEST(RepBlock, deserialize_json_account)
{
    std::string str = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "0000000000000000000000000000000000000000000000000000000000000000",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_ACCOUNT, error_code);
}

TEST(RepBlock, deserialize_json_balance)
{
    std::string str = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "340282366920938463463374607431768211455",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "32562974D0D6D173730DB22D9262D329B0D29883773BB2499489A1F212793FE37F8AFFFF246C12C2C656067EBBDF83A4C98E6D477DE23DCE88310493D8B0AE04"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "340282366920938463463374607431768211456",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "0x1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "01",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);

    str    = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "-1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);
}

TEST(RepBlock, deserialize_json_link)
{
    std::string str = R"%%%({
    "type": "representative",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_LINK, error_code);
}

TEST(RepBlock, deserialize_json_signature)
{
    std::string str = R"%%%({
    "type": "representative",
    "opcode": "send",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": ""
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_SIGNATURE, error_code);
}

TEST(RepBlock, deserialize)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    balance.DecodeDec("1");
    link.DecodeHex(
        "0000000000000000000000000000000000000000000000000000000000000000");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::RepBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                        hash, balance, link, raw_key, public_key);

    std::string str("");
    str += "01000100000001";  // opcode + credit + counter
    str += "000000005BDBC07E";  // timestamp
    str += "0000000000000001";  // height
                                // account
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    // previous
    str += "0000000000000000000000000000000000000000000000000000000000000000";
    str += "00000000000000000000000000000001";  // balance
    // link
    str += "0000000000000000000000000000000000000000000000000000000000000000";
    // signature
    str +=
        "D4959CC24A8FA672CD5FC401101174955B751C696CCA5FB318181E4D26A4366CC02BA0"
        "CD7563E3772ED1C9717A47C03466BF9A03AE3C74D25C8C58205215900F";

    std::vector<uint8_t> bytes;
    bool error = TestDecodeHex(str, bytes);
    ASSERT_FALSE(error);
    rai::BufferStream stream(bytes.data(), bytes.size());
    rai::ErrorCode error_code;
    rai::RepBlock block2(error_code, stream);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(block, block2);

    rai::BufferStream stream3(bytes.data(), bytes.size() - 1);
    rai::RepBlock block3(error_code, stream3);
    ASSERT_EQ(rai::ErrorCode::STREAM, error_code);
}

TEST(RepBlock, serialize)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    balance.DecodeDec("1");
    link.DecodeHex(
        "0000000000000000000000000000000000000000000000000000000000000000");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::RepBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                        hash, balance, link, raw_key, public_key);
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
    }

    std::string str("");
    str += "0201000100000001";  // type + opcode + credit + counter
    str += "000000005BDBC07E";  // timestamp
    str += "0000000000000001";  // height
                                // account
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    // previous
    str += "0000000000000000000000000000000000000000000000000000000000000000";
    str += "00000000000000000000000000000001";  // balance
    // link
    str += "0000000000000000000000000000000000000000000000000000000000000000";
    // signature
    str +=
        "D4959CC24A8FA672CD5FC401101174955B751C696CCA5FB318181E4D26A4366CC02BA0"
        "CD7563E3772ED1C9717A47C03466BF9A03AE3C74D25C8C58205215900F";

    std::vector<uint8_t> bytes_expect;
    bool error = TestDecodeHex(str, bytes_expect);
    ASSERT_FALSE(error);
    ASSERT_EQ(bytes_expect, bytes);
    ASSERT_EQ(200, block.Size());
}

TEST(RepBlock, hash)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    balance.DecodeDec("1");
    link.DecodeHex(
        "0000000000000000000000000000000000000000000000000000000000000000");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::RepBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                        hash, balance, link, raw_key, public_key);
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
    }

    int ret;
    rai::uint256_union expect_hash;
    blake2b_state state;

    ret = blake2b_init(&state, sizeof(expect_hash.bytes));
    ASSERT_EQ(0, ret);

    blake2b_update(&state, bytes.data(), bytes.size() - 64);

    ret = blake2b_final(&state, expect_hash.bytes.data(),
                        expect_hash.bytes.size());
    ASSERT_EQ(0, ret);

    ASSERT_EQ(expect_hash, block.Hash());
}

TEST(AdBlock, constructor)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "0000000000000000000000000000000000000000000000000000000000000000");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::AdBlock block(rai::BlockOpcode::DESTROY, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, raw_key, public_key);

    std::string str("");
    str += "0306000100000001";  // type + opcode + credit + counter
    str += "000000005BDBC07E";  // timestamp
    str += "0000000000000001";  // height
                                // account
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    str += hash.StringHex();  // hash
    // representive
    str += "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252";
    str += "00000000000000000000000000000001";  // balance
    // link
    str += "0000000000000000000000000000000000000000000000000000000000000000";

    size_t size    = str.size() / 2;
    uint8_t* bytes = new uint8_t[size];
    bool ret       = TestDecodeHex(str, bytes, size);
    EXPECT_EQ(false, ret);

    unsigned char hash_expect[32] = "";
    blake2b_state state;
    blake2b_init(&state, sizeof(hash_expect));
    blake2b_update(&state, bytes, size);
    blake2b_final(&state, hash_expect, sizeof(hash_expect));
    delete[] bytes;

    hash = block.Hash();

    std::vector<unsigned char> v1(hash_expect,
                                  hash_expect + sizeof(hash_expect));
    std::vector<unsigned char> v2(hash.bytes.begin(), hash.bytes.end());
    ASSERT_EQ(v1, v2);

    ed25519_signature sign_expect;
    ed25519_sign(hash_expect, sizeof(hash_expect), raw_key.data_.bytes.data(),
                 public_key.bytes.data(), sign_expect);

    rai::Signature signature = block.Signature();

    std::vector<unsigned char> v3(sign_expect,
                                  sign_expect + sizeof(sign_expect));
    std::vector<unsigned char> v4(signature.bytes.begin(),
                                  signature.bytes.end());
    ASSERT_EQ(v3, v4);
}


TEST(AdBlock, equal_or_not)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;
    bool result = false;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    rai::AdBlock block(rai::BlockOpcode::RECEIVE, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, raw_key, public_key);

    rai::AdBlock block1(rai::BlockOpcode::RECEIVE, 1, 1, 1541128318, 1, account,
                        hash, representive, balance, link, raw_key, public_key);
    result = block == block1;
    ASSERT_EQ(true, result);
    result = block != block1;
    ASSERT_EQ(false, result);

    rai::AdBlock block2(rai::BlockOpcode::CHANGE, 1, 1, 1541128318, 1, account,
                        hash, representive, balance, link, raw_key, public_key);
    result = block == block2;
    ASSERT_EQ(false, result);
    result = block != block2;
    ASSERT_EQ(true, result);

    rai::AdBlock block3(rai::BlockOpcode::RECEIVE, 2, 1, 1541128318, 1, account,
                        hash, representive, balance, link, raw_key, public_key);
    result = block == block3;
    ASSERT_EQ(false, result);
    result = block != block3;
    ASSERT_EQ(true, result);

    rai::AdBlock block4(rai::BlockOpcode::RECEIVE, 1, 2, 1541128318, 1, account,
                        hash, representive, balance, link, raw_key, public_key);
    result = block == block4;
    ASSERT_EQ(false, result);
    result = block != block4;
    ASSERT_EQ(true, result);

    rai::AdBlock block5(rai::BlockOpcode::RECEIVE, 1, 1, 1541128319, 1, account,
                        hash, representive, balance, link, raw_key, public_key);
    result = block == block5;
    ASSERT_EQ(false, result);
    result = block != block5;
    ASSERT_EQ(true, result);

    rai::AdBlock block6(rai::BlockOpcode::RECEIVE, 1, 1, 1541128318, 2, account,
                        hash, representive, balance, link, raw_key, public_key);
    result = block == block6;
    ASSERT_EQ(false, result);
    result = block != block6;
    ASSERT_EQ(true, result);

    rai::AdBlock block7(rai::BlockOpcode::RECEIVE, 1, 1, 1541128318, 1,
                        rai::Account(account.Number() + 1), hash, representive,
                        balance, link, raw_key, public_key);
    result = block == block7;
    ASSERT_EQ(false, result);
    result = block != block7;
    ASSERT_EQ(true, result);

    rai::AdBlock block8(rai::BlockOpcode::RECEIVE, 1, 1, 1541128318, 1, account,
                        rai::BlockHash(hash.Number() + 1), representive,
                        balance, link, raw_key, public_key);
    result = block == block8;
    ASSERT_EQ(false, result);
    result = block != block8;
    ASSERT_EQ(true, result);

    rai::AdBlock block9(rai::BlockOpcode::RECEIVE, 1, 1, 1541128318, 1, account,
                        hash, rai::Account(representive.Number() - 1), balance,
                        link, raw_key, public_key);
    result = block == block9;
    ASSERT_EQ(false, result);
    result = block != block9;
    ASSERT_EQ(true, result);

    rai::AdBlock block10(rai::BlockOpcode::RECEIVE, 1, 1, 1541128318, 1,
                         account, hash, representive,
                         rai::Amount(balance.Number() + 1), link, raw_key,
                         public_key);
    result = block == block10;
    ASSERT_EQ(false, result);
    result = block != block10;
    ASSERT_EQ(true, result);

    rai::AdBlock block11(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                         hash, representive, balance,
                         rai::uint256_union(link.Number() + 1), raw_key,
                         public_key);
    result = block == block11;
    ASSERT_EQ(false, result);
    result = block != block11;
    ASSERT_EQ(true, result);

    rai::Block& block_ref = block;
    rai::Block& block1_ref = block1;
    rai::Block& block10_ref = block10;
    result = block_ref == block1_ref;
    ASSERT_EQ(true, result);
    result = block_ref != block1_ref;
    ASSERT_EQ(false, result);
    result = block_ref == block10_ref;
    ASSERT_EQ(false, result);
    result = block_ref != block10_ref;
    ASSERT_EQ(true, result);
}

TEST(AdBlock, serialize_json)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;
    bool result = false;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    rai::AdBlock block(rai::BlockOpcode::CHANGE, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, raw_key, public_key);

    std::stringstream stream(block.Json());
    rai::Ptree ptree;
    boost::property_tree::read_json(stream, ptree);
    rai::ErrorCode error_code;
    rai::AdBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(block, block2);
}

TEST(AdBlock, deserialize_json_opcode)
{
    std::string str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "7E60727B17368093FF1C58639D8E2F7C9C03F8E6F4940753664714EEB72D5E496A1DB394092A7A473C71FB4CDB07683874F1C103D270ED4CACEC5AFBAADE6204"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockOpcode::RECEIVE, block1.Opcode());

    str.replace(str.find("receive"), std::string("receive").size(), "change");
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    ptree.put<std::string>("signature", "5355EC310FDAE559519B65070E2A0286F49DE54079D4170F62A212801DF8DD5E36720E53A47A4096E98FE11DDDBE50DBC569EAD3EE350E5907CA394CFB8DAE03");
    rai::AdBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockOpcode::CHANGE, block4.Opcode());

    str.replace(str.find("change"), std::string("change").size(), "destroy");
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    ptree.put<std::string>("signature", "9C951A2C381BAD69E8BBA671A9541C9C6AB44FD9E73B369BED2C4BEA6818755D225241C8907B86849B2D3BA9055BAAFBC322E658EDFD48330CE3102DECFCE102");
    rai::AdBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockOpcode::DESTROY, block5.Opcode());

    str.replace(str.find("destroy"), std::string("destroy").size(), "reward");
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::BLOCK_OPCODE, error_code);
}

TEST(AdBlock, deserialize_json_credit)
{
    std::string str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "68EDBFAB5AF1BC676F5197CB75110CF39256BEE7DC2B236210E66E4B202FBA0A06725E9E1BAED5994F59219122FC8B0F8FE9F0AA39DA0D5CEAA255AD39205B0E"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65536",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "68EDBFAB5AF1BC676F5197CB75110CF39256BEE7DC2B236210E66E4B202FBA0A06725E9E1BAED5994F59219122FC8B0F8FE9F0AA39DA0D5CEAA255AD39205B0E"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "0xFFFF",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "68EDBFAB5AF1BC676F5197CB75110CF39256BEE7DC2B236210E66E4B202FBA0A06725E9E1BAED5994F59219122FC8B0F8FE9F0AA39DA0D5CEAA255AD39205B0E"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "01",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "68EDBFAB5AF1BC676F5197CB75110CF39256BEE7DC2B236210E66E4B202FBA0A06725E9E1BAED5994F59219122FC8B0F8FE9F0AA39DA0D5CEAA255AD39205B0E"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);


    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "0xFFFF",
    "counter": "-1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "68EDBFAB5AF1BC676F5197CB75110CF39256BEE7DC2B236210E66E4B202FBA0A06725E9E1BAED5994F59219122FC8B0F8FE9F0AA39DA0D5CEAA255AD39205B0E"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);


    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "68EDBFAB5AF1BC676F5197CB75110CF39256BEE7DC2B236210E66E4B202FBA0A06725E9E1BAED5994F59219122FC8B0F8FE9F0AA39DA0D5CEAA255AD39205B0E"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);
}

TEST(AdBlock, deserialize_json_counter)
{
    std::string str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "0A646B2A6A290FD006F54A5EA1FAFC9D8DAE4C2B2F59B095ADCEF425A1D304FEFEB37698BFD1F3EB30278E57484CB241E6BD92C9F3356C90C7484BED94B5750D"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967296",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "68EDBFAB5AF1BC676F5197CB75110CF39256BEE7DC2B236210E66E4B202FBA0A06725E9E1BAED5994F59219122FC8B0F8FE9F0AA39DA0D5CEAA255AD39205B0E"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_COUNTER, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "0x1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "68EDBFAB5AF1BC676F5197CB75110CF39256BEE7DC2B236210E66E4B202FBA0A06725E9E1BAED5994F59219122FC8B0F8FE9F0AA39DA0D5CEAA255AD39205B0E"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_COUNTER, error_code);
}

TEST(AdBlock, deserialize_json_timestamp)
{
    std::string str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "9A13CEAADD05C9B72E45477F59F76E2060C166A6A2CD53379B0D5AA1B8D7993A22B5F379A4E58CE313F37C1FA99A6AD9558BECCB3CA9E82FCF2BDB9BC9951406"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551616",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "0A646B2A6A290FD006F54A5EA1FAFC9D8DAE4C2B2F59B095ADCEF425A1D304FEFEB37698BFD1F3EB30278E57484CB241E6BD92C9F3356C90C7484BED94B5750D"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_TIMESTAMP, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "0x1",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "0A646B2A6A290FD006F54A5EA1FAFC9D8DAE4C2B2F59B095ADCEF425A1D304FEFEB37698BFD1F3EB30278E57484CB241E6BD92C9F3356C90C7484BED94B5750D"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_TIMESTAMP, error_code);
}

TEST(AdBlock, deserialize_json_height)
{
    std::string str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "D499D46818BF5DC0D2BBEA8A81F6F2D63F3CB03879FADA14500FB71BE19E815BBBAFE3CEC6C17FE07A6EBFFBD9592E7A2A8C16AC9069A5972AA11DEF5D5DE80A"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551616",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "9A13CEAADD05C9B72E45477F59F76E2060C166A6A2CD53379B0D5AA1B8D7993A22B5F379A4E58CE313F37C1FA99A6AD9558BECCB3CA9E82FCF2BDB9BC9951406"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "0x1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "9A13CEAADD05C9B72E45477F59F76E2060C166A6A2CD53379B0D5AA1B8D7993A22B5F379A4E58CE313F37C1FA99A6AD9558BECCB3CA9E82FCF2BDB9BC9951406"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "9A13CEAADD05C9B72E45477F59F76E2060C166A6A2CD53379B0D5AA1B8D7993A22B5F379A4E58CE313F37C1FA99A6AD9558BECCB3CA9E82FCF2BDB9BC9951406"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "01",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "9A13CEAADD05C9B72E45477F59F76E2060C166A6A2CD53379B0D5AA1B8D7993A22B5F379A4E58CE313F37C1FA99A6AD9558BECCB3CA9E82FCF2BDB9BC9951406"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "-1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252",
    "signature": "9A13CEAADD05C9B72E45477F59F76E2060C166A6A2CD53379B0D5AA1B8D7993A22B5F379A4E58CE313F37C1FA99A6AD9558BECCB3CA9E82FCF2BDB9BC9951406"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);
}

TEST(AdBlock, deserialize_json_account)
{
    std::string str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "0000000000000000000000000000000000000000000000000000000000000000",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "0000000000000000000000000000000000000000000000000000000000000000",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_ACCOUNT, error_code);
}

TEST(AdBlock, deserialize_json_representative)
{
    std::string str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "1",
    "link": "0000000000000000000000000000000000000000000000000000000000000000",
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_REPRESENTATIVE, error_code);
}

TEST(AdBlock, deserialize_json_balance)
{
    std::string str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "340282366920938463463374607431768211455",
    "link": "0000000000000000000000000000000000000000000000000000000000000000",
    "signature": "F5352B1A581310CD4095A63EB8D31CEDAB29189BD03D091EAF985C257BDA9CE18E4052661C957D1F6CFEEBD29935336663E0DCDF7C2E6F06C9BAA47474F1CB0A"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "340282366920938463463374607431768211456",
    "link": "0000000000000000000000000000000000000000000000000000000000000000",
    "signature": "F5352B1A581310CD4095A63EB8D31CEDAB29189BD03D091EAF985C257BDA9CE18E4052661C957D1F6CFEEBD29935336663E0DCDF7C2E6F06C9BAA47474F1CB0A"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "0x1",
    "link": "0000000000000000000000000000000000000000000000000000000000000000",
    "signature": "F5352B1A581310CD4095A63EB8D31CEDAB29189BD03D091EAF985C257BDA9CE18E4052661C957D1F6CFEEBD29935336663E0DCDF7C2E6F06C9BAA47474F1CB0A"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "",
    "link": "0000000000000000000000000000000000000000000000000000000000000000",
    "signature": "F5352B1A581310CD4095A63EB8D31CEDAB29189BD03D091EAF985C257BDA9CE18E4052661C957D1F6CFEEBD29935336663E0DCDF7C2E6F06C9BAA47474F1CB0A"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "01",
    "link": "0000000000000000000000000000000000000000000000000000000000000000",
    "signature": "F5352B1A581310CD4095A63EB8D31CEDAB29189BD03D091EAF985C257BDA9CE18E4052661C957D1F6CFEEBD29935336663E0DCDF7C2E6F06C9BAA47474F1CB0A"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);

    str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "-1",
    "link": "0000000000000000000000000000000000000000000000000000000000000000",
    "signature": "F5352B1A581310CD4095A63EB8D31CEDAB29189BD03D091EAF985C257BDA9CE18E4052661C957D1F6CFEEBD29935336663E0DCDF7C2E6F06C9BAA47474F1CB0A"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BALANCE, error_code);
}

TEST(AdBlock, deserialize_json_link)
{
    std::string str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "340282366920938463463374607431768211455",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "signature": "F5352B1A581310CD4095A63EB8D31CEDAB29189BD03D091EAF985C257BDA9CE18E4052661C957D1F6CFEEBD29935336663E0DCDF7C2E6F06C9BAA47474F1CB0A"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_LINK, error_code);
}

TEST(AdBlock, deserialize_json_signature)
{
    std::string str = R"%%%({
    "type": "airdrop",
    "opcode": "receive",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "340282366920938463463374607431768211455",
    "link": "0000000000000000000000000000000000000000000000000000000000000000",
    "signature": "F5352B1A581310CD4095A63EB8D31CEDAB29189BD03D091EAF985C257BDA9CE18E4052661C957D1F6CFEEBD29935336663E0DCDF7C2E6F06C9BAA47474F1CB0"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_SIGNATURE, error_code);
}

TEST(AdBlock, deserialize)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::AdBlock block(rai::BlockOpcode::DESTROY, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, raw_key, public_key);
    std::string str("");
    str += "06000100000001";  // opcode + credit + counter
    str += "000000005BDBC07E";  // timestamp
    str += "0000000000000001";  // height
                                // account
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    str += hash.StringHex();  // hash
    // representive
    str += "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252";
    str += "00000000000000000000000000000001";  // balance
    // link
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    // signature
    str +=
        "2E119431BA93B502C2B0E3A8AF45C7561830FE7B952C19ACF60A2E425BC9201D425110488907CBEE54E54FA1FEF446AB0ED4DAF41321E027D8C28FC65D151107";

    std::vector<uint8_t> bytes;
    bool error = TestDecodeHex(str, bytes);
    ASSERT_FALSE(error);
    rai::BufferStream stream(bytes.data(), bytes.size());
    rai::ErrorCode error_code;
    rai::AdBlock block2(error_code, stream);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(block, block2);

    rai::BufferStream stream3(bytes.data(), bytes.size() - 1);
    rai::AdBlock block3(error_code, stream3);
    ASSERT_EQ(rai::ErrorCode::STREAM, error_code);
}

TEST(AdBlock, serialize)
{

    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::AdBlock block(rai::BlockOpcode::DESTROY, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, raw_key, public_key);
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
    }
                       
    std::string str("");
    str += "0306000100000001";  // type + opcode + credit + counter
    str += "000000005BDBC07E";  // timestamp
    str += "0000000000000001";  // height
                                // account
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    str += hash.StringHex();  // hash
    // representive
    str += "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252";
    str += "00000000000000000000000000000001";  // balance
    // link
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    // signature
    str +=
        "2E119431BA93B502C2B0E3A8AF45C7561830FE7B952C19ACF60A2E425BC9201D425110488907CBEE54E54FA1FEF446AB0ED4DAF41321E027D8C28FC65D151107";
    std::vector<uint8_t> bytes_expect;
    bool error = TestDecodeHex(str, bytes_expect);
    ASSERT_FALSE(error);
    ASSERT_EQ(bytes_expect, bytes);
    ASSERT_EQ(232, block.Size());
}

TEST(blocks, DeserializeBlockJson_TxBlock)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::TxBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, 11,
                       {0, 1, 0, 7, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                       public_key);
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(block.Json());
    boost::property_tree::read_json(stream, ptree);
    std::unique_ptr<rai::Block> ptr =
        rai::DeserializeBlockJson(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockType::TX_BLOCK, ptr->Type());
}

TEST(blocks, DeserializeBlockJson_RepBlock)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    balance.DecodeDec("1");
    link.DecodeHex(
        "0000000000000000000000000000000000000000000000000000000000000000");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::RepBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                        hash, balance, link, raw_key, public_key);
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(block.Json());
    boost::property_tree::read_json(stream, ptree);
    std::unique_ptr<rai::Block> ptr =
        rai::DeserializeBlockJson(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockType::REP_BLOCK, ptr->Type());
}

TEST(blocks, DeserializeBlockJson_AdBlock)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::AdBlock block(rai::BlockOpcode::DESTROY, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, raw_key, public_key);
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(block.Json());
    boost::property_tree::read_json(stream, ptree);
    std::unique_ptr<rai::Block> ptr =
        rai::DeserializeBlockJson(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockType::AD_BLOCK, ptr->Type());
}

TEST(blocks, DeserializeBlock_TxBlock)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::TxBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, 9,
                       {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
                       public_key);
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
    }

    rai::BufferStream stream(bytes.data(), bytes.size());
    rai::ErrorCode error_code;
    std::unique_ptr<rai::Block> ptr = rai::DeserializeBlock(error_code, stream);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(block, *ptr);
}

TEST(blocks, DeserializeBlock_RepBlock)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    balance.DecodeDec("1");
    link.DecodeHex(
        "0000000000000000000000000000000000000000000000000000000000000000");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::RepBlock block(rai::BlockOpcode::SEND, 1, 1, 1541128318, 1, account,
                        hash, balance, link, raw_key, public_key);
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
    }

    rai::BufferStream stream(bytes.data(), bytes.size());
    rai::ErrorCode error_code;
    std::unique_ptr<rai::Block> ptr = rai::DeserializeBlock(error_code, stream);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(block, *ptr);
}

TEST(blocks, DeserializeBlock_AdBlock)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Account representive;
    rai::Amount balance;
    rai::uint256_union link;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    representive.DecodeHex(
        "0311B25E0D1E1D7724BBA5BD523954F1DBCFC01CB8671D55ED2D32C7549FB252");
    balance.DecodeDec("1");
    link.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::AdBlock block(rai::BlockOpcode::DESTROY, 1, 1, 1541128318, 1, account,
                       hash, representive, balance, link, raw_key, public_key);
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
    }

    rai::BufferStream stream(bytes.data(), bytes.size());
    rai::ErrorCode error_code;
    std::unique_ptr<rai::Block> ptr = rai::DeserializeBlock(error_code, stream);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(block, *ptr);
}

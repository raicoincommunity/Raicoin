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
                       hash, representive, balance, link, 9,
                       {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
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
    str += "00000009";            // note_length
    str += "0101726169636F696E";  // note_data

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
                       hash, representive, balance, link, 9,
                       {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
    "signature": "1140988520DD99E3FFC30A81303F1BEAA84BF974E12A95AB4B700CAE8D22D6893698C8CEC1B879C4939F486F91EA748EE5D45D9A1D32B1D7896C1ED3BF659302"
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
    ptree.put<std::string>("signature", "D5BA3123E0559C41F14B0BE00988189603971785CEB83D7E5D8D00FAD6B82029E269186A103486C62E17C071C681F5E10D1FFB63DA019C7C602D63A6E245EC0B");
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
    "signature": "FA25BFF7B0AF4C2B7456D3F77D9C8074EA89083595477D5189F8D7594C9B4DCDD593329C93220D7CFC01C1A1DCDE702114A63D33FE25AD6CD9ABE5C40A217B07"
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
    "signature": "1301BADA5E3455D8C31F533C76EC108A7A395137C69D64BA1D892AE2D0156D0F68530AF96AC87A405F1D4D3DC1034042DB11DF8ADF31095B165DBED55E10C107"
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
    "counter": "4294967296",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "representative": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "balance": "1",
    "link": "rai_11rjpbh1t9ixgwkdqbfxcawobwgusz13sg595ocytdbkrxcbzekkcqkc3dn1",
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
    "signature": "73BDA9F1A835E580C07DF3364F9CBC4AC3CC6C686F8A05B698A036DDDF2EF217F95AA50110D58BFC1920973BE08C982FAFA418743A4C710DE3A11E7DD36D1006"
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
    "signature": "1C60BA94AC235A14731E94208A0BEAA98C1BBEB17F74CDE65FFFE3B7B962D24BE247FB0157BD7F1311A4D37DFA469908222F4FFC5E94723E472A1BE43B7C4C0A"
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "1",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
    "signature": "5A695DD4FB9B197771BA959F563982744B84CD2A640ACF448FF1EF791BAE9125FBEFEBA538747497CCE4410AB7CAC381CAF17831F9245E3C56D5EAC47CD9CB0E"
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_LINK, error_code);
}

TEST(TxBlock, deserialize_json_note)
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
    "note_length": "4294967295",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::NOTE_LENGTH, error_code);

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
    "note_length": "4294967296",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_NOTE_LENGTH, error_code);

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
    "note_length": "9",
    "note": {
        "type": "",
        "encode": "utf8",
        "data": "raicoin"
    },
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_NOTE_TYPE, error_code);

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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf-8",
        "data": "raicoin"
    },
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_NOTE_ENCODE, error_code);

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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin_"
    },
    "signature": "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_NOTE_DATA, error_code);
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
    "note_length": "9",
    "note": {
        "type": "text",
        "encode": "utf8",
        "data": "raicoin"
    },
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
                       hash, representive, balance, link, 9,
                       {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
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
    str += "00000009";            // note_length
    str += "0101726169636F696E";  // note_data
    // signature
    str +=
        "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A"
        "661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07";

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
                       hash, representive, balance, link, 9,
                       {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
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
    str += "00000009";            // note_length
    str += "0101726169636F696E";  // note_data
    // signature
    str +=
        "6411A4E7F1BA3FF832BB6D82462FC11679A477FF471E3C755A4E00814FA3F2D0C7ED3A"
        "661A72C4E21BDF210EF1D3053801EA0D1A295C22CB132CEE8939225C07";

    std::vector<uint8_t> bytes_expect;
    bool error = TestDecodeHex(str, bytes_expect);
    ASSERT_FALSE(error);
    ASSERT_EQ(bytes_expect, bytes);
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
}

#if 0
TEST(AdminBlock, constructor)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount price;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    price.DecodeDec("1");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::AdminBlock block(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1, account,
                          hash, price, 1541128318, 1543720318, raw_key,
                          public_key);

    std::string str("");
    str += "0307000100000001";  // type + opcode + credit + counter
    str += "000000005BDBC07E";  // timestamp
    str += "0000000000000001";  // height
                                // account
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    // previous
    str += "0000000000000000000000000000000000000000000000000000000000000000";
    str += "00000000000000000000000000000001";  // price
    str += "000000005BDBC07E";                  // begin_time
    str += "000000005C034D7E";                  // end_time

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

TEST(AdminBlock, equal)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount price;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    price.DecodeDec("1");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::AdminBlock block(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1, account,
                          hash, price, 1541128318, 1543720318, raw_key,
                          public_key);

    rai::AdminBlock block1(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1,
                           account, hash, price, 1541128318, 1543720318,
                           raw_key, public_key);
    bool result = block == block1;
    ASSERT_EQ(true, result);

    rai::AdminBlock block2(rai::BlockOpcode::CHANGE, 1, 1, 1541128318, 1,
                           account, hash, price, 1541128318, 1543720318,
                           raw_key, public_key);
    result = block == block2;
    ASSERT_EQ(false, result);

    rai::AdminBlock block3(rai::BlockOpcode::PRICE, 2, 1, 1541128318, 1,
                           account, hash, price, 1541128318, 1543720318,
                           raw_key, public_key);
    result = block == block3;
    ASSERT_EQ(false, result);

    rai::AdminBlock block4(rai::BlockOpcode::PRICE, 1, 2, 1541128318, 1,
                           account, hash, price, 1541128318, 1543720318,
                           raw_key, public_key);
    result = block == block4;
    ASSERT_EQ(false, result);

    rai::AdminBlock block5(rai::BlockOpcode::PRICE, 1, 1, 1541128319, 1,
                           account, hash, price, 1541128318, 1543720318,
                           raw_key, public_key);
    result = block == block5;
    ASSERT_EQ(false, result);

    rai::AdminBlock block6(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 2,
                           account, hash, price, 1541128318, 1543720318,
                           raw_key, public_key);
    result = block == block6;
    ASSERT_EQ(false, result);

    rai::AdminBlock block7(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1,
                           rai::Account(account.Number() + 1), hash, price,
                           1541128318, 1543720318, raw_key, public_key);
    result = block == block7;
    ASSERT_EQ(false, result);

    rai::AdminBlock block8(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1,
                           account, rai::BlockHash(hash.Number() + 1), price,
                           1541128318, 1543720318, raw_key, public_key);
    result = block == block8;
    ASSERT_EQ(false, result);

    rai::AdminBlock block9(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1,
                           account, hash, rai::Amount(price.Number() + 1),
                           1541128318, 1543720318, raw_key, public_key);
    result = block == block9;
    ASSERT_EQ(false, result);

    rai::AdminBlock block10(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1,
                            account, hash, price, 1541128319, 1543720318,
                            raw_key, public_key);
    result = block == block10;
    ASSERT_EQ(false, result);

    rai::AdminBlock block11(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1,
                            account, hash, price, 1541128318, 1543720319,
                            raw_key, public_key);
    result = block == block11;
    ASSERT_EQ(false, result);
}

TEST(AdminBlock, serialize_json)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount price;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    price.DecodeDec("1");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::AdminBlock block(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1, account,
                          hash, price, 1541128318, 1543720318, raw_key,
                          public_key);

    std::stringstream stream(block.Json());
    rai::Ptree ptree;
    boost::property_tree::read_json(stream, ptree);
    rai::ErrorCode error_code;
    rai::AdminBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(block, block2);
}

TEST(AdminBlock, deserialize_json_opcode)
{
    std::string str = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockOpcode::PRICE, block1.Opcode());

    str    = R"%%%({
    "type": "admin",
    "opcode": "change",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::BLOCK_OPCODE, error_code);
}

TEST(AdminBlock, deserialize_json_credit)
{
    std::string str = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65536",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "0xFFFF",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "01",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "-1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_CREDIT, error_code);
}

TEST(AdminBlock, deserialize_json_counter)
{
    std::string str = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "1",
    "counter": "1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "1",
    "counter": "4294967296",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_COUNTER, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "1",
    "counter": "0x1",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_COUNTER, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "1",
    "counter": "01",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_COUNTER, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "1",
    "counter": "",
    "timestamp": "1541128318",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_COUNTER, error_code);
}

TEST(AdminBlock, deserialize_json_timestamp)
{
    std::string str = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "1",
    "counter": "1",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "7C4B3EC489C7DC095291F09E1913C08B0E3D1F531024E506E6D7B68B13E33E115FE61AFCCB36D46DF63616828B485494A9420D6A80D65906E87D1DE485BE9606"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "1",
    "counter": "1",
    "timestamp": "18446744073709551616",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_TIMESTAMP, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "1",
    "counter": "1",
    "timestamp": "",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_TIMESTAMP, error_code);
}

TEST(AdminBlock, deserialize_json_height)
{
    std::string str = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551615",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "0A7038E5B93CD1760ACD3266DE703D0583BF79C3D900573DD69979C1DA8C758954A6E07B262188B37CFF815251A5A65B487E4C48EE405BE190FAD0C4E9ABAE0E"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "18446744073709551616",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "0x1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "-1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "01",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_HEIGHT, error_code);
}

TEST(AdminBlock, deserialize_json_account)
{
    std::string str = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdp",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_ACCOUNT, error_code);
}

TEST(AdminBlock, deserialize_json_price)
{
    std::string str = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "340282366920938463463374607431768211455",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "060464D2926DB3514ED9BEFF6EA7DD29A141B96D4D01B3DFE16E5108F04A72F2E229A674C9128C87E6313A9646DE8107755A8CC2A28621482D2926C5CFF90602"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "340282366920938463463374607431768211456",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_PRICE, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "0x1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block3(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_PRICE, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block4(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_PRICE, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block5(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_PRICE, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "-1",
    "begin_time": "1541128318",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block6(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_PRICE, error_code);
}

TEST(AdminBlock, deserialize_json_begin_time)
{
    std::string str = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "18446744073709551615",
    "begin_time": "18446744073709551615",
    "end_time": "1543720318",
    "signature": "A84CE68AA004EC1B8CB0D1AECF8E1D5BF5F72C0FFF5FA1F60C1785D808B410B53B3F3EE5959EF0F779B826849DAF318ABFA7224FFB4318E06E85131E662FEF03"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "18446744073709551615",
    "begin_time": "18446744073709551616",
    "end_time": "1543720318",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_BEGIN_TIME, error_code);
}

TEST(AdminBlock, deserialize_json_end_time)
{
    std::string str = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "18446744073709551615",
    "begin_time": "18446744073709551615",
    "end_time": "18446744073709551615",
    "signature": "7D0A95380C70243E5DB031F41BDE5BCF7791030E5C1ACEF456CB573F3AFA89D8E0FAC599825CC69F5CFEC65F91E34A724F3818703065C66293E4DA58FC97BD0F"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);

    str    = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "18446744073709551615",
    "begin_time": "18446744073709551615",
    "end_time": "18446744073709551616",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04"
})%%%";
    stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block2(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_END_TIME, error_code);
}

TEST(AdminBlock, deserialize_json_signature)
{
    std::string str = R"%%%({
    "type": "admin",
    "opcode": "price",
    "credit": "65535",
    "counter": "4294967295",
    "timestamp": "18446744073709551615",
    "height": "1",
    "account": "rai_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "price": "18446744073709551615",
    "begin_time": "18446744073709551615",
    "end_time": "18446744073709551615",
    "signature": "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F0"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::AdminBlock block1(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::JSON_BLOCK_SIGNATURE, error_code);
}

TEST(AdminBlock, deserialize)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount price;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    price.DecodeDec("1");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::AdminBlock block(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1, account,
                          hash, price, 1541128318, 1543720318, raw_key,
                          public_key);

    std::string str("");
    str += "07000100000001";  // opcode + credit + counter
    str += "000000005BDBC07E";  // timestamp
    str += "0000000000000001";  // height
                                // account
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    // previous
    str += "0000000000000000000000000000000000000000000000000000000000000000";
    str += "00000000000000000000000000000001";  // price
    str += "000000005BDBC07E";                  // begin_time
    str += "000000005C034D7E";                  // end_time
    // signature
    str +=
        "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18"
        "361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04";

    std::vector<uint8_t> bytes;
    bool error = TestDecodeHex(str, bytes);
    ASSERT_FALSE(error);
    rai::BufferStream stream(bytes.data(), bytes.size());
    rai::ErrorCode error_code;
    rai::AdminBlock block2(error_code, stream);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(block, block2);

    rai::BufferStream stream3(bytes.data(), bytes.size() - 1);
    rai::AdminBlock block3(error_code, stream3);
    ASSERT_EQ(rai::ErrorCode::STREAM, error_code);
}

TEST(AdminBlock, serialize)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount price;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    price.DecodeDec("1");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::AdminBlock block(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1, account,
                          hash, price, 1541128318, 1543720318, raw_key,
                          public_key);

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
    }

    std::string str("");
    str += "0307000100000001";  // type + opcode + credit + counter
    str += "000000005BDBC07E";  // timestamp
    str += "0000000000000001";  // height
                                // account
    str += "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    // previous
    str += "0000000000000000000000000000000000000000000000000000000000000000";
    str += "00000000000000000000000000000001";  // price
    str += "000000005BDBC07E";                  // begin_time
    str += "000000005C034D7E";                  // end_time
    // signature
    str +=
        "68C510664C6C1F06ED23D4E7C504DFB66833707453BB47715C0CC336479FF17D550B18"
        "361943BAB2FCF3A15CC73FA05411E7BE57EC32D72B02D0221BFCF96F04";

    std::vector<uint8_t> bytes_expect;
    bool error = TestDecodeHex(str, bytes_expect);
    ASSERT_FALSE(error);
    ASSERT_EQ(bytes_expect, bytes);
}
#endif

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
                       hash, representive, balance, link, 9,
                       {1, 1, 'r', 'a', 'i', 'c', 'o', 'i', 'n'}, raw_key,
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

#if 0
TEST(blocks, DeserializeBlockJson_AdminBlock)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount price;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    price.DecodeDec("1");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::AdminBlock block(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1, account,
                          hash, price, 1541128318, 1543720318, raw_key,
                          public_key);
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(block.Json());
    boost::property_tree::read_json(stream, ptree);
    std::unique_ptr<rai::Block> ptr =
        rai::DeserializeBlockJson(error_code, ptree);
    ASSERT_EQ(rai::ErrorCode::SUCCESS, error_code);
    ASSERT_EQ(rai::BlockType::ADMIN_BLOCK, ptr->Type());
}
#endif

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

#if 0
TEST(blocks, DeserializeBlock_AdminBlock)
{
    rai::Account account;
    rai::BlockHash hash;
    rai::Amount price;
    rai::RawKey raw_key;
    rai::PublicKey public_key;

    account.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    price.DecodeDec("1");
    raw_key.data_.DecodeHex(
        "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    public_key.DecodeHex(
        "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");

    rai::AdminBlock block(rai::BlockOpcode::PRICE, 1, 1, 1541128318, 1, account,
                          hash, price, 1541128318, 1543720318, raw_key,
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
#endif
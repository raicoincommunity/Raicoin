#include <chrono>
#include <vector>
#include <string>
#include <gtest/gtest.h>
#include <ed25519-donna/ed25519.h>
#include <rai/core_test/test_util.hpp>
#include <rai/core_test/config.hpp>

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::cout;
using std::string;
using std::endl;

extern bool DecodeAccount(string const& in, uint8_t* out);

TEST(ed25519, sign)
{
    bool ret;
    ed25519_secret_key private_key;
    ed25519_public_key public_key;
    ed25519_public_key public_key_expect;
    ed25519_signature sign;
    ed25519_signature sign_expect;

    string private_key_hex
        = "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4";
    ret = TestDecodeHex(private_key_hex, private_key, 32);
    ASSERT_EQ(false, ret);

    ed25519_publickey(private_key, public_key);
    string public_key_expect_hex
        = "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    ret = TestDecodeHex(public_key_expect_hex, public_key_expect, 32);
    ASSERT_EQ(false, ret);

    std::vector<unsigned char> v1(public_key_expect, public_key_expect + 32);
    std::vector<unsigned char> v2(public_key, public_key + 32);
    ASSERT_EQ(v1, v2);

    string message_hex =
        "04270D7F11C4B2B472F2854C5A59F2A7E84226CE9ED799DE75744BD7D85FC9D9";
    constexpr int len = 32;
    unsigned char message[len] = "";

    ret = TestDecodeHex(message_hex, message, len);
    ASSERT_EQ(false, ret);

    ed25519_sign(message, len, private_key, public_key, sign);

   string sign_expect_hex =
       "ECDA914373A2F0CA1296475BAEE40500A7F0A7AD72A5A80C81D7FAB7F6C802B2CC7DB50"
       "F5DD0FB25B2EF11761FA7344A158DD5A700B21BD47DE5BD0F63153A02";
   ret = TestDecodeHex(sign_expect_hex, sign_expect, 64);
   ASSERT_EQ(false, ret);
   std::vector<unsigned char> v3(sign_expect, sign_expect + 64);
   std::vector<unsigned char> v4(sign, sign + 64);
   ASSERT_EQ(v3, v4);

   auto valid1 = ed25519_sign_open(message, len, public_key, sign);
   ASSERT_EQ(0, valid1);
   sign[0] ^= 0x1;
   auto valid2 = ed25519_sign_open(message, len, public_key, sign);
   ASSERT_NE(0, valid2);
}

#if EXECUTE_LONG_TIME_CASE
TEST(ed25519, perfmance)
{
    bool ret;
    ed25519_secret_key private_key;
    ed25519_public_key public_key;
    ed25519_public_key public_key_expect;
    ed25519_signature sign;

    string private_key_hex
        = "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4";
    ret = TestDecodeHex(private_key_hex, private_key, 32);
    ASSERT_EQ(false, ret);

    ed25519_publickey(private_key, public_key);
    string public_key_expect_hex
        = "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0";
    ret = TestDecodeHex(public_key_expect_hex, public_key_expect, 32);
    ASSERT_EQ(false, ret);

    string message_hex =
        "04270D7F11C4B2B472F2854C5A59F2A7E84226CE9ED799DE75744BD7D85FC9D9";
    constexpr int len = 32;
    unsigned char message[len] = "";

    ret = TestDecodeHex(message_hex, message, len);
    ASSERT_EQ(false, ret);

    uint64_t num = 10000;
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    for (uint32_t i = 0; i < num; ++i)
    {
        ed25519_sign(message, len, private_key, public_key, sign);
    }
    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(t2 - t1).count();
    auto put_per_second = num * 1000 / duration;
    cout << put_per_second << " sign/second." << endl;

    num = 10000;
    t1 = high_resolution_clock::now();
    for (uint32_t i = 0; i < num; ++i)
    {
        auto valid = ed25519_sign_open(message, len, public_key, sign);
        ASSERT_EQ(0, valid);
    }
    t2 = high_resolution_clock::now();

    duration = duration_cast<milliseconds>(t2 - t1).count();
    put_per_second = num * 1000 / duration;
    cout << put_per_second << " open/second." << endl;
}
#endif













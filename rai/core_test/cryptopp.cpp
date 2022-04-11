#include <string>
#include <gtest/gtest.h>
#include <rai/core_test/test_util.hpp>
#include <rai/core_test/config.hpp>
#include <cryptopp/osrng.h>
#include <cryptopp/xed25519.h>

using namespace CryptoPP;

TEST(x25519, Share)
{
    AutoSeededRandomPool rnd;
    x25519 ecdh1, ecdh2;

    uint8_t pri1[32] = {0, };
    uint8_t pri2[32] = {0, };
    ecdh1.GeneratePrivateKey(rnd, pri1);
    ecdh2.GeneratePrivateKey(rnd, pri2);

    uint8_t pub1[32] = {0, };
    uint8_t pub2[32] = {0, };
    ecdh1.GeneratePublicKey(rnd, pri1, pub1);
    ecdh2.GeneratePublicKey(rnd, pri2, pub2);

    uint8_t share1[32] = {0, };
    uint8_t share2[32] = {0, };
    bool ret1 = ecdh1.Agree(share1, pri1, pub2);
    bool ret2 = ecdh2.Agree(share2, pri2, pub1);
    ASSERT_EQ(true, ret1);
    ASSERT_EQ(true, ret2);
    std::vector<unsigned char> v1(share1, share1 + 32);
    std::vector<unsigned char> v2(share2, share2 + 32);
    ASSERT_EQ(v1, v2);

    x25519 ecdh3;
    uint8_t pri3[32] = {0, };
    uint8_t pub3[32] = {0, };
    std::string pri3_hex =
        "202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F";
    bool ret3 = TestDecodeHex(pri3_hex, pri3, 32);
    ASSERT_EQ(false, ret3);
    ASSERT_EQ(false, ecdh3.IsClamped(pri3));
    ecdh3.GeneratePublicKey(rnd, pri3, pub3);

    uint8_t pub4[32] = {0, };
    std::string pub4_hex =
        "358072D6365880D1AEEA329ADF9121383851ED21A28E3B75E965D0D2CD166254";
    bool ret4 = TestDecodeHex(pub4_hex, pub4, 32);
    ASSERT_EQ(false, ret4);

    std::vector<unsigned char> v3(pub3, pub3 + 32);
    std::vector<unsigned char> v4(pub4, pub4 + 32);
    ASSERT_EQ(v3, v4);
}

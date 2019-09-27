#include <chrono>
#include <iostream>
#include <string>
#include <gtest/gtest.h>
#include <blake2/blake2.h>
#include <rai/core_test/config.hpp>
#include <rai/core_test/test_util.hpp>

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::cout;
using std::endl;
using std::string;

TEST(blake2b, hash256)
{
    bool ret;
    unsigned char message[32] = "raicoin";
    unsigned char hash[32] = "";
    unsigned char hash_expect[32] = "";
    blake2b_state state;

    blake2b_init (&state, sizeof(hash));
    blake2b_update (&state, message, sizeof(message));
    blake2b_final (&state, hash, sizeof(hash));

    string hash_expect_str = "15485b2e98f8aefae9c126ce22644e6c3801f275e5645a296af62e6da075ea6e";
    ret = TestDecodeHex(hash_expect_str, hash_expect, sizeof(hash_expect));
    ASSERT_EQ(false, ret);

    std::vector<unsigned char> v1(hash, hash + sizeof(hash));
    std::vector<unsigned char> v2(hash_expect, hash_expect + sizeof(hash_expect));
    ASSERT_EQ(v1, v2);

    string str("");
    str += "B2EC73C1F503F47E051AD72ECB512C63BA8E1A0ACC2CEE4EA9A22FE1CBDB693F";
    str += "2298FAB7C61058E77EA554CB93EDEEDA0692CBFCC540AB213B2836B29029E23A";
    str += "A30E0A32ED41C8607AA9212843392E853FCBCB4E7CB194E35C94F07F91DE59EF";
    size_t size = str.size() / 2;
    uint8_t* bytes = new uint8_t[size];
    ret = TestDecodeHex(str, bytes, size);
    EXPECT_EQ(false, ret);

    blake2b_init (&state, sizeof(hash));
    blake2b_update (&state, bytes, size);
    blake2b_final (&state, hash, sizeof(hash));
    delete []bytes;
    bytes = nullptr;

    hash_expect_str = "8E4383AF71E18E81911CF8202AE1C6DA96F52835FB358D0753C873620BFC051A";
    ret = TestDecodeHex(hash_expect_str, hash_expect, sizeof(hash_expect));
    ASSERT_EQ(false, ret);

    std::vector<unsigned char> v3(hash, hash + sizeof(hash));
    std::vector<unsigned char> v4(hash_expect, hash_expect + sizeof(hash_expect));
    ASSERT_EQ(v3, v4);

    str = "";
    str += "CC22F7D7EA3E9DC71C1F189361F31985149CEE5F8871A6CF6CA54F2DA498FE2D";
    str += "0A93BD8702CDE0231430867D95766B79B91A1463F16C51A1487EAF79854AA72F";
    size = str.size() / 2;
    bytes = new uint8_t[size];
    ret = TestDecodeHex(str, bytes, size);
    EXPECT_EQ(false, ret);

    blake2b_init (&state, sizeof(hash));
    blake2b_update (&state, bytes, size);
    blake2b_final (&state, hash, sizeof(hash));
    delete []bytes;
    bytes = nullptr;
    hash_expect_str = "608449009E40C4E25FCC33B5B1E5880A60FFABC2177362682973071AB15A9E24";
    ret = TestDecodeHex(hash_expect_str, hash_expect, sizeof(hash_expect));
    ASSERT_EQ(false, ret);

    std::vector<unsigned char> v5(hash, hash + sizeof(hash));
    std::vector<unsigned char> v6(hash_expect, hash_expect + sizeof(hash_expect));
    ASSERT_EQ(v5, v6);
}

#if EXECUTE_LONG_TIME_CASE
TEST(blake2b, perfmance)
{
    long long int num = 10000000;
    unsigned char message[256] = "";
    unsigned char hash[32] = "";
    blake2b_state state;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    for (int i = 0; i < num; ++i)
    {
        blake2b_init (&state, sizeof(hash));
        blake2b_update (&state, message, sizeof(message));
        blake2b_final (&state, hash, sizeof(hash));
    }
    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(t2 - t1).count();
    auto khash_per_second = num / duration;

    cout << khash_per_second << " khash/second." << endl;
}
#endif
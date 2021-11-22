#include <gtest/gtest.h>
#include <rai/core_test/test_util.hpp>
#include <rai/common/util.hpp>

TEST(CommonUtil, CheckUtf8)
{
    bool ctrl;
    ASSERT_EQ(false, rai::CheckUtf8("Raicoin", ctrl));
    ASSERT_EQ(false, ctrl);
    ASSERT_EQ(false, rai::CheckUtf8("κόσμε", ctrl));
    ASSERT_EQ(false, ctrl);

    std::vector<uint8_t> boundary_first{
        0x00,
        0xC2, 0x80,
        0xE0, 0xA0, 0x80,
        0xF0, 0x90, 0x80, 0x80  
    };
    ASSERT_EQ(false, rai::CheckUtf8(boundary_first, ctrl));
    ASSERT_EQ(true, ctrl);

    std::vector<uint8_t> boundary_first_2{
        0xF8, 0x88, 0x80, 0x80, 0x80
    };
    ASSERT_EQ(true, rai::CheckUtf8(boundary_first_2, ctrl));

    std::vector<uint8_t> boundary_first_3{
        0xFC, 0x84, 0x80, 0x80, 0x80, 0x80
    };
    ASSERT_EQ(true, rai::CheckUtf8(boundary_first_3, ctrl));

    std::vector<uint8_t> boundary_last{
        0x7F,
        0xDF, 0xBF,
        0xEF, 0xBF, 0xBF, 
        0xF4, 0x8F, 0xBF, 0xBF
    };
    ASSERT_EQ(false, rai::CheckUtf8(boundary_last, ctrl));
    ASSERT_EQ(true, ctrl);

    std::vector<uint8_t> boundary_last_2{
        0xFB, 0xBF, 0xBF, 0xBF, 0xBF
    };
    ASSERT_EQ(true, rai::CheckUtf8(boundary_last_2, ctrl));

    std::vector<uint8_t> boundary_last_3{
        0xFD, 0xBF, 0xBF, 0xBF, 0xBF, 0xBF
    };
    ASSERT_EQ(true, rai::CheckUtf8(boundary_last_3, ctrl));

    std::vector<uint8_t> boundary_other{
        0xED, 0x9F, 0xBF, // U-0000D7FF
        0xEE, 0x80, 0x80, // U-0000E000
        0xEF, 0xBF, 0xBD, // U-0000FFFD
        0xF4, 0x8F, 0xBF, 0xBF // U-0010FFFF
    };
    ASSERT_EQ(false, rai::CheckUtf8(boundary_other, ctrl));
    ASSERT_EQ(false, ctrl);

    std::vector<uint8_t> boundary_other_2{
        0xF4, 0x90, 0x80, 0x80 // U-00110000
    };
    ASSERT_EQ(true, rai::CheckUtf8(boundary_other_2, ctrl));

    std::vector<uint8_t> continuation{
        0x80
    };
    ASSERT_EQ(true, rai::CheckUtf8(continuation, ctrl));

    std::vector<uint8_t> continuation_2{
        0xBF
    };
    ASSERT_EQ(true, rai::CheckUtf8(continuation_2, ctrl));

    std::vector<uint8_t> continuation_3{
        0x80, 0xBF
    };
    ASSERT_EQ(true, rai::CheckUtf8(continuation_3, ctrl));

    std::vector<uint8_t> continuation_4{
        0xEF, 0xBF
    };
    ASSERT_EQ(true, rai::CheckUtf8(continuation_4, ctrl));

    std::vector<uint8_t> lonely_start{
        0xC0, 0x20, 0xC1
    };
    ASSERT_EQ(true, rai::CheckUtf8(lonely_start, ctrl));

    std::vector<uint8_t> impossible{
        0xFE
    };
    ASSERT_EQ(true, rai::CheckUtf8(impossible, ctrl));

    std::vector<uint8_t> overlong{
        0xE0, 0x80, 0xAF
    };
    ASSERT_EQ(true, rai::CheckUtf8(overlong, ctrl));

    std::vector<uint8_t> overlong_2{
        0xF0, 0x8F, 0xBF, 0xBF
    };
    ASSERT_EQ(true, rai::CheckUtf8(overlong_2, ctrl));

    std::vector<uint8_t> illegal{
        0xED, 0xAD, 0xBF
    };
    ASSERT_EQ(true, rai::CheckUtf8(illegal, ctrl));

    std::vector<uint8_t> illegal_2{
        0xED, 0xAE, 0x80, 0xED, 0xB0, 0x80
    };
    ASSERT_EQ(true, rai::CheckUtf8(illegal_2, ctrl));
}

TEST(CommonUtil, StreamBool)
{
    std::vector<uint8_t> bytes1 = {0, };
    rai::BufferStream stream1(bytes1.data(), bytes1.size());
    bool bool1 = true;
    bool ret = rai::Read(stream1, bool1);
    ASSERT_EQ(false, ret);
    ASSERT_EQ(false, bool1);
    ret = rai::Read(stream1, bool1);
    ASSERT_EQ(true, ret);

    std::vector<uint8_t> bytes2 = {1, };
    rai::BufferStream stream2(bytes2.data(), bytes2.size());
    bool bool2 = false;
    ret = rai::Read(stream2, bool2);
    ASSERT_EQ(false, ret);
    ASSERT_EQ(true, bool2);
    ret = rai::Read(stream2, bool2);
    ASSERT_EQ(true, ret);

    std::vector<uint8_t> bytes3 = {2, };
    rai::BufferStream stream3(bytes3.data(), bytes3.size());
    bool bool3 = false;
    ret = rai::Read(stream3, bool3);
    ASSERT_EQ(true, ret);

    std::vector<uint8_t> bytes4;
    {
        rai::VectorStream stream4(bytes4);
        bool bool4 = false;
        rai::Write(stream4, bool4);
    }
    ASSERT_EQ(1, bytes4.size());
    ASSERT_EQ(0, bytes4[0]);

    std::vector<uint8_t> bytes5;
    {
        rai::VectorStream stream5(bytes5);
        bool bool5 = true;
        rai::Write(stream5, bool5);
    }
    ASSERT_EQ(1, bytes5.size());
    ASSERT_EQ(1, bytes5[0]);
}

TEST(CommonUtil, StreamString)
{
    std::vector<uint8_t> bytes1 = {7, 'R', 'a', 'i', 'c', 'o', 'i', 'n'};
    rai::BufferStream stream1(bytes1.data(), bytes1.size());
    std::string str1;
    bool ret = rai::Read(stream1, str1);
    ASSERT_EQ(false, ret);
    ASSERT_EQ("Raicoin", str1);
    ret = rai::Read(stream1, str1);
    ASSERT_EQ(true, ret);

    std::vector<uint8_t> bytes2 = {0, };
    rai::BufferStream stream2(bytes2.data(), bytes2.size());
    std::string str2;
    ret = rai::Read(stream2, str2);
    ASSERT_EQ(false, ret);
    ASSERT_EQ("", str2);
    ret = rai::Read(stream2, str2);
    ASSERT_EQ(true, ret);


    std::vector<uint8_t> bytes3 = {1, };
    rai::BufferStream stream3(bytes3.data(), bytes3.size());
    std::string str3;
    ret = rai::Read(stream3, str3);
    ASSERT_EQ(true, ret);

    std::vector<uint8_t> bytes4;
    {
        rai::VectorStream stream4(bytes4);
        std::string str4 = "Raicoin";
        rai::Write(stream4, str4);
    }
    std::vector<uint8_t> bytes4_expected = {7,   'R', 'a', 'i',
                                            'c', 'o', 'i', 'n'};
    ASSERT_EQ(bytes4_expected, bytes4);

    std::vector<uint8_t> bytes5;
    {
        rai::VectorStream stream5(bytes5);
        std::string str5 = "";
        rai::Write(stream5, str5);
    }
    std::vector<uint8_t> bytes5_expected = {0, };
    ASSERT_EQ(bytes5_expected, bytes5);
}

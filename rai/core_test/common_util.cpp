#include <gtest/gtest.h>
#include <rai/core_test/test_util.hpp>
#include <rai/common/util.hpp>

TEST(CommonUtil, CheckUtf8)
{
    bool ctrl;
    EXPECT_EQ(false, rai::CheckUtf8("Raicoin", ctrl));
    EXPECT_EQ(false, ctrl);
    EXPECT_EQ(false, rai::CheckUtf8("κόσμε", ctrl));
    EXPECT_EQ(false, ctrl);

    std::vector<uint8_t> boundary_first{
        0x00,
        0xC2, 0x80,
        0xE0, 0xA0, 0x80,
        0xF0, 0x90, 0x80, 0x80  
    };
    EXPECT_EQ(false, rai::CheckUtf8(boundary_first, ctrl));
    EXPECT_EQ(true, ctrl);

    std::vector<uint8_t> boundary_first_2{
        0xF8, 0x88, 0x80, 0x80, 0x80
    };
    EXPECT_EQ(true, rai::CheckUtf8(boundary_first_2, ctrl));

    std::vector<uint8_t> boundary_first_3{
        0xFC, 0x84, 0x80, 0x80, 0x80, 0x80
    };
    EXPECT_EQ(true, rai::CheckUtf8(boundary_first_3, ctrl));

    std::vector<uint8_t> boundary_last{
        0x7F,
        0xDF, 0xBF,
        0xEF, 0xBF, 0xBF, 
        0xF7, 0xBF, 0xBF, 0xBF
    };
    EXPECT_EQ(false, rai::CheckUtf8(boundary_last, ctrl));
    EXPECT_EQ(true, ctrl);

    std::vector<uint8_t> boundary_last_2{
        0xFB, 0xBF, 0xBF, 0xBF, 0xBF
    };
    EXPECT_EQ(true, rai::CheckUtf8(boundary_last_2, ctrl));

    std::vector<uint8_t> boundary_last_3{
        0xFD, 0xBF, 0xBF, 0xBF, 0xBF, 0xBF
    };
    EXPECT_EQ(true, rai::CheckUtf8(boundary_last_3, ctrl));

    std::vector<uint8_t> boundary_other{
        0xED, 0x9F, 0xBF, // U-0000D7FF
        0xEE, 0x80, 0x80, // U-0000E000
        0xEF, 0xBF, 0xBD, // U-0000FFFD
        0xF4, 0x8F, 0xBF, 0xBF // U-0010FFFF
    };
    EXPECT_EQ(false, rai::CheckUtf8(boundary_other, ctrl));
    EXPECT_EQ(false, ctrl);

    std::vector<uint8_t> boundary_other_2{
        0xF4, 0x90, 0x80, 0x80 // U-00110000
    };
    EXPECT_EQ(true, rai::CheckUtf8(boundary_other_2, ctrl));

    std::vector<uint8_t> continuation{
        0x80
    };
    EXPECT_EQ(true, rai::CheckUtf8(continuation, ctrl));

    std::vector<uint8_t> continuation_2{
        0xBF
    };
    EXPECT_EQ(true, rai::CheckUtf8(continuation_2, ctrl));

    std::vector<uint8_t> continuation_3{
        0x80, 0xBF
    };
    EXPECT_EQ(true, rai::CheckUtf8(continuation_3, ctrl));

    std::vector<uint8_t> continuation_4{
        0xEF, 0xBF
    };
    EXPECT_EQ(true, rai::CheckUtf8(continuation_4, ctrl));

    std::vector<uint8_t> lonely_start{
        0xC0, 0x20, 0xC1
    };
    EXPECT_EQ(true, rai::CheckUtf8(lonely_start, ctrl));

    std::vector<uint8_t> impossible{
        0xFE
    };
    EXPECT_EQ(true, rai::CheckUtf8(impossible, ctrl));

    std::vector<uint8_t> overlong{
        0xE0, 0x80, 0xAF
    };
    EXPECT_EQ(true, rai::CheckUtf8(overlong, ctrl));

    std::vector<uint8_t> overlong_2{
        0xF0, 0x8F, 0xBF, 0xBF
    };
    EXPECT_EQ(true, rai::CheckUtf8(overlong_2, ctrl));

    
    std::vector<uint8_t> illegal{
        0xED, 0xAD, 0xBF
    };
    EXPECT_EQ(true, rai::CheckUtf8(illegal, ctrl));

    std::vector<uint8_t> illegal_2{
        0xED, 0xAE, 0x80, 0xED, 0xB0, 0x80
    };
    EXPECT_EQ(true, rai::CheckUtf8(illegal_2, ctrl));
}
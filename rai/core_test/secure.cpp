#include <gtest/gtest.h>
#include <rai/core_test/config.hpp>
#include <rai/core_test/test_util.hpp>

#include <rai/common/errors.hpp>
#include <rai/secure/common.hpp>

TEST(secure, DeriveKey)
{
    rai::RawKey raw_key;
    std::string password("Passw0rd^&");
    rai::uint256_union salt;
    salt.DecodeHex("8F191A72BC85B3D68CB0945F48A732CA812BB60F40819AD5A4A6D732E71EFBF3");
    rai::ErrorCode error_code = rai::Kdf::DeriveKey(raw_key, password, salt);
    ASSERT_EQ(error_code, rai::ErrorCode::SUCCESS);

    rai::uint256_union expect;
    expect.DecodeHex("40F2F2E07B1DFB9C2DFC1132AFCD5EF697CA2FF24E403A0CF0091E25BD6A19DB");
    ASSERT_EQ(raw_key.data_, expect);
}
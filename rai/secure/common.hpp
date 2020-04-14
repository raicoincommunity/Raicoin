#pragma once

#include <rai/common/errors.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/util.hpp>
#include <rai/common/blocks.hpp>

namespace rai
{
class KeyPair
{
public:
    KeyPair();
    rai::ErrorCode Serialize(const std::string&, std::ofstream&) const;
    rai::ErrorCode Show(std::ifstream&) const;
    rai::ErrorCode Deserialize(const std::string&, std::ifstream&);

    rai::PublicKey public_key_;
    rai::RawKey private_key_;

    static uint32_t constexpr VERSION = 1;
};

class Kdf
{
public:
    Kdf() = delete;
    static rai::ErrorCode DeriveKey(rai::RawKey&, const std::string&,
                                    const rai::uint256_union&);
};

class Fan
{
public:
    Fan();
    Fan(const rai::uint256_union&, size_t);
    void Get(rai::RawKey&) const;
    void Set(const rai::RawKey&);

    static size_t constexpr FAN_OUT = 1024;

private:
    void Get_(rai::RawKey&) const;
    void Set_(const rai::RawKey&);

    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<rai::uint256_union>> values_;
};

class Genesis
{
public:
    Genesis();

    std::shared_ptr<rai::Block> block_;
};
} // namespace rai
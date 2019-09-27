#include <rai/secure/common.hpp>

#include <phc-winner-argon2/include/argon2.h>
#include <boost/endian/conversion.hpp>
#include <rai/common/parameters.hpp>

rai::KeyPair::KeyPair()
{
    rai::random_pool.GenerateBlock(private_key_.data_.bytes.data(),
                                   private_key_.data_.bytes.size());
    public_key_ = rai::GeneratePublicKey(private_key_.data_);
}

rai::ErrorCode rai::KeyPair::Serialize(const std::string& password,
                                       std::ofstream& stream) const
{
    rai::WriteFile(stream, boost::endian::native_to_big(rai::KeyPair::VERSION));

    rai::uint256_union salt;
    rai::random_pool.GenerateBlock(salt.bytes.data(), salt.bytes.size());
    rai::WriteFile(stream, salt.bytes);

    rai::RawKey key;
    rai::ErrorCode error_code = rai::Kdf::DeriveKey(key, password, salt);
    IF_NOT_SUCCESS_RETURN(error_code);
    rai::uint128_union iv;
    rai::random_pool.GenerateBlock(iv.bytes.data(), iv.bytes.size());
    rai::WriteFile(stream, iv.bytes);
    
    rai::RawKey encrypt;
    encrypt.Decrypt(private_key_.data_, key, iv);
    rai::WriteFile(stream, encrypt.data_.bytes);
    rai::WriteFile(stream, public_key_.bytes);

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::KeyPair::Deserialize(const std::string& password,
                                         std::ifstream& stream)
{
    bool error = false;
    uint32_t version;
    error =  rai::ReadFile(stream, version);
    IF_ERROR_RETURN(error, rai::ErrorCode::INVALID_KEY_FILE);
    boost::endian::big_to_native_inplace(version);
    if (version > rai::KeyPair::VERSION)
    {
        return rai::ErrorCode::KEY_FILE_VERSION;
    }

    rai::uint256_union salt;
    error = rai::ReadFile(stream, salt.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::INVALID_KEY_FILE);

    rai::uint128_union iv;
    error = rai::ReadFile(stream, iv.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::INVALID_KEY_FILE);

    rai::RawKey encrypt;
    error = rai::ReadFile(stream, encrypt.data_.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::INVALID_KEY_FILE);

    rai::PublicKey public_key;
    error = rai::ReadFile(stream, public_key.bytes);
    IF_ERROR_RETURN(error, rai::ErrorCode::INVALID_KEY_FILE);

    uint8_t end;
    rai::ReadFile(stream, end);
    if (!stream.eof())
    {
        return rai::ErrorCode::INVALID_KEY_FILE;
    }

    rai::RawKey key;
    rai::ErrorCode error_code = rai::Kdf::DeriveKey(key, password, salt);
    IF_NOT_SUCCESS_RETURN(error_code);

    private_key_.Decrypt(encrypt.data_, key, iv);
    public_key_ = rai::GeneratePublicKey(private_key_.data_);
    if (public_key != public_key_)
    {
        return rai::ErrorCode::PASSWORD_ERROR;
    }

    return rai::ErrorCode::SUCCESS;
}


rai::ErrorCode rai::Kdf::DeriveKey(rai::RawKey& key,
                                   const std::string& password,
                                   const rai::uint256_union& salt)
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    auto ret = argon2_hash(1, 64 * 1024, 1, password.data(), password.size(),
                           salt.bytes.data(), salt.bytes.size(),
                           key.data_.bytes.data(), key.data_.bytes.size(), NULL,
                           0, Argon2_d, 0x10);
    if (ret != ARGON2_OK)
    {
        return rai::ErrorCode::DERIVE_KEY;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::Fan::Fan(const rai::uint256_union& key, size_t count)
{
    std::unique_ptr<rai::uint256_union> first(new rai::uint256_union(key));
    for (size_t i = 1; i < count; ++i)
    {
        std::unique_ptr<rai::uint256_union> entry(new rai::uint256_union);
        random_pool.GenerateBlock(entry->bytes.data(), entry->bytes.size());
        *first ^= *entry;
        values_.push_back(std::move(entry));
    }
    values_.push_back(std::move(first));
}

void rai::Fan::Get(rai::RawKey& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Get_(key);
}

void rai::Fan::Set(const rai::RawKey& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Set_(key);
}

void rai::Fan::Get_(rai::RawKey& key)
{
    key.data_.Clear();
    for (const auto& i : values_)
    {
        key.data_ ^= *i;
    }
}

void rai::Fan::Set_(const rai::RawKey& key)
{
    rai::RawKey current;
    Get_(current);
    *(values_[0]) ^= current.data_;
    *(values_[0]) ^= key.data_;
}

rai::Genesis::Genesis() : block_(nullptr)
{
    std::string public_key_str;
    std::string block_json_str;

    switch (rai::RAI_NETWORK)
    {
        case rai::RaiNetworks::TEST:
        {
            public_key_str = rai::TEST_PUBLIC_KEY;
            block_json_str = rai::TEST_GENESIS_BLOCK;
            break;
        }
        case rai::RaiNetworks::BETA:
        {
            throw std::runtime_error("Genesis data missing");
            break;
        }
        case rai::RaiNetworks::LIVE:
        {
            throw std::runtime_error("Genesis data missing");
            break;
        }
        default:
        {
            throw std::runtime_error("Unknown rai::RAI_NETWORK");
        }
    }

    rai::PublicKey public_key;
    bool error = public_key.DecodeHex(public_key_str);
    if (error)
    {
        throw std::invalid_argument("Invalid genesis public key");
    }

    rai::Ptree ptree;
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    std::stringstream stream(block_json_str);
    boost::property_tree::read_json(stream, ptree);
    block_ = rai::DeserializeBlockJson(error_code, ptree);
    if (error_code != rai::ErrorCode::SUCCESS || block_ == nullptr)
    {
        throw std::invalid_argument("Failed to deserialize genesis block");
    }

    if (block_->Type() != rai::BlockType::REP_BLOCK)
    {
        throw std::invalid_argument("Invalid genesis block type");
    }

    if (block_->Opcode() != rai::BlockOpcode::RECEIVE)
    {
        throw std::invalid_argument("Invalid genesis block opcode");
    }

    if (block_->Credit() != 512)
    {
        throw std::invalid_argument("Invalid genesis block credit");
    }

    if (block_->Counter() != 1)
    {
        throw std::invalid_argument("Invalid genesis block counter");
    }

    if (block_->Timestamp() != rai::EPOCH_TIMESTAMP)
    {
        throw std::invalid_argument("Invalid genesis block timestamp");
    }

    if (block_->Height() != 0)
    {
        throw std::invalid_argument("Invalid genesis block height");
    }

    if (block_->Account() != public_key)
    {
        throw std::invalid_argument("Invalid genesis block account");
    }

    if (!block_->Previous().IsZero())
    {
        throw std::invalid_argument("Invalid genesis block previous");
    }

    if (block_->Balance() != rai::Amount(10000000 * rai::RAI))
    {
        throw std::invalid_argument("Invalid genesis block balance");
    }

    if (block_->Link() != public_key)
    {
        throw std::invalid_argument("Invalid genesis block link");
    }
}

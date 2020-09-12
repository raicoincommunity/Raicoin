#include <rai/secure/util.hpp>

#include <iostream>
#include <string>
#include <rai/secure/common.hpp>
#include <rai/secure/plat.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/util.hpp>

namespace 
{
rai::ErrorCode InputPassword(const std::string& prompt1,
                             const std::string& prompt2, std::string& password)
{
    password.reserve(1024);
    rai::SetStdinEcho(false);
    std::cout << prompt1;
    std::string password_1;
    password_1.reserve(1024);
    std::cin >> password_1;
    std::cout << std::endl;
    if ((password_1.size() < 8) || (password_1.size() > 256))
    {
        rai::SecureClearString(password_1);
        rai::SetStdinEcho(true);
        return rai::ErrorCode::PASSWORD_LENGTH;
    }

    std::cout << prompt2;
    std::string password_2;
    password_2.reserve(1024);
    std::cin >> password_2;
    std::cout << std::endl;
    if (password_1 != password_2)
    {
        rai::SecureClearString(password_1);
        rai::SecureClearString(password_2);
        rai::SetStdinEcho(true);
        return rai::ErrorCode::PASSWORD_MATCH;
    }

    password = password_1;
    rai::SecureClearString(password_1);
    rai::SecureClearString(password_2);
    rai::SetStdinEcho(true);

    return rai::ErrorCode::SUCCESS;
}

void InputPassword(const std::string& prompt, std::string& password)
{
    password.reserve(1024);
    rai::SetStdinEcho(false);
    std::cout << prompt;
    std::cin >> password;
    std::cout << std::endl;
    rai::SetStdinEcho(true);
}
} // namespace

void rai::SecureClearString(std::string& str)
{
    volatile char* volatile ptr =
        const_cast<volatile char* volatile>(str.c_str());
    size_t size = str.size();
    for (size_t i = 0; i < size; ++i)
    {
        ptr[i] = 0;
    }
}

boost::filesystem::path rai::WorkingPath()
{
    boost::filesystem::path result(rai::AppPath());
    switch (rai::RAI_NETWORK)
    {
        case rai::RaiNetworks::TEST:
        {
            result /= "RaicoinTest";
            break;
        }
        case rai::RaiNetworks::BETA:
        {
            result /= "RaicoinBeta";
            break;
        }
        case rai::RaiNetworks::LIVE:
        {
            result /= "Raicoin";
            break;
        }
        default:
        {
            throw std::runtime_error("Unknown rai::RAI_NETWORK");
        }
    }
    return result;
}

rai::Password::Password()
{
}

rai::Password::~Password()
{
    rai::SecureClearString(password_);
}

const std::string& rai::Password::Get() const
{
    return password_;
}

void rai::Password::Input(const std::string& prompt)
{
    InputPassword(prompt, password_);
}

rai::ErrorCode rai::Password::Input(const std::string& prompt1,
                                    const std::string& prompt2)
{
    return InputPassword(prompt1, prompt2, password_);
}

void rai::OpenOrCreate(std::fstream& stream, const std::string& path)
{
    stream.open(path, std::ios_base::in);
    if (stream.fail())
    {
        stream.open(path, std::ios_base::out);
    }
    stream.close();
    stream.open(path, std::ios_base::in | std::ios_base::out);
}

rai::ErrorCode rai::CreateKey(const boost::filesystem::path& path, bool dir)
{
    if (!dir && boost::filesystem::exists(path))
    {
        return rai::ErrorCode::KEY_FILE_EXIST;
    }

    rai::ErrorCode error_code;
    rai::Password password;
    error_code = password.Input("Set key pair password:", "Retype password:");
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::KeyPair key_pair;
    boost::filesystem::path full_path(path);
    if (dir)
    {
        full_path = path / (key_pair.public_key_.StringAccount() + ".dat");
    }

    std::ofstream key_file(full_path.string(),
                           std::ios::binary | std::ios::out);
    if (key_file)
    {
        error_code = key_pair.Serialize(password.Get(), key_file);
    }
    key_file.close();
    IF_NOT_SUCCESS_RETURN(error_code);

    if (!boost::filesystem::exists(full_path))
    {
        return rai::ErrorCode::WRITE_FILE;
    }

    std::cout << "The key pair was saved to:" << full_path.string()
              << std::endl;
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::DecryptKey(rai::Fan& key, const boost::filesystem::path& path)
{
    rai::Password password;
    password.Input("Key pair password:");

    rai::KeyPair key_pair;
    std::ifstream stream(path.string(), std::ios::in | std::ios::binary);
    rai::ErrorCode error_code = rai::ErrorCode::INVALID_KEY_FILE;
    if (stream)
    {
         error_code = key_pair.Deserialize(password.Get(), stream);
    }
    stream.close();
    IF_NOT_SUCCESS_RETURN(error_code);

    key.Set(key_pair.private_key_);
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::DecryptKey(rai::KeyPair& key_pair, const std::string& path,
                               const std::string& password)
{
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    rai::ErrorCode error_code = rai::ErrorCode::INVALID_KEY_FILE;
    if (stream)
    {
         error_code = key_pair.Deserialize(password, stream);
    }
    stream.close();
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

#include <rai/secure/util.hpp>

#include <iostream>
#include <string>
#include <rai/secure/common.hpp>
#include <rai/secure/plat.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/util.hpp>

namespace 
{
void SecureClearString(std::string& str)
{
    volatile char* volatile ptr =
        const_cast<volatile char* volatile>(str.c_str());
    size_t size = str.size();
    for (size_t i = 0; i < size; ++i)
    {
        ptr[i] = 0;
    }
}

rai::ErrorCode SetKeyPassword(std::string& password)
{
    rai::SetStdinEcho(false);
    std::cout << "Set key pair password:";
    std::string password_1;
    password_1.reserve(1024);
    std::cin >> password_1;
    std::cout << std::endl;
    if ((password_1.size() < 8) || (password_1.size() > 256))
    {
        SecureClearString(password_1);
        rai::SetStdinEcho(true);
        return rai::ErrorCode::PASSWORD_LENGTH;
    }

    std::cout << "Retype password:";
    std::string password_2;
    password_2.reserve(1024);
    std::cin >> password_2;
    std::cout << std::endl;
    if (password_1 != password_2)
    {
        SecureClearString(password_1);
        SecureClearString(password_2);
        rai::SetStdinEcho(true);
        return rai::ErrorCode::PASSWORD_MATCH;
    }

    password = password_1;
    SecureClearString(password_1);
    SecureClearString(password_2);
    rai::SetStdinEcho(true);

    return rai::ErrorCode::SUCCESS;
}

void InputKeyPassword(std::string& password)
{
    rai::SetStdinEcho(false);
    std::cout << "Key pair password:";
    std::string password_l;
    password_l.reserve(1024);
    std::cin >> password_l;
    std::cout << std::endl;
    password = password_l;
    SecureClearString(password_l);
    rai::SetStdinEcho(true);
}
} // namespace

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

rai::ErrorCode rai::CreateKey(const boost::filesystem::path& path,
                              const std::string& file)
{
    if (!file.empty() && boost::filesystem::exists(path / file))
    {
        return rai::ErrorCode::KEY_FILE_EXIST;
    }

    rai::ErrorCode error_code;
    std::string password;
    error_code = SetKeyPassword(password);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::KeyPair key_pair;
    boost::filesystem::path full_path;
    if (file.empty())
    {
        full_path = path / (key_pair.public_key_.StringAccount() + ".dat");
    }
    else
    {
        full_path = path / file;
    }
    std::ofstream key_file(full_path.string(),
                           std::ios::binary | std::ios::out);
    if (key_file)
    {
        error_code = key_pair.Serialize(password, key_file);
    }
    key_file.close();
    SecureClearString(password);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (!boost::filesystem::exists(full_path))
    {
        return rai::ErrorCode::WRITE_FILE;
    }

    std::cout << "New account:" << key_pair.public_key_.StringAccount()
              << std::endl;
    std::cout << "The key pair was saved to:" << full_path.string()
              << std::endl;
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::DecryptKey(rai::Fan& key, const boost::filesystem::path& path)
{
    std::string password;
    InputKeyPassword(password);

    rai::KeyPair key_pair;
    std::ifstream stream(path.string(), std::ios::in | std::ios::binary);
    rai::ErrorCode error_code = rai::ErrorCode::INVALID_KEY_FILE;
    if (stream)
    {
         error_code = key_pair.Deserialize(password, stream);
    }
    stream.close();
    SecureClearString(password);
    IF_NOT_SUCCESS_RETURN(error_code);

    key.Set(key_pair.private_key_);
    return rai::ErrorCode::SUCCESS;
}

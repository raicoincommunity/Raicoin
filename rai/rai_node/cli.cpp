#include <rai/rai_node/cli.hpp>

#include <iostream>
#include <boost/filesystem.hpp>
#include <rai/secure/util.hpp>
#include <rai/rai_node/daemon.hpp>

namespace
{
rai::ErrorCode GetKeyPath(const boost::program_options::variables_map& vm,
                          const boost::filesystem::path& data_path,
                          boost::filesystem::path& key_path)
{
    if (!vm.count("key"))
    {
        return rai::ErrorCode::CMD_MISS_KEY;
    }

    std::string key_file = vm["key"].as<std::string>();
    if ((key_file.find("/") != std::string::npos)
        || (key_file.find("\\") != std::string::npos))
    {
        key_path = boost::filesystem::path(key_file);
        if (key_file.find(".") == 0)
        {
            key_path = boost::filesystem::canonical(key_path);
        }
    }
    else
    {
        key_path = data_path / key_file;
    }

    if (!boost::filesystem::exists(key_path))
    {
        return rai::ErrorCode::KEY_FILE_NOT_EXIST;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode ProcessDaemon(const boost::program_options::variables_map& vm,
                             const boost::filesystem::path& data_path)
{
    boost::filesystem::path key_path;
    rai::ErrorCode error_code = GetKeyPath(vm, data_path, key_path);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::Daemon daemon;
    return daemon.Run(data_path, key_path);
}

rai::ErrorCode ProcessKeyCreate(const boost::program_options::variables_map& vm,
                                const boost::filesystem::path& data_path)
{
    std::string file;
    if (vm.count("file"))
    {
        file = vm["file"].as<std::string>();
    }

    return rai::CreateKey(data_path, file);
}

rai::ErrorCode ProcessSign(const boost::program_options::variables_map& vm,
                           const boost::filesystem::path& data_path)
{
    boost::filesystem::path key_path;
    rai::ErrorCode error_code = GetKeyPath(vm, data_path, key_path);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (!vm.count("hash"))
    {
        return rai::ErrorCode::CMD_MISS_HASH;
    }

    rai::BlockHash hash;
    bool error = hash.DecodeHex(vm["hash"].as<std::string>());
    if (error)
    {
        return rai::ErrorCode::CMD_INVALID_HASH;
    }

    rai::Fan key(rai::uint256_union(0), rai::Fan::FAN_OUT);
    error_code = rai::DecryptKey(key, key_path);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::RawKey private_key;
    key.Get(private_key);
    rai::PublicKey public_key = rai::GeneratePublicKey(private_key.data_);
    rai::Signature signature = rai::SignMessage(private_key, public_key, hash);
    std::cout << "signature:" << signature.StringHex() << std::endl;

    return rai::ErrorCode::SUCCESS;
}

}  // namespace

void rai::CliAddOptions(boost::program_options::options_description& desc){
    // clang-format off
    desc.add_options()
        ("daemon", "Start node daemon with a specified <key>")
        ("data_path", boost::program_options::value<std::string>(), "Use the supplied path as the data directory")
        ("file", boost::program_options::value<std::string>(), "Define <file> for other commands")
        ("hash",  boost::program_options::value<std::string>(), "Define <hash> for sign command")
        ("key", boost::program_options::value<std::string>(), "Define key file for daemon command")
        ("key_create", "Generate a random key pair and save it to <file>")
        ("sign", "Sign <hash> with a specified <key>")
        ;

    // clang-format on
}

rai::ErrorCode rai::CliProcessOptions(
    const boost::program_options::variables_map& vm)
{
    try 
    {
        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        boost::filesystem::path data_path;

        if (vm.count("data_path"))
        {
            data_path =
                boost::filesystem::path(vm["data_path"].as<std::string>());
            std::cout << data_path.string() << std::endl;
            if (data_path.string().find(".") == 0)
            {
                data_path = boost::filesystem::canonical(data_path);
            }
        }
        else
        {
            data_path = rai::WorkingPath();
        }

        boost::system::error_code ec;
        boost::filesystem::create_directories(data_path, ec);
        if (ec)
        {
            return rai::ErrorCode::DATA_PATH;
        }

        if (vm.count("daemon"))
        {
            error_code = ProcessDaemon(vm, data_path);
        }
        else if (vm.count("key_create"))
        {
            error_code = ProcessKeyCreate(vm, data_path);
        }
        else if (vm.count("sign"))
        {
            error_code = ProcessSign(vm, data_path);
        }
        else
        {
            error_code = rai::ErrorCode::UNKNOWN_COMMAND;
        }

        return error_code;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception(" << e.what () << ")\n";
    }
    catch (...)
    {
        std::cerr << "Unknown exception\n";
    }
    return rai::ErrorCode::SUCCESS;
}
 
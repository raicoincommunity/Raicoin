#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <rai/common/errors.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/runner.hpp>
#include <rai/rai_wallet_rpc/wallet_rpc.hpp>
#include <rai/secure/util.hpp>

namespace
{

rai::ErrorCode ProcessKeyCreate(const boost::program_options::variables_map& vm,
                                const boost::filesystem::path& data_path)
{
    bool dir = false;
    std::string file;
    boost::filesystem::path key_path;
    if (vm.count("file"))
    {
        file = vm["file"].as<std::string>();
        if ((file.find("/") != std::string::npos)
            || (file.find("\\") != std::string::npos))
        {
            key_path = boost::filesystem::path(file);
            if (file.find(".") == 0)
            {
                auto parent_path = key_path.parent_path();
                key_path = boost::filesystem::canonical(parent_path)
                           / key_path.filename();
            }
        }
        else
        {
            key_path = data_path / file;
        }
    }
    else
    {
        dir = true;
        key_path = data_path;
    }

    rai::ErrorCode error_code =  rai::CreateKey(key_path, dir);
    IF_NOT_SUCCESS_RETURN(error_code);

    std::cout
        << "IMPORTANT: the key file will be used as wallet seed, please back "
           "up it and remember the passowrd, you may need it to restore wallet"
        << std::endl;
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode ProcessVersion(const boost::program_options::variables_map& vm,
                              const boost::filesystem::path& data_path)
{
    std::cout << rai::RAI_VERSION_STRING << " " << rai::NetworkString()
              << " network" << std::endl;
    return rai::ErrorCode::SUCCESS;
}


rai::ErrorCode ProcessConfigCreate(
    const boost::program_options::variables_map& vm,
    const boost::filesystem::path& data_path)
{
    boost::filesystem::path config_path = data_path / "wallet_rpc_config.json";
    if (boost::filesystem::exists(config_path))
    {
        std::cout << "Error: the config file already exists:" << config_path
                  << std::endl;
        return rai::ErrorCode::SUCCESS;
    }

    if (!vm.count("callback_url"))
    {
        std::cout << "Error: please specify the 'callback_url' parameter to "
                     "receive wallet events"
                  << std::endl;
        return rai::ErrorCode::SUCCESS;
    }
    rai::Url url;
    bool error = url.Parse(vm["callback_url"].as<std::string>());
    if (error)
    {
        std::cout << "Error: invalid 'callback_url' parameter" << std::endl;
        rai::ErrorCode::SUCCESS;
    }

    if (!vm.count("representative"))
    {
        std::cout << "Error: please specify the 'representative' parameter, it "
                     "should be a node account which is shown when the "
                     "rai_node starts"
                  << std::endl;
        return rai::ErrorCode::SUCCESS;
    }
    rai::Account rep;
    error = rep.DecodeAccount(vm["representative"].as<std::string>());
    if (error)
    {
        std::cout << "Error: invalid 'representative' parameter" << std::endl;
        return rai::ErrorCode::SUCCESS;
    }

    rai::WalletRpcConfig config;
    config.callback_url_ = url;
    config.wallet_.preconfigured_reps_.clear();
    config.wallet_.preconfigured_reps_.push_back(rep);

    std::fstream config_file;
    rai::ErrorCode error_code =
        rai::WriteObject(config, config_path, config_file);
    config_file.close();
    IF_NOT_SUCCESS_RETURN(error_code);

    std::cout << "Success, the config file was saved to:" << config_path
              << std::endl;
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode Process(const boost::program_options::variables_map& vm,
                       const boost::filesystem::path& data_path)
{
    try
    {
        boost::filesystem::path dir(data_path);

        rai::ErrorCode error_code;
        rai::RawKey seed;
        if (vm.count("key"))
        {
            boost::filesystem::path key_path =
                dir / vm["key"].as<std::string>();
            if (!boost::filesystem::exists(key_path))
            {
                return rai::ErrorCode::KEY_FILE_NOT_EXIST;
            }

            rai::Fan key(rai::uint256_union(0), rai::Fan::FAN_OUT);
            error_code = rai::DecryptKey(key, key_path);
            IF_NOT_SUCCESS_RETURN(error_code);
            key.Get(seed);
        }
        else
        {
            if (!boost::filesystem::exists(dir / "wallet_data.ldb"))
            {
                std::cout << "Error: please specify a key file to create wallet"
                          << std::endl;
                return rai::ErrorCode::SUCCESS;
            }
        }

        rai::WalletRpcConfig config;
        boost::filesystem::path config_path = dir / "wallet_rpc_config.json";
        std::fstream config_file;
        error_code = rai::FetchObject(config, config_path, config_file);
        config_file.close();
        IF_NOT_SUCCESS_RETURN(error_code);
        if (!config.callback_url_)
        {
            return rai::ErrorCode::JSON_CONFIG_CALLBACK_URL;
        }
        if ((config.wallet_.server_.protocol_ == "wss"
             || config.callback_url_.protocol_ == "https")
            && !boost::filesystem::exists("cacert.pem"))
        {
            std::cout << "Error: the cacert.pem is missing, you can download "
                         "it from "
                         "https://github.com/raicoincommunity/Raicoin/releases"
                      << std::endl;
            return rai::ErrorCode::SUCCESS;
        }

        boost::asio::io_service service;
        rai::Alarm alarm(service);
        boost::asio::io_service service_wallets;
        std::shared_ptr<rai::Wallets> wallets = std::make_shared<rai::Wallets>(
            error_code, service_wallets, alarm, dir, config.wallet_,
            rai::BlockType::TX_BLOCK, seed);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            return error_code;
        }

        uint32_t wallet_id = wallets->SelectedWalletId();
        if (wallet_id != 1)
        {
            return rai::ErrorCode::GENERIC;
        }
        if (!seed.data_.IsZero())
        {
            if (wallets->WalletLocked(wallet_id))
            {
                return rai::ErrorCode::WALLET_EXISTS;
            }
            rai::RawKey seed_current;
            error_code = wallets->WalletSeed(wallet_id, seed_current);
            IF_NOT_SUCCESS_RETURN(error_code);
            if (seed.data_ != seed_current.data_)
            {
                return rai::ErrorCode::WALLET_EXISTS;
            }
            seed.data_.SecureClear();
        }

        if (wallets->WalletVulnerable(wallet_id))
        {
            rai::Password password;
            error_code = password.Input("Set wallet password:",
                                        "Retype wallet password");
            IF_NOT_SUCCESS_RETURN(error_code);
            error_code = wallets->ChangePassword(password.Get());
            IF_NOT_SUCCESS_RETURN(error_code);
        }
        else
        {
            rai::Password password;
            password.Input("Input wallet password:");
            bool error = wallets->EnterPassword(password.Get());
            if (error)
            {
                wallets->Stop();
                return rai::ErrorCode::PASSWORD_ERROR;
            }
        }
        wallets->Start();
        std::cout << "Current account:"
                  << wallets->SelectedAccount().StringAccount() << std::endl;

        auto wallet_rpc = std::make_shared<rai::WalletRpc>(wallets, config);

        std::unique_ptr<rai::Rpc> rpc =
            rai::MakeRpc(service, config.rpc_, wallet_rpc->RpcHandlerMaker());
        if (rpc != nullptr)
        {
            rpc->Start();
        }
        else
        {
            std::cout << "Make RPC failed\n";
            return rai::ErrorCode::GENERIC;
        }

        rai::ServiceRunner runner(service, 1);
        runner.Join();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error while running wallet_rpc (" << e.what() << ")\n";
    }
    return rai::ErrorCode::SUCCESS;
}
}  // namespace

int main(int argc, char* const* argv)
{
    boost::program_options::options_description desc("Command line options");
    desc.add_options()
    ("key", boost::program_options::value<std::string>(),
                       "Define key file to create wallet")
    ("file", boost::program_options::value<std::string>(), "Define <file> for other commands")
    ("key_create", "Generate a random key pair and save it to <file>")
    ("config_create", "Generate the wallet_rpc_config.json file")
    ("callback_url", boost::program_options::value<std::string>(), "Specify a url to receive wallet events")
    ("representative", boost::program_options::value<std::string>(), "Specify a node account as your wallet's representative")
    ("help", "Print usage help")
    ;

    boost::program_options::variables_map vm;
    try
    {
        boost::program_options::store(
            boost::program_options::parse_command_line(argc, argv, desc), vm);
    }
    catch (const boost::program_options::error& err)
    {
        std::cerr << err.what() << std::endl;
        return 1;
    }
    boost::program_options::notify(vm);

    boost::filesystem::path data_path(boost::filesystem::current_path());

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    if (vm.count("key_create"))
    {
        error_code = ProcessKeyCreate(vm, data_path);
    }
    else if (vm.count("version"))
    {
        error_code = ProcessVersion(vm, data_path);
    }
    else if (vm.count("config_create"))
    {
        error_code = ProcessConfigCreate(vm, data_path);
    }
    else if (vm.count("help"))
    {
        std::cout << desc << std::endl;
    }
    else
    {
        error_code = Process(vm, data_path);
    }

    if (rai::ErrorCode::SUCCESS != error_code)
    {
        std::cerr << rai::ErrorString(error_code) << ": "
                  << static_cast<int>(error_code) << std::endl;
        return 1;
    }

    return 0;
}

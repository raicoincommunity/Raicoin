#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <rai/common/errors.hpp>
#include <rai/common/numbers.hpp>
#include <rai/rai_airdrop/airdrop.hpp>
#include <rai/secure/util.hpp>


namespace
{
rai::ErrorCode Process(const boost::program_options::variables_map& vm)
{
    try
    {
        boost::filesystem::path dir(boost::filesystem::current_path());

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

        rai::AirdropConfig config;
        boost::filesystem::path config_path = dir / "airdrop_config.json";
        if (!boost::filesystem::exists(config_path))
        {
            return rai::ErrorCode::JSON_CONFIG_AIRDROP_MISS;
        }
        std::fstream config_file;
        error_code = rai::FetchObject(config, config_path, config_file);
        config_file.close();
        IF_NOT_SUCCESS_RETURN(error_code);

        boost::asio::io_service service;
        rai::Alarm alarm(service);
        std::shared_ptr<rai::Wallets> wallets = std::make_shared<rai::Wallets>(
            error_code, service, alarm, dir, config.wallet_,
            rai::BlockType::AD_BLOCK, seed, 10);
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

        auto airdrop = std::make_shared<rai::Airdrop>(wallets, config);
        IF_NOT_SUCCESS_RETURN(error_code);
        wallets->Start();
        airdrop->Start();

        airdrop->Join();
        wallets->Stop();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error while running node (" << e.what() << ")\n";
    }
    return rai::ErrorCode::SUCCESS;
}
}  // namespace

#if 1
int main(int argc, char* const* argv)
{
    boost::program_options::options_description desc("Command line options");
    desc.add_options()("key", boost::program_options::value<std::string>(),
                       "Define key file for daemon command");

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

    rai::ErrorCode error_code = Process(vm);
    if (rai::ErrorCode::UNKNOWN_COMMAND == error_code)
    {
        std::cout << desc << std::endl;
        return 0;
    }

    if (rai::ErrorCode::SUCCESS != error_code)
    {
        std::cerr << rai::ErrorString(error_code) << ": "
                  << static_cast<int>(error_code) << std::endl;
        return 1;
    }

    return 0;
}

#else
#include <rai/secure/http.hpp>
int main()
{
    uint32_t constexpr num = 129;
    std::array<uint32_t, num> a = {0,};
    for (uint32_t i = 0; i < 1000 * num; ++i)
    {
        auto r = rai::Random(0, num - 1);
        a[r]++;
    }

    uint64_t count = 0;
    for (uint32_t i = 0; i < num; ++i)
    {
        count += a[i];
        std::cout << i << ":" << a[i] << std::endl;
    }
    std::cout << "total:" << count << std::endl;
    return 0;
}
#endif

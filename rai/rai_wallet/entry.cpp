#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <exception>
#include <iostream>
#include <rai/secure/util.hpp>
#include <rai/rai_wallet/qt.hpp>
#include <rai/rai_wallet/icon.hpp>
#include <rai/wallet/config.hpp>

namespace
{
void ShowError(const std::string& message)
{
    QMessageBox message_box(QMessageBox::Critical,
                            "raicoin wallet", message.c_str());
    message_box.setModal(true);
    message_box.show();
    message_box.exec();
}

void ShowError(rai::ErrorCode error_code)
{
    std::string error_str = "error: ";
    error_str += rai::ErrorString(error_code) + "\n";
    error_str += "error_code: ";
    error_str += std::to_string(static_cast<uint32_t>(error_code));
    ShowError(error_str);
}

rai::ErrorCode Run(QApplication& application,
                   const boost::filesystem::path& data_path)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    try
    {
        rai::QtEventProcessor processor;
        boost::filesystem::path config_path = data_path / "wallet_config.json";
        rai::WalletConfig config;
        if (boost::filesystem::exists(config_path))
        {
            std::fstream config_file;
            error_code = rai::FetchObject(config, config_path, config_file);
            config_file.close();
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                ShowError(error_code);
                return error_code;
            }
        }

        boost::asio::io_service service;
        rai::Alarm alarm(service);
        std::shared_ptr<rai::Wallets> wallets = std::make_shared<rai::Wallets>(
            error_code, service, alarm, data_path, config,
            rai::BlockType::TX_BLOCK);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            ShowError(error_code);
            return error_code;
        }

        QObject::connect(&application, &QApplication::aboutToQuit, [&]() {
            wallets->Stop();
        });

        std::shared_ptr<rai::QtMain> gui(nullptr);
        application.postEvent(&processor, new rai::QtEvent([&]() {
            gui =
                std::make_shared<rai::QtMain>(application, processor, wallets);
            gui->Start();
            wallets->Start();
            gui->window_->show();
        }));
        application.exec();
        return rai::ErrorCode::SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::stringstream stream;
        stream << "Error while running rai_wallet (" << e.what() << ")\n";
        ShowError(stream.str());
    }

    return rai::ErrorCode::SUCCESS;
}
}  // namespace

int main(int argc, char* const* argv)
{
    try
    {
        QApplication application(argc, const_cast<char**>(argv));
        boost::program_options::options_description description(
            "Command line options");
    // clang-format off
        description.add_options()
            ("help", "Print out options")
            ("data_path", boost::program_options::value<std::string>(), "Use the supplied path as the data directory")
        ;
    // clang-format on
        boost::program_options::variables_map vm;
        boost::program_options::store(
            boost::program_options::command_line_parser(argc, argv)
                .options(description)
                .allow_unregistered()
                .run(),
            vm);
        boost::program_options::notify(vm);
        if (vm.count("help") != 0)
        {
            std::cout << description << std::endl;
            return 0;
        }

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
            std::cerr
                << "Failed to create data directory, please check <data_path>";
            return 1;
        }

        rai::SetAppIcon(application);
        rai::ErrorCode error_code = Run(application, data_path);
        if (rai::ErrorCode::SUCCESS != error_code)
        {
            std::cerr << rai::ErrorString(error_code) << ": "
                    << static_cast<int>(error_code) << std::endl;
            return 1;
        }
    }
    catch (std::exception const& e)
    {
        std::cerr << boost::str(
            boost::format("Exception while initializing %1%") % e.what());
    }
    catch (...)
    {
        std::cerr << boost::str(
            boost::format("Unknown exception while initializing"));
    }
}

#if 0
int main(int argc, char* const* argv)
{
    rai::KeyPair key_pair;
    key_pair.private_key_.data_.DecodeHex("34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4");
    key_pair.public_key_ = rai::GeneratePublicKey(key_pair.private_key_.data_);

    std::ofstream key_file("test_genesis.dat",
                           std::ios::binary | std::ios::out);
    if (key_file)
    {
        key_pair.Serialize("00000000", key_file);
    }
    key_file.close();
}
#endif

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <rai/common/parameters.hpp>
#include <rai/common/errors.hpp>
#include <rai/secure/util.hpp>
#include <rai/common/alarm.hpp>
#include <rai/rai_alias/config.hpp>
#include <rai/rai_alias/alias.hpp>

namespace
{
rai::ErrorCode ProcessVersion(const boost::program_options::variables_map& vm,
                              const boost::filesystem::path& data_path)
{
    std::cout << rai::RAI_VERSION_STRING << " " << rai::NetworkString()
              << " network" << std::endl;
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode Process(const boost::program_options::variables_map& vm,
                       const boost::filesystem::path& data_path)
{
    try
    {
        rai::AliasConfig config;
        boost::filesystem::path config_path = data_path / "alias_config.json";
        std::fstream config_file;
        rai::ErrorCode error_code =
            rai::FetchObject(config, config_path, config_file);
        config_file.close();
        IF_NOT_SUCCESS_RETURN(error_code);

        if (config.app_.node_gateway_.Ssl()
            && !boost::filesystem::exists("cacert.pem"))
        {
            std::cout << "Error: the cacert.pem is missing, you can download "
                         "it from "
                         "https://github.com/raicoincommunity/Raicoin/releases"
                      << std::endl;
            return rai::ErrorCode::SUCCESS;
        }

        rai::Log::Init(data_path, config.log_);
        boost::asio::io_service service;
        rai::Alarm alarm(service);
        auto alias = std::make_shared<rai::Alias>(error_code, service, data_path,
                                                alarm, config);
        IF_NOT_SUCCESS_RETURN(error_code);
        alias->Start();

        std::shared_ptr<rai::Rpc> rpc;
        if (config.rpc_enable_ || config.app_.ws_enable_)
        {
            rpc = rai::MakeRpc(service, config.rpc_, alias->RpcHandlerMaker());
            if (rpc == nullptr)
            {
                std::cout << "Error: failed to contruct rpc" << std::endl;
                return rai::ErrorCode::SUCCESS;
            }
        }
        alias->rpc_ = rpc;

        if (config.rpc_enable_)
        {
            rpc->Start();
        }

        if (rpc)
        {
            rai::ServiceRunner runner(
                service,
                std::max<size_t>(4, std::thread::hardware_concurrency()));
            runner.Join();
        }
        else
        {
            auto runner =
                std::make_shared<rai::OngoingServiceRunner>(service, 1);
            alias->runner_ = runner;
            runner->Join();
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error while running rai_alias (" << e.what() << ")\n";
    }

    return rai::ErrorCode::SUCCESS;
}

}  // namespace

int main(int argc, char* const* argv)
{
    boost::program_options::options_description desc("Command line options");
    desc.add_options()
    ("data_path", boost::program_options::value<std::string>(), "Use the supplied path as the data directory")
    ("version", "Prints out version")
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

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    try 
    {
        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return 0;
        }

        boost::filesystem::path data_path;

        if (vm.count("data_path"))
        {
            data_path =
                boost::filesystem::path(vm["data_path"].as<std::string>());
            if (data_path.string().find(".") == 0)
            {
                data_path = boost::filesystem::canonical(data_path);
            }
        }
        else
        {
            data_path = boost::filesystem::current_path();
        }
        data_path /= "RaiAlias";

        boost::system::error_code ec;
        boost::filesystem::create_directories(data_path, ec);
        if (ec)
        {
            error_code = rai::ErrorCode::DATA_PATH;
            std::cerr << rai::ErrorString(error_code) << ": "
                    << static_cast<int>(error_code) << std::endl;
            std::cerr << data_path.string() << std::endl;
            return 1;
        }

        if (vm.count("version"))
        {
            error_code = ProcessVersion(vm, data_path);
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
    catch (const std::exception& e)
    {
        std::cerr << "Exception(" << e.what () << ")\n";
    }

    return 1;
}
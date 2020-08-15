#include <rai/rai_node/daemon.hpp>

#include <iostream>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <rai/common/util.hpp>
#include <rai/secure/util.hpp>
#include <rai/common/log.hpp>
#include <rai/common/runner.hpp>
#include <rai/node/node.hpp>

rai::ErrorCode rai::Daemon::Run(const boost::filesystem::path& data_path,
                                const boost::filesystem::path& key_path)
{
    try
    {
        rai::DaemonConfig config(data_path);
        boost::filesystem::path config_path = data_path / "config.json";
        std::fstream config_file;
        rai::ErrorCode error_code =
            rai::FetchObject(config, config_path, config_file);
        config_file.close();
        IF_NOT_SUCCESS_RETURN(error_code);
        rai::Log::Init(data_path, config.node_.log_);

        rai::Fan key(rai::uint256_union(0), rai::Fan::FAN_OUT);
        error_code = rai::DecryptKey(key, key_path);
        IF_NOT_SUCCESS_RETURN(error_code);
        
        boost::asio::io_service service;
        rai::Alarm alarm(service);
        auto node = std::make_shared<rai::Node>(error_code, service, data_path,
                                                alarm, config.node_, key);
        IF_NOT_SUCCESS_RETURN(error_code);
        node->Start();

        std::unique_ptr<rai::Rpc> rpc;
        if (config.rpc_.enable_)
        {
            rpc = rai::MakeRpc(service, config.rpc_.RpcConfig(),
                               node->RpcHandlerMaker());
            if (rpc != nullptr)
            {
                rpc->Start();
            }
        }

        rai::ServiceRunner runner(service, config.node_.io_threads_);
        runner.Join();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error while running node (" << e.what () << ")\n";
    }

    return rai::ErrorCode::SUCCESS;
}

rai::DaemonConfig::DaemonConfig(const boost::filesystem::path& data_path)
    : node_(), rpc_()
{
}

rai::ErrorCode rai::DaemonConfig::DeserializeJson(bool& upgraded,
                                                  rai::Ptree& ptree)
{
    if (ptree.empty())
    {
        upgraded = true;
        SerializeJson(ptree);
        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    try
    {
        error_code = rai::ErrorCode::JSON_CONFIG_VERSION;
        std::string version_str = ptree.get<std::string>("version");
        uint32_t version = 0;
        bool error = rai::StringToUint(version_str, version);
        IF_ERROR_RETURN(error, error_code);

        error_code = UpgradeJson(upgraded, version, ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_NODE;
        rai::Ptree node_ptree = ptree.get_child("node");
        error_code = node_.DeserializeJson(upgraded, node_ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_RPC;
        rai::Ptree rpc_ptree = ptree.get_child("rpc");
        error_code = rpc_.DeserializeJson(upgraded, rpc_ptree);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    catch (const std::exception&)
    {
        return error_code;
    }
    return rai::ErrorCode::SUCCESS;
}

void rai::DaemonConfig::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("version", "1");

    rai::Ptree rpc_ptree;
    rpc_.SerializeJson(rpc_ptree);
    ptree.add_child("rpc", rpc_ptree);

    rai::Ptree node_ptree;
    node_.SerializeJson(node_ptree);
    ptree.add_child("node", node_ptree);
}

rai::ErrorCode rai::DaemonConfig::UpgradeJson(bool& upgraded, uint32_t version,
                                              rai::Ptree& ptree) const
{
    switch (version)
    {
        case 1:
        {
            break;
        }
        default:
        {
            return rai::ErrorCode::CONFIG_VERSION;
        }
    }

    return rai::ErrorCode::SUCCESS;
}



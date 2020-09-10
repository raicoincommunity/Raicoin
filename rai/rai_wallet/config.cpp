#include <rai/rai_wallet/config.hpp>

rai::QtWalletConfig::QtWalletConfig() : auto_receive_(true)
{
}

rai::ErrorCode rai::QtWalletConfig::DeserializeJson(bool& upgraded,
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

        error_code = rai::ErrorCode::JSON_CONFIG_AUTO_RECEIVE;
        auto auto_receive_o = ptree.get_optional<bool>("auto_receive");
        auto_receive_ = auto_receive_o ? *auto_receive_o : true;


        error_code = rai::ErrorCode::JSON_CONFIG_WALLET;
        rai::Ptree wallet_ptree = ptree.get_child("wallet");
        error_code = wallet_.DeserializeJson(upgraded, wallet_ptree);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    catch (const std::exception&)
    {
        return error_code;
    }
    return rai::ErrorCode::SUCCESS;
}

void rai::QtWalletConfig::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("version", "2");
    ptree.put("auto_receive", auto_receive_);

    rai::Ptree wallet_ptree;
    wallet_.SerializeJson(wallet_ptree);
    ptree.add_child("wallet", wallet_ptree);
}

rai::ErrorCode rai::QtWalletConfig::UpgradeJson(bool& upgraded, uint32_t version,
                                               rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    switch (version)
    {
        case 1:
        {
            error_code = UpgradeV1V2(ptree);
            IF_NOT_SUCCESS_RETURN(error_code);
            upgraded = true;
        }
        case 2:
        {
            break;
        }
        default:
        {
            return rai::ErrorCode::CONFIG_VERSION;
        }
    }

    return error_code;
}

rai::ErrorCode rai::QtWalletConfig::UpgradeV1V2(rai::Ptree& ptree) const
{
    rai::Ptree wallet;
    ptree.swap(wallet);
    ptree.put("version", "2");
    ptree.put("auto_receive", auto_receive_);
    ptree.put_child("wallet", wallet);
    return rai::ErrorCode::SUCCESS;
}

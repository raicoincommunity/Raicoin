#include <rai/wallet/config.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/util.hpp>

namespace
{
std::string DefaultServer()
{
    switch (rai::RAI_NETWORK)
    {
        case rai::RaiNetworks::TEST:
        {
            return "wss://server-test.raicoin.org";
        }
        case rai::RaiNetworks::BETA:
        {
            return "wss://server-beta.raicoin.org";
        }
        case rai::RaiNetworks::LIVE:
        {
            return "wss://server.raicoin.org";
        }
        default:
        {
            throw std::runtime_error("Unknown rai::RAI_NETWORK");
        }
    }
}

std::vector<std::string> DefaultPreconfiguredReps()
{
    std::vector<std::string> result;
    switch (rai::RAI_NETWORK)
    {
        case rai::RaiNetworks::TEST:
        {
            break;
        }
        case rai::RaiNetworks::BETA:
        {
            result.push_back("rai_1jngsd4mnzfb4h5j8eji5ynqbigaz16dmjjdz8us6run1ziyzuqfhz8yz3uf");
            result.push_back("rai_1936xw8zcxzsqx1rr97xwajigtdtsws45gatwu1ztt658gnpw1s1ttcwu11b");
            result.push_back("rai_36bu4g1tegdbwwqnec4otqncqkjzyn5134skna4i8gexhx11eeawry4akjhu");
            result.push_back("rai_14k51wrdhpfyf3ikh811pedndjof6fffziokwymapwjbk8obrztmawsw9bnm");
            break;
        }
        case rai::RaiNetworks::LIVE:
        {
            result.push_back("rai_3h5s5bgaf1jp1rofe5umxan84kiwxj3ppeuyids7zzaxahsohzchcyxqzwp6");
            result.push_back("rai_1os5ozxsjajpnkdj6zghzdy5fjncpa6egkjisgggutxkbmqicc8mjjfy87ja");
            result.push_back("rai_1khj7pa81ffn3o44jfqopeoq3apxpdagpjo9gu1nwc6x9ccpjggjouhifkkw");
            result.push_back("rai_1b5wb8hs5d3u5q8cnesk5xprr4damryn9xmrju7grdrtq6sxpanrwjo4s4r3");
            break;
        }
        default:
        {
            throw std::runtime_error("Unknown rai::RAI_NETWORK");
        }
    }
    return result;
}
};  // namespace

rai::WalletConfig::WalletConfig() : server_("wss")
{
    bool error = server_.Parse(DefaultServer());
    if (error)
    {
        throw std::runtime_error("Invalid default wallet server");
    }

    std::vector<std::string> reps = DefaultPreconfiguredReps();
    for (const auto& i : reps)
    {
        rai::Account rep;
        bool error = rep.DecodeAccount(i);
        if (error)
        {
            throw std::runtime_error("Invalid default preconfigured representative:" + i);
        }
        preconfigured_reps_.push_back(rep);
    }
}

rai::ErrorCode rai::WalletConfig::DeserializeJson(bool& update,
                                                  rai::Ptree& ptree)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    try
    {
        error_code = rai::ErrorCode::JSON_CONFIG_WALLET_VERSION;
        std::string version_str = ptree.get<std::string>("version");
        uint32_t version = 0;
        bool error = rai::StringToUint(version_str, version);
        IF_ERROR_RETURN(error, error_code);

        error_code = UpgradeJson(update, version, ptree);
        IF_NOT_SUCCESS_RETURN(error_code);

        error_code = rai::ErrorCode::JSON_CONFIG_WALLET_SERVER;
        auto server = ptree.get_optional<std::string>("server");
        if (!server || server_.Parse(*server))
        {
            return error_code;
        }

        error_code = rai::ErrorCode::JSON_CONFIG_WALLET_PRECONFIGURED_REP;
        auto reps_o = ptree.get_child_optional("preconfigured_representatives");
        if (reps_o)
        {
            std::vector<rai::Account> reps;
            for (const auto& i : *reps_o)
            {
                rai::Account rep;
                bool error = rep.DecodeAccount(i.second.get<std::string>(""));
                IF_ERROR_RETURN(error, error_code);
                reps.push_back(rep);
            }
            if (!reps.empty())
            {
                preconfigured_reps_ = std::move(reps);
            }
        }
        if (preconfigured_reps_.empty())
        {
            return error_code;
        }
    }
    catch (const std::exception&)
    {
        return error_code;
    }
    return rai::ErrorCode::SUCCESS;
}

void rai::WalletConfig::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("version", "1");

    if (DefaultServer() != server_.String())
    {
        ptree.put("server", server_.String());
    }

    rai::Ptree preconfigured_reps;
    for (const auto& i : preconfigured_reps_)
    {
        rai::Ptree entry;
        entry.put("", i.StringAccount());
        preconfigured_reps.push_back(std::make_pair("", entry));
    }
    ptree.add_child("preconfigured_representatives", preconfigured_reps);
}

rai::ErrorCode rai::WalletConfig::UpgradeJson(bool& update, uint32_t version,
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
            return rai::ErrorCode::CONFIG_WALLET_VERSION;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

#include <rai/rai_token/crosschain/config.hpp>

namespace
{
std::vector<std::string> DefaultEndpoints(rai::Chain chain)
{
    std::vector<std::string> result;
    switch (chain)
    {
        case rai::Chain::ETHEREUM:
        {
            result.push_back("https://rpc.ankr.com/eth");
            result.push_back("https://eth-rpc.gateway.pokt.network");
            result.push_back("https://main-rpc.linkpool.io/");
            result.push_back("https://api.mycryptoapi.com/eth");
            result.push_back("https://eth.public-rpc.com");
            break;
        }
        case rai::Chain::BINANCE_SMART_CHAIN:
        {
            result.push_back("https://bsc-dataseed.binance.org/");
            result.push_back("https://bsc-dataseed1.defibit.io/");
            result.push_back("https://bsc-dataseed1.ninicoin.io/");
            result.push_back("https://bsc.nodereal.io");
            result.push_back("https://rpc.ankr.com/bsc");
            break;
        }
        case rai::Chain::ETHEREUM_TEST_GOERLI:
        {
            result.push_back("https://rpc.ankr.com/eth_goerli");
            break;
        }
        case rai::Chain::BINANCE_SMART_CHAIN_TEST:
        {
            result.push_back("https://data-seed-prebsc-1-s1.binance.org:8545/");
            result.push_back("https://data-seed-prebsc-1-s2.binance.org:8545/");
            result.push_back("https://data-seed-prebsc-1-s3.binance.org:8545/");
            result.push_back("https://data-seed-prebsc-2-s1.binance.org:8545/");
            result.push_back("https://data-seed-prebsc-2-s2.binance.org:8545/");
            result.push_back("https://data-seed-prebsc-2-s3.binance.org:8545/");
            break;
        }
        default:
        {
            break;
        }
    }
    return result;
}
}  // namespace

rai::CrossChainConfig::CrossChainConfig()
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    std::vector<std::string> endpoints;
    switch (rai::RAI_NETWORK)
    {
        case rai::RaiNetworks::TEST:
        case rai::RaiNetworks::BETA:
        {
            endpoints = DefaultEndpoints(rai::Chain::ETHEREUM_TEST_GOERLI);
            error_code = MakeUrls_(endpoints, eth_test_goerli_urls_);
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                throw std::runtime_error(
                    "Failed to parse default eth goerli testnet endpoints");
            }

            endpoints = DefaultEndpoints(rai::Chain::BINANCE_SMART_CHAIN_TEST);
            error_code = MakeUrls_(endpoints, bsc_test_urls_);
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                throw std::runtime_error(
                    "Failed to parse default bsc testnet endpoints");
            }
            break;
        }
        case rai::RaiNetworks::LIVE:
        {
            endpoints = DefaultEndpoints(rai::Chain::ETHEREUM);
            error_code = MakeUrls_(endpoints, eth_urls_);
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                throw std::runtime_error(
                    "Failed to parse default eth endpoints");
            }

            endpoints = DefaultEndpoints(rai::Chain::BINANCE_SMART_CHAIN);
            error_code = MakeUrls_(endpoints, bsc_urls_);
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                throw std::runtime_error(
                    "Failed to parse default bsc endpoints");
            }
            break;
        }
        default:
        {
            throw std::runtime_error("Unknown rai::RAI_NETWORK");
        }
    }
}

rai::ErrorCode rai::CrossChainConfig::DeserializeJson(bool& upgraded,
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

        switch (rai::RAI_NETWORK)
        {
            case rai::RaiNetworks::TEST:
            case rai::RaiNetworks::BETA:
            {
                error_code =
                    rai::ErrorCode::JSON_CONFIG_CROSS_CHAIN_ETH_TEST_GOERLI;
                error_code = DeserializeEndpoints_(
                    ptree.get_child("eth_test_goerli"), eth_test_goerli_urls_);
                IF_NOT_SUCCESS_RETURN(error_code);

                error_code = rai::ErrorCode::JSON_CONFIG_CROSS_CHAIN_BSC_TEST;
                error_code = DeserializeEndpoints_(
                    ptree.get_child("bsc_test"), bsc_test_urls_);
                IF_NOT_SUCCESS_RETURN(error_code);
                break;
            }
            case rai::RaiNetworks::LIVE:
            {
                error_code = rai::ErrorCode::JSON_CONFIG_CROSS_CHAIN_ETH;
                error_code =
                    DeserializeEndpoints_(ptree.get_child("eth"), eth_urls_);
                IF_NOT_SUCCESS_RETURN(error_code);
                error_code = rai::ErrorCode::JSON_CONFIG_CROSS_CHAIN_BSC;
                error_code =
                    DeserializeEndpoints_(ptree.get_child("bsc"), bsc_urls_);
                IF_NOT_SUCCESS_RETURN(error_code);
                break;
            }
            default:
            {
                throw std::runtime_error("Unknown rai::RAI_NETWORK");
            }
        }
    }
    catch (const std::exception&)
    {
        return error_code;
    }
    return rai::ErrorCode::SUCCESS;
}

void rai::CrossChainConfig::SerializeJson(rai::Ptree& ptree) const
{
    ptree.put("version", "1");

    switch (rai::RAI_NETWORK)
    {
        case rai::RaiNetworks::TEST:
        case rai::RaiNetworks::BETA:
        {
            rai::Ptree eth_test_goerli;
            SerializeEndpoints_(eth_test_goerli_urls_, eth_test_goerli);
            ptree.put_child("eth_test_goerli", eth_test_goerli);

            rai::Ptree bsc_test;
            SerializeEndpoints_(bsc_test_urls_, bsc_test);
            ptree.put_child("bsc_test", bsc_test);
            break;
        }
        case rai::RaiNetworks::LIVE:
        {
            rai::Ptree eth;
            SerializeEndpoints_(eth_urls_, eth);
            ptree.put_child("eth", eth);

            rai::Ptree bsc;
            SerializeEndpoints_(bsc_urls_, bsc);
            ptree.put_child("bsc", bsc);
            break;
        }
        default:
        {
            throw std::runtime_error("Unknown rai::RAI_NETWORK");
        }
    }
}


rai::ErrorCode rai::CrossChainConfig::UpgradeJson(bool& upgraded, uint32_t version,
                                              rai::Ptree& ptree) const
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
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

rai::ErrorCode rai::CrossChainConfig::DeserializeEndpoints_(
    const rai::Ptree& ptree, std::vector<rai::Url>& urls) const
{
    rai::Ptree endpoints_ptree = ptree.get_child("endpoints");
    std::vector<std::string> endpoints;
    for (const auto& i : endpoints_ptree)
    {
        endpoints.push_back(i.second.get<std::string>(""));
    }
    if (endpoints.size() < 2)
    {
        return rai::ErrorCode::JSON_CONFIG_CROSS_CHAIN_NO_ENOUGH_ENDPOINTS;
    }
    return MakeUrls_(endpoints, urls);
}

void rai::CrossChainConfig::SerializeEndpoints_(
    const std::vector<rai::Url>& urls, rai::Ptree& ptree) const
{
    rai::Ptree endpoints;
    for (const auto& i : urls)
    {
        rai::Ptree entry;
        entry.put("", i.String());
        endpoints.push_back(std::make_pair("", entry));
    }
    ptree.put_child("endpoints", endpoints);
}

rai::ErrorCode rai::CrossChainConfig::MakeUrls_(
    const std::vector<std::string>& strs, std::vector<rai::Url>& urls) const
{
    urls.clear();
    for (const auto& i : strs)
    {
        rai::Url url;
        if (url.Parse(i) || (url.protocol_ != "http" && url.protocol_ != "https"))
        {
            return rai::ErrorCode::JSON_CONFIG_CROSS_CHAIN_INVALID_ENDPOINT;
        }
        urls.push_back(url);
    }
    return rai::ErrorCode::SUCCESS;
}
#include <rai/rai_alias/alias.hpp>

rai::Alias::Alias(rai::ErrorCode& error_code, boost::asio::io_service& service,
                  const boost::filesystem::path& data_path, rai::Alarm& alarm,
                  const rai::AliasConfig& config): App(error_code, service, data_path / "alias_data.ldb", alarm, config.app_, subscribe_, rai::Alias::BlockTypes(), rai::Alias::Provide()), service_(service), alarm_(alarm), config_(config), subscribe_(*this)
{
    IF_NOT_SUCCESS_RETURN_VOID(error_code);
}

std::shared_ptr<rai::Alias> rai::Alias::Shared()
{
    return Shared_<rai::Alias>();
}

std::vector<rai::BlockType> rai::Alias::BlockTypes()
{
    std::vector<rai::BlockType> types{rai::BlockType::TX_BLOCK,
                                      rai::BlockType::REP_BLOCK};
    return types;
}

rai::Provider::Info rai::Alias::Provide()
{
    using P = rai::Provider;
    P::Info info;
    info.id_ = P::Id::ALIAS;

    info.filters_.push_back(P::Filter::APP_ACCOUNT);

    info.actions_.push_back(P::Action::APP_ACCOUNT_SYNC);

    return info;
}

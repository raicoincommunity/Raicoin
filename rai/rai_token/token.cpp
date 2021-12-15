#include <rai/rai_token/token.hpp>

rai::Token::Token(rai::ErrorCode& error_code, boost::asio::io_service& service,
                  const boost::filesystem::path& data_path, rai::Alarm& alarm,
                  const rai::TokenConfig& config)
    : App(error_code, service, data_path / "token_data.ldb", alarm, config.app_,
          subscribe_, rai::Token::BlockTypes(), rai::Token::Provide()),
      service_(service),
      alarm_(alarm),
      config_(config),
      subscribe_(*this)
{
    IF_NOT_SUCCESS_RETURN_VOID(error_code);
    error_code = InitLedger_();
}

std::vector<rai::BlockType> rai::Token::BlockTypes()
{
    std::vector<rai::BlockType> types{rai::BlockType::TX_BLOCK,
                                      rai::BlockType::REP_BLOCK};
    return types;
}

rai::Provider::Info rai::Token::Provide()
{
    using P = rai::Provider;
    P::Info info;
    info.id_ = P::Id::TOKEN;

    info.filters_.push_back(P::Filter::APP_ACCOUNT);

    info.actions_.push_back(P::Action::APP_SERVICE_SUBSCRIBE);
    info.actions_.push_back(P::Action::APP_ACCOUNT_SYNC);
    // todo:

    return info;
}

rai::ErrorCode rai::Token::InitLedger_()
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, true);
    IF_NOT_SUCCESS_RETURN(error_code);

    uint32_t version = 0;
    bool error = ledger_.VersionGet(transaction, version);
    if (error)
    {
        error =
            ledger_.VersionPut(transaction, rai::Token::CURRENT_LEDGER_VERSION);
        IF_ERROR_RETURN(error, rai::ErrorCode::TOKEN_LEDGER_PUT);
        return rai::ErrorCode::SUCCESS;
    }

    if (version > rai::Token::CURRENT_LEDGER_VERSION)
    {
        std::cout << "[Error] Ledger version=" << version << std::endl;
        return rai::ErrorCode::LEDGER_VERSION;
    }

    return rai::ErrorCode::SUCCESS;
}
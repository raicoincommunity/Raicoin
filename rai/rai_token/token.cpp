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

rai::ErrorCode rai::Token::PreBlockAppend(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    bool confirmed)
{
    if (confirmed)
    {
        return rai::ErrorCode::SUCCESS;
    }

    do
    {
        if (block->Extensions().empty())
        {
            break;
        }

        rai::Extensions extensions;
        rai::ErrorCode error_code = extensions.FromBytes(block->Extensions());
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            break;
        }

        if (extensions.Count(rai::ExtensionType::TOKEN) > 0)
        {
            return rai::ErrorCode::APP_PROCESS_CONFIRM_REQUIRED;
        }
    } while (0);
    

    // todo: check swap waitings
    

    return rai::ErrorCode::SUCCESS;
}


rai::ErrorCode rai::Token::AfterBlockAppend(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    bool confirmed)
{
    do
    {
        if (block->Extensions().empty())
        {
            break;
        }

        rai::Extensions extensions;
        rai::ErrorCode error_code = extensions.FromBytes(block->Extensions());
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            break;
        }

        if (extensions.Count(rai::ExtensionType::TOKEN) == 0)
        {
            break;
        }

        if (!confirmed)
        {
            rai::Log::Error(
                "Alias::AfterBlockAppend: unexpected unconfirmed block");
            return rai::ErrorCode::APP_PROCESS_CONFIRM_REQUIRED;
        }

        error_code =  Process(transaction, block, extensions);
        IF_NOT_SUCCESS_RETURN(error_code);
    } while (0);
    


    // todo: process swap waitings


    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::Process(rai::Transaction& transaction,
                                   const std::shared_ptr<rai::Block>& block,
                                   const rai::Extensions& extensions)
{
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
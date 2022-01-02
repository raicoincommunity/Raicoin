#include <rai/rai_token/token.hpp>

#include <rai/common/parameters.hpp>

rai::TokenError::TokenError() : return_code_(rai::ErrorCode::SUCCESS)
{
}

rai::TokenError::TokenError(rai::ErrorCode error_code)
    : return_code_(error_code)
{
}

rai::TokenError::TokenError(rai::ErrorCode error_code,
                            const rai::TokenKey& token)
    : return_code_(error_code), token_(token)
{
}

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
    // todo: process swap timeout


    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::Process(rai::Transaction& transaction,
                                   const std::shared_ptr<rai::Block>& block,
                                   const rai::Extensions& extensions)
{
    rai::Account account = block->Account();
    uint64_t height = block->Height();
    rai::AccountTokensInfo info;
    bool error = ledger_.AccountTokensInfoGet(transaction, account, info);
    if (!error && info.head_ >= height)
    {
        rai::Log::Error(rai::ToString("Token::Process: block re-entry, hash=",
                                      block->Hash().StringHex()));
        return rai::ErrorCode::SUCCESS;
    }

    if (extensions.Count(rai::ExtensionType::TOKEN) != 1)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_MULTI_EXTENSIONS);
    }

    rai::ExtensionToken token;
    rai::ErrorCode error_code =
        token.FromExtension(extensions.Get(rai::ExtensionType::TOKEN));
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return UpdateLedgerCommon_(transaction, block, error_code);
    }

    if (token.op_data_ == nullptr)
    {
        rai::Log::Error(
            rai::ToString("Token::Process: op_data is null, hash=",
                            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    using Op = rai::ExtensionToken::Op;
    rai::TokenError token_error;
    switch (token.op_)
    {
        case Op::CREATE:
        {
            token_error = ProcessCreate_(transaction, block, token);
            break;
        }
        case Op::MINT:
        {
            token_error = ProcessMint_(transaction, block, token);
            break;
        }
        case Op::BURN:
        {
            token_error = ProcessBurn_(transaction, block, token);
            break;
        }
        case Op::SEND:
        {
            token_error = ProcessSend_(transaction, block, token);
            break;
        }
        case Op::RECEIVE:
        {
            token_error = ProcessReceive_(transaction, block, token);
            break;
        }
        // todo:
        default:
        {
            rai::Log::Error(
                rai::ToString("Token::Process: unknown token operation, hash=",
                              block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }
    }

    error_code = ProcessError_(transaction, token_error);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

void rai::Token::Start()
{
    std::weak_ptr<rai::Token> token(Shared());
    receivable_observer_ = [token](const rai::TokenReceivableKey& key,
                                   const rai::TokenReceivable& receivable) {
        auto token_s = token.lock();
        if (token_s == nullptr) return;

        token_s->Background([token, key, receivable]() {
            if (auto token_s = token.lock())
            {
                token_s->observers_.receivable_.Notify(key, receivable);
            }
        });
    };

    token_creation_observer_ = [token](const rai::TokenKey& key) {
        auto token_s = token.lock();
        if (token_s == nullptr) return;

        token_s->Background([token, key]() {
            if (auto token_s = token.lock())
            {
                token_s->observers_.token_creation_.Notify(key);
            }
        });
    };

    App::Start();
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

rai::TokenError rai::Token::ProcessCreate_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    rai::TokenKey key(rai::CurrentChain(), block->Account());
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, key, info);
    if (!error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_RECREATE, keys);
    }

    auto create =
        std::static_pointer_cast<rai::ExtensionTokenCreate>(extension.op_data_);
    rai::ErrorCode error_code = create->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessCreate_: check data failed, error_code=", error_code,
            ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (create->creation_data_ == nullptr)
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessCreate_: creation data is null, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (create->type_ == rai::TokenType::_20)
    {
        auto create20 = std::static_pointer_cast<rai::ExtensionToken20Create>(
            create->creation_data_);
        rai::TokenInfo info(
            rai::TokenType::_20, create20->symbol_, create20->name_,
            create20->decimals_, create20->burnable_, create20->mintable_,
            create20->circulable_, create20->cap_supply_, block->Height());
        if (create20->init_supply_ > 0)
        {
            info.total_supply_ = create20->init_supply_;
            info.local_supply_ = create20->init_supply_;
            rai::TokenReceivableKey receivable_key(
                block->Account(), key, rai::CurrentChain(), block->Hash());
            rai::TokenReceivable receivable(
                block->Account(), create20->init_supply_, block->Height(),
                rai::TokenType::_20, rai::TokenSource::MINT);
            error_code = UpdateLedgerReceivable_(transaction, receivable_key,
                                                 receivable);
            IF_NOT_SUCCESS_RETURN(error_code);
        }

        error = ledger_.TokenInfoPut(transaction, key, info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessCreate_: put token info, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }
    }
    else if (create->type_ == rai::TokenType::_721)
    {
        auto create721 = std::static_pointer_cast<rai::ExtensionToken721Create>(
            create->creation_data_);
        rai::TokenInfo info(rai::TokenType::_721, create721->symbol_,
                            create721->name_, create721->burnable_,
                            create721->circulable_, create721->cap_supply_,
                            create721->base_uri_, block->Height());
        error = ledger_.TokenInfoPut(transaction, key, info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessCreate_: put token info, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }
    }
    else
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessCreate_: unexpected token type, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (token_creation_observer_)
    {
        token_creation_observer_(key);
    }

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessMint_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    rai::TokenKey key(rai::CurrentChain(), block->Account());
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_NOT_CREATED);
    }

    if (!info.mintable_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_UNMINTABLE, keys);
    }

    auto mint =
        std::static_pointer_cast<rai::ExtensionTokenMint>(extension.op_data_);
    rai::ErrorCode error_code = mint->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessMint_: check data failed, error_code=", error_code,
            ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (mint->type_ != info.type_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_TYPE_INVALID, keys);
    }

    rai::TokenValue total_supply(0);
    rai::TokenValue local_supply(0);
    if (mint->type_ == rai::TokenType::_20)
    {
        total_supply = info.total_supply_ + mint->value_;
        if (total_supply <= info.total_supply_)
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_TOTAL_SUPPLY_OVERFLOW,
                keys);
        }

        if (info.cap_supply_ > 0 && total_supply > info.cap_supply_)
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_CAP_SUPPLY_EXCEEDED,
                keys);
        }

        local_supply = info.local_supply_ + mint->value_;
        if (local_supply <= info.local_supply_)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessMint_: local supply overflow, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        rai::TokenReceivableKey receivable_key(
            mint->to_, key, rai::CurrentChain(), block->Hash());
        rai::TokenReceivable receivable(
            block->Account(), mint->value_, block->Height(),
            rai::TokenType::_20, rai::TokenSource::MINT);
        error_code =
            UpdateLedgerReceivable_(transaction, receivable_key, receivable);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else if (mint->type_ == rai::TokenType::_721)
    {
        total_supply = info.total_supply_ + 1;
        if (total_supply <= info.total_supply_)
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_TOTAL_SUPPLY_OVERFLOW,
                keys);
        }

        if (info.cap_supply_ > 0 && total_supply > info.cap_supply_)
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_CAP_SUPPLY_EXCEEDED,
                keys);
        }

        local_supply = info.local_supply_ + 1;
        if (local_supply <= info.local_supply_)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessMint_: local supply overflow, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        rai::TokenIdInfo token_id_info;
        error = ledger_.TokenIdInfoGet(transaction, key, mint->value_,
                                       token_id_info);
        if (!error && !token_id_info.burned_)
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_ID_REMINT,
                keys);
        }

        if (error)
        {
            token_id_info = rai::TokenIdInfo(mint->uri_);
        }
        else
        {
            token_id_info.burned_ = false;
            token_id_info.uri_ = mint->uri_;
        }

        error = ledger_.TokenIdInfoPut(transaction, key, mint->value_,
                                       token_id_info);
        if (error)
        {
            rai::Log::Error(
                rai::ToString("Token::ProcessMint_: put token id info, hash=",
                              block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }

        rai::TokenReceivableKey receivable_key(
            mint->to_, key, rai::CurrentChain(), block->Hash());
        rai::TokenReceivable receivable(block->Account(), mint->value_,
                                        block->Height(), rai::TokenType::_721,
                                        rai::TokenSource::MINT);
        error_code =
            UpdateLedgerReceivable_(transaction, receivable_key, receivable);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessMint_: unexpected token type, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    error_code =
        UpdateLedgerSupplies_(transaction, key, total_supply, local_supply);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessBurn_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    rai::TokenKey key(rai::CurrentChain(), block->Account());
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_NOT_CREATED);
    }

    if (!info.burnable_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_UNBURNABLE, keys);
    }

    auto burn =
        std::static_pointer_cast<rai::ExtensionTokenBurn>(extension.op_data_);
    rai::ErrorCode error_code = burn->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessBurn_: check data failed, error_code=", error_code,
            ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (burn->type_ != info.type_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_TYPE_INVALID, keys);
    }

    rai::AccountTokenInfo account_token_info;
    error = ledger_.AccountTokenInfoGet(transaction, block->Account(),
                                        rai::CurrentChain(), block->Account(),
                                        account_token_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessBurn_: get account token info failed, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    rai::TokenValue balance(account_token_info.balance_);
    rai::TokenValue total_supply(0);
    rai::TokenValue local_supply(0);
    if (burn->type_ == rai::TokenType::_20)
    {
        if (burn->value_ > account_token_info.balance_)
        {
            return UpdateLedgerCommon_(
                transaction, block,
                rai::ErrorCode::TOKEN_BURN_MORE_THAN_BALANCE, keys);
        }
        balance = account_token_info.balance_ - burn->value_;

        if (burn->value_ > info.total_supply_)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: burn more than total supply, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        total_supply = info.total_supply_ - burn->value_;

        if (burn->value_ > info.local_supply_)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: burn more than local supply, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        local_supply = info.local_supply_ - burn->value_;
    }
    else if (burn->type_ == rai::TokenType::_721)
    {
        rai::TokenValue token_id = burn->value_;
        rai::AccountTokenId account_token_id(block->Account(), key, token_id);
        if (!ledger_.AccountTokenIdExist(transaction, account_token_id))
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_ID_NOT_OWNED, keys);
        }

        if (account_token_info.balance_ < 1)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: account token balance underflow, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        balance = account_token_info.balance_ - 1;

        if (info.total_supply_ < 1)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: token total supply underflow, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        total_supply = info.total_supply_ - 1;

        if (info.local_supply_ < 1)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: token local supply underflow, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT; // Ledger inconsistency
        }
        local_supply = info.local_supply_ - 1;

        rai::TokenIdInfo token_id_info;
        error =
            ledger_.TokenIdInfoGet(transaction, key, token_id, token_id_info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: get token id info failed, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT; // Ledger inconsistency
        }
        token_id_info.burned_ = true;
        error =
            ledger_.TokenIdInfoPut(transaction, key, token_id, token_id_info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: put token id info failed, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }

        error = ledger_.AccountTokenIdDel(transaction, account_token_id);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessMint_: del account token id, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
        }

        error_code = UpdateLedgerTokenIdTransfer_(
            transaction, key, burn->value_, block->Account(), block->Height());
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessBurn_: unexpected token type, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    error_code =
        UpdateLedgerTokenTransfer_(transaction, key, block->Timestamp(),
                                   block->Account(), block->Height());
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerBalance_(transaction, block->Account(), key,
                             account_token_info.balance_, balance);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerSupplies_(transaction, key, total_supply, local_supply);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessSend_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    auto send =
        std::static_pointer_cast<rai::ExtensionTokenSend>(extension.op_data_);
    rai::ErrorCode error_code = send->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSend_: check data failed, error_code=", error_code,
            ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    rai::AccountTokenInfo account_token_info;
    bool error = ledger_.AccountTokenInfoGet(
        transaction, block->Account(), send->token_.chain_,
        send->token_.address_, account_token_info);
    if (error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_BALANCE_IS_EMPTY);
    }
    if (account_token_info.balance_.IsZero())
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_BALANCE_IS_ZERO);
    }

    rai::TokenKey key(send->token_.chain_, send->token_.address_);
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessSend_: get token info failed, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    if (send->token_.type_ != info.type_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_TYPE_INVALID, keys);
    }

    if (!info.circulable_ && rai::IsRaicoin(key.chain_))
    {
        if (block->Account() != key.address_ && send->to_ != key.address_)
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_UNCIRCULABLE, keys);
        }
    }

    rai::TokenValue balance(account_token_info.balance_);
    if (info.type_ == rai::TokenType::_20)
    {
        if (send->value_ > account_token_info.balance_)
        {
            return UpdateLedgerCommon_(
                transaction, block,
                rai::ErrorCode::TOKEN_SEND_MORE_THAN_BALANCE, keys);
        }
        balance = account_token_info.balance_ - send->value_;

        rai::TokenReceivableKey receivable_key(
            send->to_, key, rai::CurrentChain(), block->Hash());
        rai::TokenReceivable receivable(
            block->Account(), send->value_, block->Height(),
            rai::TokenType::_20, rai::TokenSource::SEND);
        error_code =
            UpdateLedgerReceivable_(transaction, receivable_key, receivable);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else if (info.type_ == rai::TokenType::_721)
    {
        balance = account_token_info.balance_ - 1;

        rai::TokenValue token_id = send->value_;
        rai::AccountTokenId account_token_id(block->Account(), key, token_id);
        if (!ledger_.AccountTokenIdExist(transaction, account_token_id))
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_ID_NOT_OWNED, keys);
        }

        error = ledger_.AccountTokenIdDel(transaction, account_token_id);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessSend_: del account token id, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
        }

        error_code = UpdateLedgerTokenIdTransfer_(
            transaction, key, token_id, block->Account(), block->Height());
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::TokenReceivableKey receivable_key(
            send->to_, key, rai::CurrentChain(), block->Hash());
        rai::TokenReceivable receivable(block->Account(), send->value_,
                                        block->Height(), rai::TokenType::_721,
                                        rai::TokenSource::SEND);
        error_code =
            UpdateLedgerReceivable_(transaction, receivable_key, receivable);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessSend_: unexpected token type, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }
    
    error_code =
        UpdateLedgerTokenTransfer_(transaction, key, block->Timestamp(),
                                   block->Account(), block->Height());
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerBalance_(transaction, block->Account(), key,
                             account_token_info.balance_, balance);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessReceive_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    auto receive = std::static_pointer_cast<rai::ExtensionTokenReceive>(
        extension.op_data_);
    rai::ErrorCode error_code = receive->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessReceive_: check data failed, error_code=",
            error_code, ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    // wait source block
    rai::Chain chain = rai::Chain::INVALID;
    if (receive->source_ == rai::TokenSource::SEND
        || receive->source_ == rai::TokenSource::MINT
        || receive->source_ == rai::TokenSource::SWAP)
    {
        std::shared_ptr<rai::Block> source_block(nullptr);
        error_code =
            WaitBlock(transaction, receive->from_, receive->block_height_,
                      block, true, source_block);
        IF_NOT_SUCCESS_RETURN(error_code);
        chain = rai::CurrentChain();
    }
    else if (receive->source_ == rai::TokenSource::MAP)
    {
        // todo: cross chain
        return rai::ErrorCode::APP_PROCESS_WAITING;
    }
    else if (receive->source_ == rai::TokenSource::UNWRAP)
    {
        // todo: cross chain
        return rai::ErrorCode::APP_PROCESS_WAITING;
    }
    else
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessReceive_: unexpected token source, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    rai::TokenKey key(receive->token_.chain_, receive->token_.address_);
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);

    rai::TokenReceivableKey receivable_key(
        block->Account(), key, chain, receive->tx_hash_);
    rai::TokenReceivable receivable;
    bool error =
        ledger_.TokenReceivableGet(transaction, receivable_key, receivable);
    if (error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                    rai::ErrorCode::TOKEN_UNRECEIVABLE);
    }

    if (receivable.token_type_ != receive->token_.type_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                    rai::ErrorCode::TOKEN_TYPE_INVALID, keys);
    }
    if (receivable.from_ != receive->from_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                    rai::ErrorCode::TOKEN_RECEIVE_FROM, keys);
    }
    if (receivable.block_height_ != receive->block_height_)
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_RECEIVE_BLOCK_HEIGHT,
            keys);
    }
    if (receivable.value_ != receive->value_)
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_RECEIVE_VALUE, keys);
    }
    if (receivable.source_ != receive->source_)
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_RECEIVE_SOURCE, keys);
    }

    rai::TokenInfo info;
    error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessReceive_: get token info failed, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    rai::TokenValue prev_balance(0);
    rai::AccountTokenInfo account_token_info;
    error =
        ledger_.AccountTokenInfoGet(transaction, block->Account(), key.chain_,
                                    key.address_, account_token_info);
    if (!error)
    {
        prev_balance = account_token_info.balance_;
    }

    rai::TokenValue balance(0);
    if (receivable.token_type_ == rai::TokenType::_20)
    {
        balance = prev_balance + receivable.value_;
        if (balance < prev_balance)
        {
            return UpdateLedgerCommon_(transaction, block,
                                       rai::ErrorCode::TOKEN_BALANCE_OVERFLOW,
                                       keys);
        }
        // 
    }
    else if (receivable.token_type_ == rai::TokenType::_721)
    {

    }
    else
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessReceive_: unexpected token type, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    ledger_.TokenReceivableDel();

    // todo:

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerCommon_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    rai::ErrorCode status, const std::vector<rai::TokenKey>& tokens)
{
    rai::Account account = block->Account();
    uint64_t height = block->Height();
    rai::AccountTokensInfo info;
    bool error = ledger_.AccountTokensInfoGet(transaction, account, info);
    if (error)
    {
        info = rai::AccountTokensInfo();
    }
    else
    {
        if (height <= info.head_)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpateLedgerCommon_: unexpected block height, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }
    }

    rai::TokenBlock token_block(info.head_, block->Hash(), status);
    error = ledger_.TokenBlockPut(transaction, account, height, token_block);
    if (error)
    {
        rai::Log::Error(
            rai::ToString("Token::UpateLedgerCommon_: put token block, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    info.head_ = height;
    info.blocks_ += 1;
    info.last_active_ = block->Timestamp();
    error = ledger_.AccountTokensInfoPut(transaction, account, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpateLedgerCommon_: put account tokens info, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    for (const auto& token : tokens)
    {
        rai::AccountTokenInfo account_token_info;
        error = ledger_.AccountTokenInfoGet(transaction, account, token.chain_,
                                            token.address_, account_token_info);
        if (error)
        {
            account_token_info = rai::AccountTokenInfo();
        }
        else
        {
            if (height <= account_token_info.head_
                && account_token_info.head_ != rai::Block::INVALID_HEIGHT)
            {
                rai::Log::Error(rai::ToString(
                    "Token::UpateLedgerCommon_: unexpected block height, hash=",
                    block->Hash().StringHex()));
                return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
            }
        }

        rai::AccountTokenLink link(account, token.chain_, token.address_,
                                   height);
        error = ledger_.AccountTokenLinkPut(transaction, link,
                                            account_token_info.head_);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpateLedgerCommon_: put account token link, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }

        account_token_info.head_ = height;
        account_token_info.blocks_ += 1;
        error = ledger_.AccountTokenInfoPut(transaction, account, token.chain_,
                                            token.address_, account_token_info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpateLedgerCommon_: put account token info, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }
    }
}

rai::ErrorCode rai::Token::UpdateLedgerReceivable_(
    rai::Transaction& transaction, const rai::TokenReceivableKey& key,
    const rai::TokenReceivable& receivable)
{
    bool error = ledger_.TokenReceivablePut(transaction, key, receivable);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerReceivable_: to=", key.to_.StringAccount(),
            ", token.chain=", key.token_.chain_, ", token.address=",
            key.token_.address_.StringHex(), ", tx_hash=", key.tx_hash_));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    if (receivable_observer_)
    {
        receivable_observer_(key, receivable);
    }
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerBalance_(rai::Transaction& transaction,
                                                const rai::Account& account,
                                                const rai::TokenKey& token,
                                                const rai::TokenValue& previous,
                                                const rai::TokenValue& current)
{
    if (previous == current)
    {
        return rai::ErrorCode::SUCCESS;
    }

    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, token, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerBalance_: get token info failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    bool updated = false;
    if (previous == 0)
    {
        info.holders_ += 1;
        updated = true;
    }

    if (current == 0)
    {
        if (info.holders_ == 0)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerBalance_: token holders inconsistency, "
                "token.chain=",
                token.chain_, ", token.address=", token.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        info.holders_ -= 1;
        updated = true;
    }

    if (updated)
    {
        error = ledger_.TokenInfoPut(transaction, token, info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerBalance_: put token info failed, "
                "token.chain=",
                token.chain_, ", token.address=", token.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }
    }

    if (previous > 0)
    {
        if (!ledger_.TokenHolderExist(transaction, token, account, previous))
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerBalance_: token holder doesn't exist, "
                "token.chain=",
                token.chain_, ", token.address=", token.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        error = ledger_.TokenHolderDel(transaction, token, account, previous);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerBalance_: del token holder, account=",
                account.StringAccount(), ", token.chain=", token.chain_,
                ", token.address=", token.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
        }
    }

    if (current > 0)
    {
        error = ledger_.TokenHolderPut(transaction, token, account, current);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerBalance_: put token holder, account=",
                account.StringAccount(), ", token.chain=", token.chain_,
                ", token.address=", token.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
        }
    }

    rai::AccountTokenInfo account_token_info;
    error = ledger_.AccountTokenInfoGet(transaction, account, token.chain_,
                                        token.address_, account_token_info);
    if (error)
    {
        account_token_info = rai::AccountTokenInfo();
    }
    account_token_info.balance_ = current;

    error = ledger_.AccountTokenInfoPut(transaction, account, token.chain_,
                                        token.address_, account_token_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerBalance_: put account token info, account=",
            account.StringAccount()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerSupplies_(
    rai::Transaction& transaction, const rai::TokenKey& token,
    const rai::TokenValue& total_supply, const rai::TokenValue& local_supply)
{
    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, token, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerSupplies_: get token info failed, token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    info.total_supply_ = total_supply;
    info.local_supply_ = local_supply;
    error = ledger_.TokenInfoPut(transaction, token, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerSupplies_: put token info failed, token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerTokenTransfer_(
    rai::Transaction& transaction, const rai::TokenKey& token,
    uint64_t timestamp, const rai::Account& account, uint64_t height)
{
    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, token, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerTokenIdTransfer_: get token info failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    ++info.transfers_;
    error = ledger_.TokenInfoPut(transaction, token, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerTokenIdTransfer_: put token info failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    rai::TokenTransfer transfer(token, timestamp, account, height);
    error = ledger_.TokenTransferPut(transaction, transfer);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerTokenIdTransfer_: put token transfer failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex(),
            ", account=", account.StringAccount(), ", height=", height));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerTokenIdTransfer_(
    rai::Transaction& transaction, const rai::TokenKey& token,
    const rai::TokenValue& id, const rai::Account& account, uint64_t height)
{
    rai::TokenIdInfo info;
    bool error = ledger_.TokenIdInfoGet(transaction, token, id, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerTokenIdTransfer_: get token id info failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex(),
            ", id=", id.StringDec()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    uint64_t index = info.transfers_++;
    error = ledger_.TokenIdInfoPut(transaction, token, id, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerTokenIdTransfer_: put token id info failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex(),
            ", id=", id.StringDec()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    rai::TokenIdTransferKey key(token, id, index);
    error = ledger_.TokenIdTransferPut(transaction, key, account, height);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerTokenIdTransfer_: put token id transfer failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex(),
            ", id=", id.StringDec()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    return rai::ErrorCode::SUCCESS;
}
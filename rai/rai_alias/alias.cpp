#include <rai/rai_alias/alias.hpp>

#include <rai/common/extensions.hpp>

rai::Alias::Alias(rai::ErrorCode& error_code, boost::asio::io_service& service,
                  const boost::filesystem::path& data_path, rai::Alarm& alarm,
                  const rai::AliasConfig& config)
    : App(error_code, service, data_path / "alias_data.ldb", alarm, config.app_,
          subscribe_, rai::Alias::BlockTypes(), rai::Alias::Provide()),
      service_(service),
      alarm_(alarm),
      config_(config),
      subscribe_(*this)
{
    IF_NOT_SUCCESS_RETURN_VOID(error_code);
    error_code = InitLedger_();
}

rai::ErrorCode rai::Alias::PreBlockAppend(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    bool confirmed)
{
    if (confirmed || block->Extensions().empty())
    {
        return rai::ErrorCode::SUCCESS;
    }

    rai::Extensions extensions;
    rai::ErrorCode error_code = extensions.FromBytes(block->Extensions());
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return rai::ErrorCode::SUCCESS;
    }

    if (extensions.Count(rai::ExtensionType::ALIAS) == 0)
    {
        return rai::ErrorCode::SUCCESS;
    }

    return rai::ErrorCode::APP_PROCESS_CONFIRM_REQUIRED;
}

rai::ErrorCode rai::Alias::AfterBlockAppend(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    bool confirmed)
{
    if (block->Extensions().empty())
    {
        return rai::ErrorCode::SUCCESS;
    }

    rai::Extensions extensions;
    rai::ErrorCode error_code = extensions.FromBytes(block->Extensions());
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return rai::ErrorCode::SUCCESS;
    }

    if (extensions.Count(rai::ExtensionType::ALIAS) == 0)
    {
        return rai::ErrorCode::SUCCESS;
    }

    if (!confirmed)
    {
        rai::Log::Error(
            "Alias::AfterBlockAppend: unexpected unconfirmed block");
        return rai::ErrorCode::APP_PROCESS_CONFIRM_REQUIRED;
    }

    return Process(transaction, block, extensions);
}

rai::ErrorCode rai::Alias::PreBlockRollback(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block)
{
    rai::AliasInfo info;
    bool error = ledger_.AliasInfoGet(transaction, block->Account(), info);
    IF_ERROR_RETURN(error, rai::ErrorCode::SUCCESS);

    if (info.head_ != rai::Block::INVALID_HEIGHT && info.head_ >= block->Height())
    {
        std::string error_info = rai::ToString(
            "Alias::PreBlockRollback: rollback confirmed block, hash=",
            block->Hash().StringHex());
        rai::Log::Error(error_info);
        std::cout << "FATAL ERROR:" << error_info << std::endl;
        return rai::ErrorCode::APP_PROCESS_HALT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Alias::AfterBlockRollback(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block)
{
    return rai::ErrorCode::SUCCESS;
}

std::shared_ptr<rai::AppRpcHandler> rai::Alias::MakeRpcHandler(
    const rai::UniqueId& uid, bool check, const std::string& body,
    const std::function<void(const rai::Ptree&)>& send_response)
{
    if (!rpc_)
    {
        return nullptr;
    }
    return std::make_shared<rai::AliasRpcHandler>(
        *this, uid, check, *rpc_, body, boost::asio::ip::address_v4::any(),
        send_response);
}

std::shared_ptr<rai::Alias> rai::Alias::Shared()
{
    return Shared_<rai::Alias>();
}

rai::RpcHandlerMaker rai::Alias::RpcHandlerMaker()
{
    return [this](rai::Rpc& rpc, const std::string& body,
                  const boost::asio::ip::address_v4& ip,
                  const std::function<void(const rai::Ptree&)>& send_response)
               -> std::unique_ptr<rai::RpcHandler> {
        return std::make_unique<rai::AliasRpcHandler>(
            *this, rai::UniqueId(), false, rpc, body, ip, send_response);
    };
}

void rai::Alias::Start()
{
    std::weak_ptr<rai::Alias> alias(Shared());
    alias_observer_ = [alias](const rai::Account& account,
                              const std::string& name, const std::string& dns) {
        auto alias_s = alias.lock();
        if (alias_s == nullptr) return;

        alias_s->Background([alias, account, name, dns]() {
            if (auto alias_s = alias.lock())
            {
                alias_s->observers_.alias_.Notify(account, name, dns);
            }
        });
    };

    App::Start();
}

void rai::Alias::Stop()
{
    if (rpc_)
    {
        rpc_->Stop();
    }

    if (runner_)
    {
        runner_->Stop();
    }

    App::Stop();
}

rai::ErrorCode rai::Alias::Process(rai::Transaction& transaction,
                         const std::shared_ptr<rai::Block>& block,
                         const rai::Extensions& extensions)
{
    rai::Account account = block->Account();
    uint64_t height = block->Height();
    rai::AliasInfo info;
    bool error = ledger_.AliasInfoGet(transaction, account, info);
    if (error)
    {
        info = rai::AliasInfo();
    }

    if (!error && info.head_ >= block->Height())
    {
        rai::Log::Error(rai::ToString("Alias::Process: block re-entry, hash=",
                                      block->Hash().StringHex()));
        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::ExtensionAlias::Op op = rai::ExtensionAlias::Op::INVALID;
    std::vector<uint8_t> value;
    do
    {
        if (extensions.Count(rai::ExtensionType::ALIAS) != 1)
        {
            error_code = rai::ErrorCode::ALIAS_MULTI_EXTENSIONS;
            break;
        }

        rai::ExtensionAlias alias;
        error_code =
            alias.FromExtension(extensions.Get(rai::ExtensionType::ALIAS));
        IF_NOT_SUCCESS_BREAK(error_code);

        error_code = alias.CheckData();
        IF_NOT_SUCCESS_BREAK(error_code);

        op = alias.op_;
        value = alias.op_value_;
    } while (0);

    using Op = rai::ExtensionAlias::Op;
    if (rai::ErrorCode::SUCCESS == error_code && op == Op::NAME)
    {
        if (!info.name_.empty())
        {
            rai::AliasIndex index{rai::Prefix(info.name_), account};
            error = ledger_.AliasIndexDel(transaction, index);
            if (error)
            {
                rai::Log::Error(
                    rai::ToString("Alias::Process: alias index del, account=",
                                  account.StringAccount(),
                                  ", name=", rai::BytesToString(info.name_)));
                return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
            }
        }

        if (!value.empty())
        {
            rai::AliasIndex index{rai::Prefix(value), account};
            error = ledger_.AliasIndexPut(transaction, index);
            if (error)
            {
                rai::Log::Error(
                    rai::ToString("Alias::Process: alias index put, account=",
                                  account.StringAccount(),
                                  ", name=", rai::BytesToString(value)));
                return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
            }
        }
    }

    if (rai::ErrorCode::SUCCESS == error_code && op == Op::DNS)
    {
        if (!info.dns_.empty())
        {
            rai::AliasIndex index{rai::Prefix(info.dns_), account};
            error = ledger_.AliasDnsIndexDel(transaction, index);
            if (error)
            {
                rai::Log::Error(rai::ToString(
                    "Alias::Process: alias dns index del, account=",
                    account.StringAccount(),
                    ", dns=", rai::BytesToString(info.dns_)));
                return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
            }
        }

        if (!value.empty())
        {
            rai::AliasIndex index{rai::Prefix(value), account};
            error = ledger_.AliasDnsIndexPut(transaction, index);
            if (error)
            {
                rai::Log::Error(rai::ToString(
                    "Alias::Process: alias dns index put, account=",
                    account.StringAccount(),
                    ", dns=", rai::BytesToString(value)));
                return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
            }
        }
    }

    rai::AliasBlock alias_block(info.head_, block->Hash(),
                                static_cast<int32_t>(error_code),
                                static_cast<uint8_t>(op), value);
    error = ledger_.AliasBlockPut(transaction, account, height, alias_block);
    if (error)
    {
        rai::Log::Error(
            rai::ToString("Alias::Process: alias block put, hash=",
                            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    info.head_ = height;
    if (rai::ErrorCode::SUCCESS == error_code)
    {
        if (op == Op::NAME)
        {
            info.name_ = value;
            info.name_valid_ = height;
        }
        else if (op == Op::DNS)
        {
            info.dns_ = value;
            info.dns_valid_ = height;
        }
    }
    error = ledger_.AliasInfoPut(transaction, account, info);
    if (error)
    {
        rai::Log::Error(
            rai::ToString("Alias::Process: alias info put, hash=",
                            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    if (alias_observer_)
    {
        alias_observer_(account, rai::BytesToString(info.name_),
                        rai::BytesToString(info.dns_));
    }

    return rai::ErrorCode::SUCCESS;
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

    info.actions_.push_back(P::Action::APP_SERVICE_SUBSCRIBE);
    info.actions_.push_back(P::Action::APP_ACCOUNT_SYNC);
    info.actions_.push_back(P::Action::ALIAS_QUERY);
    info.actions_.push_back(P::Action::ALIAS_SEARCH);

    return info;
}

rai::ErrorCode rai::Alias::InitLedger_()
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, true);
    IF_NOT_SUCCESS_RETURN(error_code);

    uint32_t version = 0;
    bool error = ledger_.VersionGet(transaction, version);
    if (error)
    {
        error =
            ledger_.VersionPut(transaction, rai::Alias::CURRENT_LEDGER_VERSION);
        IF_ERROR_RETURN(error, rai::ErrorCode::ALIAS_LEDGER_PUT);
        return rai::ErrorCode::SUCCESS;
    }

    if (version > rai::Alias::CURRENT_LEDGER_VERSION)
    {
        std::cout << "[Error] Ledger version=" << version << std::endl;
        return rai::ErrorCode::LEDGER_VERSION;
    }

    return rai::ErrorCode::SUCCESS;
}

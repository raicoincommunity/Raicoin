#include <rai/rai_token/subscribe.hpp>

#include <rai/rai_token/token.hpp>

rai::TokenSubscriptions::TokenSubscriptions(rai::Token& token)
    : AppSubscriptions(token), token_(token)
{
    token_.observers_.receivable_.Add(
        [this](const rai::TokenReceivableKey& key,
               const rai::TokenReceivable& receivable,
               const rai::TokenInfo& info,
               const std::shared_ptr<rai::Block>& block) {
            using P = rai::Provider;
            rai::Ptree ptree;
            P::PutAction(ptree, P::Action::TOKEN_RECEIVABLE);
            P::PutId(ptree, token_.provider_info_.id_);
            P::AppendFilter(ptree, P::Filter::APP_ACCOUNT,
                            key.to_.StringAccount());
            token_.MakeReceivablePtree(key, receivable, info, block, ptree);
            Notify(key.to_, ptree);
        });

    token_.observers_.received_.Add(
        [this](const rai::TokenReceivableKey& key) { NotifyReceived(key); });

    token_.observers_.token_.Add(
        [this](const rai::TokenKey& key, const rai::TokenInfo& info) {
            NotifyTokenInfo(key, info);
        });

    token_.observers_.token_id_creation_.Add(
        [this](const rai::TokenKey& key, const rai::TokenValue& id,
               const rai::TokenIdInfo& info) {
            NotifyTokenIdInfo(key, id, info);
        });

    token_.observers_.token_id_transfer_.Add(
        [this](const rai::TokenKey& key, const rai::TokenValue& id,
               const rai::TokenIdInfo& info, const rai::Account& account,
               bool receive) {
            NotifyTokenIdTransfer(key, id, info, account, receive);
        });

    token_.observers_.account_.Add(
        [this](
            const rai::Account& account, const rai::AccountTokensInfo& info,
            const std::vector<std::pair<rai::TokenKey, rai::AccountTokenInfo>>&
                pairs) { NotifyAccountTokensInfo(account, info, pairs); });

    token_.observers_.swap_.Add(
        [this](const rai::Account& account, uint64_t height) {
            NotifySwapInfo(account, height);
        });

    token_.observers_.main_account_.Add(
        [this](const rai::Account& account, const rai::Account& main_account) {
            NotifySwapMainAccount(account, main_account);
        });

    token_.observers_.order_.Add(
        [this](const rai::Account& account, uint64_t height) {
            NotifyOrderInfo(account, height);
        });

    token_.observers_.account_swap_info_.Add(
        [this](const rai::Account& account) {
            NotifyAccountSwapInfo(account);
        });

    // todo:
}

void rai::TokenSubscriptions::NotifyTokenInfo(const rai::TokenKey& key,
                                              const rai::TokenInfo& info)
{
    if (!rai::IsRaicoin(key.chain_)) return;
    if (!Subscribed(key.address_)) return;

    using P = rai::Provider;
    rai::Ptree ptree;
    P::PutAction(ptree, P::Action::TOKEN_INFO);
    P::PutId(ptree, token_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_ACCOUNT,
                    key.address_.StringAccount());
    token_.TokenKeyToPtree(key, ptree);
    token_.TokenInfoToPtree(info, ptree);
    Notify(key.address_, ptree);
}

void rai::TokenSubscriptions::NotifyReceived(const rai::TokenReceivableKey& key)
{
    if (!Subscribed(key.to_)) return;

    using P = rai::Provider;
    rai::Ptree ptree;
    P::PutAction(ptree, P::Action::TOKEN_RECEIVED);
    P::PutId(ptree, token_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_ACCOUNT, key.to_.StringAccount());
    ptree.put("to", key.to_.StringAccount());
    rai::Ptree token;
    token_.TokenKeyToPtree(key.token_, token);
    ptree.put_child("token", token);
    ptree.put("chain", rai::ChainToString(key.chain_));
    ptree.put("tx_hash", key.tx_hash_.StringHex());
    Notify(key.to_, ptree);
}

void rai::TokenSubscriptions::NotifyTokenIdInfo(const rai::TokenKey& key,
                                                const rai::TokenValue& id,
                                                const rai::TokenIdInfo& info)
{
    if (!rai::IsRaicoin(key.chain_)) return;
    if (!Subscribed(key.address_)) return;

    using P = rai::Provider;
    rai::Ptree ptree;
    P::PutAction(ptree, P::Action::TOKEN_ID_INFO);
    P::PutId(ptree, token_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_ACCOUNT,
                    key.address_.StringAccount());
    token_.TokenKeyToPtree(key, ptree);
    ptree.put("token_id", id.StringDec());
    token_.TokenIdInfoToPtree(info, ptree);
    Notify(key.address_, ptree);
}

void rai::TokenSubscriptions::NotifyTokenIdTransfer(
    const rai::TokenKey& key, const rai::TokenValue& id,
    const rai::TokenIdInfo& info, const rai::Account& account, bool receive)
{
    if (!Subscribed(account)) return;

    using P = rai::Provider;
    rai::Ptree ptree;
    P::PutAction(ptree, P::Action::TOKEN_ID_TRANSFER);
    P::PutId(ptree, token_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_ACCOUNT,
                    key.address_.StringAccount());
    ptree.put("token_id", id.StringDec());
    ptree.put("account", account.StringAccount());
    token_.TokenKeyToPtree(key, ptree);
    ptree.put("receive", rai::BoolToString(receive));
    token_.TokenIdInfoToPtree(info, ptree);
    Notify(account, ptree);
}

void rai::TokenSubscriptions::NotifyAccountTokensInfo(
    const rai::Account& account, const rai::AccountTokensInfo& info,
    const std::vector<std::pair<rai::TokenKey, rai::AccountTokenInfo>>& pairs)
{
    if (!Subscribed(account)) return;

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    using P = rai::Provider;
    rai::Ptree ptree;
    P::PutAction(ptree, P::Action::TOKEN_ACCOUNT_TOKENS_INFO);
    P::PutId(ptree, token_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_ACCOUNT, account.StringAccount());
    ptree.put("account", account.StringAccount());
    ptree.put("head_height", std::to_string(info.head_));
    ptree.put("token_block_count", std::to_string(info.blocks_));

    rai::Ptree tokens;
    size_t count = 0;
    for (const auto& i : pairs)
    {
        rai::TokenInfo token_info;
        bool error =
            token_.ledger_.TokenInfoGet(transaction, i.first, token_info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "TokenSubscriptions::NotifyAccountTokensInfo: Get token info "
                "failed, token.chain=",
                rai::ChainToString(i.first.chain_), ", token.address=",
                rai::TokenAddressToString(i.first.chain_, i.first.address_)));
            return;
        }

        rai::Ptree entry;
        token_.MakeAccountTokenInfoPtree(i.first, token_info, i.second, entry);
        count++;
        tokens.push_back(std::make_pair("", entry));
    }
    ptree.put_child("tokens", tokens);
    ptree.put("token_count", std::to_string(count));
    Notify(account, ptree);
}

void rai::TokenSubscriptions::NotifySwapInfo(const rai::Account& account,
                                             uint64_t height)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    rai::SwapInfo swap_info;
    bool error =
        token_.ledger_.SwapInfoGet(transaction, account, height, swap_info);
    IF_ERROR_RETURN_VOID(error);
    rai::Account maker(swap_info.maker_);
    if (!Subscribed(account) && !Subscribed(maker))
    {
        return;
    }

    rai::Ptree ptree;
    std::string error_info;
    error =
        token_.MakeSwapPtree(transaction, account, height, error_info, ptree);
    IF_ERROR_RETURN_VOID(error);

    using P = rai::Provider;
    P::PutAction(ptree, P::Action::TOKEN_SWAP_INFO);
    P::PutId(ptree, token_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_ACCOUNT, account.StringAccount());
    P::AppendFilter(ptree, P::Filter::APP_ACCOUNT, maker.StringAccount());

    Notify({account, maker}, ptree);
}

void rai::TokenSubscriptions::NotifySwapMainAccount(
    const rai::Account& account, const rai::Account& main_account)
{
    if (!Subscribed(account)) return;

    using P = rai::Provider;
    rai::Ptree ptree;
    P::PutAction(ptree, P::Action::TOKEN_SWAP_MAIN_ACCOUNT);
    P::PutId(ptree, token_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_ACCOUNT, account.StringAccount());
    ptree.put("account", account.StringAccount());
    ptree.put("main_account", main_account.StringAccount());
    Notify(account, ptree);
}

void rai::TokenSubscriptions::NotifyOrderInfo(const rai::Account& account,
                                              uint64_t height)
{
    if (!Subscribed(account)) return;

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    rai::Ptree ptree;
    std::string error_info;
    bool error =
        token_.MakeOrderPtree(transaction, account, height, error_info, ptree);
    IF_ERROR_RETURN_VOID(error);

    using P = rai::Provider;
    P::PutAction(ptree, P::Action::TOKEN_ORDER_INFO);
    P::PutId(ptree, token_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_ACCOUNT, account.StringAccount());

    Notify(account, ptree);
}

void rai::TokenSubscriptions::NotifyAccountSwapInfo(const rai::Account& account)
{
    if (!Subscribed(account)) return;

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    rai::Ptree ptree;
    std::string error_info;
    bool error = token_.MakeAccountSwapInfoPtree(transaction, account,
                                                 error_info, ptree);
    IF_ERROR_RETURN_VOID(error);

    using P = rai::Provider;
    P::PutAction(ptree, P::Action::TOKEN_ACCOUNT_SWAP_INFO);
    P::PutId(ptree, token_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_ACCOUNT, account.StringAccount());

    Notify(account, ptree);
}

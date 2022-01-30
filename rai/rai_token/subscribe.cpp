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

    token_.observers_.token_creation_.Add(
        [this](const rai::TokenKey& key, const rai::TokenInfo& info) {
            NotifyTokenInfo(key, info);
        });

    token_.observers_.token_total_supply_.Add(
        [this](const rai::TokenKey& key, const rai::TokenInfo& info) {
            NotifyTokenInfo(key, info);
        });
    // todo:
}

void rai::TokenSubscriptions::NotifyTokenInfo(const rai::TokenKey& key,
                                              const rai::TokenInfo& info)
{
    if (!rai::IsRaicoin(key.chain_)) return;
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

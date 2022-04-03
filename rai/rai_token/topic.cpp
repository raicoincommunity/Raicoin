#include <rai/rai_token/topic.hpp>

#include <rai/rai_token/token.hpp>

rai::TokenOrderTopic::TokenOrderTopic(const rai::OrderInfo& order)
    : token_offer_(order.token_offer_),
      type_offer_(order.type_offer_),
      token_want_(order.token_want_),
      type_want_(order.type_want_)
{
}

void rai::TokenOrderTopic::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, token_offer_.chain_);
    rai::Write(stream, token_offer_.address_.bytes);
    rai::Write(stream, type_offer_);
    rai::Write(stream, token_want_.chain_);
    rai::Write(stream, token_want_.address_.bytes);
    rai::Write(stream, type_want_);
}

rai::AccountTokenBalanceTopic::AccountTokenBalanceTopic(
    const rai::Account& account, const rai::TokenKey& token,
    rai::TokenType type, const boost::optional<rai::TokenValue>& id_o)
    : account_(account), token_(token), type_(type), id_o_(id_o)
{
}

void rai::AccountTokenBalanceTopic::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, account_.bytes);
    rai::Write(stream, token_.chain_);
    rai::Write(stream, token_.address_.bytes);
    rai::Write(stream, type_);
    if (id_o_)
    {
        rai::Write(stream, id_o_->bytes);
    }
}

rai::TokenTopics::TokenTopics(rai::Token& token) : token_(token)
{
    token_.observers_.account_swap_info_.Add(
        [this](const rai::Account& account) {
            NotifyAccountSwapInfo(account);
        });

    token_.observers_.order_.Add(
        [this](const rai::Account& account, uint64_t height) {
            NotifyOrderInfo(account, height);
        });

    token_.observers_.account_token_balance_.Add(
        [this](const rai::Account& account, const rai::TokenKey& token,
               rai::TokenType type,
               const boost::optional<rai::TokenValue>& id_o) {
            NotifyAccountTokenBalance(account, token, type, id_o);
        });
}

void rai::TokenTopics::NotifyAccountSwapInfo(const rai::Account& account)
{
    rai::Topic topic = rai::AppTopics::CalcTopic(
        rai::AppTopicType::ACCOUNT_SWAP_INFO, account);
    if (!token_.topics_.Subscribed(topic))
    {
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    std::string error_info;
    rai::Ptree ptree;
    bool error = token_.MakeAccountSwapInfoPtree(transaction, account,
                                                 error_info, ptree);
    IF_ERROR_RETURN_VOID(error);

    using P = rai::Provider;
    P::PutAction(ptree, P::Action::TOKEN_ACCOUNT_SWAP_INFO);
    P::PutId(ptree, token_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_TOPIC, topic.StringHex());
    
    token_.topics_.Notify(topic, ptree);
}

void rai::TokenTopics::NotifyOrderInfo(const rai::Account& account,
                                       uint64_t height)
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    rai::OrderInfo order_info;
    bool error =
        token_.ledger_.OrderInfoGet(transaction, account, height, order_info);
    IF_ERROR_RETURN_VOID(error);

    rai::TokenOrderTopic order_topic(order_info);
    rai::Topic pair_topic =
        rai::AppTopics::CalcTopic(rai::AppTopicType::ORDER_PAIR, order_topic);
    rai::Topic id_topic = rai::AppTopics::CalcTopic(rai::AppTopicType::ORDER_ID,
                                                    order_info.hash_);

    if (!token_.topics_.Subscribed(pair_topic)
        && !token_.topics_.Subscribed(id_topic))
    {
        return;
    }

    rai::Ptree ptree;
    std::string error_info;
    error =
        token_.MakeOrderPtree(transaction, account, height, error_info, ptree);
    IF_ERROR_RETURN_VOID(error);

    using P = rai::Provider;
    P::PutAction(ptree, P::Action::TOKEN_ORDER_INFO);
    P::PutId(ptree, token_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_TOPIC, pair_topic.StringHex());
    P::AppendFilter(ptree, P::Filter::APP_TOPIC, id_topic.StringHex());

    token_.topics_.Notify({pair_topic, id_topic}, ptree);
}

void rai::TokenTopics::NotifyAccountTokenBalance(
    const rai::Account& account, const rai::TokenKey& token,
    rai::TokenType type, const boost::optional<rai::TokenValue>& id_o)
{
    rai::AccountTokenBalanceTopic balance_topic(account, token, type, id_o);
    rai::Topic topic = rai::AppTopics::CalcTopic(
        rai::AppTopicType::ACCOUNT_TOKEN_BALANCE, balance_topic);
    if (!token_.topics_.Subscribed(topic))
    {
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    std::string error_info;
    rai::Ptree ptree;
    bool error = token_.MakeAccountTokenBalancePtree(
        transaction, account, token, type, id_o, error_info, ptree);
    IF_ERROR_RETURN_VOID(error);

    using P = rai::Provider;
    P::PutAction(ptree, P::Action::TOKEN_ACCOUNT_BALANCE);
    P::PutId(ptree, token_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_TOPIC, topic.StringHex());
    
    token_.topics_.Notify(topic, ptree);
}

#pragma once
#include <rai/common/numbers.hpp>
#include <rai/common/util.hpp>
#include <rai/secure/ledger.hpp>

namespace rai
{

class Token;

class TokenOrderTopic : public rai::Serializer
{
public:
    TokenOrderTopic(const rai::OrderInfo&);

    void Serialize(rai::Stream&) const override;
    rai::TokenKey token_offer_;
    rai::TokenType type_offer_;
    rai::TokenKey token_want_;
    rai::TokenType type_want_;
};

class AccountTokenBalanceTopic : public rai::Serializer
{
public:
    AccountTokenBalanceTopic(const rai::Account&, const rai::TokenKey&,
                             rai::TokenType,
                             const boost::optional<rai::TokenValue>&);
    void Serialize(rai::Stream&) const override;
    rai::Account account_;
    rai::TokenKey token_;
    rai::TokenType type_;
    boost::optional<rai::TokenValue> id_o_;
};

class TokenTopics
{
public:
    TokenTopics(rai::Token&);

    void NotifyAccountSwapInfo(const rai::Account&);
    void NotifyOrderInfo(const rai::Account&, uint64_t);
    void NotifyAccountTokenBalance(const rai::Account&, const rai::TokenKey&,
                                   rai::TokenType,
                                   const boost::optional<rai::TokenValue>&);

    rai::Token& token_;
};

}
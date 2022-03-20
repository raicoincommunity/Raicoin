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

class TokenTopics
{
public:
    TokenTopics(rai::Token&);

    void NotifyAccountSwapInfo(const rai::Account&);
    void NotifyOrderInfo(const rai::Account&, uint64_t);

    rai::Token& token_;
};

}
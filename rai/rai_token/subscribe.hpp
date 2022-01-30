#pragma once

#include <rai/app/subscribe.hpp>

namespace rai
{

class Token;

class TokenSubscriptions : public rai::AppSubscriptions
{
public:
    TokenSubscriptions(rai::Token&);

    void NotifyTokenInfo(const rai::TokenKey&, const rai::TokenInfo&);

    rai::Token& token_;
};

}
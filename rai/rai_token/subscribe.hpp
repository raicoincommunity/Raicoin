#pragma once

#include <rai/app/subscribe.hpp>

namespace rai
{

class Token;

class TokenSubscriptions : public rai::AppSubscriptions
{
public:
    TokenSubscriptions(rai::Token&);

    rai::Token& token_;
};

}
#include <rai/rai_token/subscribe.hpp>

#include <rai/rai_token/token.hpp>

rai::TokenSubscriptions::TokenSubscriptions(rai::Token& token)
    : AppSubscriptions(token), token_(token)
{
}
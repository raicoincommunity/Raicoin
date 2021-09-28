#include <rai/rai_alias/subscribe.hpp>

#include <rai/rai_alias/alias.hpp>

rai::AliasSubscriptions::AliasSubscriptions(rai::Alias& alias)
    : AppSubscriptions(alias), alias_(alias)
{
}
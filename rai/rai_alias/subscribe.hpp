#pragma once

#include <rai/app/subscribe.hpp>

namespace rai
{

class Alias;

class AliasSubscriptions : public rai::AppSubscriptions
{
public:
    AliasSubscriptions(rai::Alias&);

    rai::Alias& alias_;
};

}
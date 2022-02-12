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
    void NotifyReceived(const rai::TokenReceivableKey&);
    void NotifyTokenIdInfo(const rai::TokenKey&, const rai::TokenValue&,
                           const rai::TokenIdInfo&);
    void NotifyAccountTokensInfo(
        const rai::Account&, const rai::AccountTokensInfo&,
        const std::vector<std::pair<rai::TokenKey, rai::AccountTokenInfo>>&);

    rai::Token& token_;
};

}
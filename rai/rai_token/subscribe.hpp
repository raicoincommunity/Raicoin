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
    void NotifyTokenIdTransfer(const rai::TokenKey&, const rai::TokenValue&,
                               const rai::TokenIdInfo&, const rai::Account&,
                               bool);
    void NotifyAccountTokensInfo(
        const rai::Account&, const rai::AccountTokensInfo&,
        const std::vector<std::pair<rai::TokenKey, rai::AccountTokenInfo>>&);
    void NotifySwapInfo(const rai::Account&, uint64_t);
    void NotifySwapMainAccount(const rai::Account&, const rai::Account&);
    void NotifyOrderInfo(const rai::Account&, uint64_t);
    void NotifyAccountSwapInfo(const rai::Account&);
    void NotifyTakeNackBlockSubmitted(const rai::Account&, uint64_t,
                                      const std::shared_ptr<rai::Block>&);
    void NotifyPendingTokenMapInfo(const std::shared_ptr<rai::CrossChainEvent>&,
                                   bool);
    void NotifyTokenMapInfo(const rai::Account&, rai::Chain, uint64_t,
                            uint64_t);
    void NotifyTokenUnmapInfo(const rai::Account&, uint64_t);
    void NotifyTokenWrapInfo(const rai::Account&, uint64_t);

    void ProcessCrossChainEvent(const std::shared_ptr<rai::CrossChainEvent>&,
                                bool);

    rai::Token& token_;
};

}
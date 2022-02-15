#pragma once
#include <boost/optional.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/extensions.hpp>
#include <rai/app/app.hpp>
#include <rai/app/provider.hpp>
#include <rai/rai_token/config.hpp>
#include <rai/rai_token/subscribe.hpp>
#include <rai/rai_token/rpc.hpp>

namespace rai
{

class TokenObservers
{
public:
    rai::ObserverContainer<const rai::TokenReceivableKey&,
                           const rai::TokenReceivable&, const rai::TokenInfo&,
                           const std::shared_ptr<rai::Block>&>
        receivable_;
    rai::ObserverContainer<const rai::TokenReceivableKey&>
        received_;
    rai::ObserverContainer<const rai::TokenKey&, const rai::TokenInfo&>
        token_;
    rai::ObserverContainer<const rai::TokenKey&, const rai::TokenValue&,
                           const rai::TokenIdInfo&>
        token_id_creation_;
    rai::ObserverContainer<const rai::TokenKey&, const rai::TokenValue&,
                           const rai::TokenIdInfo&, const rai::Account&, bool>
        token_id_transfer_;
    rai::ObserverContainer<
        const rai::Account&, const rai::AccountTokensInfo&,
        const std::vector<std::pair<rai::TokenKey, rai::AccountTokenInfo>>&>
        account_;
};

class TokenError
{
public:
    TokenError();
    TokenError(rai::ErrorCode);
    TokenError(rai::ErrorCode, const rai::TokenKey&);

    rai::ErrorCode return_code_;
    rai::TokenKey token_;

};

class Token : public rai::App
{
public:
    Token(rai::ErrorCode&, boost::asio::io_service&,
          const boost::filesystem::path&, rai::Alarm&, const rai::TokenConfig&);
    virtual ~Token() = default;

    rai::ErrorCode PreBlockAppend(rai::Transaction&,
                                  const std::shared_ptr<rai::Block>&,
                                  bool) override;
    rai::ErrorCode AfterBlockAppend(rai::Transaction&,
                                    const std::shared_ptr<rai::Block>&,
                                    bool) override;
    rai::ErrorCode PreBlockRollback(
        rai::Transaction&, const std::shared_ptr<rai::Block>&) override;
    rai::ErrorCode AfterBlockRollback(
        rai::Transaction&, const std::shared_ptr<rai::Block>&) override;
    rai::ErrorCode Process(rai::Transaction&,
                           const std::shared_ptr<rai::Block>&,
                           const rai::Extensions&);
    std::shared_ptr<rai::AppRpcHandler> MakeRpcHandler(
        const rai::UniqueId&, bool, const std::string&,
        const std::function<void(const rai::Ptree&)>&) override;

    std::shared_ptr<rai::Token> Shared();
    rai::RpcHandlerMaker RpcHandlerMaker();

    void Start() override;
    void Stop() override;

    void TokenKeyToPtree(const rai::TokenKey&, rai::Ptree&) const;
    void TokenInfoToPtree(const rai::TokenInfo&, rai::Ptree&) const;
    void TokenIdInfoToPtree(const rai::TokenIdInfo&, rai::Ptree&) const;
    void MakeReceivablePtree(const rai::TokenReceivableKey&,
                             const rai::TokenReceivable&, const rai::TokenInfo&,
                             const std::shared_ptr<rai::Block>&,
                             rai::Ptree&) const;
    void MakeAccountTokenInfoPtree(const rai::TokenKey&, const rai::TokenInfo&,
                                   const rai::AccountTokenInfo&, rai::Ptree&);

    static std::vector<rai::BlockType> BlockTypes();
    static rai::Provider::Info Provide();

    boost::asio::io_service& service_;
    rai::Alarm& alarm_;
    const rai::TokenConfig& config_;
    rai::TokenObservers observers_;
    std::shared_ptr<rai::Rpc> rpc_;
    std::shared_ptr<rai::OngoingServiceRunner> runner_;
    rai::TokenSubscriptions subscribe_;

private:
    static uint32_t constexpr CURRENT_LEDGER_VERSION = 1;
    rai::ErrorCode InitLedger_();
    rai::TokenError ProcessCreate_(rai::Transaction&,
                                   const std::shared_ptr<rai::Block>&,
                                   const rai::ExtensionToken&);
    rai::TokenError ProcessMint_(rai::Transaction&,
                                 const std::shared_ptr<rai::Block>&,
                                 const rai::ExtensionToken&);
    rai::TokenError ProcessBurn_(rai::Transaction&,
                                 const std::shared_ptr<rai::Block>&,
                                 const rai::ExtensionToken&);
    rai::TokenError ProcessSend_(rai::Transaction&,
                                 const std::shared_ptr<rai::Block>&,
                                 const rai::ExtensionToken&);
    rai::TokenError ProcessReceive_(rai::Transaction&,
                                    const std::shared_ptr<rai::Block>&,
                                    const rai::ExtensionToken&);
    rai::TokenError ProcessSwap_(rai::Transaction&,
                                 const std::shared_ptr<rai::Block>&,
                                 const rai::ExtensionToken&);
    rai::TokenError ProcessSwapConfig_(rai::Transaction&,
                                       const std::shared_ptr<rai::Block>&,
                                       const rai::ExtensionTokenSwap&);
    rai::TokenError ProcessSwapMake_(rai::Transaction&,
                                     const std::shared_ptr<rai::Block>&,
                                     const rai::ExtensionTokenSwap&);
    rai::TokenError ProcessSwapInquiry_(rai::Transaction&,
                                        const std::shared_ptr<rai::Block>&,
                                        const rai::ExtensionTokenSwap&);
    rai::TokenError ProcessSwapInquiryAck_(rai::Transaction&,
                                           const std::shared_ptr<rai::Block>&,
                                           const rai::ExtensionTokenSwap&);
    rai::TokenError ProcessSwapTake_(rai::Transaction&,
                                     const std::shared_ptr<rai::Block>&,
                                     const rai::ExtensionTokenSwap&);
    rai::TokenError ProcessSwapTakeAck_(rai::Transaction&,
                                        const std::shared_ptr<rai::Block>&,
                                        const rai::ExtensionTokenSwap&);
    rai::TokenError ProcessSwapTakeNack_(rai::Transaction&,
                                         const std::shared_ptr<rai::Block>&,
                                         const rai::ExtensionTokenSwap&);
    rai::TokenError ProcessSwapCancel_(rai::Transaction&,
                                       const std::shared_ptr<rai::Block>&,
                                       const rai::ExtensionTokenSwap&);
    rai::TokenError ProcessSwapPing_(rai::Transaction&,
                                     const std::shared_ptr<rai::Block>&,
                                     const rai::ExtensionTokenSwap&);
    rai::TokenError ProcessSwapPong_(rai::Transaction&,
                                     const std::shared_ptr<rai::Block>&,
                                     const rai::ExtensionTokenSwap&);
    rai::TokenError ProcessUnmap_(rai::Transaction&,
                                  const std::shared_ptr<rai::Block>&,
                                  const rai::ExtensionToken&);
    rai::TokenError ProcessWrap_(rai::Transaction&,
                                 const std::shared_ptr<rai::Block>&,
                                 const rai::ExtensionToken&);
    rai::ErrorCode ProcessError_(rai::Transaction&, const rai::TokenError&);
    rai::ErrorCode PurgeInquiryWaiting_(rai::Transaction&, const rai::Account&,
                                        uint64_t);
    rai::ErrorCode PurgeTakeWaiting_(rai::Transaction&,
                                     const std::shared_ptr<rai::Block>&);
    rai::ErrorCode UpdateLedgerCommon_(
        rai::Transaction&, const std::shared_ptr<rai::Block>&, rai::ErrorCode,
        const std::vector<rai::TokenKey>& = std::vector<rai::TokenKey>());
    rai::ErrorCode UpdateLedgerCommon_(
        rai::Transaction&, const std::shared_ptr<rai::Block>&, rai::ErrorCode,
        const rai::TokenValue&, rai::TokenBlock::ValueOp,
        const std::vector<rai::TokenKey>& = std::vector<rai::TokenKey>());
    rai::ErrorCode UpdateLedgerReceivable_(rai::Transaction&,
                                           const rai::TokenReceivableKey&,
                                           const rai::TokenReceivable&,
                                           const rai::TokenInfo&,
                                           const std::shared_ptr<rai::Block>&);
    rai::ErrorCode UpdateLedgerBalance_(rai::Transaction&, const rai::Account&,
                                        const rai::TokenKey&,
                                        const rai::TokenValue&,
                                        const rai::TokenValue&);
    rai::ErrorCode UpdateLedgerSupplies_(rai::Transaction&,
                                         const rai::TokenKey&,
                                         const rai::TokenValue&,
                                         const rai::TokenValue&);
    rai::ErrorCode UpdateLedgerTokenTransfer_(rai::Transaction&,
                                              const rai::TokenKey&, uint64_t,
                                              const rai::Account&, uint64_t);
    rai::ErrorCode UpdateLedgerTokenIdTransfer_(rai::Transaction&,
                                                const rai::TokenKey&,
                                                const rai::TokenValue&,
                                                const rai::Account&, uint64_t);
    rai::ErrorCode UpdateLedgerAccountOrdersInc_(rai::Transaction&,
                                                 const rai::Account&);
    rai::ErrorCode UpdateLedgerAccountOrdersDec_(rai::Transaction&,
                                                 const rai::Account&);
    rai::ErrorCode UpdateLedgerAccountSwapsInc_(rai::Transaction&,
                                                 const rai::Account&);
    rai::ErrorCode UpdateLedgerAccountSwapsDec_(rai::Transaction&,
                                                 const rai::Account&);
    rai::ErrorCode UpdateLedgerSwapIndex_(rai::Transaction&,
                                          const rai::OrderInfo&,
                                          const rai::SwapInfo&,
                                          const rai::Account&, uint64_t,
                                          uint64_t);

    bool CheckInquiryValue_(const rai::OrderInfo&,
                            const rai::TokenValue&) const;
    bool CheckTakeAckValue_(const rai::OrderInfo&, const rai::TokenValue&,
                            const rai::TokenValue&) const;
    bool CheckOrderCirculable_(rai::ErrorCode&, rai::Transaction&,
                               const rai::OrderInfo&, const rai::Account&,
                               const rai::Account&) const;
    rai::OrderIndex MakeOrderIndex_(const rai::OrderInfo&, const rai::Account&,
                                    uint64_t) const;
    std::function<void(const rai::TokenReceivableKey&,
                       const rai::TokenReceivable&, const rai::TokenInfo&,
                       const std::shared_ptr<rai::Block>&)>
        receivable_observer_;
    std::function<void(const rai::TokenReceivableKey)> received_observer_;
    std::function<void(const rai::TokenKey&, const rai::TokenInfo&)>
        token_observer_;
    std::function<void(const rai::TokenKey&, const rai::TokenValue&,
                       const rai::TokenIdInfo&)>
        token_id_creation_observer_;
    std::function<void(const rai::TokenKey&, const rai::TokenValue&,
                       const rai::TokenIdInfo&, const rai::Account&, bool)>
        token_id_transfer_observer_;
    std::function<void(
        const rai::Account&, const rai::AccountTokensInfo&,
        const std::vector<std::pair<rai::TokenKey, rai::AccountTokenInfo>>&)>
        account_observer_;
};
}
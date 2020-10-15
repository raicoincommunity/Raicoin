#pragma once

#include <memory>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>


#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/secure/http.hpp>
#include <rai/secure/rpc.hpp>
#include <rai/wallet/wallet.hpp>

namespace rai
{
class WalletRpcConfig
{
public:
    WalletRpcConfig();
    rai::ErrorCode DeserializeJson(bool&, rai::Ptree&);
    void SerializeJson(rai::Ptree&) const;
    rai::ErrorCode UpgradeJson(bool&, uint32_t, rai::Ptree&) const;

    static uint16_t constexpr DEFAULT_PORT =
        rai::RAI_NETWORK == rai::RaiNetworks::LIVE ? 7177 : 54302;

    rai::RpcConfig rpc_;
    rai::WalletConfig wallet_;
    rai::Url callback_url_;
    bool auto_credit_;
    bool auto_receive_;
    rai::Amount receive_mininum_;
    std::string rai_api_key_;
};


class WalletRpc;
class WalletRpcHandler : public RpcHandler
{
public:
    WalletRpcHandler(rai::WalletRpc&, rai::Rpc&, const std::string&,
                   const std::string&, const boost::asio::ip::address_v4&,
                   const std::function<void(const rai::Ptree&)>&);
    virtual ~WalletRpcHandler() = default;

    void ProcessImpl() override;
    void CheckApiKey();

    void AccountInfo();
    void AccountSend();
    void BlockQuery();
    void BlockQueryByHash(std::shared_ptr<rai::Block>&);
    void BlockQueryByHeight(std::shared_ptr<rai::Block>&);
    void BlockQueryByPrevious(std::shared_ptr<rai::Block>&);
    void CurrentAccount();
    void Status();
    void Stop();

    static rai::ErrorCode ParseAccountSend(const rai::Ptree&, rai::Account&,
                                           rai::Amount&, std::vector<uint8_t>&);

    rai::WalletRpc& main_;
};

class WalletRpcAction
{
public:
    WalletRpcAction();

    uint64_t sequence_;
    bool processed_;
    bool callback_error_;
    uint64_t last_callback_;
    std::string desc_;
    rai::ErrorCode error_code_;
    std::shared_ptr<rai::Block> block_;
    rai::Ptree request_;
};

class WalletRpc : public std::enable_shared_from_this<rai::WalletRpc>
{
public:
    WalletRpc(const std::shared_ptr<rai::Wallets>&,
              const rai::WalletRpcConfig&);

    void AddRequest(const rai::Ptree&);
    void Run();
    void Stop();
    void SendCallback(const rai::Ptree&);
    void ProcessCallbackResult(rai::ErrorCode);
    void ProcessAccountActionResult(rai::ErrorCode,
                                    const std::shared_ptr<rai::Block>&,
                                    uint64_t);
    void Status(rai::Ptree&) const;
    rai::AccountActionCallback AccountActionCallback(uint64_t);
    rai::RpcHandlerMaker RpcHandlerMaker();
    uint64_t CurrentTimestamp() const;


    std::shared_ptr<rai::Wallets> wallets_;
    rai::WalletRpcConfig config_;
    rai::Ledger& ledger_;

private:
    rai::Ptree MakeResponse_(const rai::WalletRpcAction&, const rai::Amount&,
                             const std::shared_ptr<rai::Block>&) const;
    void ProcessPendings_(std::unique_lock<std::mutex>&, bool&);
    void ProcessAutoCredit_(std::unique_lock<std::mutex>&, bool&);
    void ProcessAutoReceive_(std::unique_lock<std::mutex>&, bool&);
    void ProcessRequests_(std::unique_lock<std::mutex>&, bool&);

    std::condition_variable condition_;
    std::atomic<uint64_t> sequence_;
    mutable std::mutex mutex_;
    bool stopped_;
    bool waiting_callback_;
    bool waiting_wallet_;
    uint64_t callback_height_;

    std::deque<rai::Ptree> requests_;
    boost::multi_index_container<
        rai::WalletRpcAction,
        boost::multi_index::indexed_by<
            boost::multi_index::random_access<>,
            boost::multi_index::ordered_unique<
                boost::multi_index::member<rai::WalletRpcAction, uint64_t,
                                           &rai::WalletRpcAction::sequence_>>>>
        actions_;

    std::thread thread_;
};
}  // namespace rai
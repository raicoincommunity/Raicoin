#pragma once

#include <memory>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/common/runner.hpp>
#include <rai/common/blocks.hpp>
#include <rai/common/alarm.hpp>
#include <rai/common/numbers.hpp>
#include <rai/secure/store.hpp>
#include <rai/secure/ledger.hpp>
#include <rai/secure/websocket.hpp>
#include <rai/app/blockcache.hpp>
#include <rai/app/bootstrap.hpp>
#include <rai/app/blockconfirm.hpp>

namespace rai
{

class AppObservers
{
public:
    rai::ObserverContainer<rai::WebsocketStatus> gateway_status_;
    rai::ObserverContainer<const std::shared_ptr<rai::Block>&, bool> block_;
    rai::ObserverContainer<const std::shared_ptr<rai::Block>&> block_rollback_;
};

class AppTrace
{
public:
    AppTrace();
    std::atomic<bool> message_from_gateway_;
    std::atomic<bool> message_to_gateway_;
};

enum class AppActionPri : uint32_t
{
    LOW    = 0,
    NORMAL = 1,
    HIGH   = 2,
    URGENT = 3
};

class App : public std::enable_shared_from_this<rai::App>
{
public:
    App(rai::ErrorCode&, rai::Alarm&, const boost::filesystem::path&,
        const rai::Url&, const std::vector<rai::BlockType>&);
    virtual ~App() = default;

    virtual rai::ErrorCode PreBlockAppend(rai::Transaction&,
                                          const std::shared_ptr<rai::Block>,
                                          bool) = 0;
    virtual rai::ErrorCode AfterBlockAppend(rai::Transaction&,
                                            const std::shared_ptr<rai::Block>,
                                            bool) = 0;
    virtual rai::ErrorCode PreBlockRollback(
        rai::Transaction&, const std::shared_ptr<rai::Block>) = 0;
    virtual rai::ErrorCode AfterBlockRollback(
        rai::Transaction&, const std::shared_ptr<rai::Block>) = 0;

    std::shared_ptr<rai::App> Shared();
    void Start();
    void Stop();
    void Run();
    bool Busy() const;
    void Ongoing(const std::function<void()>&, const std::chrono::seconds&);

    rai::Ptree AccountTypes() const;
    bool BlockCacheFull() const;
    void ConnectToGateway();
    bool GatewayConnected() const;
    void ProcessBlock(const std::shared_ptr<rai::Block>&, bool);
    void ProcessBlockRollback(const std::shared_ptr<rai::Block>&);
    void PullNextBlock(const std::shared_ptr<rai::Block>&);
    void PullNextBlockAsync(const std::shared_ptr<rai::Block>&);
    void QueueAction(rai::AppActionPri, const std::function<void()>&);
    void QueueBlock(const std::shared_ptr<rai::Block>&, bool,
                    rai::AppActionPri = rai::AppActionPri::NORMAL);
    void QueueBlockRollback(const std::shared_ptr<rai::Block>&);
    void ReceiveGatewayMessage(const std::shared_ptr<rai::Ptree>&);
    void ReceiveBlockAppendNotify(const std::shared_ptr<rai::Ptree>&);
    void ReceiveBlockConfirmNotify(const std::shared_ptr<rai::Ptree>&);
    void ReceiveBlockConfirmAck(const std::shared_ptr<rai::Ptree>&);
    void ReceiveBlocksQueryAck(const std::shared_ptr<rai::Ptree>&);
    void SendToGateway(const rai::Ptree&);
    void SyncAccount(const rai::Account&);
    void SyncAccount(const rai::Account&, uint64_t,
                     uint64_t = rai::App::BLOCKS_QUERY_COUNT);
    void SyncAccountAsync(const rai::Account&);
    void SyncAccountAsync(const rai::Account&, uint64_t,
                          uint64_t = rai::App::BLOCKS_QUERY_COUNT);

    template <typename T>
    void Background(T action)
    {
        service_.post(action);
    }

    static size_t constexpr MAX_ACTIONS = 32 * 1024;
    static size_t constexpr MAX_BLOCK_CACHE_SIZE = 100 * 1024;
    static uint64_t constexpr BLOCKS_QUERY_COUNT = 100;

    rai::Alarm& alarm_;
    rai::Store store_;
    rai::Ledger ledger_;
    rai::Url gateway_url_;
    std::vector<rai::BlockType> account_types_;
    boost::asio::io_service service_;
    rai::OngoingServiceRunner service_runner_;
    std::shared_ptr<rai::WebsocketClient> gateway_ws_;
    rai::AppTrace trace_;
    rai::AppObservers observers_;
    rai::BlockCache block_cache_;
    rai::AppBootstrap bootstrap_;
    rai::BlockConfirm block_confirm_;

protected:
    template <typename Derived>
    std::shared_ptr<Derived> Shared_()
    {
        return std::static_pointer_cast<Derived>(Shared());
    }

private:
    void RegisterObservers_();
    rai::ErrorCode AppendBlock_(rai::Transaction&,
                                const std::shared_ptr<rai::Block>&, bool);
    rai::ErrorCode RollbackBlock_(rai::Transaction&, const std::shared_ptr<rai::Block>&);
    bool GetHeadBlock_(rai::Transaction&, const rai::Account&,
                       std::shared_ptr<rai::Block>&);

    std::function<void(const std::shared_ptr<rai::Block>&, bool)>
        block_observer_;
    std::function<void(const std::shared_ptr<rai::Block>&)>
        block_rollback_observer_;

    std::condition_variable condition_;
    mutable std::mutex mutex_;
    bool stopped_;
    std::multimap<rai::AppActionPri, std::function<void()>,
                  std::greater<rai::AppActionPri>>
        actions_;
    std::thread thread_;
};
}
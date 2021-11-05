#pragma once
#include <atomic>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <boost/optional.hpp>
#include <rai/common/errors.hpp>
#include <rai/node/peer.hpp>
#include <rai/node/message.hpp>
#include <rai/secure/ledger.hpp>

namespace rai
{
class Node;

class BootstrapAccount
{
public:
    void Serialize(rai::Stream&) const;
    void ToBytes(std::vector<uint8_t>&) const;
    rai::ErrorCode Deserialize(rai::Stream&);

    static size_t Size();

    rai::Account account_;
    rai::BlockHash head_;
    uint64_t height_;
};

class BootstrapFork
{
public:
    void Serialize(rai::Stream&) const;
    void ToBytes(std::vector<uint8_t>&) const;

    uint16_t length_;
    std::shared_ptr<rai::Block> first_;
    std::shared_ptr<rai::Block> second_;
};

class BootstrapClient
    : public std::enable_shared_from_this<rai::BootstrapClient>
{
public:
    BootstrapClient(const std::shared_ptr<rai::Socket>&,
                    const rai::TcpEndpoint&, rai::BootstrapType);
    BootstrapClient(const rai::BootstrapClient&) = delete;

    rai::ErrorCode Connect();
    bool Finished() const;
    rai::ErrorCode Run();
    rai::ErrorCode Pause();
    void ConnectCallback(const boost::system::error_code&);
    void WriteCallback(const boost::system::error_code&, size_t);
    void ReadAccount(const boost::system::error_code&, size_t);
    void ReadForkLength(const boost::system::error_code&, size_t);
    void ReadForkBlocks(const boost::system::error_code&, size_t);

    size_t Size() const;
    size_t Total() const;
    uint64_t TimeSpan() const;

    static size_t constexpr MAX_ACCOUNTS = 8 * 1024;
    static size_t constexpr MAX_FORKS = 1024;

    const std::array<rai::BootstrapAccount, MAX_ACCOUNTS>& Accounts()
        const;
    const std::array<rai::BootstrapFork, MAX_FORKS>& Forks() const;

private:
    uint16_t MaxSize_() const;

    rai::ErrorCode error_code_;
    std::promise<bool> promise_;
    rai::TcpEndpoint endpoint_;
    std::shared_ptr<rai::Socket> socket_;
    rai::Account next_;
    uint64_t next_height_;
    rai::BootstrapType type_;
    bool connected_;
    bool finished_;
    bool continue_;
    size_t total_;
    size_t accounts_size_;
    size_t forks_size_;
    size_t curr_size_;
    uint64_t time_span_;
    std::array<rai::BootstrapAccount, MAX_ACCOUNTS> accounts_;
    std::array<rai::BootstrapFork, MAX_FORKS> forks_;

    static size_t constexpr BUFFER_SIZE_ = 2048;
    std::vector<uint8_t> send_buffer_;
    std::vector<uint8_t> receive_buffer_;
};

class Bootstrap
{
public:
    Bootstrap(rai::Node&);
    ~Bootstrap();

    uint32_t Count() const;
    bool WaitingSyncer() const;
    void Run();
    void Stop();
    void Restart();
    bool UnderAttack() const;

    static std::chrono::seconds constexpr BOOTSTRAP_INTERVAL =
        std::chrono::seconds(300);
    static uint32_t constexpr FULL_BOOTSTRAP_INTERVAL = 12;  // an hour
    static uint32_t constexpr INITIAL_FULL_BOOTSTRAPS = 3;

private:
    void SyncGenesisAccount_();
    rai::ErrorCode RunFull_();
    rai::ErrorCode RunLight_();
    rai::ErrorCode RunFork_();
    void Wait_();
    void StartSync_(rai::Transaction&, const rai::BootstrapAccount&,
                    uint32_t) const;

    rai::Node& node_;
    std::atomic<bool> stopped_;
    std::atomic<bool> waiting_;
    std::atomic<uint32_t> count_;
    std::chrono::steady_clock::time_point last_time_;
    std::thread thread_;
};

class BootstrapServer
    : public std::enable_shared_from_this<rai::BootstrapServer>
{
public:
    BootstrapServer(const std::shared_ptr<rai::Node>&,
                    const std::shared_ptr<rai::Socket>&, const rai::IP&);
    BootstrapServer(const rai::BootstrapServer&) = delete;
    ~BootstrapServer();

    void Close();
    void Receive();
    void Run(const boost::system::error_code&, size_t);

private:
    void ReadMessage_(const boost::system::error_code&, size_t);
    void RunFull_();
    void RunLight_();
    void RunFork_();
    void Send_(const std::function<void()>&);

    rai::ErrorCode error_code_;
    std::shared_ptr<rai::Node> node_;
    std::shared_ptr<rai::Socket> socket_;
    rai::IP remote_ip_;
    rai::BootstrapType type_;
    rai::Account next_;
    uint64_t height_;
    uint16_t max_size_;
    uint16_t count_;
    bool continue_;
    bool finished_;

    static size_t constexpr BUFFER_SIZE_ = 2048;
    std::vector<uint8_t> send_buffer_;
    std::vector<uint8_t> receive_buffer_;
};

class BootstrapListener
{
public:
    BootstrapListener(rai::Node&, boost::asio::io_service&, const rai::IP&,
                      uint16_t);

    void Accept();
    void Connected(const boost::system::error_code&,
                   const std::shared_ptr<rai::Socket>&);
    void Erase(const rai::IP&);
    void Start();
    void Stop();

    static size_t constexpr MAX_CONNECTIONS = 16;

private:
    rai::Node& node_;
    boost::asio::io_service& service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    rai::TcpEndpoint local_;
    std::mutex mutex_;
    bool stopped_;
    std::unordered_map<rai::IP, std::weak_ptr<rai::BootstrapServer>>
        connections_;
};

}  // namespace rai
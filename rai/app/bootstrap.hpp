#pragma once

#include <atomic>
#include <mutex>
#include <rai/common/numbers.hpp>

namespace rai
{

class App;

class AccountHead
{
public:
    bool DeserializeJson(const rai::Ptree&);

    rai::Account account_;
    rai::BlockHash head_;
    uint64_t height_;
};

class AppBootstrap 
{
public:
    AppBootstrap(rai::App&);

    void Run();
    void Reset();
    void Stop();
    void ReceiveAccountHeadMessage(const std::shared_ptr<rai::Ptree>&);

    uint64_t NewRequestId();

    rai::App& app_;

private:
    void SendRequest_(std::unique_lock<std::mutex>&);
    void ProcessAccountHeads(std::unique_lock<std::mutex>&,
                             const std::vector<rai::AccountHead>&);

    std::atomic<uint64_t> request_id_;
    mutable std::mutex mutex_;
    bool stopped_;
    bool running_;
    uint64_t count_;
    std::chrono::steady_clock::time_point last_run_;
    std::chrono::steady_clock::time_point last_request_;
    rai::Account next_;
};

} // end of namespace
#pragma once

#include <atomic>
#include <mutex>
#include <rai/common/numbers.hpp>
#include <rai/common/util.hpp>

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

    bool Ready() const;
    void Run();
    void Reset();
    void Stop();
    void Status(rai::Ptree&) const;
    void ReceiveAccountHeadMessage(const std::shared_ptr<rai::Ptree>&);

    rai::App& app_;

private:
    uint64_t NewRequestId_(std::unique_lock<std::mutex>&);
    void SendRequest_(std::unique_lock<std::mutex>&);
    void ProcessAccountHeads_(const std::vector<rai::AccountHead>&);

    mutable std::mutex mutex_;
    bool stopped_;
    bool running_;
    uint64_t count_;
    uint64_t request_id_;
    std::chrono::steady_clock::time_point last_run_;
    std::chrono::steady_clock::time_point last_request_;
    rai::Account next_;
};

} // end of namespace
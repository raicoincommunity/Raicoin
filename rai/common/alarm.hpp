#pragma once

#include <chrono>
#include <mutex>
#include <queue>
#include <boost/asio.hpp>

namespace rai
{
class Operation
{
public:
    bool operator>(const rai::Operation&) const;
    std::chrono::steady_clock::time_point wakeup_;
    std::function<void()> function_;
};

class Alarm
{
public:
    Alarm(boost::asio::io_service&);
    ~Alarm();
    void Add(const std::chrono::steady_clock::time_point&,
             const std::function<void()>&);
    void Run();
    void Stop();

private:
    boost::asio::io_service& service_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::priority_queue<rai::Operation, std::vector<rai::Operation>,
                        std::greater<rai::Operation>>
        operations_;
    bool stopped_;
    std::thread thread_;
};
};
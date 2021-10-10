#pragma once

#include <vector>
#include <boost/asio.hpp>

namespace rai
{
class ServiceRunner
{
public:
    ServiceRunner(boost::asio::io_service&, size_t);
    ~ServiceRunner();
    void Join();

private:
    std::vector<std::thread> threads_;
};

class OngoingServiceRunner
{
public:
    OngoingServiceRunner(boost::asio::io_service&, int = 3);
    ~OngoingServiceRunner();
    void Notify();
    void Run();
    void Stop();
    void Join();

private:
    boost::asio::io_service& service_;
    int interval_;
    std::condition_variable condition_;
    mutable std::mutex mutex_;
    bool stopped_;
    std::thread thread_;
};

}
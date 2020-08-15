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

}
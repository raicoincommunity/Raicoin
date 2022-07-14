#include <rai/common/runner.hpp>

#include <iostream>
#include <rai/common/log.hpp>

rai::ServiceRunner::ServiceRunner(boost::asio::io_service& service, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        threads_.push_back(std::thread([&](){
            bool running = false;
            while (true)
            {
                try
                {
                    if (!running)
                    {
                        running = true;
                        service.run();
                        break;
                    }
                }
                catch (const std::exception& e)
                {
                    running = false;
                    rai::Log::Error(std::string("Service throw exception:")
                                    + e.what());
                }
                catch (...)
                {
                    running = false;
                    rai::Log::Error("Service throw exception");
                }
            }
        }));
    }
}

rai::ServiceRunner::~ServiceRunner()
{
    Join();
}

void rai::ServiceRunner::Join()
{
    for (auto& i : threads_)
    {
        if (i.joinable())
        {
            i.join();
        }
    }
}

rai::OngoingServiceRunner::OngoingServiceRunner(
    boost::asio::io_service& service, int interval)
    : service_(service),
      interval_(interval),
      stopped_(false),
      thread_([this]() { this->Run(); })
{
}

rai::OngoingServiceRunner::~OngoingServiceRunner()
{
    Stop();
}

void rai::OngoingServiceRunner::Notify()
{
    condition_.notify_all();
}

void rai::OngoingServiceRunner::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        condition_.wait_until(lock, std::chrono::steady_clock::now()
                                        + std::chrono::seconds(interval_));
        if (stopped_)
        {
            break;
        }
        lock.unlock();

        if (service_.stopped())
        {
            service_.reset();
        }

        try
        {
            service_.run();
        }
        catch (const std::exception& e)
        {
            std::cout << "service exception: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cout << "service exception" << std::endl;
        }

        lock.lock();
    }
}

void rai::OngoingServiceRunner::Stop()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
        service_.stop();
    }

    condition_.notify_all();
    Join();
}

void rai::OngoingServiceRunner::Join()
{
    if (thread_.joinable() && thread_.get_id() != std::this_thread::get_id())
    {
        thread_.join();
    }
}
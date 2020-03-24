#include <rai/common/alarm.hpp>


bool rai::Operation::operator>(const rai::Operation& other) const
{
    return wakeup_ > other.wakeup_;
}

rai::Alarm::Alarm(boost::asio::io_service& service)
    : service_(service), stopped_(false), thread_([this]() { Run(); })
{
}

rai::Alarm::~Alarm()
{
    Stop();
}

void rai::Alarm::Stop()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
    }

    condition_.notify_all();
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void rai::Alarm::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        if (operations_.empty())
        {
            condition_.wait(lock);
            continue;
        }

        const rai::Operation& operation = operations_.top();
        if (nullptr == operation.function_)
        {
            break;
        }

        if (operation.wakeup_ <= std::chrono::steady_clock::now())
        {
            service_.post(operation.function_);
            operations_.pop();
        }
        else
        {
            condition_.wait_until(lock, operation.wakeup_);
        }
    }
}

void rai::Alarm::Add(const std::chrono::steady_clock::time_point& wakeup,
                     const std::function<void()>& operation)
{
    std::lock_guard<std::mutex> lock(mutex_);
    operations_.push(rai::Operation({wakeup, operation}));
    condition_.notify_all();
}
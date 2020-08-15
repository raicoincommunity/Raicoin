#include <rai/common/runner.hpp>

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
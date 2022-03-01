#include <rai/app/blockquery.hpp>
#include <rai/app/app.hpp>

rai::AppBlockQuery::AppBlockQuery(const rai::Account& account, uint64_t height,
                                  uint64_t count)
    : account_(account), height_(height), count_(count)
{
}

rai::AppBlockQueries::Entry::Entry(const rai::AppBlockQuery& query)
    : key_{query.account_, query.height_},
      wakeup_(std::chrono::steady_clock::now()),
      retries_(0),
      count_(query.count_)
{
}

rai::AppBlockQueries::AppBlockQueries(rai::App& app)
    : app_(app),
      stopped_(false),
      thread_([this]() { this->Run(); })
{
}

rai::AppBlockQueries::~AppBlockQueries()
{
    Stop();
}

void rai::AppBlockQueries::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);

    while (!stopped_)
    {
        if (queries_.empty() && pendings_.empty())
        {
            condition_.wait(lock);
            continue;
        }

        while (!queries_.empty()
               && pendings_.size() < rai::AppBlockQueries::CONCURRENCY)
        {
            auto it = queries_.get<1>().begin();
            Entry entry(*it);
            queries_.get<1>().erase(it);
            pendings_.insert(entry);
        }

        if (!pendings_.empty())
        {
            auto it(pendings_.get<1>().begin());
            if (it->wakeup_ <= std::chrono::steady_clock::now())
            {
                rai::Account account = it->key_.account_;
                uint64_t height = it->key_.height_;
                uint64_t count = it->count_;
                pendings_.get<1>().modify(it, [&](Entry& data) {
                    ++data.retries_;
                    uint64_t delay = 5 + data.retries_;
                    if (delay > 60)
                    {
                        delay = 60;
                    }
                    data.wakeup_ = std::chrono::steady_clock::now()
                                   + std::chrono::seconds(delay);
                });
                lock.unlock();
                app_.SendBlocksQuery(account, height, count);
                lock.lock();
            }
            else
            {
                condition_.wait_until(lock, it->wakeup_);
            }
        }
    }
}

void rai::AppBlockQueries::Stop()
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

void rai::AppBlockQueries::Add(const rai::Account& account, uint64_t height,
                               uint64_t count)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = queries_.find(account);
        if (it == queries_.end())
        {
            queries_.insert(rai::AppBlockQuery(account, height, count));
        }
        else
        {
            if (height >= it->height_)
            {
                return;
            }
            queries_.modify(it, [&](rai::AppBlockQuery& data) {
                data.height_ = height;
                data.count_ = count;
            });
        }
    }
    condition_.notify_all();
}

void rai::AppBlockQueries::Remove(const rai::Account& account, uint64_t height)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = pendings_.find(rai::AccountHeight{account, height});
        if (it == pendings_.end())
        {
            return;
        }
        pendings_.erase(it);
    }
    condition_.notify_all();
}

size_t rai::AppBlockQueries::Size() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return queries_.size();
}

bool rai::AppBlockQueries::Busy() const
{
    return Size() >= 1000;
}

#include <rai/rai_token/swaphelper.hpp>

#include <rai/rai_token/token.hpp>

rai::TakeNackEntry::TakeNackEntry(
    const rai::Account& taker, uint64_t inquiry_height,
    const std::shared_ptr<rai::Block>& take_nack_block)
    : swap_{taker, inquiry_height}, block_(take_nack_block), retries_(0)
{
    uint64_t now = rai::CurrentTimestamp();
    if (block_->Timestamp() <= now)
    {
        timeout_ = std::chrono::steady_clock::now();
    }
    else
    {
        timeout_ = std::chrono::steady_clock::now()
                   + std::chrono::seconds(block_->Timestamp() - now);
    }
}

rai::SwapHelper::SwapHelper(rai::Token& token)
    : token_(token), stopped_(false), thread_([this]() { Run(); })
{
}

rai::SwapHelper::~SwapHelper()
{
    Stop();
}


void rai::SwapHelper::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        if (entries_.empty())
        {
            condition_.wait(lock);
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        auto it = entries_.get<TakeNackByTimeout>().begin();
        if (it->timeout_ > now)
        {
            condition_.wait_until(lock, it->timeout_);
            continue;
        }

        rai::TakeNackEntry entry = *it;
        entries_.get<TakeNackByTimeout>().modify(
            it, [&](rai::TakeNackEntry& data) {
                ++data.retries_;
                uint64_t shift = 8;
                if (data.retries_ < 8)
                {
                    shift = data.retries_;
                }
                uint64_t delay = uint64_t(1) << shift;
                if (delay > 2)
                {
                    delay = rai::Random(2, delay);
                }
                data.timeout_ = now + std::chrono::seconds(delay);
            });
        lock.unlock();
        Process(entry);
        lock.lock();
    }
}

void rai::SwapHelper::Stop()
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

void rai::SwapHelper::Add(const rai::Account& taker, uint64_t inquiry_height,
                          const std::shared_ptr<rai::Block>& take_nack_block)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        rai::TakeNackEntry entry(taker, inquiry_height, take_nack_block);
        auto it = entries_.find(entry.swap_);
        if (it != entries_.end())
        {
            return;
        }
        entries_.insert(entry);
    }
    condition_.notify_all();
}

void rai::SwapHelper::Remove(const rai::Account& taker, uint64_t inquiry_height)
{
    rai::AccountHeight key{taker, inquiry_height};
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it == entries_.end())
        {
            return;
        }
        entries_.erase(it);
    }
    condition_.notify_all();
}

void rai::SwapHelper::Process(const rai::TakeNackEntry& entry)
{
    if (entry.block_->Timestamp() > rai::CurrentTimestamp())
    {
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    rai::AccountInfo maker_info;
    bool error = token_.ledger_.AccountInfoGet(
        transaction, entry.block_->Account(), maker_info);
    if (error || !maker_info.Valid())
    {
        rai::Log::Error(rai::ToString(
            "SwapHelper::Process: get maker info failed, account=",
            entry.block_->Account().StringAccount(),
            ", hash=", entry.block_->Hash().StringHex()));
        return;
    }

    if (maker_info.head_height_ < entry.block_->Height())
    {
        token_.PublishBlockAsync(entry.block_);
        return;
    }

    if (!maker_info.Confirmed(entry.block_->Height()))
    {
        return;
    }

    token_.PurgeTakeNackBlock(entry.swap_.account_, entry.swap_.height_);
    Remove(entry.swap_.account_, entry.swap_.height_);
}

size_t rai::SwapHelper::Size() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return entries_.size();
}
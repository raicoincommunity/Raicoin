#include <rai/common/stat.hpp>

rai::Stat<rai::ErrorCode> rai::Stats::error_;

rai::StatEntry::StatEntry() : count_(0), index_(0)
{
}

void rai::StatEntry::Add(uint64_t value, const std::string& detail)
{
    count_ += value;
    if (!detail.empty())
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index_ < rai::StatEntry::MAX_DETAILS)
        {
            details_.push_back(detail);
        }
        else
        {
            details_[index_ % rai::StatEntry::MAX_DETAILS] = detail;
        }
        ++index_;
    }
}

uint64_t rai::StatEntry::Get() const
{
    return count_;
}

void rai::StatEntry::Get(uint64_t& value,
                         std::vector<std::string>& details) const
{
    value = count_;
    std::lock_guard<std::mutex> lock(mutex_);
    if (index_ < rai::StatEntry::MAX_DETAILS)
    {
        details = details_;
    }
    else
    {
        uint64_t index = index_ % rai::StatEntry::MAX_DETAILS;
        details.insert(details.end(), details_.begin() + index, details_.end());
        details.insert(details.end(), details_.begin(),
                       details_.begin() + index);
    }
}

void rai::StatEntry::Reset()
{
    count_ = 0;
    std::lock_guard<std::mutex> lock(mutex_);
    index_ = 0;
    details_.clear();
}

void rai::Stats::Add(rai::ErrorCode error_code)
{
    error_.Add(error_code, 1, std::string());
}

template <>
std::vector<rai::StatResult<rai::ErrorCode>>
rai::Stats::GetAll<rai::ErrorCode>()
{
    return error_.GetAll();
}

void rai::Stats::Reset(rai::ErrorCode error_code)
{
    error_.Reset(error_code);
}

template <>
void rai::Stats::ResetAll<rai::ErrorCode>()
{
    return error_.ResetAll();
}


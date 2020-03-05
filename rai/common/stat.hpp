#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <rai/common/util.hpp>
#include <rai/common/errors.hpp>

namespace rai
{
template <typename KeyType>
class StatResult
{
public:
    KeyType index_;
    uint64_t count_;
    std::vector<std::string> details_;
};

class StatEntry
{
public:
    StatEntry();
    void Add(uint64_t, const std::string&);
    uint64_t Get() const;
    void Get(uint64_t&, std::vector<std::string>&) const;
    void Reset();

    static size_t constexpr MAX_DETAILS = 10;

private:
    std::atomic<uint64_t> count_;
    mutable std::mutex mutex_;
    uint64_t index_;
    std::vector<std::string> details_;
};

template <class KeyType,
          class = typename std::enable_if<std::is_enum<KeyType>::value>::type>
class Stat
{
public:
    void Add(KeyType key, uint64_t value, const std::string& str)
    {
        if (key >= KeyType::MAX)
        {
            return;
        }
        size_t index = static_cast<size_t>(key);
        entries_[index].Add(value, str);
    }

    uint64_t Get(KeyType key) const
    {
        if (key >= KeyType::MAX)
        {
            return 0;
        }
        size_t index = static_cast<size_t>(key);
        entries_[index].Get();
    }

    void Get(KeyType key, uint64_t& value,
             std::vector<std::string>& detail) const
    {
        if (key >= KeyType::MAX)
        {
            value = 0;
            detail.clear();
            return;
        }
        size_t index = static_cast<size_t>(key);
        entries_[index].Get(value, detail);
    }

    void Reset(KeyType key)
    {
        if (key >= KeyType::MAX)
        {
            return;
        }
        size_t index = static_cast<size_t>(key);
        entries_[index].Reset();
    }

    void ResetAll()
    {
        for (size_t index = 0; index < entries_.size(); ++index)
        {
            entries_[index].Reset();
        }
    }

    std::vector<rai::StatResult<KeyType>> GetAll() const
    {
        std::vector<rai::StatResult<KeyType>> result;
        for (size_t index = 0; index < entries_.size(); ++index)
        {
            rai::StatResult<KeyType> stat;
            stat.index_ = static_cast<KeyType>(index);
            entries_[index].Get(stat.count_, stat.details_);
            if (stat.count_ > 0 || !stat.details_.empty())
            {
                result.push_back(stat);
            }
        }
        return result;
    }

private:
    std::array<rai::StatEntry, static_cast<size_t>(KeyType::MAX)> entries_;
};


class Stats 
{
public:
    Stats() = delete;

    static void Add(rai::ErrorCode);
    template <typename... Args>
    static void Add(rai::ErrorCode error_code, Args... args)
    {
        error_.Add(error_code, 1, rai::ToString(args...));
    }
    template <typename... Args>
    static void AddDetail(rai::ErrorCode error_code, Args... args)
    {
        error_.Add(error_code, 0, rai::ToString(args...));
    }
    static uint64_t Get(rai::ErrorCode);
    static void Get(rai::ErrorCode, uint64_t&, std::string&);
    template <typename KeyType>
    static std::vector<rai::StatResult<KeyType>> GetAll();
    static void Reset(rai::ErrorCode);
    template <typename KeyType>
    static void ResetAll();

    static rai::Stat<rai::ErrorCode> error_;
};

}  // namespace rai



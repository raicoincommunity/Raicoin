#pragma once
#include <chrono>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/blocks.hpp>

namespace rai
{
class GapInfo
{
public:
    GapInfo(const rai::BlockHash&, const std::shared_ptr<rai::Block>&);
    rai::BlockHash hash_;
    rai::Account account_;
    std::chrono::steady_clock::time_point arrival_;
    std::shared_ptr<rai::Block> block_;
};

class GapInfoByAccount
{
};

class GapInfoByArrival
{
};

class GapCache
{
public:
    bool Insert(const rai::GapInfo&);
    void Remove(const rai::BlockHash&);
    boost::optional<rai::GapInfo> Query(const rai::BlockHash&) const;
    std::vector<rai::GapInfo> Age(uint64_t);

private:
    static size_t constexpr MAX_CACHES_PER_ACCOUNT = 16;
    static size_t constexpr MAX_CACHES = 128 * 1024;

    mutable std::mutex mutex_;
    boost::multi_index_container<
        rai::GapInfo,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<boost::multi_index::member<
                rai::GapInfo, rai::BlockHash, &rai::GapInfo::hash_>>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<rai::GapInfoByAccount>,
                boost::multi_index::member<rai::GapInfo, rai::Account,
                                           &rai::GapInfo::account_>>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<rai::GapInfoByArrival>,
                boost::multi_index::member<
                    rai::GapInfo, std::chrono::steady_clock::time_point,
                    &rai::GapInfo::arrival_>>>>
        caches_;
};
}  // namespace rai
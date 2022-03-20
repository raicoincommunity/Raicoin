#pragma once
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/util.hpp>
#include <rai/common/blocks.hpp>

namespace rai
{
class App;

enum class AppTopicType : uint32_t
{
    INVALID             = 0,
    ORDER_PAIR          = 1,
    ORDER_ID            = 2,
    ACCOUNT_SWAP_INFO   = 3,
    ACCOUNT_HEAD        = 4,

    MAX
};

class AppTopic
{
public:
    rai::Topic topic_;
    rai::UniqueId uid_;
    std::chrono::steady_clock::time_point time_;
};

class AppTopicByUid
{
};

class AppTopicByTime
{
};

typedef boost::multi_index_container<
    rai::AppTopic,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::composite_key<
            rai::AppTopic,
            boost::multi_index::member<rai::AppTopic, rai::Topic,
                                       &rai::AppTopic::topic_>,
            boost::multi_index::member<rai::AppTopic, rai::UniqueId,
                                       &rai::AppTopic::uid_>>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::AppTopicByUid>,
            boost::multi_index::member<rai::AppTopic, rai::UniqueId,
                                       &rai::AppTopic::uid_>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::AppTopicByTime>,
            boost::multi_index::member<rai::AppTopic,
                                       std::chrono::steady_clock::time_point,
                                       &rai::AppTopic::time_>>>>
    AppTopicContainer;

class AppTopics
{
public:
    AppTopics(rai::App&);

    void Cutoff();
    void Notify(const rai::Topic&, const rai::Ptree&);
    void Notify(const std::vector<rai::Topic>&, const rai::Ptree&);
    void NotifyAccountHead(const std::shared_ptr<rai::Block>&, bool);
    rai::ErrorCode Subscribe(const rai::Topic&, const rai::UniqueId&);
    bool Subscribed(const rai::Topic&) const;
    void Unsubscribe(const rai::UniqueId&);
    size_t Size() const;
    size_t Size(const rai::UniqueId&) const;
    void Json(rai::Ptree&) const;
    void JsonByUid(const rai::UniqueId&, rai::Ptree&) const;
    void JsonByTopic(const rai::Topic&, rai::Ptree&) const;

    static rai::Topic CalcTopic(rai::AppTopicType, const rai::uint256_union&);
    static rai::Topic CalcTopic(rai::AppTopicType, const std::vector<uint8_t>&);
    static rai::Topic CalcTopic(rai::AppTopicType, const rai::Serializer&);

    rai::App& app_;

    static std::chrono::seconds constexpr CUTOFF_TIME =
        std::chrono::seconds(900);

private:
    void Json_(const rai::AppTopic&, rai::Ptree&) const;

    mutable std::mutex mutex_;
    rai::AppTopicContainer topics_;
};

}
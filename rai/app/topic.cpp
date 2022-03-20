#include <rai/app/topic.hpp>

#include <set>
#include <blake2/blake2.h>
#include <rai/app/app.hpp>

std::chrono::seconds constexpr rai::AppTopics::CUTOFF_TIME;

rai::AppTopics::AppTopics(rai::App& app) : app_(app)
{
    app_.observers_.block_.Add(
        [this](const std::shared_ptr<rai::Block>& block, bool confirmed) {
            NotifyAccountHead(block, confirmed);
        });
}

void rai::AppTopics::Cutoff()
{
    std::unique_lock<std::mutex> lock(mutex_);

    auto cutoff =
        std::chrono::steady_clock::now() - rai::AppTopics::CUTOFF_TIME;
    auto& index = topics_.get<rai::AppTopicByTime>();
    index.erase(index.begin(), index.lower_bound(cutoff));
}

void rai::AppTopics::Notify(const rai::Topic& topic, const rai::Ptree& notify)
{
    std::vector<rai::UniqueId> clients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::pair<rai::AppTopicContainer::iterator,
                  rai::AppTopicContainer::iterator>
            it_pair = topics_.equal_range(boost::make_tuple(topic));
        for (auto& i = it_pair.first; i != it_pair.second; ++i)
        {
            clients.push_back(i->uid_);
        }
    }

    for (const auto& i : clients)
    {
        app_.SendToClient(notify, i);
    }
}

void rai::AppTopics::Notify(const std::vector<rai::Topic>& topics,
                            const rai::Ptree& notify)
{
    std::set<rai::UniqueId> clients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& topic : topics)
        {
            std::pair<rai::AppTopicContainer::iterator,
                      rai::AppTopicContainer::iterator>
                it_pair = topics_.equal_range(boost::make_tuple(topic));
            for (auto& i = it_pair.first; i != it_pair.second; ++i)
            {
                clients.insert(i->uid_);
            }
        }
    }

    for (const auto& i : clients)
    {
        app_.SendToClient(notify, i);
    }
}

void rai::AppTopics::NotifyAccountHead(const std::shared_ptr<rai::Block>& block,
                                       bool confirmed)
{
    rai::Account account = block->Account();
    rai::Topic topic =
        rai::AppTopics::CalcTopic(rai::AppTopicType::ACCOUNT_HEAD, account);
    if (!Subscribed(topic))
    {
        return;
    }

    rai::Ptree ptree;
    ptree.put("account", account.StringAccount());
    ptree.put("hash", block->Hash().StringHex());

    rai::Ptree block_ptree;
    block->SerializeJson(block_ptree);
    ptree.add_child("block", block_ptree);
    ptree.put("confirmed", rai::BoolToString(confirmed));

    using P = rai::Provider;
    P::PutAction(ptree, P::Action::APP_ACCOUNT_HEAD);
    P::PutId(ptree, app_.provider_info_.id_);
    P::AppendFilter(ptree, P::Filter::APP_TOPIC, topic.StringHex());

    Notify(topic, ptree);
}

rai::ErrorCode rai::AppTopics::Subscribe(const rai::Topic& topic,
                                         const rai::UniqueId& uid)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto it = topics_.find(boost::make_tuple(topic, uid));
    if (it == topics_.end())
    {
        rai::AppTopic sub{topic, uid, now};
        topics_.insert(sub);
    }
    else
    {
        topics_.modify(it, [&](rai::AppTopic& sub) { sub.time_ = now; });
    }

    return rai::ErrorCode::SUCCESS;
}

bool rai::AppTopics::Subscribed(const rai::Topic& topic) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::pair<rai::AppTopicContainer::iterator,
              rai::AppTopicContainer::iterator>
        it_pair = topics_.equal_range(boost::make_tuple(topic));
    return it_pair.first != it_pair.second;
}

void rai::AppTopics::Unsubscribe(const rai::UniqueId& uid)
{
    std::unique_lock<std::mutex> lock(mutex_);
    auto& index = topics_.get<rai::AppTopicByUid>();
    index.erase(index.lower_bound(uid), index.upper_bound(uid));
}

size_t rai::AppTopics::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return topics_.size();
}

size_t rai::AppTopics::Size(const rai::UniqueId& uid) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return topics_.get<rai::AppTopicByUid>().count(uid);
}

void rai::AppTopics::Json(rai::Ptree& ptree) const
{
    std::unique_lock<std::mutex> lock(mutex_);
    rai::Ptree topics;
    for (auto i = topics_.begin(), n = topics_.end(); i != n; ++i)
    {
        rai::Ptree entry;
        Json_(*i, entry);
        topics.push_back(std::make_pair("", entry));
    }
    ptree.put_child("topics", topics);
}

void rai::AppTopics::JsonByUid(const rai::UniqueId& uid,
                               rai::Ptree& ptree) const
{
    std::unique_lock<std::mutex> lock(mutex_);
    rai::Ptree topics;
    auto begin = topics_.get<rai::AppTopicByUid>().lower_bound(uid);
    auto end = topics_.get<rai::AppTopicByUid>().upper_bound(uid);
    for (auto& i = begin; i != end; ++i)
    {
        rai::Ptree entry;
        Json_(*i, entry);
        topics.push_back(std::make_pair("", entry));
    }
    ptree.put_child("topics", topics);
}

void rai::AppTopics::JsonByTopic(const rai::Topic& topic,
                                 rai::Ptree& ptree) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    rai::Ptree topics;
    std::pair<rai::AppTopicContainer::iterator,
              rai::AppTopicContainer::iterator>
        it_pair = topics_.equal_range(boost::make_tuple(topic));
    for (auto& i = it_pair.first; i != it_pair.second; ++i)
    {
        rai::Ptree entry;
        Json_(*i, entry);
        topics.push_back(std::make_pair("", entry));
    }
    ptree.put_child("topics", topics);
}

rai::Topic rai::AppTopics::CalcTopic(rai::AppTopicType type,
                                     const rai::uint256_union& data)
{
    int ret;
    rai::uint256_union result;
    blake2b_state state;

    ret = blake2b_init(&state, sizeof(result.bytes));
    assert(0 == ret);

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, type);
        rai::Write(stream, data.bytes);
    }
    blake2b_update(&state, bytes.data(), bytes.size());

    ret = blake2b_final(&state, result.bytes.data(), sizeof(result.bytes));
    assert(0 == ret);
    return result;
}

rai::Topic rai::AppTopics::CalcTopic(rai::AppTopicType type,
                                     const std::vector<uint8_t>& data)
{
    int ret;
    rai::uint256_union result;
    blake2b_state state;

    ret = blake2b_init(&state, sizeof(result.bytes));
    assert(0 == ret);

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, type);
        rai::Write(stream, data);
    }
    blake2b_update(&state, bytes.data(), bytes.size());

    ret = blake2b_final(&state, result.bytes.data(), sizeof(result.bytes));
    assert(0 == ret);
    return result;
}

rai::Topic rai::AppTopics::CalcTopic(rai::AppTopicType type,
                                     const rai::Serializer& data)
{
    int ret;
    rai::uint256_union result;
    blake2b_state state;

    ret = blake2b_init(&state, sizeof(result.bytes));
    assert(0 == ret);

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, type);
        data.Serialize(stream);
    }
    blake2b_update(&state, bytes.data(), bytes.size());

    ret = blake2b_final(&state, result.bytes.data(), sizeof(result.bytes));
    assert(0 == ret);
    return result;
}

void rai::AppTopics::Json_(const rai::AppTopic& topic,
                                  rai::Ptree& ptree) const
{
    ptree.put("topic", topic.topic_.StringHex());
    ptree.put("uid", topic.uid_.StringHex());
    auto now = std::chrono::steady_clock::now();
    auto subscribe_at =
        std::chrono::duration_cast<std::chrono::seconds>(now - topic.time_)
            .count();
    ptree.put("subscribe_at", std::to_string(subscribe_at) + " seconds ago");
}
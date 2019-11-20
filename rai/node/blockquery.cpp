#include <rai/node/blockquery.hpp>
#include <rai/node/node.hpp>

bool rai::QueryFrom::operator==(const rai::QueryFrom& other) const
{
    if (endpoint_ != other.endpoint_)
    {
        return false;
    }

    if (proxy_endpoint_ && !other.proxy_endpoint_)
    {
        return false;
    }

    if (!proxy_endpoint_ && other.proxy_endpoint_)
    {
        return false;
    }

    if (!proxy_endpoint_ && !other.proxy_endpoint_)
    {
        return true;
    }

    return *proxy_endpoint_ == *other.proxy_endpoint_;
}

bool rai::QueryFrom::operator!=(const rai::QueryFrom& other) const
{
    return !(*this == other);
}

rai::QueryAck::QueryAck() : status_(rai::QueryStatus::PENDING), block_(nullptr)
{
}

rai::QueryAck::QueryAck(rai::QueryStatus status,
                        const std::shared_ptr<rai::Block>& block)
    : status_(status), block_(block)
{
}

rai::BlockQuery::BlockQuery(uint64_t sequence, rai::QueryBy by,
                            const rai::Account& account, uint64_t height,
                            const rai::BlockHash& hash, bool only_full_node,
                            bool only_specified_node,
                            const rai::QueryCallback& callback)
    : sequence_(sequence),
      wakeup_(std::chrono::steady_clock::now()),
      by_(by),
      account_(account),
      height_(height),
      hash_(hash),
      only_full_node_(only_full_node),
      only_specified_node_(only_specified_node),
      count_(0),
      from_(),
      ack_(),
      callback_(callback)
{
}

rai::BlockQuery::BlockQuery(uint64_t sequence, rai::QueryBy by,
                            const rai::Account& account, uint64_t height,
                            const rai::BlockHash& hash, bool only_full_node,
                            bool only_specified_node,
                            const std::vector<rai::QueryFrom>& from,
                            const rai::QueryCallback& callback)
    : sequence_(sequence),
      wakeup_(std::chrono::steady_clock::now()),
      by_(by),
      account_(account),
      height_(height),
      hash_(hash),
      only_full_node_(only_full_node),
      only_specified_node_(only_specified_node),
      count_(0),
      from_(from),
      ack_(from.size()),
      callback_(callback)
{
}

rai::BlockQueries::BlockQueries(rai::Node& node)
    : node_(node),
      sequence_(0),
      stopped_(false),
      thread_([this]() { this->Run(); })
{
}

rai::BlockQueries::~BlockQueries()
{
    Stop();
}

void rai::BlockQueries::Insert(const rai::BlockQuery& query)
{
    std::lock_guard<std::mutex> lock(mutex_);
    queries_.insert(query);
    condition_.notify_all();
} 

void rai::BlockQueries::ProcessQuery(rai::BlockQuery& query)
{
    std::cout << "===========> ProcessQuery 1\n";
    if (query.count_ == 0)
    {
        SendQuery_(query);
        UpdateWakeup_(query);
        Insert(query);
        return;
    }

    for (auto& i : query.ack_)
    {
        if (i.status_ == rai::QueryStatus::PENDING)
        {
            i.status_ = rai::QueryStatus::TIMEOUT;
        }

        if (i.status_ == rai::QueryStatus::PRUNED)
        {
            query.only_full_node_ = true;
        }
    }

    std::vector<rai::QueryCallbackStatus> callback_status;
    query.callback_(query.ack_, callback_status);
    if (callback_status.size() != query.ack_.size())
    {
        assert(0);
        return;
    }

    bool finish = true;
    for (size_t i = 0; i < callback_status.size(); ++i)
    {
        if (callback_status[i] == rai::QueryCallbackStatus::CONTINUE)
        {
            finish                = false;
            query.ack_[i].status_ = rai::QueryStatus::PENDING;
        }
    }

    if (!finish)
    {
        SendQuery_(query);
        UpdateWakeup_(query);
        Insert(query);
    }
}

void rai::BlockQueries::ProcessQueryAck(
    uint64_t sequence, rai::QueryBy by, const rai::Account& account,
    uint64_t height, const rai::BlockHash& hash, rai::QueryStatus status,
    const std::shared_ptr<rai::Block>& block, const rai::Endpoint& endpoint,
    const boost::optional<rai::Endpoint>& proxy)
{
    rai::BlockQuery query;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = queries_.find(sequence);
        if (it == queries_.end())
        {
            return;
        }
        if (it->by_ != by || it->account_ != account || it->height_ != height
            || it->hash_ != hash)
        {
            return;
        }

        query = *it;
        rai::QueryFrom from{endpoint, proxy};
        for (size_t i = 0; i < query.from_.size(); ++i)
        {
            if (from != query.from_[i])
            {
                continue;
            }
            query.ack_[i].status_ = status;
            query.ack_[i].block_ = block;
            if (status == rai::QueryStatus::PRUNED)
            {
                query.only_full_node_ = true;
            }
        }
        
        for (size_t i = 0; i < query.ack_.size(); ++i)
        {
            if (query.ack_[i].status_ == rai::QueryStatus::PENDING)
            {
                queries_.modify(it, [&query](rai::BlockQuery& old){
                    old = query;
                });
                return;
            }
        }
        queries_.erase(it);
    }

    std::vector<rai::QueryCallbackStatus> callback_status;
    query.callback_(query.ack_, callback_status);
    if (callback_status.size() != query.ack_.size())
    {
        assert(0);
        return;
    }

    bool finish = true;
    for (size_t i = 0; i < callback_status.size(); ++i)
    {
        if (callback_status[i] == rai::QueryCallbackStatus::CONTINUE)
        {
            finish                = false;
            query.ack_[i].status_ = rai::QueryStatus::PENDING;
            query.ack_[i].block_ = nullptr;
        }
    }

    if (!finish)
    {
        SendQuery_(query);
        UpdateWakeup_(query);
        Insert(query);
    }
}

void rai::BlockQueries::QueryByHash(const rai::Account& account,
                                    uint64_t height, const rai::BlockHash& hash,
                                    bool only_full_node,
                                    const rai::QueryCallback& callback)
{
    rai::BlockQuery query(Sequence(), rai::QueryBy::HASH, account, height, hash,
                          only_full_node, false, callback);
    Insert(query);
}

void rai::BlockQueries::QueryByHash(const rai::Account& account,
                                    uint64_t height, const rai::BlockHash& hash,
                                    const std::vector<rai::QueryFrom>& from,
                                    const rai::QueryCallback& callback)
{
    if (from.empty())
    {
        return;
    }
    rai::BlockQuery query(Sequence(), rai::QueryBy::HASH, account, height, hash,
                          false, true, from, callback);
    Insert(query);
}

void rai::BlockQueries::QueryByHeight(const rai::Account& account,
                                      uint64_t height, bool only_full_node,
                                      const rai::QueryCallback& callback)
{
    rai::BlockQuery query(Sequence(), rai::QueryBy::HEIGHT, account, height,
                          rai::BlockHash(0), only_full_node, false, callback);
    Insert(query);
}

void rai::BlockQueries::QueryByHeight(const rai::Account& account,
                                      uint64_t height,
                                      const std::vector<rai::QueryFrom>& from,
                                      const rai::QueryCallback& callback)
{
    if (from.empty())
    {
        return;
    }
    rai::BlockQuery query(Sequence(), rai::QueryBy::HEIGHT, account, height,
                          rai::BlockHash(0), false, true, from, callback);
    Insert(query);
}

void rai::BlockQueries::QueryByPrevious(const rai::Account& account,
                                        uint64_t height,
                                        const rai::BlockHash& hash,
                                        bool only_full_node,
                                        const rai::QueryCallback& callback)
{
    rai::BlockQuery query(Sequence(), rai::QueryBy::PREVIOUS, account, height,
                          hash, only_full_node, false, callback);
    Insert(query);
}

void rai::BlockQueries::QueryByPrevious(const rai::Account& account,
                                        uint64_t height,
                                        const rai::BlockHash& hash,
                                        const std::vector<rai::QueryFrom>& from,
                                        const rai::QueryCallback& callback)
{
    if (from.empty())
    {
        return;
    }
    rai::BlockQuery query(Sequence(), rai::QueryBy::PREVIOUS, account, height,
                          hash, false, true, from, callback);
    Insert(query);
}

void rai::BlockQueries::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);

    while (!stopped_)
    {
        if (queries_.empty())
        {
            condition_.wait(lock);
            continue;
        }

        auto it(queries_.get<1>().begin());
        if (it->wakeup_ <= std::chrono::steady_clock::now())
        {
            rai::BlockQuery query(*it);
            queries_.get<1>().erase(it);
            lock.unlock();
            ProcessQuery(query);
            lock.lock();
        }
        else
        {
            condition_.wait_until(lock, it->wakeup_);
        }
    }
}

void rai::BlockQueries::Stop()
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

uint64_t rai::BlockQueries::Sequence()
{
    return sequence_++;
}

size_t rai::BlockQueries::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queries_.size();
}


void rai::BlockQueries::SendQuery_(rai::BlockQuery& query)
{
    if (query.only_specified_node_)
    {
        if (query.from_.empty())
        {
            assert(0);
            return;
        }
        for (size_t i = 0; i < query.from_.size(); ++i)
        {
            if (query.ack_[i].status_ != rai::QueryStatus::PENDING)
            {
                continue;
            }
            node_.BlockQuery(query.sequence_, query.by_, query.account_,
                             query.height_, query.hash_,
                             query.from_[i].endpoint_,
                             query.from_[i].proxy_endpoint_);
        }
        ++query.count_;
        return;
    }

    boost::optional<rai::Peer> peer(boost::none);
    if (query.only_full_node_)
    {
        peer = node_.peers_.RandomFullNodePeer();
    }
    else
    {
        peer = node_.peers_.RandomPeer();
    }

    if (!peer)
    {
        std::cout << "[DEBUG]===========> SendQuery_ 1\n";
        return;
    }
    boost::optional<rai::Endpoint> proxy_endpoint(boost::none);
    if (peer->GetProxy())
    {
        proxy_endpoint = peer->GetProxy()->Endpoint();
    }
    node_.BlockQuery(query.sequence_, query.by_, query.account_, query.height_,
                     query.hash_, peer->Endpoint(), proxy_endpoint);
    query.from_.clear();
    query.from_.push_back(rai::QueryFrom{peer->Endpoint(), proxy_endpoint});
    query.ack_.clear();
    query.ack_.push_back(rai::QueryAck());
    ++query.count_;
}

void rai::BlockQueries::UpdateWakeup_(rai::BlockQuery& query) const
{
    uint32_t max_doubles           = 8;
    uint32_t delay                 = 1 << max_doubles;
    uint32_t delay_double_interval = 3;
    uint32_t doubles               = query.count_ / delay_double_interval;
    if (doubles < max_doubles)
    {
        delay = 1 << doubles;
    }
    query.wakeup_ =
        std::chrono::steady_clock::now() + std::chrono::seconds(delay);
}

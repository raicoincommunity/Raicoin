#include <rai/node/peer.hpp>
#include <rai/common/numbers.hpp>
#include <rai/node/node.hpp>

size_t constexpr rai::Peers::MAX_PEERS_PER_IP;
size_t constexpr rai::Peers::MAX_PEERS_PER_PROXY;
std::chrono::seconds constexpr rai::Peers::COOKIE_CUTOFF_TIME;
uint32_t constexpr rai::Peers::MAX_COOKIE_ATTEMPTS;
uint64_t constexpr rai::Peers::MAX_TIMESTAMP_DIFF;
std::chrono::seconds constexpr rai::Peers::KEEPLIVE_PERIOD;
std::chrono::seconds constexpr rai::Peers::PEER_CUTOFF_TIME;
std::chrono::seconds constexpr rai::Peers::PEER_ATTEMPT_TIME;

rai::Cookie::Cookie(const rai::Endpoint& remote)
    : endpoint_(remote),
      cookie_(0),
      learned_from_(boost::none),
      account_(boost::none),
      cutoff_time_(std::chrono::steady_clock::now()
                   + rai::Peers::COOKIE_CUTOFF_TIME),
      attempts_(0),
      use_proxy_(false),
      syn_direct_(false),
      syn_proxy_(false)
{
    rai::random_pool.GenerateBlock(cookie_.bytes.data(), cookie_.bytes.size());
}

rai::Cookie::Cookie(const rai::Endpoint& remote, const rai::Account& account)
    : endpoint_(remote),
      cookie_(0),
      learned_from_(boost::none),
      account_(account),
      cutoff_time_(std::chrono::steady_clock::now()
                   + rai::Peers::COOKIE_CUTOFF_TIME),
      attempts_(0),
      use_proxy_(false),
      syn_direct_(false),
      syn_proxy_(false)
{
    rai::random_pool.GenerateBlock(cookie_.bytes.data(), cookie_.bytes.size());
}

rai::Cookie::Cookie(const rai::Endpoint& remote,
                    const rai::Endpoint& learned_from,
                    const rai::Account& account)
    : endpoint_(remote),
      cookie_(0),
      learned_from_(learned_from),
      account_(account),
      cutoff_time_(std::chrono::steady_clock::now()
                   + rai::Peers::COOKIE_CUTOFF_TIME),
      attempts_(0),
      use_proxy_(false),
      syn_direct_(false),
      syn_proxy_(false)
{
    rai::random_pool.GenerateBlock(cookie_.bytes.data(), cookie_.bytes.size());
}

rai::Peer::Peer(const rai::Account& account, const rai::IP& ip, uint16_t port,
                uint8_t version, uint8_t version_min,
                const rai::Amount& rep_weight)
    : account_(account),
      ip_(ip),
      port_(port),
      version_(version),
      version_min_(version_min),
      lost_acks_(0),
      rep_weight_(rep_weight),
      light_node_(false),
      proxy_(boost::none),
      proxy_secondary_(boost::none),
      timestamp_(0),
      last_contact_(std::chrono::steady_clock::now()),
      last_attempt_(),
      last_hash_()
{
}

rai::Peer::Peer(const rai::Account& account, const rai::IP& ip, uint16_t port,
                uint8_t version, uint8_t version_min,
                const rai::Amount& rep_weight, const rai::Proxy& proxy)
    : account_(account),
      ip_(ip),
      port_(port),
      version_(version),
      version_min_(version_min),
      lost_acks_(0),
      light_node_(false),
      rep_weight_(rep_weight),
      proxy_(proxy),
      proxy_secondary_(boost::none),
      timestamp_(0),
      last_contact_(std::chrono::steady_clock::now()),
      last_attempt_(),
      last_hash_()
{
}

rai::Peer::Peer(const rai::Cookie& cookie, const rai::Account& node_account,
                uint8_t version, uint8_t version_min,
                const rai::Amount& rep_weight)
    : account_(*cookie.account_),
      ip_(cookie.endpoint_.address().to_v4()),
      port_(cookie.endpoint_.port()),
      version_(version),
      version_min_(version_min),
      lost_acks_(0),
      rep_weight_(rep_weight),
      light_node_(false),
      proxy_(boost::none),
      proxy_secondary_(boost::none),
      timestamp_(0),
      last_contact_(std::chrono::steady_clock::now()),
      last_attempt_(),
      last_hash_()
{
    if (cookie.use_proxy_)
    {
        proxy_ =
            rai::Proxy(*cookie.learned_from_, node_account, *cookie.account_);
    }
}

bool rai::Peer::operator==(const rai::Peer& other) const
{
    return account_ == other.account_;
}

bool rai::Peer::operator<(const rai::Peer& other) const
{
    if (*this == other)
    {
        return false;
    }

    if (proxy_ && !other.proxy_)
    {
        return true;
    }

    if (!proxy_ && other.proxy_)
    {
        return false;
    }

    if (rep_weight_ != other.rep_weight_)
    {
        return rep_weight_ < other.rep_weight_;
    }

    return account_ < other.account_;
}

bool rai::Peer::operator>(const rai::Peer& other) const
{
    if (*this == other)
    {
        return false;
    }

    return !(*this < other);
}

boost::optional<rai::Proxy> rai::Peer::GetProxy() const
{
    return proxy_;
}

bool rai::Peer::SetProxy(const rai::Proxy& proxy)
{
    if (!proxy_)
    {
        proxy_ = proxy;
        return false;
    }

    if (proxy == *proxy_)
    {
        return true;
    }

    if (!proxy_secondary_
        || (proxy.Priority() > (*proxy_secondary_).Priority()))
    {
        proxy_secondary_ = proxy;
        if ((*proxy_secondary_).Priority() > (*proxy_).Priority())
        {
            std::swap(proxy_, proxy_secondary_);
        }
        return false;
    }

    return true;
}

rai::Endpoint rai::Peer::Endpoint() const
{
    return rai::Endpoint(ip_, port_);
}

rai::TcpEndpoint rai::Peer::TcpEndpoint() const
{
    return rai::TcpEndpoint(ip_, port_);
}

rai::IP rai::Peer::KeyIp() const
{
    return ip_;
}

rai::IP rai::Peer::KeyProxy() const
{
    if (proxy_)
    {
        return (*proxy_).Endpoint().address().to_v4();
    }
    return rai::Peer::InvalidIp();
}

rai::IP rai::Peer::KeyProxySecondary() const
{
    if (proxy_secondary_)
    {
        return (*proxy_secondary_).Endpoint().address().to_v4();
    }
    return rai::Peer::InvalidIp();
}

rai::Ptree rai::Peer::Ptree() const
{
    rai::Ptree ptree;
    ptree.put("endpoint", ip_.to_string() + ":" + std::to_string(port_));
    ptree.put("account", account_.StringAccount());
    ptree.put("light_node", light_node_);
    ptree.put("weight", rep_weight_.StringDec());
    ptree.put("weight_in_rai", rep_weight_.StringBalance(rai::RAI) + " RAI");
    if (proxy_)
    {
        std::stringstream stream;
        stream << proxy_->Endpoint();
        ptree.put("proxy", stream.str());
        ptree.put("proxy_priority", std::to_string(proxy_->Priority()));
    }
    if (proxy_secondary_)
    {
        std::stringstream stream;
        stream << proxy_secondary_->Endpoint();
        ptree.put("proxy_secondary", stream.str());
        ptree.put("proxy_secondary_priority",
                  std::to_string(proxy_secondary_->Priority()));
    }
    ptree.put("timestamp", timestamp_);
    auto now = std::chrono::steady_clock::now();
    auto last_contact =
        std::chrono::duration_cast<std::chrono::seconds>(now - last_contact_)
            .count();
    ptree.put("last_contact", std::to_string(last_contact) + " seconds ago");
    auto last_attempt =
        std::chrono::duration_cast<std::chrono::seconds>(now - last_attempt_)
            .count();
    ptree.put("last_attempt", std::to_string(last_attempt) + " seconds ago");
    ptree.put("lost_acks", static_cast<uint32_t>(lost_acks_));
    ptree.put("last_hash", last_hash_.StringHex());

    return ptree;
}

rai::Route rai::Peer::Route() const
{
    boost::optional<rai::Proxy> proxy = GetProxy();
    if (proxy)
    {
        return rai::Route{true, Endpoint(), proxy->Endpoint()};
    }
    else
    {
        return rai::Route{false, Endpoint(), rai::Endpoint()};
    }
}

std::string rai::Peer::Json() const
{
    std::stringstream ostream;
    boost::property_tree::write_json(ostream, Ptree());
    return ostream.str();
}

void rai::Peer::SwitchProxy()
{
    std:swap(proxy_, proxy_secondary_);
    proxy_secondary_ = boost::none;
}

rai::IP rai::Peer::InvalidIp()
{
    return rai::IP(0xffffffff);
}

rai::Peers::Peers(rai::Node& node) : node_(node)
{
}

void rai::Peers::Insert(const rai::Peer& peer)
{
    std::lock_guard<std::mutex> lock(mutex_);

    bool update = false;
    do
    {
        boost::optional<rai::Peer> optional_peer(Query_(peer.account_));
        if (!optional_peer)
        {
            Insert_(peer);
            break;
        }

        rai::Peer& old = *optional_peer;
        if (!old.proxy_ && peer.proxy_)
        {
            return;
        }
        else if (old.proxy_ && !peer.proxy_)
        {
            update = true;
            break;
        }
        else if (!old.proxy_ && !peer.proxy_)
        {
            if (old.ip_ < peer.ip_)
            {
                return;
            }
            else if (old.ip_ > peer.ip_)
            {
                update = true;
                break;
            }
            else
            {
                if (old.port_ <= peer.port_)
                {
                    return;
                }
                else
                {
                    update = true;
                    break;
                }
            }
        }
        else
        {
            if ((old.ip_ == peer.ip_) && (old.port_ == peer.port_))
            {
                bool ret = old.SetProxy(*peer.proxy_);
                if (!ret)
                {
                    Modify_(old);
                    break;
                }
                return;
            }
            else
            {
                if (peer.proxy_->Priority() > old.proxy_->Priority())
                {
                    update = true;
                    break;
                }
                return;
            }
        }
    } while (0);

    if (update)
    {
        Remove_(peer.account_);
        Insert_(peer);
    }

    Purge_(peer.account_);
}

void rai::Peers::SetPeerWeight(const rai::Account& account,
                               const rai::Amount& amount)
{
    std::lock_guard<std::mutex> lock(mutex_);
    boost::optional<rai::Peer> old(Query_(account));
    if (!old)
    {
        return;
    }

    rai::Peer peer(*old);
    peer.rep_weight_ = amount;
    if (LowWeightPeer(*old) == LowWeightPeer(peer))
    {
        Modify_(peer);
        return;
    }

    Remove_((*old).account_);
    Insert_(peer);
}

std::vector<std::pair<rai::Account, rai::Amount>> rai::Peers::PeerWeights()
    const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<rai::Account, rai::Amount>> result;
    result.reserve(peers_.size() + peers_low_weight_.size());

    for (auto i = peers_.begin(), n = peers_.end(); i != n; ++i)
    {
        result.push_back(std::make_pair(i->account_, i->rep_weight_));
    }

    for (auto i = peers_low_weight_.begin(), n = peers_low_weight_.end();
         i != n; ++i)
    {
        result.push_back(std::make_pair(i->account_, i->rep_weight_));
    }

    return result;
}

boost::optional<rai::Peer> rai::Peers::Query(const rai::Account& account) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return Query_(account);
}

bool rai::Peers::CheckTimestamp(const rai::Account& account, uint64_t timestamp)
{
    uint64_t now  = rai::CurrentTimestamp();
    uint64_t diff = now > timestamp ? now - timestamp : timestamp - now;
    if (diff > rai::Peers::MAX_TIMESTAMP_DIFF)
    {
        return true;
    }

    boost::optional<rai::Peer> peer = Query(account);
    if (!peer)
    {
        return true;
    }

    if (timestamp <= peer->timestamp_)
    {
        return true;
    }

    return false;
}

void rai::Peers::Contact(const rai::Account& account, uint64_t timestamp,
                         uint8_t version, uint8_t version_min)
{
    std::lock_guard<std::mutex> lock(mutex_);
    boost::optional<rai::Peer> peer = Query_(account);
    if (!peer)
    {
        return;
    }

    peer->timestamp_    = timestamp;
    peer->last_contact_ = std::chrono::steady_clock::now();
    peer->version_      = version;
    peer->version_min_  = version_min;
    Modify_(*peer);
}

bool rai::Peers::InsertCookie(const rai::Cookie& cookie)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rai::Cookie cookie_l(cookie);
    NeedSyn_(cookie_l);
    if (!cookie_l.syn_direct_ && !cookie_l.syn_proxy_)
    {
        return true;
    }

    auto ret = cookies_.insert(cookie_l);
    return !ret.second;
}

void rai::Peers::IncreaseCookieAttempts(const rai::Endpoint& endpoint)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto existing(cookies_.find(endpoint));
    if (existing == cookies_.end())
    {
        return;
    }
    cookies_.modify(existing, [](rai::Cookie& cookie) {
        ++cookie.attempts_;
        cookie.cutoff_time_ =
            std::chrono::steady_clock::now() + rai::Peers::COOKIE_CUTOFF_TIME;
    });
}

boost::optional<rai::Cookie> rai::Peers::ValidateCookie(
    const rai::Endpoint& endpoint, const rai::Account& account,
    const rai::Signature& signature)
{
    std::lock_guard<std::mutex> lock(mutex_);
    boost::optional<rai::Cookie> result(boost::none);
    auto existing(cookies_.find(endpoint));
    if (existing == cookies_.end())
    {
        return result;
    }

    if (existing->account_ && *existing->account_ != account)
    {
        return result;
    }

    if (rai::ValidateMessage(account, existing->cookie_, signature))
    {
        return result;
    }

    result = *existing;
    (*result).account_ = account;
    cookies_.erase(existing);
    return result;
}

bool rai::Peers::ValidateKeepliveAck(const rai::Account& account,
                                     const rai::BlockHash& hash)
{
    std::lock_guard<std::mutex> lock(mutex_);
    boost::optional<rai::Peer> peer(Query_(account));
    if (!peer)
    {
        return true;
    }

    if (peer->last_hash_ != hash)
    {
        return true;
    }

    peer->lost_acks_ = 0;
    return Modify_(*peer);
}

void rai::Peers::SynCookie(const rai::Cookie& cookie)
{
    bool error = InsertCookie(cookie);
    if (!error)
    {
        node_.HandshakeRequest(cookie, boost::none);
        IncreaseCookieAttempts(cookie.endpoint_);
    }
}

void rai::Peers::SynCookies()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
    std::vector<rai::Endpoint> endpoints;

    auto it(cookies_.get<rai::CookieByCutoffTime>().begin());
    while (it != cookies_.get<rai::CookieByCutoffTime>().end())
    {
        if (it->cutoff_time_ > now)
        {
            break;
        }
        endpoints.push_back(it->endpoint_);
        ++it;
    }

    for (const auto& endpoint : endpoints)
    {
        auto it(cookies_.find(endpoint));
        if (it == cookies_.end())
        {
            continue;
        }

        uint32_t max_attempts = rai::Peers::MAX_COOKIE_ATTEMPTS;
        if (!it->learned_from_ || !it->syn_proxy_)
        {
            max_attempts /= 2;
        }
        if (it->attempts_ >= max_attempts)
        {
            cookies_.erase(endpoint);
            continue;
        }

        if ((it->attempts_ == rai::Peers::MAX_COOKIE_ATTEMPTS / 2)
            && it->learned_from_ && it->syn_proxy_)
        {
            cookies_.modify(it, [](rai::Cookie& cookie) {
                cookie.use_proxy_ = true;
                rai::random_pool.GenerateBlock(cookie.cookie_.bytes.data(),
                                               cookie.cookie_.bytes.size());
            });
        }

        if (it->use_proxy_)
        {
            node_.HandshakeRequest(*it, it->learned_from_);
        }
        else
        {
            node_.HandshakeRequest(*it, boost::none);
        }

        cookies_.modify(it, [now](rai::Cookie& cookie) {
            ++cookie.attempts_;
            cookie.cutoff_time_ = now + rai::Peers::COOKIE_CUTOFF_TIME;
        });
    }
}

void rai::Peers::Keeplive(size_t max_peers)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Keeplive_(peers_, max_peers);
    Keeplive_(peers_low_weight_, max_peers);
}

bool rai::Peers::Reachable(const rai::Endpoint& endpoint) const
{
    rai::IP ip = endpoint.address().to_v4();
    if (rai::IsReservedIp(ip))
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (Reachable_(peers_, endpoint))
    {
        return true;
    }

    if (Reachable_(peers_low_weight_, endpoint))
    {
        return true;
    }

    return false;
}

std::vector<rai::Peer> rai::Peers::List() const
{
    std::vector<rai::Peer> result;
    std::lock_guard<std::mutex> lock(mutex_);
    List_(peers_, result);
    List_(peers_low_weight_, result);
    return result;
}

std::vector<rai::Peer> rai::Peers::RandomPeers(size_t max) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return RandomPeers_(max);
}

boost::optional<rai::Peer> rai::Peers::RandomPeer(bool exclude_self) const
{
    boost::optional<rai::Peer> result(boost::none);
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t total_peers =
        static_cast<uint32_t>(peers_.size() + peers_low_weight_.size());
    if (total_peers == 0)
    {
        return result;
    }

    while (true)
    {
        auto index = rai::random_pool.GenerateWord32(0, total_peers - 1);
        if (index < peers_.size())
        {
            result = peers_.get<1>()[index];
        }
        else
        {
            result = peers_low_weight_.get<1>()[index - peers_.size()];
        }

        if (!exclude_self || result->account_ != node_.account_)
        {
            return result;
        }

        if (total_peers == 1)
        {
            return boost::none;
        }
    }
}

boost::optional<rai::Peer> rai::Peers::RandomFullNodePeer(bool exclude_self) const
{
    boost::optional<rai::Peer> result(boost::none);
    std::lock_guard<std::mutex> lock(mutex_);
    size_t size = full_node_index_.size();
    if (size == 0)
    {
        return result;
    }

    while (true)
    {
        auto index = rai::random_pool.GenerateWord32(0, size - 1);
        result = Query_(full_node_index_.get<1>()[index]);

        if (!exclude_self || result->account_ != node_.account_)
        {
            return result;
        }

        if (size == 1)
        {
            return boost::none;
        }
    }
}

void rai::Peers::Routes(const std::unordered_set<rai::Account>& filter,
                        bool include_low_weight,
                        std::vector<rai::Route>& result)
{
    std::lock_guard<std::mutex> lock(mutex_);
    size_t size = peers_.size();
    if (include_low_weight)
    {
        size += peers_low_weight_.size();
    }
    result.reserve(size);

    Routes_(peers_, filter, result);
    if (include_low_weight)
    {
        Routes_(peers_low_weight_, filter, result);
    }
}

size_t rai::Peers::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return peers_.size() + peers_low_weight_.size();
}

std::unordered_set<rai::Account> rai::Peers::Accounts(bool all) const
{
    std::unordered_set<rai::Account> result;

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto i = peers_.begin(), n = peers_.end(); i != n; ++i)
    {
        result.insert(i->account_);
    }

    if (all)
    {
        for (auto i = peers_low_weight_.begin(), n = peers_low_weight_.end();
             i != n; ++i)
        {
            result.insert(i->account_);
        }
    }

    return result;
}

bool rai::Peers::LowWeightPeer(const rai::Peer& peer)
{
    return peer.rep_weight_.Number() < (256 * rai::RAI);
}

boost::optional<rai::Peer> rai::Peers::Query_(const rai::Account& account) const
{
    boost::optional<rai::Peer> result(boost::none);

    auto existing(peers_.find(account));
    if (existing != peers_.end())
    {
        result = *existing;
    }
    else
    {
        auto existing(peers_low_weight_.find(account));
        if (existing != peers_low_weight_.end())
        {
            result = *existing;
        }
    }

    return result;
}

bool rai::Peers::Insert_(const rai::Peer& peer)
{
    UpdateFullNodeIndex_(peer);
    if (LowWeightPeer(peer))
    {
        auto ret = peers_low_weight_.insert(peer);
        return !ret.second;
    }
    else
    {
        auto ret = peers_.insert(peer);
        return !ret.second;
    }
}

void rai::Peers::Remove_(const rai::Account& account)
{
    RemoveFullNodeIndex_(account);
    peers_.erase(account);
    peers_low_weight_.erase(account);
}

bool rai::Peers::Modify_(const rai::Peer& peer)
{
    UpdateFullNodeIndex_(peer);
    bool ret = false;
    auto existing(peers_.find(peer.account_));
    if (existing != peers_.end())
    {
        ret = peers_.modify(existing, [&](rai::Peer& old){
            old = peer;
        });
    }
    else
    {
        auto existing(peers_low_weight_.find(peer.account_));
        if (existing != peers_low_weight_.end())
        {
            ret = peers_low_weight_.modify(existing, [&](rai::Peer& old){
                old = peer;
            });
        }
    }
    return !ret;
}

size_t rai::Peers::CountByIp_(const rai::IP& ip) const
{
    size_t count = 0;
    count += peers_.get<rai::PeerByIp>().count(ip);
    count += peers_low_weight_.get<rai::PeerByIp>().count(ip);
    return count;
}

size_t rai::Peers::CountByProxy_(const rai::IP& ip) const
{
    size_t count = 0;
    count += peers_.get<rai::PeerByProxy>().count(ip);
    count += peers_low_weight_.get<rai::PeerByProxy>().count(ip);
    count += peers_.get<rai::PeerByProxySecondary>().count(ip);
    count +=
        peers_low_weight_.get<rai::PeerByProxySecondary>().count(ip);
    return count;
}

void rai::Peers::NeedSyn_(rai::Cookie& cookie)
{
    cookie.syn_direct_ = false;
    cookie.syn_proxy_ = false;
    auto existing(cookies_.find(cookie.endpoint_));
    if (existing != cookies_.end())
    {
        return;
    }

    if (!cookie.account_)
    {
        cookie.syn_direct_ = true;
        return;
    }
    rai::Account target_account = *cookie.account_;

    boost::optional<rai::Peer> optional_peer = Query_(target_account);
    if (!optional_peer)
    {
        cookie.syn_direct_ = true;
        cookie.syn_proxy_ = true;
        return;
    }

    const rai::Peer& peer = *optional_peer;
    if (!peer.GetProxy())
    {
        if (cookie.endpoint_.address().to_v4() < peer.ip_)
        {
            cookie.syn_direct_ = true;
            return;
        }

        if (cookie.endpoint_.address().to_v4() == peer.ip_
            && cookie.endpoint_.port() < peer.port_)
        {
            cookie.syn_direct_ = true;
        }

        return;
    }
    cookie.syn_direct_ = true;

    if (!cookie.learned_from_)
    {
        return;
    }
    rai::Endpoint from = *cookie.learned_from_;

    if (CountByProxy_(from.address().to_v4())
        >= rai::Peers::MAX_PEERS_PER_PROXY)
    {
        return;
    }

    rai::Proxy proxy(from, node_.account_, target_account);
    if (proxy == *peer.proxy_)
    {
        return;
    }

    if (peer.Endpoint() != cookie.endpoint_)
    {
        cookie.syn_proxy_ = proxy.Priority() > peer.GetProxy()->Priority();
        return;
    }

    if (!peer.proxy_secondary_
        || peer.proxy_secondary_->Priority() < proxy.Priority())
    {
        cookie.syn_proxy_ = true;
    }
}

void rai::Peers::Keeplive_(rai::PeerContainer& peers, size_t max_peers)
{
    std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
    std::chrono::steady_clock::time_point cutoff(
        now - rai::Peers::PEER_CUTOFF_TIME);

    auto contact_begin = peers.get<rai::PeerByLastContact>().begin();
    auto contact_end = peers.get<rai::PeerByLastContact>().lower_bound(cutoff);
    peers.get<rai::PeerByLastContact>().erase(contact_begin, contact_end);

    std::chrono::steady_clock::time_point attempt(
        now - rai::Peers::PEER_ATTEMPT_TIME);
    auto attempt_begin = peers.get<rai::PeerByLastAttempt>().begin();
    auto attempt_end = peers.get<rai::PeerByLastAttempt>().upper_bound(attempt);
    std::vector<rai::Account> accounts;
    for (auto i = attempt_begin, n = attempt_end; i != n; ++i)
    {
        accounts.push_back(i->account_);
    }

    for (const auto& account : accounts)
    {
        auto it = peers.find(account);
        if (it == peers.end())
        {
            continue;
        }

        if (it->lost_acks_ >= rai::Peers::MAX_LOST_ACKS)
        {
            peers.erase(it);
            continue;
        }

        if (it->lost_acks_ >= rai::Peers::MAX_LOST_ACKS / 2)
        {
            peers.modify(it, [&](rai::Peer& peer) {
                peer.SwitchProxy();
            });
        }

        std::vector<rai::Peer> peer_vec = RandomPeers_(max_peers);
        node_.Keeplive(*it, peer_vec, [&](const rai::BlockHash& hash){
            peers.modify(it, [&](rai::Peer& peer) {
                peer.last_attempt_ = std::chrono::steady_clock::now();
                peer.last_hash_ = hash;
                peer.lost_acks_++;
            });
        });
    }
}

std::unordered_set<rai::Peer> rai::Peers::RandomSet_(
    const rai::PeerContainer& peers, size_t max_num) const
{
    std::unordered_set<rai::Peer> result;
    if (peers.size() <= max_num)
    {
        result.insert(peers.begin(), peers.end());
        return result;
    }

    size_t count = 2 * max_num;
    for (size_t i = 0; i < count && result.size() < max_num; ++i)
    {
        auto index = rai::random_pool.GenerateWord32(0, peers.size() - 1);
        result.insert(peers.get<1>()[index]);
    }

    auto contact_rbegin = peers.get<rai::PeerByLastContact>().rbegin();
    auto contact_rend = peers.get<rai::PeerByLastContact>().rend();
    for (auto i = contact_rbegin, n = contact_rend;
         i != n && result.size() < max_num; ++i)
    {
        result.insert(*i);
    }

    return result;
}

std::vector<rai::Peer> rai::Peers::RandomPeers_(size_t max) const
{
    std::vector<rai::Peer> result;
    size_t total = peers_.size() + peers_low_weight_.size();
    if (total <= 1 || max == 0)
    {
        return result;
    }

    size_t low = max * peers_low_weight_.size() / total;
    if (low == 0 && peers_low_weight_.size() > 0 && max > 1)
    {
        low = 1;
    }
    size_t normal = max - low;
    if (normal < max / 2)
    {
        normal = std::min(max / 2, peers_.size());
        low = max - normal;
    }

    std::unordered_set<rai::Peer> peers_normal = RandomSet_(peers_, normal);
    result.insert(result.end(), peers_normal.begin(), peers_normal.end());

    std::unordered_set<rai::Peer> peers_low =
        RandomSet_(peers_low_weight_, low);
    result.insert(result.end(), peers_low.begin(), peers_low.end());

    return result;
}

bool rai::Peers::Reachable_(const rai::PeerContainer& peers,
                            const rai::Endpoint& endpoint) const
{
    rai::IP ip = endpoint.address().to_v4();
    auto begin = peers.get<rai::PeerByIp>().lower_bound(ip);
    auto end = peers.get<rai::PeerByIp>().upper_bound(ip);
    for (auto i = begin; i != end; ++i)
    {
        if (i->port_ == endpoint.port() && !i->GetProxy())
        {
            return true;
        }
    }
    return false;
}

void rai::Peers::List_(const rai::PeerContainer& peers,
                       std::vector<rai::Peer>& result) const
{
    auto begin = peers.get<rai::PeerByWeight>().begin();
    auto end = peers.get<rai::PeerByWeight>().end();
    for (auto i = begin; i != end; ++i)
    {
        result.push_back(*i);
    }
}

void rai::Peers::UpdateFullNodeIndex_(const rai::Peer& peer)
{
    if (peer.light_node_)
    {
        full_node_index_.erase(peer.account_);
    }
    else
    {
        full_node_index_.insert(peer.account_);
    }
}

void rai::Peers::RemoveFullNodeIndex_(const rai::Account& account)
{
    full_node_index_.erase(account);
}

void rai::Peers::Routes_(const rai::PeerContainer& peers,
                         const std::unordered_set<rai::Account>& filter,
                         std::vector<rai::Route>& result) const
{
    auto begin = peers.get<rai::PeerByWeight>().begin();
    auto end = peers.get<rai::PeerByWeight>().end();
    for (auto i = begin; i != end; ++i)
    {
        if (filter.find(i->account_) != filter.end())
        {
            continue;
        }
        result.push_back(i->Route());
    }
}

void rai::Peers::Purge_(const rai::Account& account)
{
    while (true)
    {
        boost::optional<rai::Peer> peer(Query_(account));
        if (!peer)
        {
            return;
        }

        if (CountByIp_(peer->ip_) > rai::Peers::MAX_PEERS_PER_IP)
        {
            PurgeByIp_(peer->ip_);
            continue;
        }

        if (peer->proxy_)
        {
            rai::IP ip = peer->proxy_->Ip();
            if (CountByProxy_(ip) > rai::Peers::MAX_PEERS_PER_PROXY)
            {
                PurgeByProxy_(ip);
                continue;
            }
        }

        if (peer->proxy_secondary_)
        {
            rai::IP ip = peer->proxy_secondary_->Ip();
            if (CountByProxy_(ip) > rai::Peers::MAX_PEERS_PER_PROXY)
            {
                PurgeByProxy_(ip);
                continue;
            }
        }

        return;
    }
}

void rai::Peers::PurgeByIp_(const rai::IP& ip)
{
    boost::optional<rai::Peer> peer(boost::none);

    auto begin = peers_.get<rai::PeerByIp>().lower_bound(ip);
    auto end = peers_.get<rai::PeerByIp>().upper_bound(ip);
    for (auto i = begin; i != end; ++i)
    {
        if (!peer || *i < *peer)
        {
            peer = *i;
        }
    }

    auto begin_low = peers_low_weight_.get<rai::PeerByIp>().lower_bound(ip);
    auto end_low = peers_low_weight_.get<rai::PeerByIp>().upper_bound(ip);
    for (auto i = begin_low; i != end_low; ++i)
    {
        if (!peer || *i < *peer)
        {
            peer = *i;
        }
    }

    if (peer)
    {
        Remove_(peer->account_);
    }
}

void rai::Peers::PurgeByProxy_(const rai::IP& ip)
{
    boost::optional<rai::Peer> peer(boost::none);

    auto begin = peers_.get<rai::PeerByProxy>().lower_bound(ip);
    auto end = peers_.get<rai::PeerByProxy>().upper_bound(ip);
    for (auto i = begin; i != end; ++i)
    {
        if (!peer || *i < *peer)
        {
            peer = *i;
        }
    }

    auto begin_sec = peers_.get<rai::PeerByProxySecondary>().lower_bound(ip);
    auto end_sec = peers_.get<rai::PeerByProxySecondary>().upper_bound(ip);
    for (auto i = begin_sec; i != end_sec; ++i)
    {
        if (!peer || *i < *peer)
        {
            peer = *i;
        }
    }

    auto begin_low = peers_low_weight_.get<rai::PeerByProxy>().lower_bound(ip);
    auto end_low = peers_low_weight_.get<rai::PeerByProxy>().upper_bound(ip);
    for (auto i = begin_low; i != end_low; ++i)
    {
        if (!peer || *i < *peer)
        {
            peer = *i;
        }
    }

    auto begin_low_sec =
        peers_low_weight_.get<rai::PeerByProxySecondary>().lower_bound(ip);
    auto end_low_sec =
        peers_low_weight_.get<rai::PeerByProxySecondary>().upper_bound(ip);
    for (auto i = begin_low_sec; i != end_low_sec; ++i)
    {
        if (!peer || *i < *peer)
        {
            peer = *i;
        }
    }

    if (peer)
    {
        Remove_(peer->account_);
    }
}

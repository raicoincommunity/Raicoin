#pragma once
#include <chrono>
#include <unordered_set>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/optional.hpp>
#include <rai/common/numbers.hpp>
#include <rai/node/network.hpp>

namespace rai
{
class Cookie
{
public:
    Cookie(const rai::Endpoint&);
    Cookie(const rai::Endpoint&, const rai::Account&);
    Cookie(const rai::Endpoint&, const rai::Endpoint&, const rai::Account&);

    rai::Endpoint endpoint_;
    rai::uint256_union cookie_;
    boost::optional<rai::Endpoint> learned_from_;
    boost::optional<rai::Account> account_;
    std::chrono::steady_clock::time_point cutoff_time_;
    uint32_t attempts_;
    bool use_proxy_;
    bool syn_direct_;
    bool syn_proxy_;
};

class CookieByCutoffTime
{
};

typedef boost::multi_index_container<
    rai::Cookie,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<boost::multi_index::member<
            rai::Cookie, rai::Endpoint, &rai::Cookie::endpoint_>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::CookieByCutoffTime>,
            boost::multi_index::member<rai::Cookie,
                                       std::chrono::steady_clock::time_point,
                                       &rai::Cookie::cutoff_time_>>>>
    CookieContainer;
   
class Peer
{
public:
    Peer(const rai::Account&, const rai::IP&, uint16_t, uint8_t, uint8_t,
         const rai::Amount&);
    Peer(const rai::Account&, const rai::IP&, uint16_t, uint8_t, uint8_t,
         const rai::Amount&, const rai::Proxy&);
    Peer(const rai::Cookie&, const rai::Account&, uint8_t, uint8_t,
         const rai::Amount&);
    bool operator==(const rai::Peer&) const;
    bool operator<(const rai::Peer&) const;
    bool operator>(const rai::Peer&) const;
    boost::optional<rai::Proxy> GetProxy() const;
    bool SetProxy(const rai::Proxy&);
    rai::Endpoint Endpoint() const;
    rai::TcpEndpoint TcpEndpoint() const;
    rai::IP KeyIp() const;
    rai::IP KeyProxy() const;
    rai::IP KeyProxySecondary() const;
    rai::Ptree Ptree() const;
    rai::Route Route() const;
    std::string Json() const;
    void SwitchProxy();

    static rai::IP InvalidIp();

    rai::Account account_;
    rai::IP ip_; 
    uint16_t port_;
    uint8_t version_;
    uint8_t version_min_;
    uint8_t lost_acks_;
    bool light_node_;
    rai::Amount rep_weight_;
    boost::optional<rai::Proxy> proxy_;
    boost::optional<rai::Proxy> proxy_secondary_;
    uint64_t timestamp_;
    std::chrono::steady_clock::time_point last_contact_;
    std::chrono::steady_clock::time_point last_attempt_;
    rai::BlockHash last_hash_;
};

class PeerByIp
{
};

class PeerByWeight
{
};

class PeerByProxy
{
};

class PeerByProxySecondary
{
};

class PeerByLastContact
{
};

class PeerByLastAttempt
{
};

typedef boost::multi_index_container<
    rai::Peer,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<boost::multi_index::member<
            rai::Peer, rai::Account, &rai::Peer::account_>>,
        boost::multi_index::random_access<>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::PeerByWeight>,
            boost::multi_index::member<rai::Peer, rai::Amount,
                                       &rai::Peer::rep_weight_>,
            std::greater<rai::Amount>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::PeerByIp>,
            boost::multi_index::const_mem_fun<rai::Peer, rai::IP,
                                              &rai::Peer::KeyIp>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::PeerByProxy>,
            boost::multi_index::const_mem_fun<rai::Peer, rai::IP,
                                              &rai::Peer::KeyProxy>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::PeerByProxySecondary>,
            boost::multi_index::const_mem_fun<rai::Peer, rai::IP,
                                              &rai::Peer::KeyProxySecondary>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::PeerByLastContact>,
            boost::multi_index::member<rai::Peer,
                                       std::chrono::steady_clock::time_point,
                                       &rai::Peer::last_contact_>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<rai::PeerByLastAttempt>,
            boost::multi_index::member<rai::Peer,
                                       std::chrono::steady_clock::time_point,
                                       &rai::Peer::last_attempt_>>>>
    PeerContainer;

typedef boost::multi_index_container<
    rai::Account, boost::multi_index::indexed_by<
                      boost::multi_index::hashed_unique<
                          boost::multi_index::identity<rai::Account>>,
                      boost::multi_index::random_access<>>>
    PeerRandomIndex;

class Node;
class Peers
{
public:
    Peers(rai::Node&);
    void Insert(const rai::Peer&);
    void SetPeerWeight(const rai::Account&, const rai::Amount&);
    std::vector<std::pair<rai::Account, rai::Amount>> PeerWeights() const;
    boost::optional<rai::Peer> Query(const rai::Account&) const;
    bool CheckTimestamp(const rai::Account&, uint64_t);
    void Contact(const rai::Account&, uint64_t, uint8_t, uint8_t);
    bool InsertCookie(const rai::Cookie&);
    void IncreaseCookieAttempts(const rai::Endpoint&);
    boost::optional<rai::Cookie> ValidateCookie(const rai::Endpoint&,
                                                const rai::Account&,
                                                const rai::Signature&);
    bool ValidateKeepliveAck(const rai::Account&, const rai::BlockHash&);
    void SynCookie(const rai::Cookie&);
    void SynCookies();
    void Keeplive(size_t);
    bool Reachable(const rai::Endpoint&) const;
    std::vector<rai::Peer> List() const;
    std::vector<rai::Peer> RandomPeers(size_t) const;
    boost::optional<rai::Peer> RandomPeer(bool = true) const;
    boost::optional<rai::Peer> RandomFullNodePeer(bool = true) const;
    void Routes(const std::unordered_set<rai::Account>&, bool,
                std::vector<rai::Route>&);
    void Routes(const std::vector<rai::Account>&, std::vector<rai::Route>&);
    size_t Size() const;
    size_t FullPeerSize() const;
    std::unordered_set<rai::Account> Accounts(bool) const;

    static bool LowWeightPeer(const rai::Peer&);

    static size_t constexpr MAX_PEERS_PER_IP = 1;
    static size_t constexpr MAX_PEERS_PER_PROXY = 2;
    static std::chrono::seconds constexpr COOKIE_CUTOFF_TIME =
        std::chrono::seconds(3);
    static uint32_t constexpr MAX_COOKIE_ATTEMPTS = 4;
    static uint64_t constexpr MAX_TIMESTAMP_DIFF = 300;
    static std::chrono::seconds constexpr KEEPLIVE_PERIOD =
        std::chrono::seconds(1);
    static std::chrono::seconds constexpr PEER_CUTOFF_TIME =
        std::chrono::seconds(300);
    static std::chrono::seconds constexpr PEER_ATTEMPT_TIME =
        std::chrono::seconds(60);
    static uint8_t constexpr MAX_LOST_ACKS = 5;

private:
    boost::optional<rai::Peer> Query_(const rai::Account&) const;
    bool Insert_(const rai::Peer&);
    void Remove_(const rai::Account&);
    bool Modify_(const rai::Peer&);
    size_t CountByIp_(const rai::IP&) const;
    size_t CountByProxy_(const rai::IP&) const;
    void NeedSyn_(rai::Cookie&);
    void Keeplive_(rai::PeerContainer&, size_t);
    std::unordered_set<rai::Peer> RandomSet_(const rai::PeerContainer&,
                                             size_t) const;
    std::vector<rai::Peer> RandomPeers_(size_t) const;
    bool Reachable_(const rai::PeerContainer&, const rai::Endpoint&) const;
    void List_(const rai::PeerContainer&, std::vector<rai::Peer>&) const;
    void UpdateFullNodeIndex_(const rai::Peer&);
    void RemoveFullNodeIndex_(const rai::Account&);
    void Routes_(const rai::PeerContainer&,
                 const std::unordered_set<rai::Account>&,
                 std::vector<rai::Route>&) const;
    void Purge_(const rai::Account&);
    void PurgeByIp_(const rai::IP&);
    void PurgeByProxy_(const rai::IP&);

    rai::Node& node_;
    mutable std::mutex mutex_;
    rai::PeerContainer peers_;
    rai::PeerContainer peers_low_weight_;
    rai::CookieContainer cookies_;
    rai::PeerRandomIndex full_node_index_;
};

} // namespace rai

namespace std
{
template <>
struct hash<rai::Peer>
{
    size_t operator()(const rai::Peer& data) const
    {
        return *reinterpret_cast<const size_t*>(data.account_.bytes.data());
    }
};
}  // namespace boost

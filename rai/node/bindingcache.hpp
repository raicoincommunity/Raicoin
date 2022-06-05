#pragma once

#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/optional.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/chain.hpp>
#include <rai/common/blocks.hpp>

namespace rai
{

class BindingCache
{
public:
    
    rai::Account account_;
    rai::Chain chain_;
    rai::SignerAddress signer_;
};

typedef boost::multi_index_container<
    rai::BindingCache,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::composite_key<
            rai::BindingCache,
            boost::multi_index::member<rai::BindingCache, rai::Account,
                                       &rai::BindingCache::account_>,
            boost::multi_index::member<rai::BindingCache, rai::Chain,
                                       &rai::BindingCache::chain_>>>>>
    BindingCacheContainer;

class BindingCaches
{
public:
    void Insert(const rai::Account&, rai::Chain, const rai::SignerAddress&);
    void Remove(const rai::Account&, rai::Chain);
    boost::optional<rai::SignerAddress> Query(const rai::Account&,
                                              rai::Chain) const;
    void Update(const std::shared_ptr<rai::Block>&, bool);

private:
    void Insert_(const rai::BindingCache&);
    void Remove_(const rai::Account&, rai::Chain);

    mutable std::mutex mutex_; 
    rai::BindingCacheContainer caches_;
};

}  // namespace rai
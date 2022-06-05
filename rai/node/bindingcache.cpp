#include <rai/node/bindingcache.hpp>

void rai::BindingCaches::Insert(const rai::Account& account, rai::Chain chain,
                                const rai::SignerAddress& signer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Insert_(BindingCache{account, chain, signer});
}

void rai::BindingCaches::Remove(const rai::Account& account, rai::Chain chain)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Remove_(account, chain);
}

boost::optional<rai::SignerAddress> rai::BindingCaches::Query(
    const rai::Account& account, rai::Chain chain) const
{
    boost::optional<rai::SignerAddress> result(boost::none);
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = caches_.find(boost::make_tuple(account, chain));
    if (it != caches_.end())
    {
        result = it->signer_;
    }
    return result;
}

void rai::BindingCaches::Update(const std::shared_ptr<rai::Block>& block,
                                bool append)
{
    if (block->Opcode() != rai::BlockOpcode::BIND || !block->HasChain()) return;

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = caches_.find(boost::make_tuple(block->Account(),
                                             block->Chain()));
    if (it == caches_.end())
    {
        return;
    }
    if (append)
    {
        Insert_(
            rai::BindingCache{block->Account(), block->Chain(), block->Link()});
    }
    else
    {
        Remove_(block->Account(), block->Chain());
    }
}

void rai::BindingCaches::Insert_(const rai::BindingCache& cache)
{
    auto it = caches_.find(boost::make_tuple(cache.account_, cache.chain_));
    if (it == caches_.end())
    {
        caches_.insert(cache);
    }
    else
    {
        caches_.modify(it, [&](rai::BindingCache& data) {
            data.signer_ = cache.signer_;
        });
    }
}

void rai::BindingCaches::Remove_(const rai::Account& account, rai::Chain chain)
{
    auto it = caches_.find(boost::make_tuple(account, chain));
    if (it != caches_.end())
    {
        caches_.erase(it);
    }
}
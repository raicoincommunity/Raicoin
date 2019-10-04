#pragma once
#include <unordered_map>
#include <rai/common/blocks.hpp>
#include <rai/secure/util.hpp>
#include <rai/secure/store.hpp>

namespace rai
{
class Ledger;

class RepWeightOpration
{
public:
    RepWeightOpration(bool, const rai::Account&, const rai::Amount&);

private:
    friend class rai::Ledger;

    bool add_;
    rai::Account representative_;
    rai::Amount weight_;
};

class Transaction
{
public:
    Transaction(rai::ErrorCode&, rai::Ledger&, bool);
    Transaction(const rai::Transaction&) = delete;
    ~Transaction();
    rai::Transaction& operator=(const rai::Transaction&) = delete;
    void Abort();

private:
    friend class rai::Ledger;

    rai::Ledger& ledger_;
    bool write_;
    bool aborted_;
    rai::MdbTransaction mdb_transaction_;
    std::vector<rai::RepWeightOpration> rep_weight_operations_;

};

class Iterator
{
public:
    Iterator(rai::StoreIterator&&);
    Iterator(const rai::Iterator&) = delete;
    Iterator(rai::Iterator&&);

    rai::Iterator& operator++();
    bool operator==(const rai::Iterator&) const;
    bool operator!=(const rai::Iterator&) const;

private:
    friend class rai::Ledger;

    rai::StoreIterator store_it_;

};

class AccountInfo
{
public:
    AccountInfo();
    AccountInfo(rai::BlockType, const rai::BlockHash&); // first block
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);
    bool Valid() const;

    rai::BlockType type_;
    uint16_t forks_;
    uint64_t head_height_;
    uint64_t tail_height_;
    uint64_t confirmed_height_;
    rai::BlockHash head_;
    rai::BlockHash tail_;
};

class ReceivableInfo
{
public:
    ReceivableInfo() = default;
    ReceivableInfo(const rai::Account&, const rai::Amount&);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);

    rai::Account source_;
    rai::Amount amount_;
};

class RewardableInfo
{
public:
    RewardableInfo() = default;
    RewardableInfo(const rai::Account&, const rai::Amount&, uint64_t);
    void Serialize(rai::Stream&) const;
    bool Deserialize(rai::Stream&);
    
    rai::Account source_;
    rai::Amount amount_;
    uint64_t valid_timestamp_;
};

class Ledger
{
public:
    Ledger(rai::ErrorCode&, rai::Store&);

    bool AccountInfoPut(rai::Transaction&, const rai::Account&,
                        const rai::AccountInfo&);
    bool AccountInfoGet(rai::Transaction&, const rai::Account&,
                        rai::AccountInfo&) const;
    bool AccountInfoGet(const rai::Iterator&, rai::Account&,
                        rai::AccountInfo&) const;
    bool AccountInfoDel(rai::Transaction&, const rai::Account&);
    rai::Iterator AccountInfoBegin(rai::Transaction&);
    rai::Iterator AccountInfoEnd(rai::Transaction&);
    bool AccountCount(rai::Transaction&, size_t&) const;
    bool BlockPut(rai::Transaction&, const rai::BlockHash&, const rai::Block&);
    bool BlockPut(rai::Transaction&, const rai::BlockHash&, const rai::Block&,
                  const rai::BlockHash&);
    bool BlockGet(rai::Transaction&, const rai::BlockHash&,
                  std::shared_ptr<rai::Block>&) const;
    bool BlockGet(rai::Transaction&, const rai::BlockHash&,
                  std::shared_ptr<rai::Block>&, rai::BlockHash&) const;
    bool BlockGet(rai::Transaction&, const rai::Account&, uint64_t,
                  std::shared_ptr<rai::Block>&) const;
    bool BlockDel(rai::Transaction&, const rai::BlockHash&);
    bool BlockCount(rai::Transaction&, size_t&) const;
    bool BlockExists(rai::Transaction&, const rai::BlockHash&) const;
    bool BlockSuccessorSet(rai::Transaction&, const rai::BlockHash&,
                           const rai::BlockHash&);
    bool BlockSuccessorGet(rai::Transaction&, const rai::BlockHash&,
                           rai::BlockHash&) const;
    bool Empty(rai::Transaction&) const;
    bool ForkPut(rai::Transaction&, const rai::Account&, uint64_t,
                 const rai::Block&, const rai::Block&);
    bool ForkGet(rai::Transaction&, const rai::Account&, uint64_t,
                 std::shared_ptr<rai::Block>&,
                 std::shared_ptr<rai::Block>&) const;
    bool ForkGet(const rai::Iterator&, std::shared_ptr<rai::Block>&,
                 std::shared_ptr<rai::Block>&) const;
    bool ForkDel(rai::Transaction&, const rai::Account&, uint64_t);
    bool ForkExists(rai::Transaction&, const rai::Account&, uint64_t) const;
    rai::Iterator ForkLowerBound(rai::Transaction&, const rai::Account&);
    rai::Iterator ForkUpperBound(rai::Transaction&, const rai::Account&);
    bool ReceivableInfoPut(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&, const rai::ReceivableInfo&);
    bool ReceivableInfoGet(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&, rai::ReceivableInfo&) const;
    bool ReceivableInfoDel(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&);
    bool ReceivableInfoCount(rai::Transaction&, size_t&) const;
    bool RewardableInfoPut(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&, const rai::RewardableInfo&);
    bool RewardableInfoGet(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&, rai::RewardableInfo&) const;
    bool RewardableInfoDel(rai::Transaction&, const rai::Account&,
                           const rai::BlockHash&);
    bool RollbackBlockPut(rai::Transaction&, const rai::BlockHash&,
                          const rai::Block&);
    bool RollbackBlockGet(rai::Transaction&, const rai::BlockHash&,
                          std::shared_ptr<rai::Block>&) const;

    void RepWeightAdd(rai::Transaction&, const rai::Account&,
                      const rai::Amount&);
    void RepWeightSub(rai::Transaction&, const rai::Account&,
                      const rai::Amount&);
    bool RepWeightGet(const rai::Account&, rai::Amount&) const;
    void RepWeightsGet(rai::Amount&,
                       std::unordered_map<rai::Account, rai::Amount>&) const;

private:
    friend class rai::Transaction;

    bool BlockIndexPut_(rai::Transaction&, const rai::Account&, uint64_t,
                        const rai::BlockHash&);
    bool BlockIndexGet_(rai::Transaction&, const rai::Account&, uint64_t,
                        rai::BlockHash&) const;
    bool BlockIndexDel_(rai::Transaction&, const rai::Account&, uint64_t);
    void RepWeightsCommit_(const std::vector<rai::RepWeightOpration>&);
    rai::ErrorCode InitRepWeights_(rai::Transaction&);

    static uint32_t constexpr BLOCKS_PER_INDEX = 8;

    rai::Store& store_;
    mutable std::mutex rep_weights_mutex_;
    rai::Amount total_rep_weight_;
    std::unordered_map<rai::Account, rai::Amount> rep_weights_;

};
}  // namespace rai
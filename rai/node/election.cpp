#include <algorithm>
#include <rai/node/election.hpp>
#include <rai/node/node.hpp>


std::chrono::seconds constexpr rai::Elections::FORK_ELECTION_DELAY;
std::chrono::seconds constexpr rai::Elections::FORK_ELECTION_INTERVAL;
std::chrono::seconds constexpr rai::Elections::NON_FORK_ELECTION_DELAY;
std::chrono::seconds constexpr rai::Elections::NON_FORK_ELECTION_INTERVAL;

rai::Vote::Vote() : timestamp_(0), signature_(0), hash_(0)
{
}

rai::Vote::Vote(uint64_t timestamp, const rai::Signature& signature,
                const rai::BlockHash& hash)
    : timestamp_(timestamp), signature_(signature), hash_(hash)
{
}

rai::RepVoteInfo::RepVoteInfo()
    : conflict_found_(false), weight_(0), last_vote_()
{
}

rai::RepVoteInfo::RepVoteInfo(bool conflict_found, const rai::Amount& weight,
                              const rai::Vote& vote)
    : conflict_found_(conflict_found), weight_(weight), last_vote_(vote)
{
}

rai::Election::Election()
    : account_(0),
      height_(rai::Block::INVALID_HEIGHT),
      fork_found_(false),
      rounds_(0),
      rounds_fork_(0),
      wins_(0),
      confirms_(0),
      winner_(0),
      wakeup_(std::chrono::steady_clock::now()
              + rai::Elections::NON_FORK_ELECTION_DELAY)
{
}

void rai::Election::AddBlock(const std::shared_ptr<rai::Block>& block)
{
    auto it = blocks_.find(block->Hash());
    if (it == blocks_.end())
    {
        blocks_.insert({block->Hash(), rai::BlockReference{1, block}});
        if (!fork_found_ && blocks_.size() > 1)
        {
            fork_found_ = true;
            wakeup_ = std::chrono::steady_clock::now()
                      + rai::Elections::FORK_ELECTION_DELAY;
        }
    }
    else
    {
        it->second.refs_ += 1;
    }
}

void rai::Election::DelBlock(const rai::BlockHash& hash)
{
    auto it = blocks_.find(hash);
    if (it == blocks_.end())
    {
        return;
    }

    if (it->second.refs_ <= 1)
    {
        if (blocks_.size() == 1)
        {
            assert(0);
            return;
        }
        blocks_.erase(it);
        return;
    }
    it->second.refs_ -= 1;
}

bool rai::Election::ForkFound() const
{
    return fork_found_;
}

rai::ElectionStatus::ElectionStatus()
    : error_(false), win_(false), confirm_(false), block_(nullptr)
{
}

rai::Elections::Elections(rai::Node& node)
    : node_(node), stopped_(false), thread_([this]() { this->Run(); })

{
}

rai::Elections::~Elections()
{
    Stop();
}

void rai::Elections::Add(const std::vector<std::shared_ptr<rai::Block>>& blocks)
{
    if (blocks.empty())
    {
        return;
    }
    rai::Account account = blocks[0]->Account();
    uint64_t height = blocks[0]->Height();
    for (const auto& i : blocks)
    {
        if (i->Account() != account || i->Height() != height)
        {
            return;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = elections_.find(account);
    if (it != elections_.end())
    {
        if (it->height_ < height)
        {
            return;
        }
        else if (it->height_ > height)
        {
            elections_.erase(it);
        }
        else
        {
            for (const auto& i : blocks)
            {
                AddBlock_(*it, i);
            }
            return;
        }
    }

    rai::Election election;
    election.account_ = account;
    election.height_ = height;
    for (const auto& i : blocks)
    {
        election.AddBlock(i);
    }
    elections_.insert(election);
    if (!election.ForkFound())
    {
        node_.RequestConfirms(blocks[0], std::unordered_set<rai::Account>());
    }
}

void rai::Elections::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);

    while (!stopped_)
    {
        if (elections_.empty())
        {
            condition_.wait(lock);
            continue;
        }

        auto it = elections_.get<1>().begin();
        if (it->wakeup_ <= std::chrono::steady_clock::now())
        {
            ProcessElection(*it);
            lock.unlock();
            lock.lock();
        }
        else
        {
            condition_.wait_until(lock, it->wakeup_);
        }
    }
}

void rai::Elections::Stop()
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

// lock acquired in Run()
void rai::Elections::ProcessElection(const rai::Election& election)
{
    rai::RepWeights reps;
    node_.RepWeights(reps);
    rai::ElectionStatus status = Tally_(election, reps.total_, reps.weights_);
    if (status.error_)
    {
        // stat
        elections_.erase(election.account_);
        return;
    }

    if (!election.ForkFound())
    {
        if (status.confirm_)
        {
            node_.ForceConfirmBlock(status.block_);
            elections_.erase(election.account_);
            return;
        }
        RequestConfirms_(election);
        ModifyRounds_(election, election.rounds_ + 1);
        ModifyWakeup_(election, NextWakeup_(election));
        return;
    }

    // fork found
    if (election.rounds_fork_ == 0)
    {
        BroadcastConfirms_(election);
    }

    if (status.win_)
    {
        if (status.block_->Hash() == election.winner_)
        {
            ModifyWins_(election, election.wins_ + 1);
            ModifyConfirms_(election,
                            status.confirm_ ? election.confirms_ + 1 : 0);
        }
        else
        {
            ModifyWins_(election, 1);
            ModifyConfirms_(election, status.confirm_ ? 1 : 0);
            ModifyWinner_(election, status.block_->Hash());
        }
    }
    else
    {
        ModifyWins_(election, 0);
        ModifyConfirms_(election, 0);
    }
    ModifyRounds_(election, election.rounds_ + 1);
    ModifyRoundsFork_(election, election.rounds_fork_ + 1);

    if (election.confirms_ >= rai::FORK_ELECTION_ROUNDS_THRESHOLD)
    {
        node_.ForceConfirmBlock(status.block_);
        elections_.erase(election.account_);
        return;
    }
    else if (election.wins_ == rai::FORK_ELECTION_ROUNDS_THRESHOLD)
    {
        node_.ForceAppendBlock(status.block_);
    }

    if (node_.IsQualifiedRepresentative())
    {
        node_.BroadcastConfirm(election.account_, election.height_);
    }
    if (election.rounds_fork_ > rai::FORK_ELECTION_ROUNDS_THRESHOLD)
    {
        RequestConfirms_(election);
    }

    ModifyWakeup_(election, NextWakeup_(election));
}

void rai::Elections::ProcessConfirm(const rai::Account& representative,
                                    uint64_t timestamp,
                                    const rai::Signature& signature,
                                    const std::shared_ptr<rai::Block>& block,
                                    const rai::Amount& weight)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = elections_.find(block->Account());
    if (it == elections_.end() || it->height_ != block->Height())
    {
        return;
    }
    const rai::Election& election = *it;
    rai::Vote vote(timestamp, signature, block->Hash());

    auto it_info = election.votes_.find(representative);
    if (it_info == election.votes_.end())
    {
        rai::RepVoteInfo info(false, weight, vote);
        AddRepVoteInfo_(election, representative, info);
        AddBlock_(election, block);

        if (election.rounds_fork_ > 0 && weight >= rai::QUALIFIED_REP_WEIGHT)
        {
            node_.BroadcastConfirm(representative, timestamp, signature, block);
        }
        return;
    }

    if (it_info->second.conflict_found_)
    {
        return;
    }
    const rai::Vote& last_vote = it_info->second.last_vote_;

    if (CheckConflict_(last_vote, vote))
    {
        rai::RepVoteInfo info(true, weight, last_vote);
        AddRepVoteInfo_(election, representative, info);
        AddConflict_(election, representative, vote);
        AddBlock_(election, block);
        if (election.rounds_fork_ > 0 && weight >= rai::QUALIFIED_REP_WEIGHT)
        {
            std::shared_ptr<rai::Block> last_block(nullptr);
            bool error = GetBlock_(election, last_vote.hash_, last_block);
            if (error)
            {
                assert(0);
                return;
            }
            node_.BroadcastConflict(representative, last_vote.timestamp_,
                                    timestamp, last_vote.signature_, signature,
                                    last_block, block);
        }
        return;
    }

    if (last_vote.timestamp_ >= timestamp)
    {
        return;
    }

    DelBlock_(election, last_vote.hash_);
    rai::RepVoteInfo info(false, weight, vote);
    AddRepVoteInfo_(election, representative, info);
    AddBlock_(election, block);

    if (election.rounds_fork_ > 0 && weight >= rai::QUALIFIED_REP_WEIGHT)
    {
        node_.BroadcastConfirm(representative, timestamp, signature, block);
    }
}

void rai::Elections::ProcessConflict(
    const rai::Account& representative, uint64_t timestamp_first,
    uint64_t timestamp_second, const rai::Signature& signature_first,
    const rai::Signature& signature_second,
    const std::shared_ptr<rai::Block>& block_first,
    const std::shared_ptr<rai::Block>& block_second, const rai::Amount& weight)
{
    if (block_first->Account() != block_second->Account()
        || block_first->Height() != block_second->Height())
    {
        return;
    }
    rai::Vote vote_first(timestamp_first, signature_first, block_first->Hash());
    rai::Vote vote_second(timestamp_second, signature_second,
                          block_second->Hash());
    if (!CheckConflict_(vote_first, vote_second))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = elections_.find(block_first->Account());
    if (it == elections_.end() || it->height_ != block_first->Height())
    {
        return;
    }
    const rai::Election& election = *it;

    auto it_info = election.votes_.find(representative);
    if (it_info != election.votes_.end())
    {
        if (it_info->second.conflict_found_)
        {
            return;
        }
        DelBlock_(election, it_info->second.last_vote_.hash_);
    }

    rai::RepVoteInfo info(true, weight, vote_first);
    AddRepVoteInfo_(election, representative, info);
    AddBlock_(election, block_first);
    AddConflict_(election, representative, vote_second);
    AddBlock_(election, block_second);

    if (election.rounds_fork_ > 0 && weight >= rai::QUALIFIED_REP_WEIGHT)
    {
        node_.BroadcastConflict(representative, timestamp_first,
                                timestamp_second, signature_first,
                                signature_second, block_first, block_second);
    }
}

void rai::Elections::AddBlock_(const rai::Election& election,
                               const std::shared_ptr<rai::Block>& block)
{
    auto it = elections_.find(election.account_);
    if (it != elections_.end())
    {
        elections_.modify(
            it, [&block](rai::Election& data) { data.AddBlock(block); });
    }
}

void rai::Elections::DelBlock_(const rai::Election& election,
                               const rai::BlockHash& hash)
{
    auto it = elections_.find(election.account_);
    if (it != elections_.end())
    {
        elections_.modify(
            it, [&hash](rai::Election& data) { data.DelBlock(hash); });
    }
}

bool rai::Elections::GetBlock_(const rai::Election& election,
                               const rai::BlockHash& hash,
                               std::shared_ptr<rai::Block>& block) const
{
    auto it = elections_.find(election.account_);
    if (it == elections_.end())
    {
        return true;
    }

    auto it_block = it->blocks_.find(hash);
    if (it_block == it->blocks_.end())
    {
        return true;
    }

    block = it_block->second.block_;
    return false;
}

void rai::Elections::AddConflict_(const rai::Election& election,
                                  const rai::Account& representative,
                                  const rai::Vote& vote)
{
    auto it = elections_.find(election.account_);
    if (it != elections_.end())
    {
        elections_.modify(it, [&](rai::Election& data) {
            data.conflicts_[representative] = vote;
        });
    }
}

bool rai::Elections::GetConflict_(const rai::Election& election,
                                  const rai::Account& representative,
                                  rai::Vote& vote) const
{
    auto it = elections_.find(election.account_);
    if (it == elections_.end())
    {
        return true;
    }

    auto it_conflict = it->conflicts_.find(representative);
    if (it_conflict == it->conflicts_.end())
    {
        return true;
    }

    vote = it_conflict->second;
    return false;
}

void rai::Elections::AddRepVoteInfo_(const rai::Election& election,
                                     const rai::Account& representative,
                                     const rai::RepVoteInfo& rep_vote_info)
{
    auto it = elections_.find(election.account_);
    if (it != elections_.end())
    {
        elections_.modify(it, [&](rai::Election& data) {
            data.votes_[representative] = rep_vote_info;
        });
    }
}

void rai::Elections::ModifyRounds_(const rai::Election& election,
                                   uint32_t rounds)
{
    auto it = elections_.find(election.account_);
    if (it != elections_.end())
    {
        elections_.modify(
            it, [rounds](rai::Election& data) { data.rounds_ = rounds; });
    }
}

void rai::Elections::ModifyRoundsFork_(const rai::Election& election,
                                       uint32_t rounds_fork)
{
    auto it = elections_.find(election.account_);
    if (it != elections_.end())
    {
        elections_.modify(it, [rounds_fork](rai::Election& data) {
            data.rounds_fork_ = rounds_fork;
        });
    }
}

void rai::Elections::ModifyWins_(const rai::Election& election, uint32_t wins)
{
    auto it = elections_.find(election.account_);
    if (it != elections_.end())
    {
        elections_.modify(it,
                          [wins](rai::Election& data) { data.wins_ = wins; });
    }
}

void rai::Elections::ModifyConfirms_(const rai::Election& election,
                                     uint32_t confirms)
{
    auto it = elections_.find(election.account_);
    if (it != elections_.end())
    {
        elections_.modify(
            it, [confirms](rai::Election& data) { data.confirms_ = confirms; });
    }
}

void rai::Elections::ModifyWinner_(const rai::Election& election,
                                   const rai::BlockHash& winner)
{
    auto it = elections_.find(election.account_);
    if (it != elections_.end())
    {
        elections_.modify(
            it, [&winner](rai::Election& data) { data.winner_ = winner; });
    }
}

void rai::Elections::ModifyWakeup_(
    const rai::Election& election,
    const std::chrono::steady_clock::time_point& wakeup)
{
    auto it = elections_.find(election.account_);
    if (it != elections_.end())
    {
        elections_.modify(
            it, [&wakeup](rai::Election& data) { data.wakeup_ = wakeup; });
    }
}

bool rai::Elections::CheckConflict_(const rai::Vote& first,
                                    const rai::Vote& second) const
{
    if (first.timestamp_ == second.timestamp_ && first.hash_ == second.hash_)
    {
        return false;
    }

    uint64_t diff = first.timestamp_ > second.timestamp_
                        ? first.timestamp_ - second.timestamp_
                        : second.timestamp_ - first.timestamp_;
    if (diff < rai::MIN_CONFIRM_INTERVAL)
    {
        return true;
    }

    return false;
}

rai::ElectionStatus rai::Elections::Tally_(
    const rai::Election& election, const rai::Amount& rep_total,
    const std::unordered_map<rai::Account, rai::Amount>& rep_weights) const
{
    rai::ElectionStatus result;
    rai::Amount voting_total(0);
    rai::Amount conflict_total(0);
    std::unordered_map<rai::BlockHash, rai::Amount> candidates;

    for (const auto& vote : election.votes_)
    {
        auto it = rep_weights.find(vote.first);
        if (it == rep_weights.end())
        {
            continue;
        }
        if (vote.second.conflict_found_)
        {
            conflict_total += it->second;
            continue;
        }
        candidates[vote.second.last_vote_.hash_] += it->second;
        voting_total += it->second;
    }

    if (candidates.empty())
    {
        return result;
    }

    if (voting_total + conflict_total > rep_total)
    {
        assert(0);
        result.error_ = true;
        return result;
    }
    rai::Amount not_voting = rep_total - voting_total - conflict_total;

    std::vector<std::pair<rai::BlockHash, rai::Amount>> vec(candidates.begin(),
                                                            candidates.end());
    std::sort(vec.begin(), vec.end(),
              [](const std::pair<rai::BlockHash, rai::Amount>& lhs,
                 const std::pair<rai::BlockHash, rai::Amount>& rhs) {
                  if (lhs.second != rhs.second)
                  {
                      return lhs.second > rhs.second;
                  }
                  return lhs.first > rhs.first;
              });

    rai::uint256_t first_s(vec[0].second.Number());
    rai::uint256_t second_s(0);
    if (vec.size() > 1)
    {
        second_s = vec[1].second.Number();
    }
    rai::uint256_t total_s(rep_total.Number());
    result.win_ = first_s > second_s + not_voting.Number();
    result.confirm_ = first_s * 100 > total_s * rai::CONFIRM_WEIGHT_PERCENTAGE;

    auto it = election.blocks_.find(vec[0].first);
    if (it == election.blocks_.end())
    {
        assert(0);
        result.error_ = true;
        return result;
    }
    result.block_ = it->second.block_;
    return result;
}

void rai::Elections::RequestConfirms_(const rai::Election& election)
{
    auto it = election.blocks_.begin();
    if (it == election.blocks_.end())
    {
        assert(0);
        return;
    }

    std::unordered_set<rai::Account> reps;
    for (const auto& vote : election.votes_)
    {
        reps.insert(vote.first);
    }

    node_.RequestConfirms(it->second.block_, std::move(reps));
}

void rai::Elections::BroadcastConfirms_(const rai::Election& election)
{
    for (const auto& i : election.votes_)
    {
        const rai::Account& rep = i.first;
        const rai::RepVoteInfo& info = i.second;
        const rai::Vote& vote = info.last_vote_;

        if (info.weight_ < rai::QUALIFIED_REP_WEIGHT)
        {
            continue;
        }

        std::shared_ptr<rai::Block> block(nullptr);
        bool error = GetBlock_(election, vote.hash_, block);
        if (error)
        {
            assert(0);
            continue;
        }

        if (!info.conflict_found_)
        {
            node_.BroadcastConfirm(rep, vote.timestamp_, vote.signature_,
                                   block);
            continue;
        }

        rai::Vote conflict;
        error = GetConflict_(election, rep, conflict);
        if (error)
        {
            assert(0);
            continue;
        }

        std::shared_ptr<rai::Block> block_conflict(nullptr);
        error = GetBlock_(election, conflict.hash_, block_conflict);
        if (error)
        {
            assert(0);
            continue;
        }

        node_.BroadcastConflict(rep, vote.timestamp_, conflict.timestamp_,
                                vote.signature_, conflict.signature_, block,
                                block_conflict);
    }
}

std::chrono::steady_clock::time_point rai::Elections::NextWakeup_(
    const rai::Election& election) const
{
    auto now = std::chrono::steady_clock::now();
    if (election.ForkFound())
    {
        return now + rai::Elections::FORK_ELECTION_INTERVAL;
    }

    uint32_t shift = election.rounds_ / 5;
    if (shift > 8)
    {
        shift = 8;
    }
    auto base = rai::Elections::NON_FORK_ELECTION_INTERVAL.count();
    return now + std::chrono::seconds(base << shift);
}

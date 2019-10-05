#include <rai/secure/ledger.hpp>

rai::RepWeightOpration::RepWeightOpration(bool add,
                                          const rai::Account& representative,
                                          const rai::Amount& weight)
    : add_(add), representative_(representative), weight_(weight)
{
}

rai::Transaction::Transaction(rai::ErrorCode& error_code, rai::Ledger& ledger,
                              bool write)
    : ledger_(ledger),
      write_(write),
      aborted_(false),
      mdb_transaction_(error_code, ledger.store_.env_, nullptr, write)
{
}

rai::Transaction::~Transaction()
{
    if (aborted_)
    {
        return;
    }
    ledger_.RepWeightsCommit_(rep_weight_operations_);
}

void rai::Transaction::Abort()
{
    aborted_ = true;
    mdb_transaction_.Abort();
}

rai::Iterator::Iterator(rai::StoreIterator&& store_it)
    : store_it_(std::move(store_it))
{
}

rai::Iterator::Iterator(rai::Iterator&& other)
    : store_it_(std::move(other.store_it_))
{
}

rai::Iterator& rai::Iterator::operator++()
{
    ++store_it_;
    return *this;
}

bool rai::Iterator::operator==(const rai::Iterator& other) const
{
    return store_it_ == other.store_it_;
}
bool rai::Iterator::operator!=(const rai::Iterator& other) const
{
    return store_it_ != other.store_it_;
}

rai::AccountInfo::AccountInfo()
    : type_(rai::BlockType::INVALID),
      forks_(0),
      head_height_(rai::Block::INVALID_HEIGHT),
      tail_height_(rai::Block::INVALID_HEIGHT),
      confirmed_height_(rai::Block::INVALID_HEIGHT)
{
}

rai::AccountInfo::AccountInfo(rai::BlockType type, const rai::BlockHash& hash)
    : type_(type),
      forks_(0),
      head_height_(0),
      tail_height_(0),
      confirmed_height_(rai::Block::INVALID_HEIGHT),
      head_(hash),
      tail_(hash)
{
}

void rai::AccountInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, type_);
    rai::Write(stream, forks_);
    rai::Write(stream, head_height_);
    rai::Write(stream, tail_height_);
    rai::Write(stream, confirmed_height_);
    rai::Write(stream, head_.bytes);
    rai::Write(stream, tail_.bytes);
}

bool rai::AccountInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error      = rai::Read(stream, type_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, forks_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, head_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, tail_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, confirmed_height_);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, head_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, tail_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

bool rai::AccountInfo::Valid() const
{
    if (head_height_ == rai::Block::INVALID_HEIGHT
        || tail_height_ == rai::Block::INVALID_HEIGHT)
    {
        return false;
    }

    if (head_.IsZero() || tail_.IsZero())
    {
        return false;
    }

    return true;
}

rai::ReceivableInfo::ReceivableInfo(const rai::Account& source,
                                    const rai::Amount& amount)
    : source_(source), amount_(amount)
{
}

void rai::ReceivableInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, source_.bytes);
    rai::Write(stream, amount_.bytes);
}

bool rai::ReceivableInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error      = rai::Read(stream, source_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, amount_.bytes);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::RewardableInfo::RewardableInfo(const rai::Account& source,
                                    const rai::Amount& amount,
                                    uint64_t valid_timestamp)
    : source_(source), amount_(amount), valid_timestamp_(valid_timestamp)
{
}

void rai::RewardableInfo::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, source_.bytes);
    rai::Write(stream, amount_.bytes);
    rai::Write(stream, valid_timestamp_);
}

bool rai::RewardableInfo::Deserialize(rai::Stream& stream)
{
    bool error = false;
    error      = rai::Read(stream, source_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, amount_.bytes);
    IF_ERROR_RETURN(error, true);
    error = rai::Read(stream, valid_timestamp_);
    IF_ERROR_RETURN(error, true);
    return false;
}

rai::Ledger::Ledger(rai::ErrorCode& error_code, rai::Store& store)
    : store_(store), total_rep_weight_(0)
{
    IF_NOT_SUCCESS_RETURN_VOID(error_code);
    rai::Transaction transaction(error_code, *this, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);
    InitRepWeights_(transaction);
}

bool rai::Ledger::AccountInfoPut(rai::Transaction& transaction,
                                 const rai::Account& account,
                                 const rai::AccountInfo& account_info)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        account_info.Serialize(stream);
    }
    rai::MdbVal key(account);
    rai::MdbVal value(bytes.size(), bytes.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.accounts_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::AccountInfoGet(rai::Transaction& transaction,
                                 const rai::Account& account,
                                 rai::AccountInfo& account_info) const
{
    rai::MdbVal key(account);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.accounts_, key, value);
    if (error)
    {
        return true;
    }
    rai::BufferStream stream(value.Data(), value.Size());
    return account_info.Deserialize(stream);
}

bool rai::Ledger::AccountInfoGet(const rai::Iterator& it, rai::Account& account,
                                 rai::AccountInfo& info) const
{
    if (it.store_it_->first.Data() == nullptr
        || it.store_it_->first.Size() == 0)
    {
        return true;
    }
    account = it.store_it_->first.uint256_union();

    auto data = it.store_it_->second.Data();
    auto size = it.store_it_->second.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }
    rai::BufferStream stream(data, size);
    return info.Deserialize(stream);
}

bool rai::Ledger::AccountInfoDel(rai::Transaction& transaction,
                                 const rai::Account& account)
{
    if (!transaction.write_)
    {
        return true;
    }

    rai::MdbVal key(account);
    return store_.Put(transaction.mdb_transaction_, store_.accounts_, key,
                      nullptr);
}

rai::Iterator rai::Ledger::AccountInfoBegin(rai::Transaction& transaction)
{

    rai::StoreIterator store_it(transaction.mdb_transaction_, store_.accounts_);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::AccountInfoEnd(rai::Transaction& transaction)
{
    rai::StoreIterator store_it(nullptr);
    return rai::Iterator(std::move(store_it));
}

bool rai::Ledger::AccountCount(rai::Transaction& transaction,
                               size_t& count) const
{
    MDB_stat stat;
    auto ret = mdb_stat(transaction.mdb_transaction_, store_.accounts_, &stat);
    if (ret != MDB_SUCCESS)
    {
        assert(0);
        return true;
    }

    count = stat.ms_entries;
    return false;
}

bool rai::Ledger::BlockPut(rai::Transaction& transaction,
                           const rai::BlockHash& hash, const rai::Block& block)
{
    return BlockPut(transaction, hash, block, rai::BlockHash(0));
}

bool rai::Ledger::BlockPut(rai::Transaction& transaction,
                           const rai::BlockHash& hash, const rai::Block& block,
                           const rai::BlockHash& successor)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
        rai::Write(stream, successor.bytes);
    }
    rai::MdbVal key(hash);
    rai::MdbVal value(bytes.size(), bytes.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.blocks_, key, value);
    IF_ERROR_RETURN(error, error);

    if (block.Height() % rai::Ledger::BLOCKS_PER_INDEX == 0)
    {
        error = BlockIndexPut_(transaction, block.Account(), block.Height(),
                               block.Hash());
        IF_ERROR_RETURN(error, error);
    }

    return false;
}

bool rai::Ledger::BlockGet(rai::Transaction& transaction,
                           const rai::BlockHash& hash,
                           std::shared_ptr<rai::Block>& block) const
{
    rai::MdbVal key(hash);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.blocks_, key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block                     = rai::DeserializeBlock(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    return false;
}

bool rai::Ledger::BlockGet(rai::Transaction& transaction,
                           const rai::BlockHash& hash,
                           std::shared_ptr<rai::Block>& block,
                           rai::BlockHash& successor) const
{
    rai::MdbVal key(hash);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.blocks_, key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block                     = rai::DeserializeBlock(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }
    error = rai::Read(stream, successor.bytes);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::BlockGet(rai::Transaction& transaction,
                           const rai::Account& account, uint64_t height,
                           std::shared_ptr<rai::Block>& block) const
{
    rai::AccountInfo info;
    bool error = AccountInfoGet(transaction, account, info);
    IF_ERROR_RETURN(error, error);

    if (height < info.tail_height_ || height > info.head_height_)
    {
        return true;
    }

    uint64_t start = (height / rai::Ledger::BLOCKS_PER_INDEX)
                     * rai::Ledger::BLOCKS_PER_INDEX;
    uint64_t end = start + rai::Ledger::BLOCKS_PER_INDEX;
    if (start < info.tail_height_)
    {
        start = info.tail_height_;
    }
    if (end > info.head_height_)
    {
        end = info.head_height_;
    }

    if (height - start < end - height)
    {
        rai::BlockHash hash(info.tail_);
        if (start != info.tail_height_)
        {
            error = BlockIndexGet_(transaction, account, start, hash);
            assert(error == false);
            IF_ERROR_RETURN(error, error);
        }

        for (uint64_t i = start; i <= height; ++i)
        {
            std::shared_ptr<rai::Block> block_l(nullptr);
            error = BlockGet(transaction, hash, block_l, hash);
            assert(error == false);
            IF_ERROR_RETURN(error, true);
            if (height == block_l->Height() && account == block_l->Account())
            {
                block = block_l;
                return false;
            }
        }
    }
    else
    {
        rai::BlockHash hash(info.head_);
        if (end != info.head_height_)
        {
            error = BlockIndexGet_(transaction, account, end, hash);
            assert(error == false);
            IF_ERROR_RETURN(error, error);
        }

        for (uint64_t i = height; i <= end; ++i)
        {
            std::shared_ptr<rai::Block> block_l(nullptr);
            error = BlockGet(transaction, hash, block_l);
            assert(error == false);
            IF_ERROR_RETURN(error, error);
            if (height == block_l->Height() && account == block_l->Account())
            {
                block = block_l;
                return false;
            }
            hash = block_l->Previous();
        }
    }

    assert(0);
    return true;
}

bool rai::Ledger::BlockDel(rai::Transaction& transaction,
                           const rai::BlockHash& hash)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::shared_ptr<rai::Block> block(nullptr);
    bool error = BlockGet(transaction, hash, block);
    IF_ERROR_RETURN(error, error);
    if (block->Height() % rai::Ledger::BLOCKS_PER_INDEX == 0)
    {
        error = BlockIndexDel_(transaction, block->Account(), block->Height());
        IF_ERROR_RETURN(error, error);
    }

    rai::MdbVal key(hash);
    return store_.Del(transaction.mdb_transaction_, store_.blocks_, key,
                      nullptr);
}

bool rai::Ledger::BlockCount(rai::Transaction& transaction, size_t& count) const
{
    MDB_stat stat;
    auto ret =
        mdb_stat(transaction.mdb_transaction_, store_.blocks_, &stat);
    if (ret != MDB_SUCCESS)
    {
        assert(0);
        return true;
    }

    count = stat.ms_entries;
    return false;
}

bool rai::Ledger::BlockExists(rai::Transaction& transaction,
                              const rai::BlockHash& hash) const
{
    rai::MdbVal key(hash);
    rai::MdbVal junk;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.blocks_, key, junk);
    if (error)
    {
        return false;
    }
    return true;
}

bool rai::Ledger::BlockSuccessorSet(rai::Transaction& transaction,
                                    const rai::BlockHash& hash,
                                    const rai::BlockHash& successor)
{
    if (!transaction.write_)
    {
        return true;
    }

    rai::MdbVal key(hash);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.blocks_, key, value);
    IF_ERROR_RETURN(error, error);
    std::vector<uint8_t> bytes(value.Data(), value.Data() + value.Size());
    std::copy(successor.bytes.begin(), successor.bytes.end(),
              bytes.end() - successor.bytes.size());

    rai::MdbVal value_new(bytes.size(), bytes.data());
    error = store_.Put(transaction.mdb_transaction_, store_.blocks_, key,
                       value_new);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::BlockSuccessorGet(rai::Transaction& transaction,
                                    const rai::BlockHash& hash,
                                    rai::BlockHash& successor) const
{
    rai::MdbVal key(hash);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.blocks_, key, value);
    IF_ERROR_RETURN(error, error);
    std::copy(value.Data() + value.Size() - successor.bytes.size(),
              value.Data() + value.Size(), successor.bytes.begin());
    return false;
}

bool rai::Ledger::Empty(rai::Transaction& transaction) const
{
    MDB_stat stat;
    auto ret = mdb_stat(transaction.mdb_transaction_, store_.accounts_, &stat);
    if (ret != MDB_SUCCESS)
    {
        assert(0);
        return false;
    }
    if (stat.ms_entries > 0)
    {
        return false;
    }

    ret = mdb_stat(transaction.mdb_transaction_, store_.blocks_, &stat);
    if (ret != MDB_SUCCESS)
    {
        assert(0);
        return false;
    }
    if (stat.ms_entries > 0)
    {
        return false;
    }

    return true;
}

bool rai::Ledger::ForkPut(rai::Transaction& transaction,
                          const rai::Account& account, uint64_t height,
                          const rai::Block& first, const rai::Block& second)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        first.Serialize(stream);
        second.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.forks_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::ForkGet(rai::Transaction& transaction,
                          const rai::Account& account, uint64_t height,
                          std::shared_ptr<rai::Block>& first,
                          std::shared_ptr<rai::Block>& second) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.forks_, key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    first                     = rai::DeserializeBlock(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }
    second = rai::DeserializeBlock(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    return false;
}

bool rai::Ledger::ForkGet(const rai::Iterator& it,
                          std::shared_ptr<rai::Block>& first,
                          std::shared_ptr<rai::Block>& second) const
{
    auto data = it.store_it_->second.Data();
    auto size = it.store_it_->second.Size();
    if (data == nullptr || size == 0)
    {
        return true;
    }

    rai::BufferStream stream(data, size);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    first = rai::DeserializeBlock(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }
    second = rai::DeserializeBlock(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    return false;
}

bool rai::Ledger::ForkDel(rai::Transaction& transaction,
                          const rai::Account& account, uint64_t height)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.forks_, key,
                      nullptr);
}

bool rai::Ledger::ForkExists(rai::Transaction& transaction,
                             const rai::Account& account, uint64_t height) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.forks_, key, value);
    IF_ERROR_RETURN(error, false);

    return true;
}

rai::Iterator rai::Ledger::ForkLowerBound(rai::Transaction& transaction,
                                          const rai::Account& account)
{
    uint64_t height = 0;
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_, store_.forks_,
                                key);
    return rai::Iterator(std::move(store_it));
}

rai::Iterator rai::Ledger::ForkUpperBound(rai::Transaction& transaction,
                                          const rai::Account& account)
{
    uint64_t height = std::numeric_limits<uint64_t>::max();
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::StoreIterator store_it(transaction.mdb_transaction_, store_.forks_,
                                key);
    return rai::Iterator(std::move(store_it));
}


bool rai::Ledger::ReceivableInfoPut(rai::Transaction& transaction,
                                    const rai::Account& destination,
                                    const rai::BlockHash& hash,
                                    const rai::ReceivableInfo& info)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, destination.bytes);
        rai::Write(stream, hash.bytes);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        info.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_, store_.receivables_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::ReceivableInfoGet(rai::Transaction& transaction,
                                    const rai::Account& destination,
                                    const rai::BlockHash& hash,
                                    rai::ReceivableInfo& info) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, destination.bytes);
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_, store_.receivables_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    return info.Deserialize(stream);
}

bool rai::Ledger::ReceivableInfoDel(rai::Transaction& transaction,
                                    const rai::Account& destination,
                                    const rai::BlockHash& hash)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, destination.bytes);
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.receivables_, key,
                      nullptr);
}

bool rai::Ledger::ReceivableInfoCount(rai::Transaction& transaction,
                                      size_t& count) const
{
    MDB_stat stat;
    auto ret =
        mdb_stat(transaction.mdb_transaction_, store_.receivables_, &stat);
    if (ret != MDB_SUCCESS)
    {
        assert(0);
        return true;
    }

    count = stat.ms_entries;
    return false;
}

bool rai::Ledger::RewardableInfoPut(rai::Transaction& transaction,
                                    const rai::Account& representative,
                                    const rai::BlockHash& hash,
                                    const rai::RewardableInfo& info)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, representative.bytes);
        rai::Write(stream, hash.bytes);
    }
    std::vector<uint8_t> bytes_value;
    {
        rai::VectorStream stream(bytes_value);
        info.Serialize(stream);
    }

    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value(bytes_value.size(), bytes_value.data());
    bool error = store_.Put(transaction.mdb_transaction_, store_.rewardables_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::RewardableInfoGet(rai::Transaction& transaction,
                                    const rai::Account& representative,
                                    const rai::BlockHash& hash,
                                    rai::RewardableInfo& info) const
{
    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, representative.bytes);
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_, store_.rewardables_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    return info.Deserialize(stream);
}

bool rai::Ledger::RewardableInfoDel(rai::Transaction& transaction,
                                    const rai::Account& representative,
                                    const rai::BlockHash& hash)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes_key;
    {
        rai::VectorStream stream(bytes_key);
        rai::Write(stream, representative.bytes);
        rai::Write(stream, hash.bytes);
    }
    rai::MdbVal key(bytes_key.size(), bytes_key.data());
    return store_.Del(transaction.mdb_transaction_, store_.rewardables_, key,
                      nullptr);
}

bool rai::Ledger::RollbackBlockPut(rai::Transaction& transaction,
                                   const rai::BlockHash& hash,
                                   const rai::Block& block)
{
    if (!transaction.write_)
    {
        return true;
    }

    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        block.Serialize(stream);
    }
    rai::MdbVal key(hash);
    rai::MdbVal value(bytes.size(), bytes.data());
    bool error =
        store_.Put(transaction.mdb_transaction_, store_.rollbacks_, key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::RollbackBlockGet(rai::Transaction& transaction,
                                   const rai::BlockHash& hash,
                                   std::shared_ptr<rai::Block>& block) const
{
    rai::MdbVal key(hash);
    rai::MdbVal value;
    bool error =
        store_.Get(transaction.mdb_transaction_, store_.rollbacks_, key, value);
    IF_ERROR_RETURN(error, error);

    rai::BufferStream stream(value.Data(), value.Size());
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    block                     = rai::DeserializeBlock(error_code, stream);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return true;
    }

    return false;
}

void rai::Ledger::RepWeightAdd(rai::Transaction& transaction,
                               const rai::Account& representative,
                               const rai::Amount& weight)
{
    rai::RepWeightOpration op(true, representative, weight);
    transaction.rep_weight_operations_.push_back(op);
}

void rai::Ledger::RepWeightSub(rai::Transaction& transaction,
                               const rai::Account& representative,
                               const rai::Amount& weight)
{
    rai::RepWeightOpration op(false, representative, weight);
    transaction.rep_weight_operations_.push_back(op);
}

bool rai::Ledger::RepWeightGet(const rai::Account& representative,
                               rai::Amount& weight) const
{
    std::lock_guard<std::mutex> lock(rep_weights_mutex_);
    auto it = rep_weights_.find(representative);
    if (it == rep_weights_.end())
    {
        return true;
    }
    weight = it->second;
    return false;
}

void rai::Ledger::RepWeightsGet(
    rai::Amount& total,
    std::unordered_map<rai::Account, rai::Amount>& weights) const
{
    std::lock_guard<std::mutex> lock(rep_weights_mutex_);
    total = total_rep_weight_;
    weights = rep_weights_;
}

bool rai::Ledger::BlockIndexPut_(rai::Transaction& transaction,
                                 const rai::Account& account, uint64_t height,
                                 const rai::BlockHash& hash)
{
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes.size(), bytes.data());
    rai::MdbVal value(hash);
    bool error = store_.Put(transaction.mdb_transaction_, store_.blocks_index_,
                            key, value);
    IF_ERROR_RETURN(error, error);

    return false;
}

bool rai::Ledger::BlockIndexGet_(rai::Transaction& transaction,
                                 const rai::Account& account, uint64_t height,
                                 rai::BlockHash& hash) const
{
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes.size(), bytes.data());
    rai::MdbVal value;
    bool error = store_.Get(transaction.mdb_transaction_, store_.blocks_index_,
                            key, value);
    IF_ERROR_RETURN(error, error);
    assert(value.Size() == sizeof(hash));

    hash = value.uint256_union();
    return false;
}

bool rai::Ledger::BlockIndexDel_(rai::Transaction& transaction,
                                 const rai::Account& account, uint64_t height)
{
    std::vector<uint8_t> bytes;
    {
        rai::VectorStream stream(bytes);
        rai::Write(stream, account.bytes);
        rai::Write(stream, height);
    }
    rai::MdbVal key(bytes.size(), bytes.data());
    return store_.Del(transaction.mdb_transaction_, store_.blocks_index_, key,
                      nullptr);
}

void rai::Ledger::RepWeightsCommit_(
    const std::vector<rai::RepWeightOpration>& ops)
{
    std::lock_guard<std::mutex> lock(rep_weights_mutex_);
    for (const auto& op : ops)
    {
        if (op.weight_.IsZero())
        {
            continue;
        }

        if (op.add_)
        {
            if (rep_weights_.find(op.representative_) == rep_weights_.end())
            {
                rep_weights_[op.representative_] = op.weight_;
            }
            else
            {
                rep_weights_[op.representative_] += op.weight_;
                assert(rep_weights_[op.representative_] > op.weight_);
            }
            total_rep_weight_ += op.weight_;
        }
        else
        {
            if (rep_weights_.find(op.representative_) == rep_weights_.end())
            {
                assert(0);
                continue;
            }
            if (op.weight_ > rep_weights_[op.representative_])
            {
                assert(0);
                rep_weights_.erase(op.representative_);
                total_rep_weight_ -= rep_weights_[op.representative_];
            }
            else if (op.weight_ == rep_weights_[op.representative_])
            {
                rep_weights_.erase(op.representative_);
                total_rep_weight_ -= op.weight_;
            }
            else
            {
                rep_weights_[op.representative_] -= op.weight_;
                total_rep_weight_ -= op.weight_;
            }
        }
    }
}

rai::ErrorCode rai::Ledger::InitRepWeights_(rai::Transaction& transaction)
{
    std::lock_guard<std::mutex> lock(rep_weights_mutex_);
    for (auto i = AccountInfoBegin(transaction),
              n = AccountInfoEnd(transaction);
         i != n; ++i)
    {
        rai::Account account;
        rai::AccountInfo info;
        bool error = AccountInfoGet(i, account, info);
        IF_ERROR_RETURN(error, rai::ErrorCode::LEDGER_ACCOUNT_INFO_GET);

        if (info.head_height_ == rai::Block::INVALID_HEIGHT)
        {
            assert(0);
            continue;
        }
        std::shared_ptr<rai::Block> block(nullptr);
        error = BlockGet(transaction, info.head_, block);
        IF_ERROR_RETURN(error, rai::ErrorCode::LEDGER_BLOCK_GET);

        if (block->HasRepresentative())
        {
            rep_weights_[block->Representative()] += block->Balance();
            total_rep_weight_ += block->Balance();
        }
    }

    return rai::ErrorCode::SUCCESS;
}


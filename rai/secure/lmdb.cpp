#include <rai/secure/lmdb.hpp>

rai::MdbEnv::MdbEnv(rai::ErrorCode& error_code,
                    const boost::filesystem::path& path, int max_dbs)
{
    if (!path.has_parent_path())
    {
        error_code = rai::ErrorCode::DATA_PATH;
        env_ = nullptr;
        return;
    }

    auto error = mdb_env_create(&env_);
    if (error)
    {
        error_code = rai::ErrorCode::MDB_ENV_CREATE;
        return;
    }

    error = mdb_env_set_maxdbs(env_, max_dbs);
    if (error)
    {
        error_code = rai::ErrorCode::MDB_ENV_SET_MAXDBS;
        return;
    }

    error = mdb_env_set_mapsize(env_, 1ULL * 1024 * 1024 * 1024 * 128);
    if (error)
    {
        error_code = rai::ErrorCode::MDB_ENV_SET_MAPSIZE;
        return;
    }

    error = mdb_env_open(env_, path.string().c_str(), MDB_NOSUBDIR | MDB_NOTLS,
                         00600);
    if (error)
    {
        error_code = rai::ErrorCode::MDB_ENV_OPEN;
        return;
    }
}

rai::MdbEnv::~MdbEnv()
{
    if (env_)
    {
        mdb_env_close(env_);
        env_ = nullptr;
    }
}

rai::MdbEnv::operator MDB_env*() const
{
    return env_;
}

rai::MdbVal::MdbVal() : value_{0, nullptr}
{
}

rai::MdbVal::MdbVal(const MDB_val& value) : value_(value)
{
}

rai::MdbVal::MdbVal(size_t size, void* data) : value_({size, data})
{
}

rai::MdbVal::MdbVal(const rai::uint128_union& value)
    : value_({sizeof(value), const_cast<rai::uint128_union*>(&value)})
{
}

rai::MdbVal::MdbVal(const rai::uint256_union& value)
    : value_({sizeof(value), const_cast<rai::uint256_union*>(&value)})
{
}

const uint8_t* rai::MdbVal::Data() const
{
    return reinterpret_cast<const uint8_t*>(value_.mv_data);
}

size_t rai::MdbVal::Size() const
{
    return value_.mv_size;
}

rai::uint256_union rai::MdbVal::uint256_union() const
{
    rai::uint256_union result;
    if (Size() != sizeof(result))
    {
        return result;
    }

    std::copy(Data(), Data() + sizeof(result), result.bytes.data());
    return result;
}

rai::MdbVal::operator MDB_val*() const
{
    return const_cast<MDB_val*>(&value_);
}

rai::MdbVal::operator const MDB_val&() const
{
    return value_;
}

rai::MdbTransaction::MdbTransaction(rai::ErrorCode& error_code,
                                    rai::MdbEnv& env, MDB_txn* parent,
                                    bool write)
    : env_(env)
{
    auto error = mdb_txn_begin(env_, parent, write ? 0 : MDB_RDONLY, &handle_);
    if (error)
    {
        error_code = rai::ErrorCode::MDB_TXN_BEGIN;
    }
}

rai::MdbTransaction::~MdbTransaction()
{
    if (handle_)
    {
        mdb_txn_commit(handle_);
    }
}

rai::MdbTransaction::operator MDB_txn*() const
{
    return handle_;
}

void rai::MdbTransaction::Abort()
{
    if (handle_)
    {
        mdb_txn_abort(handle_);
        handle_ = nullptr;
    }
}


rai::StoreIterator::StoreIterator(MDB_txn* txn, MDB_dbi dbi) : cursor_(nullptr)
{
    auto ret = mdb_cursor_open(txn, dbi, &cursor_);
    assert(ret == 0);
    ret = mdb_cursor_get(cursor_, &current_.first.value_,
                         &current_.second.value_, MDB_FIRST);
    assert(ret == 0 || ret == MDB_NOTFOUND);
    if (ret == 0)
    {
        ret = mdb_cursor_get(cursor_, &current_.first.value_,
                            &current_.second.value_, MDB_GET_CURRENT);
        assert(ret == 0 || ret == MDB_NOTFOUND);
    }
    else
    {
        Clear();
    }
}

rai::StoreIterator::StoreIterator(std::nullptr_t) : cursor_(nullptr)
{
}

rai::StoreIterator::StoreIterator(MDB_txn* txn, MDB_dbi dbi, const MDB_val& key)
    : cursor_(nullptr)
{
    auto ret = mdb_cursor_open(txn, dbi, &cursor_);
    assert(ret == 0);
    current_.first.value_ = key;
    ret = mdb_cursor_get(cursor_, &current_.first.value_,
                         &current_.second.value_, MDB_SET_RANGE);
    assert(ret == 0 || ret == MDB_NOTFOUND);
    if (ret == 0)
    {
        ret = mdb_cursor_get(cursor_, &current_.first.value_,
                            &current_.second.value_, MDB_GET_CURRENT);
        assert(ret == 0 || ret == MDB_NOTFOUND);
    }
    else
    {
        Clear();
    }
}

rai::StoreIterator::StoreIterator(rai::StoreIterator&& other)
{
    cursor_ = other.cursor_;
    other.cursor_ = nullptr;
    current_ = other.current_;
}

rai::StoreIterator::~StoreIterator()
{
    if (cursor_ != nullptr)
    {
        mdb_cursor_close(cursor_);
    }
}

rai::StoreIterator& rai::StoreIterator::operator++()
{
    if (cursor_ == nullptr)
    {
        Clear();
        return *this;
    }
    auto ret = mdb_cursor_get(cursor_, &current_.first.value_,
                              &current_.second.value_, MDB_NEXT);
    assert(ret == 0 || ret == MDB_NOTFOUND);
    if (ret != 0)
    {
        Clear();
    }
    return *this;
}

void rai::StoreIterator::NextDup()
{
    if (cursor_ == nullptr)
    {
        Clear();
        return;
    }
    auto ret = mdb_cursor_get(cursor_, &current_.first.value_,
                              &current_.second.value_, MDB_NEXT_DUP);
    assert(ret == 0 || ret == MDB_NOTFOUND);
    if (ret != 0)
    {
        Clear();
    }
}

rai::StoreIterator& rai::StoreIterator::operator=(rai::StoreIterator&& other)
{
    if (cursor_ != nullptr)
    {
        mdb_cursor_close(cursor_);
    }
    cursor_ = other.cursor_;
    other.cursor_ = nullptr;
    current_ = other.current_;
    other.Clear();
    return *this;
}

bool rai::StoreIterator::operator==(const rai::StoreIterator& other) const
{
    bool result = current_.first.Data() == other.current_.first.Data();
    assert(!result || (current_.first.Size() == other.current_.first.Size()));
    assert(!result || (current_.second.Data() == other.current_.second.Data()));
    assert(!result || (current_.second.Size() == other.current_.second.Size()));
    return result;
}

bool rai::StoreIterator::operator!=(const rai::StoreIterator& other) const
{
    return !(*this == other);
}

std::pair<rai::MdbVal, rai::MdbVal>* rai::StoreIterator::operator->()
{
    return &current_;
}

const std::pair<rai::MdbVal, rai::MdbVal>* rai::StoreIterator::operator->()
    const
{
    return &current_;
}

void rai::StoreIterator::Clear()
{
    current_.first  = rai::MdbVal();
    current_.second = rai::MdbVal();
}
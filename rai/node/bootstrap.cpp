#include <rai/node/bootstrap.hpp>
#include <rai/node/node.hpp>

std::chrono::seconds constexpr rai::Bootstrap::BOOTSTRAP_INTERVAL;

namespace
{
size_t BootstrapMessageSize()
{
    static std::atomic<size_t> size(0);
    if (size > 0)
    {
        return size;
    }

    rai::BootstrapMessage message(rai::BootstrapType::FULL, rai::Account(), 0,
                                  0);
    std::vector<uint8_t> bytes;
    message.ToBytes(bytes);
    size = bytes.size();

    return size;
}
}  // namespace

void rai::BootstrapAccount::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, account_.bytes);
    rai::Write(stream, head_.bytes);
    rai::Write(stream, height_);
}

void rai::BootstrapAccount::ToBytes(std::vector<uint8_t>& bytes) const
{
    rai::VectorStream stream(bytes);
    Serialize(stream);
}

rai::ErrorCode rai::BootstrapAccount::Deserialize(rai::Stream& stream)
{
    bool error = rai::Read(stream, account_.bytes);
    if (error)
    {
        rai::Stats::AddDetail(rai::ErrorCode::STREAM,
                              "BootstrapAccount::Deserialize::account");
        return rai::ErrorCode::STREAM;
    }

    error = rai::Read(stream, head_.bytes);
    if (error)
    {
        rai::Stats::AddDetail(rai::ErrorCode::STREAM,
                              "BootstrapAccount::Deserialize::head");
        return rai::ErrorCode::STREAM;
    }

    error = rai::Read(stream, height_);
    if (error)
    {
        rai::Stats::AddDetail(rai::ErrorCode::STREAM,
                              "BootstrapAccount::Deserialize::height");
        return rai::ErrorCode::STREAM;
    }

    return rai::ErrorCode::SUCCESS;
}

size_t rai::BootstrapAccount::Size()
{
    static size_t size = 0;
    if (size > 0)
    {
        return size;
    }

    rai::BootstrapAccount account;
    std::vector<uint8_t> bytes;
    account.ToBytes(bytes);
    size = bytes.size();

    return size;
}

void rai::BootstrapFork::Serialize(rai::Stream& stream) const
{
    rai::Write(stream, length_);
    if (length_ > 0 && first_ != nullptr && second_ != nullptr)
    {
        first_->Serialize(stream);
        second_->Serialize(stream);
    }
}

void rai::BootstrapFork::ToBytes(std::vector<uint8_t>& bytes) const
{
    rai::VectorStream stream(bytes);
    Serialize(stream);
}

rai::BootstrapClient::BootstrapClient(
    const std::shared_ptr<rai::Socket>& socket,
    const rai::TcpEndpoint& endpoint, rai::BootstrapType type)
    : endpoint_(endpoint),
      socket_(socket),
      next_(rai::Account(0)),
      next_height_(0),
      type_(type),
      connected_(false),
      finished_(false),
      total_(0),
      accounts_size_(0),
      forks_size_(0),
      time_span_(0)
{
    send_buffer_.reserve(rai::BootstrapClient::BUFFER_SIZE_);
    receive_buffer_.resize(rai::BootstrapClient::BUFFER_SIZE_);
}

rai::ErrorCode rai::BootstrapClient::Connect()
{
    if (connected_)
    {
        return rai::ErrorCode::SUCCESS;
    }

    error_code_              = rai::ErrorCode::SUCCESS;
    promise_                 = std::promise<bool>();
    std::future<bool> future = promise_.get_future();
    std::shared_ptr<rai::BootstrapClient> this_s(shared_from_this());
    socket_->AsyncConnect(endpoint_,
                          [this_s](const boost::system::error_code& ec) {
                              this_s->ConnectCallback(ec);
                          });
    future.get();
    
    if (error_code_ == rai::ErrorCode::SUCCESS)
    {
        connected_ = true;
    }
    return error_code_;
}

bool rai::BootstrapClient::Finished() const
{
    return finished_;
}

rai::ErrorCode rai::BootstrapClient::Run()
{
    rai::ErrorCode error_code = Connect();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Stats::AddDetail(error_code, "Failed to connect to ", endpoint_);
        return error_code;
    }

    error_code_ = rai::ErrorCode::SUCCESS;
    promise_ = std::promise<bool>();
    std::future<bool> future = promise_.get_future();
    std::shared_ptr<rai::BootstrapClient> this_s(shared_from_this());
    rai::BootstrapMessage message(type_, next_, next_height_, MaxSize_());
    send_buffer_.clear();
    message.ToBytes(send_buffer_);

    socket_->AsyncWrite(
        send_buffer_,
        [this_s](const boost::system::error_code& ec, size_t size) {
            this_s->WriteCallback(ec, size);
        });
    future.get();
    IF_NOT_SUCCESS_RETURN(error_code_);

    curr_size_ = 0;
    continue_  = true;
    auto start = std::chrono::high_resolution_clock::now();
    while (true)
    {
        IF_NOT_SUCCESS_RETURN(error_code_);
        if (finished_ || continue_ == false)
        {
            total_ += curr_size_;
            auto end = std::chrono::high_resolution_clock::now();
            time_span_ += std::chrono::duration_cast<std::chrono::milliseconds>(
                              end - start)
                              .count();
            return rai::ErrorCode::SUCCESS;
        }

        if (type_ == rai::BootstrapType::FULL
            || type_ == rai::BootstrapType::LIGHT)
        {
            promise_ = std::promise<bool>();
            future   = promise_.get_future();
            socket_->AsyncRead(
                receive_buffer_, rai::BootstrapAccount::Size(),
                [this_s](const boost::system::error_code& ec, size_t size) {
                    this_s->ReadAccount(ec, size);
                });
            future.get();
        }
        else if (type_ == rai::BootstrapType::FORK)
        {
            promise_ = std::promise<bool>();
            future   = promise_.get_future();
            socket_->AsyncRead(
                receive_buffer_, sizeof(forks_[0].length_),
                [this_s](const boost::system::error_code& ec, size_t size) {
                    this_s->ReadForkLength(ec, size);
                });
            future.get();
            IF_NOT_SUCCESS_RETURN(error_code_);
            if (finished_ || continue_ == false)
            {
                continue;
            }

            if (forks_[curr_size_].length_ > 0)
            {
                promise_ = std::promise<bool>();
                future   = promise_.get_future();
                socket_->AsyncRead(
                    receive_buffer_, forks_[curr_size_].length_,
                    [this_s](const boost::system::error_code& ec, size_t size) {
                        this_s->ReadForkBlocks(ec, size);
                    });
                future.get();
            }
        }
        else
        {
            return rai::ErrorCode::BOOTSTRAP_TYPE;
        }
    }
}

rai::ErrorCode rai::BootstrapClient::Pause()
{
    if (connected_)
    {
        error_code_              = rai::ErrorCode::SUCCESS;
        promise_                 = std::promise<bool>();
        std::future<bool> future = promise_.get_future();
        std::shared_ptr<rai::BootstrapClient> this_s(shared_from_this());
        rai::BootstrapMessage message(type_, next_, next_height_, 0);
        send_buffer_.clear();
        message.ToBytes(send_buffer_);

        socket_->AsyncWrite(
            send_buffer_,
            [this_s](const boost::system::error_code& ec, size_t size) {
                this_s->WriteCallback(ec, size);
            });
        future.get();
        IF_NOT_SUCCESS_RETURN(error_code_);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    return rai::ErrorCode::SUCCESS;
}

void rai::BootstrapClient::ConnectCallback(const boost::system::error_code& ec)
{
    error_code_ =
        ec ? rai::ErrorCode::BOOTSTRAP_CONNECT : rai::ErrorCode::SUCCESS;
    promise_.set_value(true);
}

void rai::BootstrapClient::WriteCallback(const boost::system::error_code& ec,
                                         size_t size)
{
    if (ec || size == 0)
    {
        error_code_ = rai::ErrorCode::BOOTSTRAP_SEND;
    }
    else
    {
        error_code_ = rai::ErrorCode::SUCCESS;
    }

    promise_.set_value(true);
}

void rai::BootstrapClient::ReadAccount(const boost::system::error_code& ec,
                                       size_t size)
{
    do
    {
        if (ec)
        {
            error_code_ = rai::ErrorCode::BOOTSTRAP_RECEIVE;
            rai::Stats::AddDetail(
                error_code_, "BootstrapClient::ReadAccount: ec=", ec.message());
            break;
        }

        if (size != rai::BootstrapAccount::Size())
        {
            error_code_ = rai::ErrorCode::BOOTSTRAP_RECEIVE;
            rai::Stats::AddDetail(error_code_,
                                "BootstrapClient::ReadAccount: bad size=", size);
            break;
        }

        rai::BufferStream stream(receive_buffer_.data(), size);
        rai::BootstrapAccount account;
        error_code_ = account.Deserialize(stream);
        if (error_code_ != rai::ErrorCode::SUCCESS)
        {
            break;
        }

        if (account.height_ == rai::Block::INVALID_HEIGHT)
        {
            if (curr_size_ == 0)
            {
                finished_ = true;
            }
            continue_       = false;
            accounts_size_ = curr_size_;
            break;
        }

        if (account.account_ < next_)
        {
            error_code_ = rai::ErrorCode::BOOTSTRAP_ACCOUNT;
            break;
        }

        if (curr_size_ >= MaxSize_())
        {
            error_code_ = rai::ErrorCode::BOOTSTRAP_SIZE;
            break;
        }

        accounts_[curr_size_++] = account;
        next_                   = account.account_ + 1;

    } while (0);

    promise_.set_value(true);
}

void rai::BootstrapClient::ReadForkLength(const boost::system::error_code& ec,
                                          size_t size)
{
    do
    {
        if (ec)
        {
            error_code_ = rai::ErrorCode::BOOTSTRAP_RECEIVE;
            rai::Stats::AddDetail(
                error_code_,
                "BootstrapClient::ReadForkLength: ec=", ec.message());
            break;
        }

        auto length = forks_[0].length_;
        if (size != sizeof(length))
        {
            error_code_ = rai::ErrorCode::BOOTSTRAP_RECEIVE;
            rai::Stats::AddDetail(
                error_code_,
                "BootstrapClient::ReadForkLength: bad size=", size);
            break;
        }

        rai::BufferStream stream(receive_buffer_.data(), size);
        bool error = rai::Read(stream, length);
        if (error)
        {
            error_code_ = rai::ErrorCode::STREAM;
            rai::Stats::AddDetail(error_code_,
                                  "BootstrapClient::ReadForkLength");
            break;
        }

        if (length > rai::BootstrapClient::BUFFER_SIZE_)
        {
            error_code_ = rai::ErrorCode::BOOTSTRAP_FORK_LENGTH;
            break;
        }
        
        if (length == 0)
        {
            if (curr_size_ == 0)
            {
                finished_ = true;
            }
            continue_   = false;
            forks_size_ = curr_size_;
            break;
        }

        if (curr_size_ >= MaxSize_())
        {
            error_code_ = rai::ErrorCode::BOOTSTRAP_SIZE;
            break;
        }
        forks_[curr_size_].length_ = length;
    } while (0);

    promise_.set_value(true);
}

void rai::BootstrapClient::ReadForkBlocks(const boost::system::error_code& ec,
                                          size_t size)
{
    do
    {
        if (ec)
        {
            error_code_ = rai::ErrorCode::BOOTSTRAP_RECEIVE;
            rai::Stats::AddDetail(
                error_code_,
                "BootstrapClient::ReadForkBlocks: ec=", ec.message());
            break;
        }

        if (size != forks_[curr_size_].length_)
        {
            error_code_ = rai::ErrorCode::BOOTSTRAP_RECEIVE;
            rai::Stats::AddDetail(
                error_code_,
                "BootstrapClient::ReadForkBlocks: bad size=", size);
            break;
        }

        rai::BufferStream stream(receive_buffer_.data(), size);
        std::shared_ptr<rai::Block> first =
            rai::DeserializeBlock(error_code_, stream);
        if (error_code_ != rai::ErrorCode::SUCCESS)
        {
            rai::Stats::AddDetail(error_code_,
                                  "BootstrapClient::ReadForkBlocks::first");
            break;
        }
        std::shared_ptr<rai::Block> second =
            rai::DeserializeBlock(error_code_, stream);
        if (error_code_ != rai::ErrorCode::SUCCESS)
        {
            rai::Stats::AddDetail(error_code_,
                                  "BootstrapClient::ReadForkBlocks::second");
            break;
        }

        if (first == nullptr || second == nullptr)
        {
            error_code_ = rai::ErrorCode::STREAM;
            break;
        }

        if (!first->ForkWith(*second) || first->Account() < next_)
        {
            error_code_ = rai::ErrorCode::BOOTSTRAP_FORK_BLOCK;
            break;
        }

        if (curr_size_ > 0 && first->Account() == next_
            && first->Height() <= forks_[curr_size_ - 1].first_->Height())
        {
            error_code_ = rai::ErrorCode::BOOTSTRAP_FORK_BLOCK;
            break;
        }

        forks_[curr_size_].first_  = first;
        forks_[curr_size_].second_ = second;
        ++curr_size_;
        if (first->Height() < std::numeric_limits<uint64_t>::max())
        {
            next_        = first->Account();
            next_height_ = first->Height() + 1;
        }
        else
        {
            next_        = first->Account() + 1;
            next_height_ = 0;
        }
    } while (0);

    promise_.set_value(true);
}

size_t rai::BootstrapClient::Size() const
{
    if (type_ == rai::BootstrapType::FULL || type_ == rai::BootstrapType::LIGHT)
    {
        return accounts_size_;
    }
    else if (type_ == rai::BootstrapType::FORK)
    {
        return forks_size_;
    }
    else
    {
        return 0;
    }
}

size_t rai::BootstrapClient::Total() const
{
    return total_;
}

uint64_t rai::BootstrapClient::TimeSpan() const
{
    return time_span_ / 1000;
}

const std::array<rai::BootstrapAccount, rai::BootstrapClient::MAX_ACCOUNTS>&
rai::BootstrapClient::Accounts() const
{
    return accounts_;
}

const std::array<rai::BootstrapFork, rai::BootstrapClient::MAX_FORKS>&
rai::BootstrapClient::Forks() const
{
    return forks_;
}

uint16_t rai::BootstrapClient::MaxSize_() const
{
    size_t size = 0;
    if (type_ == rai::BootstrapType::FULL || type_ == rai::BootstrapType::LIGHT)
    {
        size = rai::BootstrapClient::MAX_ACCOUNTS;
    }
    else if (type_ == rai::BootstrapType::FORK)
    {
        size = rai::BootstrapClient::MAX_FORKS;
    }
    else
    {
        assert(0);
    }

    return static_cast<uint16_t>(size);
}

rai::Bootstrap::Bootstrap(rai::Node& node)
    : node_(node),
      stopped_(false),
      waiting_(false),
      count_(0),
      last_time_(std::chrono::steady_clock::duration::zero()),
      thread_([this]() { this->Run(); })
{
}

rai::Bootstrap::~Bootstrap()
{
    Stop();
}

uint32_t rai::Bootstrap::Count() const
{
    return count_;
}

bool rai::Bootstrap::WaitingSyncer() const
{
    return waiting_;
}

void rai::Bootstrap::Run()
{
    while (!stopped_)
    {
        auto now = std::chrono::steady_clock::now();
        if (count_ >= rai::Bootstrap::INITIAL_FULL_BOOTSTRAPS
            && now < last_time_ + rai::Bootstrap::BOOTSTRAP_INTERVAL)
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
        if (count_ < rai::Bootstrap::INITIAL_FULL_BOOTSTRAPS
            || 0 == count_ % rai::Bootstrap::FULL_BOOTSTRAP_INTERVAL)
        {
            error_code = RunFull_();
        }
        else
        {
            error_code = RunLight_();
        }

        if (count_ == rai::Bootstrap::INITIAL_FULL_BOOTSTRAPS
            || (count_ > 0
                && 0 == count_ % rai::Bootstrap::FULL_BOOTSTRAP_INTERVAL))
        {
            if (error_code == rai::ErrorCode::SUCCESS)
            {
                error_code = RunFork_();
            }
        }

        if (error_code == rai::ErrorCode::SUCCESS)
        {
            // stat
            last_time_ = now;
            ++count_;
            if (count_ <= rai::Bootstrap::INITIAL_FULL_BOOTSTRAPS)
            {
                Wait_();
                if (count_ == rai::Bootstrap::INITIAL_FULL_BOOTSTRAPS)
                {
                    node_.SetStatus(rai::NodeStatus::RUN);
                }
            }
        }
        else
        {
            rai::Stats::Add(error_code, "Bootstrap::Run");
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

void rai::Bootstrap::Stop()
{
    stopped_ = true;
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void rai::Bootstrap::Restart()
{
    count_ = 0;
}

bool rai::Bootstrap::UnderAttack() const
{
    rai::SyncStat stat = node_.syncer_.Stat();
    if (stat.total_ < 10240)
    {
        return false;
    }

    if (stat.miss_ > stat.total_ / 2)
    {
        return true;
    }

    return false;
}

void rai::Bootstrap::SyncGenesisAccount_()
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, node_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    rai::Account account = node_.genesis_.block_->Account();
    rai::AccountInfo account_info;
    bool error =
        node_.ledger_.AccountInfoGet(transaction, account, account_info);
    if (error || !account_info.Valid())
    {
        node_.syncer_.Add(account, 0, false, count_);
    }
    else
    {
        node_.syncer_.Add(account, account_info.head_height_ + 1,
                          account_info.head_, false, count_);
    }
}

rai::ErrorCode rai::Bootstrap::RunFull_()
{
    boost::optional<rai::Peer> peer = node_.peers_.RandomPeer();
    if (!peer)
    {
        return rai::ErrorCode::BOOTSTRAP_PEER;
    }

    uint32_t count = count_;
    if (count < rai::Bootstrap::INITIAL_FULL_BOOTSTRAPS)
    {
        node_.SetStatus(rai::NodeStatus::SYNC);
    }
    SyncGenesisAccount_();
    node_.syncer_.ResetStat();

    std::shared_ptr<rai::Socket> socket =
        std::make_shared<rai::Socket>(node_.Shared());
    std::shared_ptr<rai::BootstrapClient> client =
        std::make_shared<rai::BootstrapClient>(socket, peer->TcpEndpoint(),
                                               rai::BootstrapType::FULL);
    while (true)
    {
        if (stopped_)
        {
            return rai::ErrorCode::SUCCESS;
        }

        if (count != count_)
        {
            return rai::ErrorCode::BOOTSTRAP_RESET;
        }

        if (UnderAttack())
        {
            return rai::ErrorCode::BOOTSTRAP_ATTACK;
        }

        if (client->TimeSpan() >= 10
            && client->Total() / client->TimeSpan() < 1000)
        {
            return rai::ErrorCode::BOOTSTRAP_SLOW_CONNECTION;
        }

        if (node_.Busy())
        {
            rai::ErrorCode error_code = client->Pause();
            IF_NOT_SUCCESS_RETURN(error_code);
            continue;
        }

        rai::ErrorCode error_code = client->Run();
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::Transaction transaction(error_code, node_.ledger_, false);
        IF_NOT_SUCCESS_RETURN(error_code);

        auto data = client->Accounts();
        for (size_t i = 0; i < client->Size(); ++i)
        {
            StartSync_(transaction, data[i], count);
        }

        if (client->Finished())
        {
            return rai::ErrorCode::SUCCESS;
        }
    }
}

rai::ErrorCode rai::Bootstrap::RunLight_()
{
    boost::optional<rai::Peer> peer = node_.peers_.RandomPeer();
    if (!peer)
    {
        return rai::ErrorCode::BOOTSTRAP_PEER;
    }
    node_.syncer_.ResetStat();

    std::shared_ptr<rai::Socket> socket =
        std::make_shared<rai::Socket>(node_.Shared());
    std::shared_ptr<rai::BootstrapClient> client =
        std::make_shared<rai::BootstrapClient>(socket, peer->TcpEndpoint(),
                                               rai::BootstrapType::LIGHT);
    uint32_t count = count_;
    while (true)
    {
        if (stopped_)
        {
            return rai::ErrorCode::SUCCESS;
        }

        if (count != count_)
        {
            return rai::ErrorCode::BOOTSTRAP_RESET;
        }

        if (UnderAttack())
        {
            return rai::ErrorCode::BOOTSTRAP_ATTACK;
        }

        if (client->TimeSpan() >= 10
            && client->Total() / client->TimeSpan() < 1000)
        {
            return rai::ErrorCode::BOOTSTRAP_SLOW_CONNECTION;
        }

        if (node_.Busy())
        {
            rai::ErrorCode error_code = client->Pause();
            IF_NOT_SUCCESS_RETURN(error_code);
            continue;
        }

        rai::ErrorCode error_code = client->Run();
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::Transaction transaction(error_code, node_.ledger_, false);
        IF_NOT_SUCCESS_RETURN(error_code);

        auto data = client->Accounts();
        for (size_t i = 0; i < client->Size(); ++i)
        {
            StartSync_(transaction, data[i], count);
        }

        if (client->Finished())
        {
            return rai::ErrorCode::SUCCESS;
        }
    }
}

rai::ErrorCode rai::Bootstrap::RunFork_()
{
    boost::optional<rai::Peer> peer = node_.peers_.RandomPeer();
    if (!peer)
    {
        return rai::ErrorCode::BOOTSTRAP_PEER;
    }

    uint32_t count = count_;
    std::shared_ptr<rai::Socket> socket =
        std::make_shared<rai::Socket>(node_.Shared());
    std::shared_ptr<rai::BootstrapClient> client =
        std::make_shared<rai::BootstrapClient>(socket, peer->TcpEndpoint(),
                                               rai::BootstrapType::FORK);
    size_t miss = 0;
    while (true)
    {
        if (stopped_)
        {
            return rai::ErrorCode::SUCCESS;
        }

        if (count != count_)
        {
            return rai::ErrorCode::BOOTSTRAP_RESET;
        }

        if (client->Total() >= 1000 && miss * 100 / client->Total() >= 50)
        {
            return rai::ErrorCode::BOOTSTRAP_ATTACK;
        }

        if (client->TimeSpan() >= 10
            && client->Total() / client->TimeSpan() < 100)
        {
            return rai::ErrorCode::BOOTSTRAP_SLOW_CONNECTION;
        }

        if (node_.block_processor_.Busy())
        {
            rai::ErrorCode error_code = client->Pause();
            IF_NOT_SUCCESS_RETURN(error_code);
            continue;
        }

        rai::ErrorCode error_code = client->Run();
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::Transaction transaction(error_code, node_.ledger_, false);
        IF_NOT_SUCCESS_RETURN(error_code);

        auto data = client->Forks();
        for (size_t i = 0; i < client->Size(); ++i)
        {
            rai::AccountInfo account_info;
            bool error = node_.ledger_.AccountInfoGet(
                transaction, data[i].first_->Account(), account_info);
            if (error|| !account_info.Valid())
            {
                ++miss;
            }
            
            rai::BlockFork fork{data[i].first_, data[i].second_, false};
            node_.block_processor_.AddFork(fork);
        }

        if (client->Finished())
        {
            return rai::ErrorCode::SUCCESS;
        }
    }
}

void rai::Bootstrap::Wait_()
{
    waiting_ = true;
    while (true)
    {
        if (stopped_)
        {
            break;
        }

        bool finished = true;
        for (uint32_t count = 0; count < count_; ++count)
        {
            if (!node_.syncer_.Finished(count))
            {
                finished = false;
                break;
            }
        }
        if (finished)
        {
            break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    waiting_ = false;
}

void rai::Bootstrap::StartSync_(rai::Transaction& transaction,
                                const rai::BootstrapAccount& data,
                                uint32_t batch) const
{
    rai::AccountInfo info;
    bool error = node_.ledger_.AccountInfoGet(transaction, data.account_, info);
    bool account_exists = !error && info.Valid();
    if (!account_exists)
    {
        node_.syncer_.Add(data.account_, 0, true, batch);
        return;
    }

    if (data.height_ == info.head_height_ && data.head_ == info.head_)
    {
        return;
    }

    if (data.height_ < info.tail_height_)
    {
        return;
    }
    else if (data.height_ < info.head_height_)
    {
        if (node_.ledger_.BlockExists(transaction, data.head_))
        {
            return;
        }
        std::shared_ptr<rai::Block> block(nullptr);
        error = node_.ledger_.BlockGet(transaction, data.account_, data.height_,
                                       block);
        if (error || block == nullptr)
        {
            rai::Stats::Add(rai::ErrorCode::LEDGER_BLOCK_GET,
                            "Bootstrap::StartSync_");
            return;
        }
        node_.syncer_.Add(data.account_, data.height_ + 1, block->Hash(), true,
                          batch);
    }
    else
    {
        node_.syncer_.Add(data.account_, info.head_height_ + 1, info.head_,
                          true, batch);
    }
}

rai::BootstrapServer::BootstrapServer(
    const std::shared_ptr<rai::Node>& node,
    const std::shared_ptr<rai::Socket>& socket, const rai::IP& ip)
    : error_code_(rai::ErrorCode::SUCCESS),
      node_(node),
      socket_(socket),
      remote_ip_(ip),
      type_(rai::BootstrapType::INVALID),
      finished_(false)
{
    send_buffer_.reserve(rai::BootstrapServer::BUFFER_SIZE_);
    receive_buffer_.resize(rai::BootstrapServer::BUFFER_SIZE_);
}

rai::BootstrapServer::~BootstrapServer()
{
    node_->bootstrap_listener_.Erase(remote_ip_);
}

void rai::BootstrapServer::Close()
{
    socket_->Close();
}

void rai::BootstrapServer::Receive()
{
    std::shared_ptr<rai::BootstrapServer> this_s = shared_from_this();
    socket_->AsyncRead(
        receive_buffer_, BootstrapMessageSize(),
        [this_s](const boost::system::error_code& ec, size_t size) {
            this_s->Run(ec, size);
        });
}

void rai::BootstrapServer::Run(const boost::system::error_code& ec, size_t size)
{
    ReadMessage_(ec, size);
    if (error_code_ != rai::ErrorCode::SUCCESS)
    {
        rai::Stats::Add(error_code_, "BootstrapServer::Run");
        return;
    }

    if (max_size_ == 0)
    {
        Receive();
        return;
    }

    count_ = 0;
    continue_ = true;
    if (type_ == rai::BootstrapType::FULL)
    {
        RunFull_();
    }
    else if (type_ == rai::BootstrapType::LIGHT)
    {
        RunLight_();
    }
    else if (type_ == rai::BootstrapType::FORK)
    {
        RunFork_();
    }
}

void rai::BootstrapServer::ReadMessage_(const boost::system::error_code& ec,
                                       size_t size)
{
    if (ec)
    {
        error_code_ = rai::ErrorCode::BOOTSTRAP_RECEIVE;
        rai::Stats::AddDetail(error_code_,
                              "BootstrapServer::ReadMessage_:ec=", ec.message());
        return;
    }

    if (size != BootstrapMessageSize())
    {
        error_code_ = rai::ErrorCode::BOOTSTRAP_RECEIVE;
        rai::Stats::AddDetail(error_code_,
                              "BootstrapServer::ReadMessage_: bad size=", size);
        return;
    }

    rai::BufferStream stream(receive_buffer_.data(), size);
    rai::MessageHeader header(error_code_, stream);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    if (header.type_ != rai::MessageType::BOOTSTRAP)
    {
        error_code_ = rai::ErrorCode::BOOTSTRAP_MESSAGE_TYPE;
        return;
    }

    rai::BootstrapMessage message(error_code_, stream, header);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    if (!rai::StreamEnd(stream))
    {
        error_code_ = rai::ErrorCode::STREAM;
        rai::Stats::AddDetail(error_code_, "BootstrapServer::ReadMessage_");
        return;
    }

    if (type_ == rai::BootstrapType::INVALID)
    {
        type_ = message.type_;
    }
    if (type_ != message.type_)
    {
        error_code_ = rai::ErrorCode::BOOTSTRAP_TYPE;
        return;
    }
    next_  = message.start_;
    height_ = message.height_;
    max_size_ = message.MaxSize();
}

void rai::BootstrapServer::RunFull_()
{
    if (finished_)
    {
        // stat
        return;
    }

    if (continue_ == false)
    {
        Receive();
        return;
    }

    rai::BootstrapAccount bootstrap_account;
    if (count_ < max_size_)
    {
        rai::Transaction transaction(error_code_, node_->ledger_, false);
        IF_NOT_SUCCESS_RETURN_VOID(error_code_);
        rai::AccountInfo info;
        bool error = node_->ledger_.NextAccountInfo(transaction, next_, info);
        if (error)
        {
            bootstrap_account.height_ = rai::Block::INVALID_HEIGHT;
            if (count_ == 0)
            {
                finished_ = true;
            }
            continue_ = false;
        }
        else
        {
            bootstrap_account.account_ = next_;
            bootstrap_account.head_ = info.head_;
            bootstrap_account.height_ = info.head_height_;
            ++count_;
            next_ += 1;
        }
    }
    else
    {
        bootstrap_account.height_ = rai::Block::INVALID_HEIGHT;
        continue_ = false;
    }

    send_buffer_.clear();
    bootstrap_account.ToBytes(send_buffer_);
    Send_(std::bind(&rai::BootstrapServer::RunFull_, this));
}

void rai::BootstrapServer::RunLight_()
{
    if (finished_)
    {
        // stat
        return;
    }

    if (continue_ == false)
    {
        Receive();
        return;
    }

    rai::BootstrapAccount bootstrap_account;
    if (count_ < max_size_)
    {
        rai::Transaction transaction(error_code_, node_->ledger_, false);
        IF_NOT_SUCCESS_RETURN_VOID(error_code_);
        rai::AccountInfo info;
        bool finished = false;
        while (true)
        {
            bool error = node_->active_accounts_.Next(next_);
            if (error)
            {
                bootstrap_account.height_ = rai::Block::INVALID_HEIGHT;
                if (count_ == 0)
                {
                    finished_ = true;
                }
                continue_ = false;
                break;
            }

            error = node_->ledger_.AccountInfoGet(transaction, next_, info);
            if (error || !info.Valid())
            {
                next_ += 1;
                continue;
            }

            bootstrap_account.account_ = next_;
            bootstrap_account.head_ = info.head_;
            bootstrap_account.height_ = info.head_height_;
            ++count_;
            next_ += 1;
            break;
        }
    }
    else
    {
        bootstrap_account.height_ = rai::Block::INVALID_HEIGHT;
        continue_ = false;
    }

    send_buffer_.clear();
    bootstrap_account.ToBytes(send_buffer_);
    Send_(std::bind(&rai::BootstrapServer::RunLight_, this));
}

void rai::BootstrapServer::RunFork_()
{
    if (finished_)
    {
        // stat
        return;
    }

    if (continue_ == false)
    {
        Receive();
        return;
    }

    rai::BootstrapFork fork;
    if (count_ < max_size_)
    {
        rai::Transaction transaction(error_code_, node_->ledger_, false);
        IF_NOT_SUCCESS_RETURN_VOID(error_code_);
        bool error = node_->ledger_.NextFork(transaction, next_, height_,
                                             fork.first_, fork.second_);
        if (error)
        {
            fork.length_ = 0;
            if (count_ == 0)
            {
                finished_ = true;
            }
            continue_ = false;
        }
        else
        {
            fork.length_ = static_cast<uint16_t>(fork.first_->Size()
                                                 + fork.second_->Size());
            if (height_ == rai::Block::INVALID_HEIGHT)
            {
                next_ += 1;
                height_ = 0;
            }
            else
            {
                height_ += 1;
            }
            ++count_;
        }
    }
    else
    {
        fork.length_ = 0;
        continue_    = false;
    }

    send_buffer_.clear();
    fork.ToBytes(send_buffer_);
    Send_(std::bind(&rai::BootstrapServer::RunFork_, this));
}

void rai::BootstrapServer::Send_(const std::function<void()>& callback)
{
    std::shared_ptr<rai::BootstrapServer> this_s(shared_from_this());
    size_t buffer_size = send_buffer_.size();
    socket_->AsyncWrite(send_buffer_,
                        [this_s, buffer_size, callback](
                            const boost::system::error_code& ec, size_t size) {
                            if (ec || size != buffer_size)
                            {
                                // stat
                                return;
                            }
                            callback(); 
                        });
}

rai::BootstrapListener::BootstrapListener(rai::Node& node,
                                          boost::asio::io_service& service,
                                          uint16_t port)
    : node_(node),
      service_(service),
      acceptor_(service),
      local_(rai::TcpEndpoint(boost::asio::ip::address_v4::any(), port)),
      stopped_(false)
{
}

void rai::BootstrapListener::Accept()
{

    auto socket = std::make_shared<rai::Socket>(node_.Shared());
    if (socket == nullptr)
    {
        // log
        // stat
        return;
    }
    acceptor_.async_accept(socket->socket_,
                           [this, socket](const boost::system::error_code& ec) {
                               Connected(ec, socket);
                           });
}

void rai::BootstrapListener::Connected(
    const boost::system::error_code& ec,
    const std::shared_ptr<rai::Socket>& socket)
{
    std::shared_ptr<rai::BootstrapServer> server(nullptr);
    do
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }

        if (ec)
        {
            // stat
            // log
            break;
        }

        rai::IP remote_ip = socket->Remote().address().to_v4();
        if (rai::IsReservedIp(remote_ip))
        {
            // stat
            break;
        }

        if (connections_.size() >= rai::BootstrapListener::MAX_CONNECTIONS)
        {
            break;
        }

        if (connections_.find(remote_ip) != connections_.end())
        {
            break;
        }

        server = std::make_shared<rai::BootstrapServer>(
            node_.Shared(), socket, remote_ip);
        if (server == nullptr)
        {
            // log
            // stat
            break;
        }
        connections_[remote_ip] = server;
    } while (0);
    
    if (server != nullptr)
    {
        server->Receive();
    }

    Accept();
}

void rai::BootstrapListener::Erase(const rai::IP& ip)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(ip);
    if (it == connections_.end())
    {
        return;
    }
    connections_.erase(it);
}

void rai::BootstrapListener::Start()
{
    acceptor_.open(local_.protocol());
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    boost::system::error_code ec;
    acceptor_.bind(local_, ec);
    if (ec)
    {
        // log
        throw std::runtime_error(ec.message());
    }

    acceptor_.listen();
    Accept();
}

void rai::BootstrapListener::Stop()
{
    std::unordered_map<rai::IP, std::weak_ptr<rai::BootstrapServer>>
        connections;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
        connections.swap(connections_);
    }

    acceptor_.close();
    for (auto& i : connections)
    {
        auto connection = i.second.lock();
        if (connection)
        {
            connection->Close();
        }
    }
}

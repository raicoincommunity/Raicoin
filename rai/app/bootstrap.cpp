#include <rai/app/bootstrap.hpp>

#include <rai/common/util.hpp>
#include <rai/app/app.hpp>

bool rai::AccountHead::DeserializeJson(const rai::Ptree& ptree)
{
    auto account_o = ptree.get_optional<std::string>("account");
    if (!account_o || account_.DecodeAccount(*account_o))
    {
        return true;
    }

    auto head_o = ptree.get_optional<std::string>("head");
    if (!head_o || head_.DecodeHex(*head_o))
    {
        return true;
    }

    auto height_o = ptree.get_optional<std::string>("height");
    if (!height_o || rai::StringToUint(*height_o, height_))
    {
        return true;
    }

    return false;
}

rai::AppBootstrap::AppBootstrap(rai::App& app)
    : app_(app),
      request_id_(0),
      stopped_(false),
      running_(false),
      count_(0),
      next_(0)
{
}

bool rai::AppBootstrap::Ready()
{
    if (!app_.GatewayConnected())
    {
        return false;
    }

    if (app_.Busy())
    {
        return false;
    }

    return true;
}

void rai::AppBootstrap::Run()
{
    if (!Ready())
    {
        return;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    if (stopped_)
    {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (!running_)
    {
        auto interval = std::chrono::seconds(300);
        if (last_run_ + interval > now)
        {
            return;
        }
        last_run_ = now;
        running_ = true;
        next_ = rai::Account(0);
        count_++;
    }

    auto retry = std::chrono::seconds(5);
    if (last_request_ + retry > now)
    {
        return;
    }

    SendRequest_(lock);
}

void rai::AppBootstrap::Reset()
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (stopped_)
    {
        return;
    }

    ++request_id_;
    running_ = false;
    count_ = 0;
    last_run_ = std::chrono::steady_clock::time_point();
    last_request_ = std::chrono::steady_clock::time_point();
    next_ = rai::Account(0);
}

void rai::AppBootstrap::Stop()
{
    std::unique_lock<std::mutex> lock(mutex_);
    stopped_ = true;
}

void rai::AppBootstrap::ReceiveAccountHeadMessage(
    const std::shared_ptr<rai::Ptree>& message)
{
    auto request_id_o = message->get_optional<std::string>("request_id");
    if (!request_id_o)
    {
        return;
    }

    uint64_t request_id;
    bool error = rai::StringToUint(*request_id_o, request_id);
    IF_ERROR_RETURN_VOID(error);

    auto status_o = message->get_optional<std::string>("status");
    if (!status_o || *status_o != "success")
    {
        return;
    }

    auto more_o = message->get_optional<std::string>("more");
    if (!more_o)
    {
        return;
    }
    bool more = *more_o == "true";

    rai::Account next;
    auto next_o = message->get_optional<std::string>("next");
    if (!next_o || next.DecodeAccount(*next_o))
    {
        return;
    }

    bool ready = Ready();
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stopped_ || request_id != request_id_)
        {
            return;
        }

        if (more)
        {
            next_ = next;
            if (ready)
            {
                SendRequest_(lock);
            }
        }
        else
        {
            next_ = 0;
            running_ = false;
        }
    }

    std::vector<rai::AccountHead> heads;
    auto heads_o  = message->get_child_optional("heads");
    if (heads_o && !heads_o->empty())
    {
        for (const auto& i : *heads_o)
        {
            rai::AccountHead head;
            if (head.DeserializeJson(i.second))
            {
                return;
            }
            heads.push_back(head);
        }
    }
    ProcessAccountHeads_(heads);
}

uint64_t rai::AppBootstrap::NewRequestId_(std::unique_lock<std::mutex>& lock)
{
    return ++request_id_;
}

void rai::AppBootstrap::SendRequest_(std::unique_lock<std::mutex>& lock)
{
    last_request_ = std::chrono::steady_clock::now();
    uint64_t request_id = NewRequestId_(lock);
    rai::Account next = next_;
    lock.unlock();

    rai::Ptree ptree;

    if (count_ <= 3 || 0 == count_ % 12)
    {
        ptree.put("action", "account_heads");
    }
    else
    {
        ptree.put("action", "active_account_heads");
    }
    ptree.put("request_id", std::to_string(request_id));
    ptree.put("next", next.StringAccount());
    ptree.put("count", "1000");
    ptree.put_child("account_types", app_.AccountTypes());

    app_.SendToGateway(ptree);

    lock.lock();
}

void rai::AppBootstrap::ProcessAccountHeads_(
    const std::vector<rai::AccountHead>& heads)
{
    if (heads.empty())
    {
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, app_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    bool error;
    bool account_exists;
    rai::AccountInfo info;
    for (const auto& i : heads)
    {
        error = app_.ledger_.AccountInfoGet(transaction, i.account_, info);
        account_exists = !error && info.Valid();
        if (!account_exists)
        {
            app_.SyncAccountAsync(i.account_, 0);
            continue;
        }

        if (i.head_ == info.head_)
        {
            continue;
        }

        if (i.height_ < info.tail_height_ || info.Confirmed(i.height_))
        {
            continue;
        }

        if (i.height_ > info.head_height_)
        {
            app_.SyncAccountAsync(i.account_, info.head_height_ + 1);
            continue;
        }

        if (app_.ledger_.BlockExists(transaction, i.head_))
        {
            continue;
        }

        // fork
        app_.block_confirm_.Add(i.account_, i.height_, i.head_);
    }
}

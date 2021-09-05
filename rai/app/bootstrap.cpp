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

void rai::AppBootstrap::Run()
{
    if (!app_.GatewayConnected())
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
    if (last_request_ + retry < now)
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
    std::unique_lock<std::mutex> lock(mutex_);
    if (stopped_)
    {
        return;
    }

    auto status_o = message->get_optional<std::string>("status");
    if (!status_o)
    {
        return;
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



}

uint64_t rai::AppBootstrap::NewRequestId()
{
    return ++request_id_;
}

void rai::AppBootstrap::SendRequest_(std::unique_lock<std::mutex>& lock)
{
    rai::Ptree ptree;

    if (count_ <= 3 || 0 == count_ % 12)
    {
        ptree.put("action", "account_heads");
    }
    else
    {
        ptree.put("action", "active_account_heads");
    }
    ptree.put("request_id", std::to_string(NewRequestId()));
    ptree.put("next", next_.StringAccount());
    ptree.put("count", "1000");
    ptree.put_child("type", app_.AccountTypes());

    last_request_ = std::chrono::steady_clock::now();

    lock.unlock();
    app_.SendToGateway(ptree);
    lock.lock();
}

void rai::AppBootstrap::ProcessAccountHeads(
    std::unique_lock<std::mutex>& lock,
    const std::vector<rai::AccountHead>& heads)
{
    if (heads.empty())
    {
        return;
    }

    lock.unlock();
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, app_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code);

    for (const auto& i : heads)
    {

    }

    lock.lock();
}

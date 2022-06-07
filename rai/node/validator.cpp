#include <rai/node/validator.hpp>

#include <rai/node/node.hpp>

rai::Validator::Validator(rai::Node& node, boost::asio::io_service& service,
                          rai::Alarm& alarm, const rai::Url& url)
    : node_(node), service_(service), alarm_(alarm), url_(url), epoch_(0)
{
}

void rai::Validator::Start()
{
    if (!url_ || (url_.protocol_ != "ws" && url_.protocol_ != "wss"))
    {
        return;
    }

    websocket_ = std::make_shared<rai::WebsocketClient>(
        service_, url_.host_, url_.port_, url_.path_, url_.protocol_ == "wss");

    websocket_->message_processor_ =
        [this](const std::shared_ptr<rai::Ptree>& message) {
            ReceiveWsMessage(message);
        };

    OngoingSnapshot();

    node_.Ongoing(std::bind(&rai::Validator::ConnectToValidator, this),
                  std::chrono::seconds(5));
}

void rai::Validator::Stop()
{
    if (websocket_)
    {
        websocket_->Close();
    }
}

void rai::Validator::ConnectToValidator()
{
    if (websocket_)
    {
        websocket_->Run();
    }
}

void rai::Validator::OngoingSnapshot()
{
    uint64_t now = rai::CurrentTimestamp();
    uint32_t epoch = rai::CrossChainEpoch(now);
    Snapshot(epoch);

    uint64_t next = rai::CrossChainEpochTimestamp(epoch + 1);
    uint64_t delay = next > now ? next - now : 1;
    std::weak_ptr<rai::Node> node(node_.Shared());
    alarm_.Add(std::chrono::steady_clock::now() + std::chrono::seconds(delay),
               [node]() {
                   std::shared_ptr<rai::Node> node_l = node.lock();
                   if (node_l)
                   {
                       node_l->validator_.OngoingSnapshot();
                   }
               });
}

void rai::Validator::Snapshot(uint32_t epoch)
{
    rai::RepWeights rep_weights;
    node_.RepWeights(rep_weights);

    std::lock_guard<std::mutex> lock(mutex_);
    if (epoch <= epoch_)
    {
        return;
    }
    epoch_ = epoch;
    weights_ = std::move(rep_weights.weights_);
}

void rai::Validator::QueryWeight(const rai::Account& rep, uint32_t& epoch,
                                 rai::Amount& weight) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    epoch = epoch_;
    weight = 0;
    auto it = weights_.find(rep);
    if (it != weights_.end())
    {
        weight = it->second;
    }
}

void rai::Validator::ReceiveWsMessage(
    const std::shared_ptr<rai::Ptree>& message)
{
    // todo:
}

void rai::Validator::ProcessWeightQueryAck(const rai::WeightMessage& message)
{
    // todo:
}

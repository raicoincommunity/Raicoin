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
        service_, url_.host_, url_.port_, url_.path_, url_.protocol_ == "wss",
        true);

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
    auto action_o = message->get_optional<std::string>("action");
    if (!action_o)
    {
        return;
    }

    std::string action = *action_o;
    if (action == "weight_query")
    {
        ReceiveWsWeightQueryMessage_(message);
    }
    else if (action == "cross_chain")
    {
        ReceiveWsCrossChainMessage_(message);
    }
    else if (action == "bind")
    {
        ReceiveWsBindMessage_(message);
    }
    else if (action == "bind_query")
    {
        ReceiveWsBindQueryMessage_(message);
    }
    else if (action == "node_account")
    {
        ReceiveWsNodeAccountMessage_(message);
    }
    else if (action == "weight_snapshot")
    {
        ReceiveWsWeightSnapshotMessage_(message);
    }
}

void rai::Validator::ProcessWeightQueryAck(const rai::WeightMessage& message)
{
    if (!websocket_)
    {
        return;
    }

    rai::Ptree ptree;
    ptree.put("action", "weight_query_ack");
    ptree.put("request_id", message.request_id_.StringHex());
    ptree.put("representative", message.rep_.StringAccount());
    ptree.put("representative_hex", message.rep_.StringHex());
    ptree.put("epoch", std::to_string(message.epoch_));
    ptree.put("weight", message.weight_.StringDec());
    ptree.put("replier", message.replier_.StringAccount());
    ptree.put("replier_hex", message.replier_.StringHex());

    websocket_->Send(ptree);
}

void rai::Validator::ProcessCrosschainMessage(
    const rai::CrosschainMessage& message)
{
    if (!websocket_)
    {
        return;
    }

    if (message.destination_ != node_.account_)
    {
        rai::Stats::Add(rai::ErrorCode::CROSS_CHAIN_MESSAGE_DESTINATION,
                        "destination=", message.destination_.StringAccount());
        return;
    }

    rai::Ptree ptree;
    ptree.put("action", "cross_chain");
    ptree.put("source", message.source_.StringAccount());
    ptree.put("source_hex", message.source_.StringHex());
    ptree.put("destination", message.destination_.StringAccount());
    ptree.put("destination_hex", message.destination_.StringHex());
    ptree.put("chain", rai::ChainToString(message.chain_));
    ptree.put("chain_id",
              std::to_string(static_cast<uint32_t>(message.chain_)));
    ptree.put("payload", rai::BytesToHex(message.payload_.data(),
                                         message.payload_.size()));

    boost::optional<rai::SignerAddress> signer_o =
        node_.BindingQuery(message.source_, message.chain_);
    ptree.put("source_signer", signer_o ? signer_o->StringHex() : "");

    websocket_->Send(ptree);
}

void rai::Validator::ReceiveWsWeightQueryMessage_(
    const std::shared_ptr<rai::Ptree>& message)
{
    try
    {
        std::string request_id_str = message->get<std::string>("request_id");
        rai::uint256_union request_id;
        bool error = request_id.DecodeHex(request_id_str);
        IF_ERROR_THROW(error, "invalid request_id");

        std::string rep_str = message->get<std::string>("representative");
        rai::Account rep;
        error = DecodeAccount_(rep_str, rep);
        IF_ERROR_THROW(error, "invalid representative");

        std::string replier_str = message->get<std::string>("replier");
        rai::Account replier;
        error = DecodeAccount_(replier_str, replier);
        IF_ERROR_THROW(error, "invalid replier");

        rai::WeightMessage weight_message(request_id, rep);
        node_.SendToRep(replier, weight_message);
    }
    catch (std::exception& e)
    {
        std::stringstream ostream;
        boost::property_tree::write_json(ostream, *message);
        rai::Log::Error(rai::ToString(
            "Validator::ReceiveWsWeightQueryMessage_: exception, message=",
            ostream.str(), ", reason=", e.what()));
    }
    catch (...)
    {
        std::stringstream ostream;
        boost::property_tree::write_json(ostream, *message);
        rai::Log::Error(rai::ToString(
            "Validator::ReceiveWsWeightQueryMessage_: exception, message=",
            ostream.str()));
    }
}

void rai::Validator::ReceiveWsCrossChainMessage_(
    const std::shared_ptr<rai::Ptree>& message)
{
    try
    {
        std::string source_str = message->get<std::string>("source");
        rai::Account source;
        bool error = DecodeAccount_(source_str, source);
        IF_ERROR_THROW(error, "invalid source");

        if (source != node_.account_)
        {
            throw std::runtime_error("source missmatch");
        }

        std::string destination_str = message->get<std::string>("destination");
        rai::Account destination;
        error = DecodeAccount_(destination_str, destination);
        IF_ERROR_THROW(error, "invalid destination");

        rai::Chain chain;
        error = GetChain_(message, chain);
        IF_ERROR_THROW(error, "invalid chain");

        std::string payload_str = message->get<std::string>("payload");
        std::vector<uint8_t> payload;
        error = rai::HexToBytes(payload_str, payload);
        IF_ERROR_THROW(error, "invalid payload");

        rai::CrosschainMessage crosschain_message(source, destination, chain,
                                                  std::move(payload));
        node_.SendToRep(destination, crosschain_message);
    }
    catch (std::exception& e)
    {
        std::stringstream ostream;
        boost::property_tree::write_json(ostream, *message);
        rai::Log::Error(rai::ToString(
            "Validator::ReceiveWsCrossChainMessage_: exception, message=",
            ostream.str(), ", reason=", e.what()));
    }
    catch (...)
    {
        std::stringstream ostream;
        boost::property_tree::write_json(ostream, *message);
        rai::Log::Error(rai::ToString(
            "Validator::ReceiveWsCrossChainMessage_: exception, message=",
            ostream.str()));
    }
}

void rai::Validator::ReceiveWsBindMessage_(
    const std::shared_ptr<rai::Ptree>& message)
{
    try
    {
        rai::Chain chain;
        bool error = GetChain_(message, chain);
        IF_ERROR_THROW(error, "invalid chain");

        std::string signer_str = message->get<std::string>("signer");
        rai::SignerAddress signer;
        error = DecodeAccount_(signer_str, signer);
        IF_ERROR_THROW(error, "invalid signer");

        node_.rewarder_.Bind(chain, signer);
    }
    catch (std::exception& e)
    {
        std::stringstream ostream;
        boost::property_tree::write_json(ostream, *message);
        rai::Log::Error(rai::ToString(
            "Validator::ReceiveWsBindMessage_: exception, message=",
            ostream.str(), ", reason=", e.what()));
    }
    catch (...)
    {
        std::stringstream ostream;
        boost::property_tree::write_json(ostream, *message);
        rai::Log::Error(rai::ToString(
            "Validator::ReceiveWsBindMessage_: exception, message=",
            ostream.str()));
    }
}

void rai::Validator::ReceiveWsBindQueryMessage_(
    const std::shared_ptr<rai::Ptree>& message)
{
    if (!websocket_)
    {
        return;
    }

    try
    {
        rai::Chain chain;
        bool error = GetChain_(message, chain);
        IF_ERROR_THROW(error, "invalid chain");

        std::string validator_str = message->get<std::string>("validator");
        rai::Account validator;
        error = DecodeAccount_(validator_str, validator);
        IF_ERROR_THROW(error, "invalid validator");

        rai::SignerAddress signer(0);
        boost::optional<rai::SignerAddress> signer_o =
            node_.BindingQuery(validator, chain);
        if (signer_o)
        {
            signer = *signer_o;
        }

        rai::Ptree ptree;
        ptree.put("action", "bind_query_ack");
        ptree.put("chain", rai::ChainToString(chain));
        ptree.put("chain_id", std::to_string(static_cast<uint32_t>(chain)));
        ptree.put("validator", validator.StringAccount());
        ptree.put("validator_hex", validator.StringHex());
        ptree.put("signer", signer.StringHex());
        websocket_->Send(ptree);
    }
    catch (std::exception& e)
    {
        std::stringstream ostream;
        boost::property_tree::write_json(ostream, *message);
        rai::Log::Error(rai::ToString(
            "Validator::ReceiveWsBindQueryMessage_: exception, message=",
            ostream.str(), ", reason=", e.what()));
    }
    catch (...)
    {
        std::stringstream ostream;
        boost::property_tree::write_json(ostream, *message);
        rai::Log::Error(rai::ToString(
            "Validator::ReceiveWsBindQueryMessage_: exception, message=",
            ostream.str()));
    }
}

void rai::Validator::ReceiveWsNodeAccountMessage_(
    const std::shared_ptr<rai::Ptree>& message)
{
    if (!websocket_)
    {
        return;
    }
    rai::Ptree ptree;
    ptree.put("action", "node_account_ack");
    ptree.put("account", node_.account_.StringAccount());
    ptree.put("account_hex", node_.account_.StringHex());
    websocket_->Send(ptree);
}

void rai::Validator::ReceiveWsWeightSnapshotMessage_(
    const std::shared_ptr<rai::Ptree>& message)
{
    if (!websocket_)
    {
        return;
    }

    rai::Ptree ptree;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (epoch_ == 0)
        {
            return;
        }
        ptree.put("action", "weight_snapshot_ack");
        ptree.put("epoch", std::to_string(epoch_));
        rai::Ptree weights;
        for (auto& i : weights_)
        {
            rai::Ptree entry;
            entry.put("representative", i.first.StringAccount());
            entry.put("representative_hex", i.first.StringHex());
            entry.put("weight", i.second.StringDec());
            weights.push_back(std::make_pair("", entry));
        }
        ptree.put_child("weights", weights);
    }
    
    websocket_->Send(ptree);
}

bool rai::Validator::DecodeAccount_(const std::string& str,
                                    rai::Account& account) const
{
    bool error = true;
    if (rai::StringStartsWith(str, "rai_", false))
    {
        error = account.DecodeAccount(str);
    }
    else if (rai::StringStartsWith(str, "0x", false))
    {
        error = account.DecodeEvmHex(str);
    }
    else
    {
        error = account.DecodeHex(str);
    }
    return error;
}

bool rai::Validator::GetChain_(const std::shared_ptr<rai::Ptree>& ptree,
                               rai::Chain& chain) const
{
    try
    {
        auto chain_o = ptree->get_optional<std::string>("chain");
        if (chain_o)
        {
            chain = rai::StringToChain(*chain_o);
            return chain == rai::Chain::INVALID;
        }
        
        auto chain_id_o = ptree->get_optional<std::string>("chain_id");
        if (chain_id_o)
        {
            uint32_t chain_id;
            if (rai::StringToUint(*chain_id_o, chain_id))
            {
                return true;
            }
            chain = static_cast<rai::Chain>(chain_id);
            return chain == rai::Chain::INVALID;
        }

        return true;
    }
    catch (...)
    {
        return true;
    }
}

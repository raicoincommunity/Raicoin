#pragma once

#include <mutex>
#include <rai/common/util.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/alarm.hpp>
#include <rai/secure/websocket.hpp>
#include <rai/node/message.hpp>

namespace rai
{

class Node;

class Validator
{
public:
    Validator(rai::Node&, boost::asio::io_service&, rai::Alarm&,
              const rai::Url&);
    void Start();
    void Stop();
    void ConnectToValidator();
    void OngoingSnapshot();
    void Snapshot(uint32_t);
    void QueryWeight(const rai::Account&, uint32_t&, rai::Amount&) const;
    void ReceiveWsMessage(const std::shared_ptr<rai::Ptree>&);
    void ProcessWeightQueryAck(const rai::WeightMessage&);

    rai::Node& node_;
    boost::asio::io_service& service_;
    rai::Alarm& alarm_;
    const rai::Url& url_;

    std::shared_ptr<rai::WebsocketClient> websocket_;

private:
    mutable std::mutex mutex_;
    uint32_t epoch_;
    std::unordered_map<rai::Account, rai::Amount> weights_;
};

}
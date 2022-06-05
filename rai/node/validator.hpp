#pragma once

#include <rai/common/util.hpp>
#include <rai/secure/websocket.hpp>
#include <rai/node/message.hpp>

namespace rai
{

class Node;

class Validator
{
public:
    Validator(rai::Node&, boost::asio::io_service&, const rai::Url&);
    void ProcessWeightQueryAck(const rai::WeightMessage&);

    rai::Node& node_;
    boost::asio::io_service& service_;
    const rai::Url& url_;

    std::shared_ptr<rai::WebsocketClient> websocket_;
};

}
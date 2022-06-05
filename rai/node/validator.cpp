#include <rai/node/validator.hpp>

#include <rai/node/node.hpp>

rai::Validator::Validator(rai::Node& node, boost::asio::io_service& service,
                          const rai::Url& url)
    : node_(node), service_(service), url_(url)
{
    if (url_)
    {
        if (url_.protocol_ == "ws" || url_.protocol_ == "wss")
        {
            websocket_ = std::make_shared<rai::WebsocketClient>(
                service_, url_.host_, url_.port_, url_.path_,
                url_.protocol_ == "wss");
        }
    }
}

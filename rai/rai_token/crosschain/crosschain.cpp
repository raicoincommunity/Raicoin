#include <rai/rai_token/crosschain/crosschain.hpp>

#include <chrono>
#include <rai/rai_token/crosschain/config.hpp>
#include <rai/rai_token/crosschain/parsers/evm.hpp>
#include <rai/rai_token/token.hpp>

rai::CrossChainParser::CrossChainParser(
    const std::shared_ptr<rai::BaseParser>& parser)
    : chain_(parser->Chain()), wakeup_(), parser_(parser)
{
}

rai::CrossChain::CrossChain(rai::Token& token)
    : token_(token), stopped_(false), thread_([this]() { Run(); })
{
    const rai::CrossChainConfig& config = token.config_.cross_chain_;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!config.eth_urls_.empty())
        {
            parsers_.insert(
                rai::CrossChainParser(std::make_shared<rai::EvmParser>(
                    token, config.eth_urls_, rai::Chain::ETHEREUM, 12, 96,
                    ""))); // todo:
        }

        if (!config.bsc_urls_.empty())
        {
            parsers_.insert(
                rai::CrossChainParser(std::make_shared<rai::EvmParser>(
                    token, config.bsc_urls_, rai::Chain::BINANCE_SMART_CHAIN, 5,
                    30, ""))); // todo:
        }

        if (!config.eth_test_goerli_urls_.empty())
        {
            parsers_.insert(
                rai::CrossChainParser(std::make_shared<rai::EvmParser>(
                    token, config.eth_test_goerli_urls_,
                    rai::Chain::ETHEREUM_TEST_GOERLI, 12, 96,
                    "0xae9f9cA3eABE4AEdaaa26f0522EaD246B769ca5f")));
        }

        if (!config.bsc_test_urls_.empty())
        {
            parsers_.insert(
                rai::CrossChainParser(std::make_shared<rai::EvmParser>(
                    token, config.bsc_test_urls_,
                    rai::Chain::BINANCE_SMART_CHAIN_TEST, 5, 30,
                    "0x3e729788b5e12CC43c4B62345075b3654129a009")));
        }
    }
    condition_.notify_all();
}

rai::CrossChain::~CrossChain()
{
    Stop();
}

void rai::CrossChain::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        if (parsers_.empty())
        {
            condition_.wait(lock);
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        auto it = parsers_.begin();
        if (it->wakeup_ > now)
        {
            condition_.wait_until(lock, it->wakeup_);
            continue;
        }

        it->parser_->Run();
        uint64_t delay = it->parser_->Delay();
        parsers_.modify(it, [now, delay](CrossChainParser& data) {
            data.wakeup_ = now + std::chrono::seconds(delay);
        });
    }
}

void rai::CrossChain::Stop()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
    }

    condition_.notify_all();
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void rai::CrossChain::Status(rai::Ptree& ptree) const
{
    std::unique_lock<std::mutex> lock(mutex_);
    rai::Ptree parsers;
    auto now = std::chrono::steady_clock::now();
    for (const auto& i : parsers_)
    {
        rai::Ptree entry;
        i.parser_->Status(entry);
        auto delay = now >= i.wakeup_
                         ? 0
                         : std::chrono::duration_cast<std::chrono::seconds>(
                               i.wakeup_ - now)
                               .count();
        ptree.put("wakeup", std::to_string(delay) + " seconds later");
        parsers.push_back(std::make_pair("", entry));
    }
    ptree.add_child("parsers", parsers);
    ptree.put("parser_count", std::to_string(parsers_.size()));
}

std::shared_ptr<rai::BaseParser> rai::CrossChain::Parser(rai::Chain chain) const
{
    std::unique_lock<std::mutex> lock(mutex_);
    const auto& index = parsers_.get<1>();
    auto it = index.find(chain);
    if (it == index.end())
    {
        return nullptr;
    }
    return it->parser_;
}

void rai::CrossChain::RegisterEventObserver(
    const std::function<void(const std::shared_ptr<rai::CrossChainEvent>&,
                             bool)>& observer)
{
    std::unique_lock<std::mutex> lock(mutex_);
    for (auto& i : parsers_)
    {
        i.parser_->event_observer_ = observer;
    }
}

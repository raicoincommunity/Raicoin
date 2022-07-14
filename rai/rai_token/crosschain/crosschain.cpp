#include <rai/rai_token/crosschain/crosschain.hpp>

#include <chrono>
#include <rai/rai_token/crosschain/config.hpp>
#include <rai/rai_token/crosschain/parsers/evm.hpp>
#include <rai/rai_token/token.hpp>

rai::CrossChainParser::CrossChainParser(
    const std::shared_ptr<rai::BaseParser>& parser)
    : wakeup_(), parser_(parser)
{
}

rai::CrossChain::CrossChain(rai::Token& token)
    : token_(token), stopped_(false), thread_([this]() { Run(); })
{
    const rai::CrossChainConfig& config = token.config_.cross_chain_;

    std::unique_lock<std::mutex> lock(mutex_);
    if (!config.eth_urls_.empty())
    {
        parsers_.insert(rai::CrossChainParser(std::make_shared<rai::EvmParser>(
            token, config.eth_urls_, rai::Chain::ETHEREUM, 10, 30, "")));
    }

    if (!config.bsc_urls_.empty())
    {
        parsers_.insert(rai::CrossChainParser(std::make_shared<rai::EvmParser>(
            token, config.bsc_urls_, rai::Chain::BINANCE_SMART_CHAIN, 5, 30,
            "")));
    }

    if (!config.eth_test_goerli_urls_.empty())
    {
        parsers_.insert(rai::CrossChainParser(std::make_shared<rai::EvmParser>(
            token, config.eth_test_goerli_urls_,
            rai::Chain::ETHEREUM_TEST_GOERLI, 10, 30, "")));
    }

    if (!config.bsc_test_urls_.empty())
    {
        parsers_.insert(rai::CrossChainParser(std::make_shared<rai::EvmParser>(
            token, config.bsc_test_urls_, rai::Chain::BINANCE_SMART_CHAIN_TEST,
            5, 30, "0xC777f5b390E79c9634c9d07AF45Dc44b11893055")));
    }
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

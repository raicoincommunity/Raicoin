#pragma once

#include <mutex>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <rai/rai_token/crosschain/parsers/base.hpp>

namespace rai
{

class Token;

class CrossChainParser
{
public:
    CrossChainParser(const std::shared_ptr<rai::BaseParser>&);

    rai::Chain chain_;
    std::chrono::steady_clock::time_point wakeup_;
    std::shared_ptr<rai::BaseParser> parser_;
};

class CrossChain
{
public:
    CrossChain(rai::Token&);
    ~CrossChain();

    void Run();
    void Stop();
    void Status(rai::Ptree&) const;
    std::shared_ptr<rai::BaseParser> Parser(rai::Chain) const;

    rai::Token& token_;

private:
    mutable std::mutex mutex_;
    bool stopped_;
    boost::multi_index_container<
        CrossChainParser,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                CrossChainParser, std::chrono::steady_clock::time_point,
                &CrossChainParser::wakeup_>>,
            boost::multi_index::ordered_unique<boost::multi_index::member<
                CrossChainParser, rai::Chain, &CrossChainParser::chain_>>>>
        parsers_;
    std::condition_variable condition_;
    std::thread thread_;
};

} // namespace rai
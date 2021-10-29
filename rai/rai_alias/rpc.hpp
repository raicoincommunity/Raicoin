#pragma once

#include <map>
#include <vector>
#include <rai/secure/ledger.hpp>
#include <rai/app/rpc.hpp>

namespace rai
{

class Alias;
class AliasRpcHandler : public AppRpcHandler
{
public:
    AliasRpcHandler(rai::Alias&, const rai::UniqueId&, bool, rai::Rpc&,
                    const std::string&, const boost::asio::ip::address_v4&,
                    const std::function<void(const rai::Ptree&)>&);
    virtual ~AliasRpcHandler() = default;

    void ProcessImpl() override;

    void Stop() override;

    void AliasQuery();
    void AliasSearch();
    void LedgerVersion();

private:
    rai::Alias& alias_;

    class SearchEntry
    {
    public:
        SearchEntry() = default;
        SearchEntry(const rai::Account&, const std::vector<uint8_t>&,
                    const std::vector<uint8_t>&);
        rai::Ptree Json() const;

        rai::Account account_;
        std::string name_;
        std::string dns_;
    };

    typedef std::multimap<uint16_t, SearchEntry, std::greater<uint16_t>>
        SearchResult;

    static size_t constexpr MAX_SEARCHS_PER_QUERY = 1000;


    void AliasSearchByDns_(const std::string&, const std::string&, uint64_t);
    void AliasSearchByName_(const std::string&, uint64_t);
    bool GetSearchEntry_(rai::Transaction&, const rai::Account&, uint16_t&,
                         SearchEntry&);
};

}  // namespace rai
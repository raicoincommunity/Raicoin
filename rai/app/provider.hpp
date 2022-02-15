#pragma once

#include <vector>
#include <rai/common/util.hpp>

namespace rai
{
class Provider
{
public:
    Provider() = delete;

    enum class Id 
    {
        INVALID         = 0,
        ALIAS           = 1,
        TOKEN           = 2,
    };
    static std::string ToString(Id);

    enum class Filter
    {
        INVALID         = 0,
        APP_ACCOUNT     = 1,

    };
    static std::string ToString(Filter);
    static void PutFilter(rai::Ptree&, Filter, const std::string&);

    enum class Action
    {
        INVALID                 = 0,
        APP_SERVICE_SUBSCRIBE   = 1,
        APP_ACCOUNT_SYNC        = 2,

        ALIAS_QUERY             = 100,
        ALIAS_SEARCH            = 101,
        ALIAS_CHANGE            = 102,

        TOKEN_ACCOUNT_TOKENS_INFO           = 200,
        TOKEN_ACCOUNT_TOKEN_LINK            = 201,
        TOKEN_BLOCK                         = 202,
        TOKEN_NEXT_ACCOUNT_TOKEN_LINKS      = 203,
        TOKEN_NEXT_TOKEN_BLOCKS             = 204,
        TOKEN_PREVIOUS_ACCOUNT_TOKEN_LINKS  = 205,
        TOKEN_PREVIOUS_TOKEN_BLOCKS         = 206,
        TOKEN_RECEIVABLE                    = 207,
        TOKEN_RECEIVABLES                   = 208,
        TOKEN_INFO                          = 209,
        TOKEN_RECEIVED                      = 210,
        TOKEN_MAX_ID                        = 211,
        TOKEN_ID_INFO                       = 212,
        TOKEN_ACCOUNT_TOKEN_IDS             = 213,
        TOKEN_ID_TRANSFER                   = 214,
    };
    static std::string ToString(Action);

    template <typename T,
              typename std::enable_if<std::is_same<T, Filter>::value
                                          || std::is_same<T, Action>::value,
                                      int>::type = 0>
    static std::vector<std::string> ToString(const std::vector<T>& v)
    {
        std::vector<std::string> result;
        for (auto i : v)
        {
            std::string str = rai::Provider::ToString(i);
            if (str.empty() || str == "invalid")
            {
                return std::vector<std::string>();
            }

            if (rai::Contain(result, str))
            {
                continue;
            }

            result.push_back(str);
        }
        return result;
    }

    static void PutId(rai::Ptree& ptree, Id, const std::string& = "service");
    static void AppendFilter(rai::Ptree&, Filter, const std::string&);
    static void PutAction(rai::Ptree&, Action, const std::string& = "notify");

    struct Info
    {
        Id id_;
        std::vector<Filter> filters_;
        std::vector<Action> actions_;
    };

};
}
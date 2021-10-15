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
        INVALID             = 0,
        APP_ACCOUNT_SYNC    = 1,
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
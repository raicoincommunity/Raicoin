#pragma once

#include <vector>
#include <rai/common/util.hpp>

namespace
{
template <typename T>
struct is_filter : std::false_type
{
};

template <>
struct is_filter<rai::Provider::Filter> : std::true_type
{
};

template <typename T>
struct is_action : std::false_type
{
};

template <>
struct is_action<rai::Provider::Action> : std::true_type
{
};

}  // namespace

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
              typename std::enable_if<
                  is_filter<T>::value || is_action<T>::value, int>::type = 0>
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
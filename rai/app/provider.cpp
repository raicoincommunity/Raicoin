#include <rai/app/provider.hpp>

std::string rai::Provider::ToString(Id id)
{
    switch (id)
    {
        case rai::Provider::Id::INVALID:
        {
            return "invalid";
        }
        case rai::Provider::Id::ALIAS:
        {
            return "alias";
        }
        case rai::Provider::Id::TOKEN:
        {
            return "token";
        }
        default:
        {
            return "";
        }
    }
}

std::string rai::Provider::ToString(Filter filter)
{
    switch (filter)
    {
        case rai::Provider::Filter::INVALID:
        {
            return "invalid";
        }
        case rai::Provider::Filter::APP_ACCOUNT:
        {
            return "account";
        }
        default:
        {
            return "";
        }
    }
}

std::string rai::Provider::ToString(Action action)
{
    switch (action)
    {
        case rai::Provider::Action::INVALID:
        {
            return "invalid";
        }
        case rai::Provider::Action::APP_SERVICE_SUBSCRIBE:
        {
            return "service_subscribe";
        }
        case rai::Provider::Action::APP_ACCOUNT_SYNC:
        {
            return "account_synchronize";
        }
        case rai::Provider::Action::ALIAS_QUERY:
        {
            return "alias_query";
        }
        case rai::Provider::Action::ALIAS_SEARCH:
        {
            return "alias_search";
        }
        case rai::Provider::Action::ALIAS_CHANGE:
        {
            return "alias_change";
        }
        case rai::Provider::Action::TOKEN_ACCOUNT_TOKENS_INFO:
        {
            return "account_tokens_info";
        }
        default:
        {
            return "";
        }
    }
}

void rai::Provider::PutId(rai::Ptree& ptree, rai::Provider::Id id,
                          const std::string& path)
{
    ptree.put(path, rai::Provider::ToString(id));
}

void rai::Provider::AppendFilter(rai::Ptree& ptree, Filter filter,
                              const std::string& value)
{
    const boost::property_tree::path path("filters");
    if (!ptree.get_child_optional(path))
    {
        ptree.put_child(path, rai::Ptree());
    }
    rai::Ptree entry;
    entry.put("key", rai::Provider::ToString(filter));
    entry.put("value", value);
    rai::Ptree& filters = ptree.get_child(path);
    filters.push_back(std::make_pair("", entry));
}

void rai::Provider::PutAction(rai::Ptree& ptree, Action action,
                              const std::string& path)
{
    ptree.put(path, rai::Provider::ToString(action));
}

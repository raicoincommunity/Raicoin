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
        case rai::Provider::Filter::APP_TOPIC:
        {
            return "topic";
        }
        default:
        {
            return "";
        }
    }
}

rai::Provider::Filter rai::Provider::ToFilter(const std::string& str)
{
    if (str == "account")
    {
        return rai::Provider::Filter::APP_ACCOUNT;
    }
    else if (str == "topic")
    {
        return rai::Provider::Filter::APP_TOPIC;
    }
    else
    {
        return rai::Provider::Filter::INVALID;
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
        case rai::Provider::Action::APP_ACCOUNT_HEAD:
        {
            return "account_head";
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
        case rai::Provider::Action::TOKEN_ACCOUNT_TOKEN_LINK:
        {
            return "account_token_link";
        }
        case rai::Provider::Action::TOKEN_BLOCK:
        {
            return "token_block";
        }
        case rai::Provider::Action::TOKEN_NEXT_ACCOUNT_TOKEN_LINKS:
        {
            return "next_account_token_links";
        }
        case rai::Provider::Action::TOKEN_NEXT_TOKEN_BLOCKS:
        {
            return "next_token_blocks";
        }
        case rai::Provider::Action::TOKEN_PREVIOUS_ACCOUNT_TOKEN_LINKS:
        {
            return "previous_account_token_links";
        }
        case rai::Provider::Action::TOKEN_PREVIOUS_TOKEN_BLOCKS:
        {
            return "previous_token_blocks";
        }
        case rai::Provider::Action::TOKEN_RECEIVABLE:
        {
            return "token_receivable";
        }
        case rai::Provider::Action::TOKEN_RECEIVABLES:
        {
            return "token_receivables";
        }
        case rai::Provider::Action::TOKEN_INFO:
        {
            return "token_info";
        }
        case rai::Provider::Action::TOKEN_RECEIVED:
        {
            return "token_received";
        }
        case rai::Provider::Action::TOKEN_MAX_ID:
        {
            return "token_max_id";
        }
        case rai::Provider::Action::TOKEN_ID_INFO:
        {
            return "token_id_info";
        }
        case rai::Provider::Action::TOKEN_ACCOUNT_TOKEN_IDS:
        {
            return "account_token_ids";
        }
        case rai::Provider::Action::TOKEN_ID_TRANSFER:
        {
            return "token_id_transfer";
        }
        case rai::Provider::Action::TOKEN_RECEIVABLES_SUMMARY:
        {
            return "token_receivables_summary";
        }
        case rai::Provider::Action::TOKEN_SWAP_INFO:
        {
            return "swap_info";
        }
        case rai::Provider::Action::TOKEN_ORDER_INFO:
        {
            return "order_info";
        }
        case rai::Provider::Action::TOKEN_ACCOUNT_SWAP_INFO:
        {
            return "account_swap_info";
        }
        case rai::Provider::Action::TOKEN_SWAP_MAIN_ACCOUNT:
        {
            return "swap_main_account";
        }
        case rai::Provider::Action::TOKEN_ACCOUNT_ORDERS:
        {
            return "account_orders";
        }
        case rai::Provider::Action::TOKEN_ORDER_SWAPS:
        {
            return "order_swaps";
        }
        case rai::Provider::Action::TOKEN_ACCOUNT_BALANCE:
        {
            return "account_token_balance";
        }
        case rai::Provider::Action::TOKEN_ACCOUNT_ACTIVE_SWAPS:
        {
            return "account_active_swaps";
        }
        case rai::Provider::Action::TOKEN_TAKE_NACK_BLOCK_SUBMITTED:
        {
            return "take_nack_block_submitted";
        }
        case rai::Provider::Action::TOKEN_MAKER_SWAPS:
        {
            return "maker_swaps";
        }
        case rai::Provider::Action::TOKEN_TAKER_SWAPS:
        {
            return "taker_swaps";
        }
        case rai::Provider::Action::TOKEN_SEARCH_ORDERS:
        {
            return "search_orders";
        }
        case rai::Provider::Action::TOKEN_SUBMIT_TAKE_NACK_BLOCK:
        {
            return "submit_take_nack_block";
        }
        case rai::Provider::Action::TOKEN_ID_OWNER:
        {
            return "token_id_owner";
        }
        case rai::Provider::Action::TOKEN_UNMAP_INFO:
        {
            return "token_unmap_info";
        }
        case rai::Provider::Action::TOKEN_WRAP_INFO:
        {
            return "token_wrap_info";
        }
        case rai::Provider::Action::TOKEN_MAP_INFO:
        {
            return "token_map_info";
        }
        case rai::Provider::Action::TOKEN_MAP_INFOS:
        {
            return "token_map_infos";
        }
        case rai::Provider::Action::TOKEN_PENDING_MAP_INFOS:
        {
            return "pending_token_map_infos";
        }
        case rai::Provider::Action::TOKEN_UNMAP_INFOS:
        {
            return "token_unmap_infos";
        }
        case rai::Provider::Action::TOKEN_PENDING_MAP_INFO:
        {
            return "pending_token_map_info";
        }
        case rai::Provider::Action::TOKEN_WRAP_INFOS:
        {
            return "token_wrap_infos";
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

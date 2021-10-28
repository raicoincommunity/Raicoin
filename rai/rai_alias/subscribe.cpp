#include <rai/rai_alias/subscribe.hpp>

#include <rai/rai_alias/alias.hpp>

rai::AliasSubscriptions::AliasSubscriptions(rai::Alias& alias)
    : AppSubscriptions(alias), alias_(alias)
{
    alias_.observers_.alias_.Add([this](const rai::Account& account,
                                        const std::string& name,
                                        const std::string& dns) {
        using P = rai::Provider;
        rai::Ptree ptree;
        P::PutAction(ptree, P::Action::ALIAS_CHANGE);
        P::PutId(ptree, alias_.provider_info_.id_);
        P::AppendFilter(ptree, P::Filter::APP_ACCOUNT, account.StringAccount());
        ptree.put("account", account.StringAccount());
        ptree.put("name", name);
        ptree.put("dns", dns);
        Notify(account, ptree);
    });
}
#include <rai/rai_token/rpc.hpp>

#include <rai/rai_token/token.hpp>

rai::TokenRpcHandler::TokenRpcHandler(
    rai::Token& token, const rai::UniqueId& uid, bool check, rai::Rpc& rpc,
    const std::string& body, const boost::asio::ip::address_v4& ip,
    const std::function<void(const rai::Ptree&)>& send_response)
    : rai::AppRpcHandler(token, uid, check, rpc, body, ip, send_response),
      token_(token)
{
}

void rai::TokenRpcHandler::ProcessImpl()
{
    std::string action = request_.get<std::string>("action");

    if (action == "account_token_link")
    {
        AccountTokenLink();
    }
    else if (action == "account_token_info")
    {
        AccountTokenInfo();
    }
    if (action == "account_tokens_info")
    {
        AccountTokensInfo();
    }
    else if (action == "ledger_version")
    {
        LedgerVersion();
    }
    else if (action == "next_token_blocks")
    {
        NextTokenBlocks();
    }
    else if (action == "previous_token_blocks")
    {
        PreviousTokenBlocks();
    }
    else if (action == "token_block")
    {
        TokenBlock();
    }
    else if (action == "stop")
    {
        if (!CheckLocal_())
        {
            Stop();
        }
    }
    else
    {
        AppRpcHandler::ProcessImpl();
    }
}

void rai::TokenRpcHandler::Stop()
{
    token_.Stop();
    response_.put("success", "");
}

void rai::TokenRpcHandler::AccountTokenLink()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    rai::Chain chain;
    error = GetChain_(chain);
    IF_ERROR_RETURN_VOID(error);
    response_.put("chain", rai::ChainToString(chain));

    rai::TokenAddress address;
    error = GetTokenAddress_(chain, address);
    IF_ERROR_RETURN_VOID(error);
    response_.put("address", rai::TokenAddressToString(chain, address));

    uint64_t height;
    error = GetHeight_(height);
    IF_ERROR_RETURN_VOID(error);
    response_.put("height", std::to_string(height));

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    uint64_t previous_height;
    uint64_t successor_height;
    rai::AccountTokenLink link(account, chain, address, height);
    error = token_.ledger_.AccountTokenLinkGet(
        transaction, link, previous_height, successor_height);
    if (error)
    {
        response_.put("error", "The account token link does not exist");
        return;
    }
    response_.put("previous_height", std::to_string(previous_height));
    response_.put("successor_height", std::to_string(successor_height));

    rai::TokenBlock token_block;
    error =
        token_.ledger_.TokenBlockGet(transaction, account, height, token_block);
    if (error)
    {
        response_.put("error", "Get token block failed");
        return;
    }
    response_.put("status", rai::ErrorString(token_block.status_));
    response_.put("status_code",
                  std::to_string(static_cast<uint32_t>(token_block.status_)));
    response_.put("hash", token_block.hash_.StringHex());

    std::shared_ptr<rai::Block> block(nullptr);
    error = token_.ledger_.BlockGet(transaction, token_block.hash_, block);
    if (error)
    {
        response_.put("error", "Get block failed");
        return;
    }
    rai::Ptree block_ptree;
    block->SerializeJson(block_ptree);
    response_.put_child("block", block_ptree);
}

void rai::TokenRpcHandler::AccountTokenInfo()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    rai::Chain chain;
    error = GetChain_(chain);
    IF_ERROR_RETURN_VOID(error);
    response_.put("chain", rai::ChainToString(chain));

    rai::TokenAddress address;
    error = GetTokenAddress_(chain, address);
    IF_ERROR_RETURN_VOID(error);
    response_.put("address", rai::TokenAddressToString(chain, address));
    response_.put("address_raw", address.StringHex());

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::AccountTokenInfo info;
    error = token_.ledger_.AccountTokenInfoGet(transaction, account, chain,
                                               address, info);
    if (error)
    {
        response_.put("error", "The account token info does not exist");
        return;
    }

    rai::TokenInfo token_info;
    error = token_.ledger_.TokenInfoGet(
        transaction, rai::TokenKey(chain, address), token_info);
    if (error)
    {
        response_.put(
            "error",
            rai::ToString("Get token info failed, token.chain=",
                          rai::ChainToString(chain), ", token.address=",
                          rai::TokenAddressToString(chain, address)));
        return;
    }

    response_.put("name", token_info.name_);
    response_.put("symbol", token_info.symbol_);
    response_.put("type", rai::TokenTypeToString(token_info.type_));
    response_.put("decimals", token_info.decimals_);

    response_.put("balance", info.balance_.StringDec());
    response_.put(
        "balance_formatted",
        info.balance_.StringBalance(token_info.decimals_, token_info.symbol_));
    response_.put("head_height", std::to_string(info.head_));
    response_.put("token_block_count", std::to_string(info.blocks_));
}

void rai::TokenRpcHandler::AccountTokensInfo()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::AccountTokensInfo info;
    error = token_.ledger_.AccountTokensInfoGet(transaction, account, info);
    if (error)
    {
        response_.put("error", "The account does not have any token yet");
        return;
    }

    response_.put("head_height", std::to_string(info.head_));
    response_.put("token_block_count", std::to_string(info.blocks_));

    rai::Ptree tokens;
    size_t count = 0;
    rai::Iterator i =
        token_.ledger_.AccountTokenInfoLowerBound(transaction, account);
    rai::Iterator n =
        token_.ledger_.AccountTokenInfoUpperBound(transaction, account);
    for (; i != n; ++i)
    {
        rai::Account account_l;
        rai::Chain chain;
        rai::TokenAddress address;
        rai::AccountTokenInfo account_token_info;
        error = token_.ledger_.AccountTokenInfoGet(i, account_l, chain, address,
                                                   account_token_info);
        if (error)
        {
            response_.put("error", "Get account token info failed");
            return;
        }
        if (account_l != account)
        {
            response_.put("error", "Account mismatch");
            return;
        }

        rai::TokenInfo token_info;
        error = token_.ledger_.TokenInfoGet(
            transaction, rai::TokenKey(chain, address), token_info);
        if (error)
        {
            response_.put(
                "error",
                rai::ToString("Get token info failed, token.chain=",
                              rai::ChainToString(chain), ", token.address=",
                              rai::TokenAddressToString(chain, address)));
            return;
        }

        rai::Ptree token;
        token.put("chain", rai::ChainToString(chain));
        token.put("address", rai::TokenAddressToString(chain, address));
        token.put("address_raw", address.StringHex());
        token.put("name", token_info.name_);
        token.put("symbol", token_info.symbol_);
        token.put("type", rai::TokenTypeToString(token_info.type_));
        token.put("decimals", token_info.decimals_);

        rai::Ptree entry;
        entry.put_child("token", token);
        entry.put("balance", account_token_info.balance_.StringDec());
        entry.put("balance_formatted",
                  account_token_info.balance_.StringBalance(
                      token_info.decimals_, token_info.symbol_));
        entry.put("head_height", std::to_string(account_token_info.head_));
        entry.put("token_block_count",
                  std::to_string(account_token_info.blocks_));
        count++;
        tokens.push_back(std::make_pair("", entry));
    }
    response_.put_child("tokens", tokens);
    response_.put("token_count", std::to_string(count));
}

void rai::TokenRpcHandler::LedgerVersion()
{
    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    uint32_t version = 0;
    bool error = token_.ledger_.VersionGet(transaction, version);
    if (error)
    {
        error_code_ = rai::ErrorCode::TOKEN_LEDGER_GET;
        return;
    }

    response_.put("verison", std::to_string(version));
}

void rai::TokenRpcHandler::NextTokenBlocks()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    uint64_t height;
    error = GetHeight_(height);
    IF_ERROR_RETURN_VOID(error);
    response_.put("height", std::to_string(height));

    uint64_t count;
    error = GetCount_(count);
    if (error || count == 0) count = 10;
    if (count > 100) count = 100;

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    uint64_t successor;
    error = token_.ledger_.TokenBlockSuccessorGet(transaction, account, height,
                                                  successor);
    if (error)
    {
        response_.put("error", "The current token block does not exist");
        return;
    }

    rai::Ptree token_blocks;
    while (successor != rai::Block::INVALID_HEIGHT && count > 0)
    {
        --count;

        rai::Ptree entry;
        entry.put("height", std::to_string(successor));

        rai::TokenBlock token_block;
        error = token_.ledger_.TokenBlockGet(transaction, account, successor,
                                             token_block, successor);
        if (error)
        {
            response_.put(
                "error",
                rai::ToString("Get token block failed, height=", successor));
            return;
        }
        entry.put("status", rai::ErrorString(token_block.status_));
        entry.put("status_code",
                    std::to_string(static_cast<uint32_t>(token_block.status_)));
        entry.put("previous_height", std::to_string(token_block.previous_));
        entry.put("successor_height", std::to_string(successor));
        entry.put("hash", token_block.hash_.StringHex());

        std::shared_ptr<rai::Block> block(nullptr);
        error = token_.ledger_.BlockGet(transaction, token_block.hash_, block);
        if (error)
        {
            response_.put("error",
                          rai::ToString("Get block failed, hash=",
                                        token_block.hash_.StringHex()));
            return;
        }
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        entry.put_child("block", block_ptree);

        token_blocks.push_back(std::make_pair("", entry));
    }

    response_.put_child("token_blocks", token_blocks);
}

void rai::TokenRpcHandler::PreviousTokenBlocks()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    uint64_t height;
    error = GetHeight_(height);
    IF_ERROR_RETURN_VOID(error);
    response_.put("height", std::to_string(height));

    uint64_t count;
    error = GetCount_(count);
    if (error || count == 0) count = 10;
    if (count > 100) count = 100;

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    rai::TokenBlock token_block;
    error =
        token_.ledger_.TokenBlockGet(transaction, account, height, token_block);
    if (error)
    {
        response_.put("error", "The current token block does not exist");
        return;
    }
    uint64_t previous = token_block.previous_;

    rai::Ptree token_blocks;
    while (previous != rai::Block::INVALID_HEIGHT && count > 0)
    {
        --count;

        rai::Ptree entry;
        entry.put("height", std::to_string(previous));

        uint64_t successor;
        error = token_.ledger_.TokenBlockGet(transaction, account, previous,
                                             token_block, successor);
        if (error)
        {
            response_.put(
                "error",
                rai::ToString("Get token block failed, height=", previous));
            return;
        }
        previous = token_block.previous_;
        entry.put("status", rai::ErrorString(token_block.status_));
        entry.put("status_code",
                    std::to_string(static_cast<uint32_t>(token_block.status_)));
        entry.put("previous_height", std::to_string(token_block.previous_));
        entry.put("successor_height", std::to_string(successor));
        entry.put("hash", token_block.hash_.StringHex());

        std::shared_ptr<rai::Block> block(nullptr);
        error = token_.ledger_.BlockGet(transaction, token_block.hash_, block);
        if (error)
        {
            response_.put("error",
                          rai::ToString("Get block failed, hash=",
                                        token_block.hash_.StringHex()));
            return;
        }
        rai::Ptree block_ptree;
        block->SerializeJson(block_ptree);
        entry.put_child("block", block_ptree);

        token_blocks.push_back(std::make_pair("", entry));
    }

    response_.put_child("token_blocks", token_blocks);
}

void rai::TokenRpcHandler::TokenBlock()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    uint64_t height;
    error = GetHeight_(height);
    IF_ERROR_RETURN_VOID(error);
    response_.put("height", std::to_string(height));

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::TokenBlock token_block;
    uint64_t successor_height;
    error = token_.ledger_.TokenBlockGet(transaction, account, height,
                                         token_block, successor_height);
    if (error)
    {
        response_.put("error", "The token block does not exist");
        return;
    }
    response_.put("status", rai::ErrorString(token_block.status_));
    response_.put("status_code",
                  std::to_string(static_cast<uint32_t>(token_block.status_)));
    response_.put("previous_height", std::to_string(token_block.previous_));
    response_.put("successor_height", std::to_string(successor_height));
    response_.put("hash", token_block.hash_.StringHex());

    std::shared_ptr<rai::Block> block(nullptr);
    error = token_.ledger_.BlockGet(transaction, token_block.hash_, block);
    if (error)
    {
        response_.put("error", "Get block failed");
        return;
    }
    rai::Ptree block_ptree;
    block->SerializeJson(block_ptree);
    response_.put_child("block", block_ptree);
}

bool rai::TokenRpcHandler::GetChain_(rai::Chain& chain)
{
    auto chain_o = request_.get_optional<std::string>("chain");
    if (!chain_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_CHAIN;
        return true;
    }

    chain = rai::StringToChain(*chain_o);
    if (chain == rai::Chain::INVALID)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_CHAIN;
        return true;
    }

    return false;
}

bool rai::TokenRpcHandler::GetTokenAddress_(rai::Chain chain,
                                            rai::TokenAddress& address)
{
    auto address_raw_o = request_.get_optional<std::string>("address_raw");
    if (address_raw_o)
    {
        bool error = address.DecodeHex(*address_raw_o);
        if (error)
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_ADDRESS_RAW;
            return true;
        }
        return false;
    }

    auto address_o = request_.get_optional<std::string>("address");
    if (!address_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_ADDRESS;
        return true;
    }
    bool error = rai::StringToTokenAddress(chain, *address_o, address);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_ADDRESS;
        return true;
    }
    return false;
}

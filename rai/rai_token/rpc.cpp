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
    else if (action == "account_token_ids")
    {
        AccountTokenIds();
    }
    else if (action == "account_tokens_info")
    {
        AccountTokensInfo();
    }
    else if (action == "ledger_version")
    {
        LedgerVersion();
    }
    else if (action == "next_account_token_links")
    {
        NextAccountTokenLinks();
    }
    else if (action == "next_token_blocks")
    {
        NextTokenBlocks();
    }
    else if (action == "previous_account_token_links")
    {
        PreviousAccountTokenLinks();
    }
    else if (action == "previous_token_blocks")
    {
        PreviousTokenBlocks();
    }
    else if (action == "token_block")
    {
        TokenBlock();
    }
    else if (action == "token_id_info")
    {
        TokenIdInfo();
    }
    else if (action == "token_info")
    {
        TokenInfo();
    }
    else if (action == "token_max_id")
    {
        TokenMaxId();
    }
    else if (action == "token_receivables")
    {
        TokenReceivables();
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
    response_.put("address_raw", address.StringHex());

    uint64_t height;
    error = GetHeight_(height);
    IF_ERROR_RETURN_VOID(error);

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
    PutTokenBlock_(transaction, height, token_block, response_);
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

    rai::TokenKey key(chain, address);
    rai::TokenInfo token_info;
    error = token_.ledger_.TokenInfoGet(transaction, key, token_info);
    if (error)
    {
        response_.put(
            "error",
            rai::ToString("Get token info failed, token.chain=",
                          rai::ChainToString(chain), ", token.address=",
                          rai::TokenAddressToString(chain, address)));
        return;
    }

    token_.MakeAccountTokenInfoPtree(key, token_info, info, response_);
}

void rai::TokenRpcHandler::AccountTokenIds()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    rai::Chain chain;
    error = GetChain_(chain);
    IF_ERROR_RETURN_VOID(error);

    rai::TokenAddress address;
    error = GetTokenAddress_(chain, address);
    IF_ERROR_RETURN_VOID(error);
    rai::TokenKey key(chain, address);
    token_.TokenKeyToPtree(key, response_);

    uint64_t count;
    error = GetCount_(count);
    error_code_ = rai::ErrorCode::SUCCESS;
    if (error || count == 0) count = 10;
    if (count > 100) count = 100;

    rai::TokenValue begin_id;
    error = GetTokenId_(begin_id, "begin_id");
    if (error)
    {
        error_code_ = rai::ErrorCode::SUCCESS;
        begin_id = 0;
    }
    rai::AccountTokenId begin(account, key, begin_id);

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::Ptree ids;
    rai::Iterator i =
        token_.ledger_.AccountTokenIdLowerBound(transaction, begin);
    rai::Iterator n =
        token_.ledger_.AccountTokenIdUpperBound(transaction, account, key);
    for (; i != n && count > 0; ++i, --count)
    {
        rai::Ptree entry;
        rai::AccountTokenId id;
        uint64_t receive_at;
        error = token_.ledger_.AccountTokenIdGet(i, id, receive_at);
        if (error)
        {
            response_.put("error",
                          "Failed to get account token id from ledger");
            return;
        }
        entry.put("token_id", id.id_.StringDec());
        entry.put("account", id.account_.StringAccount());
        token_.TokenKeyToPtree(id.token_, entry);
        entry.put("receive_at", std::to_string(receive_at));
        rai::TokenIdInfo info;
        error =
            token_.ledger_.TokenIdInfoGet(transaction, id.token_, id.id_, info);
        if (error)
        {
            response_.put("error", "Failed to get token id info from ledger");
            return;
        }
        token_.TokenIdInfoToPtree(info, entry);

        ids.push_back(std::make_pair("", entry));
    }
    response_.put_child("ids", ids);
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

        rai::TokenKey key(chain, address);
        rai::TokenInfo token_info;
        error = token_.ledger_.TokenInfoGet(transaction, key, token_info);
        if (error)
        {
            response_.put(
                "error",
                rai::ToString("Get token info failed, token.chain=",
                              rai::ChainToString(chain), ", token.address=",
                              rai::TokenAddressToString(chain, address)));
            return;
        }

        rai::Ptree entry;
        token_.MakeAccountTokenInfoPtree(key, token_info, account_token_info,
                                         entry);
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

void rai::TokenRpcHandler::NextAccountTokenLinks()
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

    uint64_t count;
    error = GetCount_(count);
    error_code_ = rai::ErrorCode::SUCCESS;
    if (error || count == 0) count = 10;
    if (count > 100) count = 100;

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    uint64_t successor;
    rai::AccountTokenLink link(account, chain, address, height);
    error = token_.ledger_.AccountTokenLinkSuccessorGet(transaction, link,
                                                        successor);
    if (error)
    {
        response_.put("error", "The current account token link does not exist");
        return;
    }

    rai::Ptree token_links;
    while (successor != rai::Block::INVALID_HEIGHT && count > 0)
    {
        --count;

        link.height_ = successor;
        uint64_t previous;
        error = token_.ledger_.AccountTokenLinkGet(transaction, link, previous,
                                                   successor);
        if (error)
        {
            response_.put(
                "error", rai::ToString("Get account token link failed, height=",
                                       link.height_));
            return;
        }

        rai::TokenBlock token_block;
        error = token_.ledger_.TokenBlockGet(transaction, account, link.height_,
                                             token_block);
        if (error)
        {
            response_.put(
                "error",
                rai::ToString("Get token block failed, height=", link.height_));
            return;
        }
        rai::Ptree entry;
        error = PutTokenBlock_(transaction, link.height_, token_block, entry);
        IF_ERROR_RETURN_VOID(error);
        entry.put("previous_height", std::to_string(previous));
        entry.put("successor_height", std::to_string(successor));

        token_links.push_back(std::make_pair("", entry));
    }
    response_.put_child("token_links", token_links);
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
    error_code_ = rai::ErrorCode::SUCCESS;
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

        uint64_t height = successor;
        rai::TokenBlock token_block;
        error = token_.ledger_.TokenBlockGet(transaction, account, height,
                                             token_block, successor);
        if (error)
        {
            response_.put(
                "error",
                rai::ToString("Get token block failed, height=", successor));
            return;
        }
        rai::Ptree entry;
        error = PutTokenBlock_(transaction, height, token_block, entry);
        IF_ERROR_RETURN_VOID(error);
        entry.put("previous_height", std::to_string(token_block.previous_));
        entry.put("successor_height", std::to_string(successor));

        token_blocks.push_back(std::make_pair("", entry));
    }

    response_.put_child("token_blocks", token_blocks);
}

void rai::TokenRpcHandler::PreviousAccountTokenLinks()
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

    uint64_t height;
    error = GetHeight_(height);
    IF_ERROR_RETURN_VOID(error);
    response_.put("height", std::to_string(height));

    uint64_t count;
    error = GetCount_(count);
    error_code_ = rai::ErrorCode::SUCCESS;
    if (error || count == 0) count = 10;
    if (count > 100) count = 100;

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    uint64_t previous;
    rai::AccountTokenLink link(account, chain, address, height);
    error = token_.ledger_.AccountTokenLinkGet(transaction, link, previous);
    if (error)
    {
        response_.put("error", "The current account token link does not exist");
        return;
    }

    rai::Ptree token_links;
    while (previous != rai::Block::INVALID_HEIGHT && count > 0)
    {
        --count;

        link.height_ = previous;
        uint64_t successor;
        error = token_.ledger_.AccountTokenLinkGet(transaction, link, previous,
                                                   successor);
        if (error)
        {
            response_.put(
                "error", rai::ToString("Get account token link failed, height=",
                                       link.height_));
            return;
        }

        rai::TokenBlock token_block;
        error = token_.ledger_.TokenBlockGet(transaction, account, link.height_,
                                             token_block);
        if (error)
        {
            response_.put(
                "error",
                rai::ToString("Get token block failed, height=", link.height_));
            return;
        }
        rai::Ptree entry;
        error = PutTokenBlock_(transaction, link.height_, token_block, entry);
        IF_ERROR_RETURN_VOID(error);
        entry.put("previous_height", std::to_string(previous));
        entry.put("successor_height", std::to_string(successor));

        token_links.push_back(std::make_pair("", entry));
    }
    response_.put_child("token_links", token_links);
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
    error_code_ = rai::ErrorCode::SUCCESS;
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
        rai::Ptree entry;
        error = PutTokenBlock_(transaction, previous, token_block, entry);
        IF_ERROR_RETURN_VOID(error);
        previous = token_block.previous_;
        entry.put("previous_height", std::to_string(token_block.previous_));
        entry.put("successor_height", std::to_string(successor));

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
    error = PutTokenBlock_(transaction, height, token_block, response_);
    IF_ERROR_RETURN_VOID(error);
    response_.put("previous_height", std::to_string(token_block.previous_));
    response_.put("successor_height", std::to_string(successor_height));
}

void rai::TokenRpcHandler::TokenIdInfo()
{
    rai::Chain chain;
    bool error = GetChain_(chain);
    IF_ERROR_RETURN_VOID(error);

    rai::TokenAddress address;
    error = GetTokenAddress_(chain, address);
    IF_ERROR_RETURN_VOID(error);
    rai::TokenKey key(chain, address);
    token_.TokenKeyToPtree(key, response_);

    rai::TokenValue id;
    error = GetTokenId_(id);
    IF_ERROR_RETURN_VOID(error);
    response_.put("token_id", id.StringDec());

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::TokenIdInfo info;
    error = token_.ledger_.TokenIdInfoGet(transaction, key, id, info);
    if (error)
    {
        response_.put("error", "missing");
        return;
    }
    
    token_.TokenIdInfoToPtree(info, response_);
}

void rai::TokenRpcHandler::TokenInfo()
{
    rai::Chain chain;
    bool error = GetChain_(chain);
    IF_ERROR_RETURN_VOID(error);
    response_.put("chain", rai::ChainToString(chain));

    rai::TokenAddress address;
    error = GetTokenAddress_(chain, address);
    IF_ERROR_RETURN_VOID(error);
    response_.put("address", rai::TokenAddressToString(chain, address));
    response_.put("address_raw", address.StringHex());

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::TokenInfo info;
    error = token_.ledger_.TokenInfoGet(transaction,
                                        rai::TokenKey(chain, address), info);
    if (error)
    {
        response_.put("error", "The token doesn't exist");
        return;
    }

    token_.TokenInfoToPtree(info, response_);
}

void rai::TokenRpcHandler::TokenMaxId()
{
    rai::Chain chain;
    bool error = GetChain_(chain);
    IF_ERROR_RETURN_VOID(error);
    response_.put("chain", rai::ChainToString(chain));

    rai::TokenAddress address;
    error = GetTokenAddress_(chain, address);
    IF_ERROR_RETURN_VOID(error);
    response_.put("address", rai::TokenAddressToString(chain, address));
    response_.put("address_raw", address.StringHex());

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);
    rai::TokenValue id;
    error = token_.ledger_.MaxTokenIdGet(transaction,
                                         rai::TokenKey(chain, address), id);
    if (error)
    {
        response_.put("error", "missing");
        return;
    }

    response_.put("token_id", id.StringDec());
}


void rai::TokenRpcHandler::TokenReceivables()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    uint64_t count = 0;
    error = GetCount_(count);
    IF_ERROR_RETURN_VOID(error);
    if (count == 0) count = 100;
    if (count > 1000) count = 1000;

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::Ptree receivables;
    rai::Iterator i = token_.ledger_.TokenReceivableLowerBound(transaction, account);
    rai::Iterator n = token_.ledger_.TokenReceivableUpperBound(transaction, account);
    for (; i != n && count > 0; ++i, --count)
    {
        rai::TokenReceivableKey key;
        rai::TokenReceivable receivable;
        error = token_.ledger_.TokenReceivableGet(i, key, receivable);
        if (error)
        {
            response_.put("error", "Get token receivable info failed");
            return;
        }

        rai::TokenInfo info;
        error = token_.ledger_.TokenInfoGet(transaction, key.token_, info);
        if (error)
        {
            response_.put("error",
                          rai::ToString("Get token info failed, token.chain=",
                                        rai::ChainToString(key.token_.chain_),
                                        ", token.address_raw=",
                                        key.token_.address_.StringHex()));
            return;
        }

        std::shared_ptr<rai::Block> block(nullptr);
        if (rai::IsLocalSource(receivable.source_))
        {
            error = token_.ledger_.BlockGet(transaction, key.tx_hash_, block);
            if (error)
            {
                response_.put("error", rai::ToString("Get block failed, hash=",
                                                     key.tx_hash_.StringHex()));
                return;
            }
        }

        rai::Ptree entry;
        token_.MakeReceivablePtree(key, receivable, info, block, entry);
        receivables.push_back(std::make_pair("", entry));
    }
    response_.put_child("receivables", receivables);
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

bool rai::TokenRpcHandler::GetTokenId_(rai::TokenValue& id,
                                       const std::string& key)
{
    auto id_o = request_.get_optional<std::string>(key);
    if (!id_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_TOKEN_ID;
        return true;
    }

    bool error = id.DecodeDec(*id_o);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_TOKEN_ID;
        return true;
    }
    return false;
}

bool rai::TokenRpcHandler::PutTokenBlock_(rai::Transaction& transaction,
                                          uint64_t height,
                                          const rai::TokenBlock& token_block,
                                          rai::Ptree& entry)
{
    entry.put("height", std::to_string(height));
    entry.put("status", rai::ErrorString(token_block.status_));
    entry.put("status_code",
                std::to_string(static_cast<uint32_t>(token_block.status_)));
    entry.put("value", token_block.value_.StringDec());
    std::string value_op = "none";
    if (token_block.value_op_ == rai::TokenBlock::ValueOp::INCREASE)
    {
        value_op = "increase";
    }
    else if (token_block.value_op_ == rai::TokenBlock::ValueOp::DECREASE)
    {
        value_op = "decrease";
    }
    entry.put("value_op", value_op);
    rai::Chain chain = token_block.token_.chain_;
    rai::TokenAddress address = token_block.token_.address_;
    entry.put("chain", rai::ChainToString(chain));
    entry.put("address", rai::TokenAddressToString(chain, address));
    entry.put("address_raw", address.StringHex());
    if (token_block.token_.Valid())
    {
        rai::TokenInfo token_info;
        bool error = token_.ledger_.TokenInfoGet(
            transaction, token_block.token_, token_info);
        if (error)
        {
            response_.put(
                "error",
                rai::ToString("Get token info failed, token.chain=",
                              rai::ChainToString(chain), ", token.address=",
                              rai::TokenAddressToString(chain, address)));
            return true;
        }
        entry.put("name", token_info.name_);
        entry.put("symbol", token_info.symbol_);
        entry.put("type", rai::TokenTypeToString(token_info.type_));
        entry.put("decimals", token_info.decimals_);
    }

    entry.put("hash", token_block.hash_.StringHex());
    std::shared_ptr<rai::Block> block(nullptr);
    bool error = token_.ledger_.BlockGet(transaction, token_block.hash_, block);
    if (error)
    {
        response_.put("error", "Get block failed");
        return true;
    }
    rai::Ptree block_ptree;
    block->SerializeJson(block_ptree);
    entry.put_child("block", block_ptree);
    return false;
}

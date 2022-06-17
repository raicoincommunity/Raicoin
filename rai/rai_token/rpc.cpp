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

    if (action == "account_active_swaps")
    {
        AccountActiveSwaps();
    }
    else if (action == "account_orders")
    {
        AccountOrders();
    }
    else if (action == "account_swap_info")
    {
        AccountSwapInfo();
    }
    else if (action == "account_token_balance")
    {
        AccountTokenBalance();
    }
    else if (action == "account_token_link")
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
    else if (action == "cross_chain_status")
    {
        CrossChainStatus();
    }
    else if (action == "ledger_version")
    {
        LedgerVersion();
    }
    else if (action == "maker_swaps")
    {
        MakerSwaps();
    }
    else if (action == "next_account_token_links")
    {
        NextAccountTokenLinks();
    }
    else if (action == "next_token_blocks")
    {
        NextTokenBlocks();
    }
    else if (action == "order_count")
    {
        OrderCount();
    }
    else if (action == "order_info")
    {
        OrderInfo();
    }
    else if (action == "order_swaps")
    {
        OrderSwaps();
    }
    else if (action == "previous_account_token_links")
    {
        PreviousAccountTokenLinks();
    }
    else if (action == "previous_token_blocks")
    {
        PreviousTokenBlocks();
    }
    else if (action == "search_orders")
    {
        SearchOrders();
    }
    else if (action == "submit_take_nack_block")
    {
        SubmitTakeNackBlock();
    }
    else if (action == "swap_helper_status")
    {
        SwapHelperStatus();
    }
    else if (action == "swap_info")
    {
        SwapInfo();
    }
    else if (action == "swap_main_account")
    {
        SwapMainAccount();
    }
    else if (action == "taker_swaps")
    {
        TakerSwaps();
    }
    else if (action == "token_block")
    {
        TokenBlock();
    }
    else if (action == "token_id_info")
    {
        TokenIdInfo();
    }
    else if (action == "token_id_owner")
    {
        TokenIdOwner();
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
    else if (action == "token_receivables_all")
    {
        TokenReceivablesAll();
    }
    else if (action == "token_receivables_summary")
    {
        TokenReceivablesSummary();
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

void rai::TokenRpcHandler::AccountActiveSwaps()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    uint64_t count = 0;
    uint64_t max = 1000;
    uint64_t now = rai::CurrentTimestamp();
    rai::Ptree swaps;
    rai::Iterator i = token_.ledger_.SwapInfoLowerBound(transaction, account);
    rai::Iterator n = token_.ledger_.SwapInfoUpperBound(transaction, account);
    for (; i != n && count < max; ++i)
    {
        uint64_t height;
        rai::SwapInfo info;
        error = token_.ledger_.SwapInfoGet(i, account, height, info);
        if (error)
        {
            response_.put("error", "Failed to get swap info");
            return;
        }

        std::shared_ptr<rai::Block> block;
        error = token_.ledger_.BlockGet(transaction, info.hash_, block);
        if (error || block == nullptr)
        {
            response_.put("error", "Failed to get block");
            return;
        }

        if (block->Timestamp() < now - 600)
        {
            break;
        }

        if (info.Finished())
        {
            continue;
        }

        std::string error_info;
        rai::Ptree swap;
        error = token_.MakeSwapPtree(transaction, account, height, error_info,
                                     swap);
        if (error)
        {
            response_.put("error", error_info);
            return;
        }

        swaps.push_back(std::make_pair("", swap));
        ++count;
    }

    if (count >= max)
    {
        response_.put_child("swaps", swaps);
        return;
    }

    rai::AccountInfo account_info;
    error = token_.ledger_.AccountInfoGet(transaction, account, account_info);
    if (error || !account_info.Valid())
    {
        response_.put_child("swaps", swaps);
        return;
    }

    uint64_t next = account_info.head_height_ + 1;
    i = token_.ledger_.InquiryWaitingLowerBound(transaction, account, next);
    n = token_.ledger_.InquiryWaitingUpperBound(transaction, account, next);
    for (; i != n && count < max; ++i)
    {
        rai::InquiryWaiting waiting;
        error = token_.ledger_.InquiryWaitingGet(i, waiting);
        if (error)
        {
            response_.put("error", "Failed to get inquiry waiting info");
            return;
        }

        std::string error_info;
        rai::Ptree swap;
        error = token_.MakeSwapPtree(transaction, waiting.taker_,
                                     waiting.inquiry_height_, error_info, swap);
        if (error)
        {
            response_.put("error", error_info);
            return;
        }

        swaps.push_back(std::make_pair("", swap));
        ++count;
    }

    if (count >= max)
    {
        response_.put_child("swaps", swaps);
        return;
    }

    i = token_.ledger_.TakeWaitingLowerBound(transaction, account, next);
    n = token_.ledger_.TakeWaitingUpperBound(transaction, account, next);
    for (; i != n && count < max; ++i)
    {
        rai::TakeWaiting waiting;
        error = token_.ledger_.TakeWaitingGet(i, waiting);
        if (error)
        {
            response_.put("error", "Failed to get take waiting info");
            return;
        }

        std::string error_info;
        rai::Ptree swap;
        error = token_.MakeSwapPtree(transaction, waiting.taker_,
                                     waiting.inquiry_height_, error_info, swap);
        if (error)
        {
            response_.put("error", error_info);
            return;
        }

        swaps.push_back(std::make_pair("", swap));
        ++count;
    }
    response_.put_child("swaps", swaps);
}

void rai::TokenRpcHandler::AccountOrders()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    uint64_t height;
    error = GetHeight_(height);
    if (error)
    {
        height = std::numeric_limits<uint64_t>::max();
        error_code_ = rai::ErrorCode::SUCCESS;
    }

    uint64_t count;
    error = GetCount_(count);
    error_code_ = rai::ErrorCode::SUCCESS;
    if (error || count == 0) count = 10;
    if (count > 100) count = 100;

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::Ptree orders;
    rai::Iterator i =
        token_.ledger_.OrderInfoLowerBound(transaction, account, height);
    rai::Iterator n = token_.ledger_.OrderInfoUpperBound(transaction, account);
    for (; i != n && count > 0; ++i, --count)
    {
        rai::OrderInfo info;
        error = token_.ledger_.OrderInfoGet(i, account, height, info);
        if (error)
        {
            response_.put("error", "Failed to get order info from ledger");
            return;
        }

        std::string error_info;
        rai::Ptree order;
        error = token_.MakeOrderPtree(transaction, account, height, error_info,
                                      order);
        if (error)
        {
            response_.put("error", error_info);
            return;
        }
        orders.push_back(std::make_pair("", order));
    }
    response_.put_child("orders", orders);
    response_.put("more", rai::BoolToString(i != n));
}

void rai::TokenRpcHandler::AccountSwapInfo()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    std::string error_info;
    error = token_.MakeAccountSwapInfoPtree(transaction, account, error_info,
                                            response_);
    if (error)
    {
        response_.put("error", error_info);
    }
}

void rai::TokenRpcHandler::AccountTokenBalance()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);

    rai::Chain chain;
    error = GetChain_(chain);
    IF_ERROR_RETURN_VOID(error);

    rai::TokenAddress address;
    error = GetTokenAddress_(chain, address);
    IF_ERROR_RETURN_VOID(error);

    rai::TokenType type;
    error = GetTokenType_(type);
    IF_ERROR_RETURN_VOID(error);

    boost::optional<rai::TokenValue> id_o;
    rai::TokenValue id;
    error = GetTokenId_(id);
    if (error)
    {
        if (error_code_ != rai::ErrorCode::RPC_MISS_FIELD_TOKEN_ID)
        {
            return;
        }
        error_code_ = rai::ErrorCode::SUCCESS;
    }
    else
    {
        id_o = id;
    }

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::TokenKey token(chain, address);
    std::string error_info;
    error = token_.MakeAccountTokenBalancePtree(
        transaction, account, token, type, id_o, error_info, response_);
    if (error)
    {
        response_.put("error", error_info);
        return;
    }
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

void rai::TokenRpcHandler::CrossChainStatus()
{
    token_.cross_chain_.Status(response_);
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

void rai::TokenRpcHandler::MakerSwaps()
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

    uint64_t prev_height = rai::Block::INVALID_HEIGHT;
    rai::Ptree swaps;
    rai::Iterator i =
        token_.ledger_.MakerSwapIndexLowerBound(transaction, account, height);
    rai::Iterator n =
        token_.ledger_.MakerSwapIndexUpperBound(transaction, account);
    for (; i != n; ++i)
    {
        rai::Account taker;
        uint64_t inquiry_height;
        rai::MakerSwapIndex index;
        error =
            token_.ledger_.MakerSwapIndexGet(i, index, taker, inquiry_height);
        if (error)
        {
            response_.put("error", "Failed to get maker swap index from ledger");
            return;
        }

        if (count == 0 && prev_height != index.trade_height_)
        {
            break;
        }

        rai::Ptree swap;
        std::string error_info;
        error = token_.MakeSwapPtree(transaction, taker, inquiry_height,
                                     error_info, swap);
        if (error)
        {
            response_.put("error", error_info);
            return;
        }
        swaps.push_back(std::make_pair("", swap));
        if (count > 0)
        {
            --count;
            prev_height = index.trade_height_;
        }
    }

    response_.put_child("swaps", swaps);
    response_.put("more", rai::BoolToString(i != n));
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

void rai::TokenRpcHandler::OrderCount()
{
    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    size_t count = 0;
    bool error = token_.ledger_.OrderCount(transaction, count);
    if (error)
    {
        error_code_ = rai::ErrorCode::UNEXPECTED;
        return;
    }

    response_.put("count", count);
}

void rai::TokenRpcHandler::OrderInfo()
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

    std::string error_info;
    rai::Ptree order;
    error =
        token_.MakeOrderPtree(transaction, account, height, error_info, order);
    if (error)
    {
        response_.put("error", error_info);
        return;
    }
    response_.put_child("order", order);
}

void rai::TokenRpcHandler::OrderSwaps()
{
    rai::Account maker;
    bool error = GetMaker_(maker);
    IF_ERROR_RETURN_VOID(error);
    response_.put("maker", maker.StringAccount());

    uint64_t height;
    error = GetHeight_(height);
    IF_ERROR_RETURN_VOID(error);
    response_.put("height", std::to_string(height));

    uint64_t count;
    error = GetCount_(count);
    error_code_ = rai::ErrorCode::SUCCESS;
    if (error || count == 0) count = 10;
    if (count > 100) count = 100;

    uint64_t trade_height;
    error = GetTradeHeight_(trade_height);
    IF_ERROR_RETURN_VOID(error);

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::Ptree swaps;
    rai::Iterator i = token_.ledger_.OrderSwapIndexLowerBound(
        transaction, maker, height, trade_height);
    rai::Iterator n =
        token_.ledger_.OrderSwapIndexUpperBound(transaction, maker, height);
    for (; i != n && count > 0; ++i, --count)
    {
        rai::OrderSwapIndex index;
        error = token_.ledger_.OrderSwapIndexGet(i, index);
        if (error)
        {
            response_.put("error", "Failed to get order swap index");
            return;
        }

        std::string error_info;
        rai::Ptree swap;
        error = token_.MakeSwapPtree(transaction, index.taker_,
                                     index.inquiry_height_, error_info, swap);
        if (error)
        {
            response_.put("error", error_info);
            return;
        }

        swaps.push_back(std::make_pair("", swap));
    }

    response_.put_child("swaps", swaps);
    response_.put("more", rai::BoolToString(i != n));
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

void rai::TokenRpcHandler::SearchOrders()
{
    auto by_o = request_.get_optional<std::string>("by");
    if (!by_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_BY;
        return;
    }
    response_.put("by", *by_o);

    if (*by_o == "id")
    {
        SearchOrdersById();
    }
    else if (*by_o == "pair")
    {
        SearchOrdersByPair();
    }
    else
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_BY;
    }
}

void rai::TokenRpcHandler::SearchOrdersById()
{
    rai::BlockHash hash;
    bool error = GetHash_(hash);
    IF_ERROR_RETURN_VOID(error);
    response_.put("hash", hash.StringHex());

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    std::shared_ptr<rai::Block> block;
    error = token_.ledger_.BlockGet(transaction, hash, block);
    if (error)
    {
        response_.put("error", "missing");
        return;
    }
    rai::Account account = block->Account();

    std::string error_info;
    rai::Ptree order;
    error = token_.MakeOrderPtree(transaction, block->Account(),
                                  block->Height(), error_info, order);
    if (error)
    {
        response_.put("error", error_info);
        return;
    }

    rai::Ptree orders;
    orders.push_back(std::make_pair("", order));
    response_.put_child("orders", orders);
}

void rai::TokenRpcHandler::SearchOrdersByPair()
{
    rai::TokenKey from_token;
    rai::TokenType from_type;
    bool error = GetToken_(
        "from_token", rai::ErrorCode::RPC_MISS_FIELD_FROM_TOKEN,
        rai::ErrorCode::RPC_INVALID_FIELD_FROM_TOKEN, from_token, from_type);
    IF_ERROR_RETURN_VOID(error);
    response_.put_child("from_token", request_.get_child("from_token"));

    rai::TokenKey to_token;
    rai::TokenType to_type;
    error = GetToken_("to_token", rai::ErrorCode::RPC_MISS_FIELD_TO_TOKEN,
                      rai::ErrorCode::RPC_INVALID_FIELD_TO_TOKEN, to_token,
                      to_type);
    IF_ERROR_RETURN_VOID(error);
    response_.put_child("to_token", request_.get_child("to_token"));

    std::string limit_by;
    auto limit_by_o = request_.get_optional<std::string>("limit_by");
    if (limit_by_o)
    {
        limit_by = *limit_by_o;
        response_.put("limit_by", limit_by);
    }

    if (limit_by != "" && limit_by != "from_token" && limit_by != "to_token")
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_LIMIT_BY;
        return;
    }
    auto include_inactive_o =
        request_.get_optional<std::string>("include_inactive");
    bool include_inactive =
        include_inactive_o && *include_inactive_o != "false";

    rai::TokenValue limit_value;
    if (limit_by != "")
    {
        auto limit_value_o = request_.get_optional<std::string>("limit_value");
        if (!limit_value_o)
        {
            error_code_ = rai::ErrorCode::RPC_MISS_FIELD_LIMIT_VALUE;
            return;
        }
        response_.put("limit_value", *limit_value_o);
        error = limit_value.DecodeDec(*limit_value_o);
        if (error)
        {
            error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_LIMIT_VALUE;
            return;
        }
    }

    uint64_t count;
    error = GetCount_(count);
    error_code_ = rai::ErrorCode::SUCCESS;
    if (error || count == 0) count = 10;
    if (count > 100) count = 100;

    bool has_order = false;
    rai::Account maker;
    uint64_t order_height;
    error = GetOrder_(maker, order_height);
    if (error)
    {
        error_code_ = rai::ErrorCode::SUCCESS;
    }
    else
    {
        has_order = true;
    }

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::OrderIndex order_index;
    if (has_order)
    {
        rai::OrderInfo order_info;
        error = token_.ledger_.OrderInfoGet(transaction, maker, order_height,
                                            order_info);
        if (error)
        {
            response_.put("error", "Failed to get order info");
            return;
        }
        order_index = order_info.GetIndex(maker, order_height);
        if (order_index.token_offer_ != from_token
            || order_index.token_want_ != to_token
            || order_index.type_offer_ != from_type
            || order_index.type_want_ != to_type)
        {
            response_.put("error", "Invalid order parameter");
            return;
        }
        if (limit_by == "from_token" && from_type == rai::TokenType::_721)
        {
            if (order_index.id_offer_ != limit_value)
            {
                response_.put("error", "Invalid order parameter");
                return;
            }
        }
        if (limit_by == "to_token" && to_type == rai::TokenType::_721)
        {
            if (order_index.id_want_ != limit_value)
            {
                response_.put("error", "Invalid order parameter");
                return;
            }
        }
    }

    rai::Iterator i, n;
    if (has_order)
    {
        i = token_.ledger_.OrderIndexLowerBound(transaction, order_index);
    }
    if (limit_by == "from_token" && from_type == rai::TokenType::_721)
    {
        if (!has_order)
        {
            i = token_.ledger_.OrderIndexLowerBound(transaction, from_token,
                                                    from_type, to_token,
                                                    to_type, limit_value);
        }
        n = token_.ledger_.OrderIndexUpperBound(
            transaction, from_token, from_type, to_token, to_type, limit_value);
    }
    else
    {
        if (!has_order)
        {
            i = token_.ledger_.OrderIndexLowerBound(
                transaction, from_token, from_type, to_token, to_type);
        }
        n = token_.ledger_.OrderIndexUpperBound(transaction, from_token,
                                                from_type, to_token, to_type);
    }

    uint64_t now = rai::CurrentTimestamp();
    rai::Ptree orders;
    for (; i != n && count > 0; ++i)
    {
        rai::OrderIndex index;
        error = token_.ledger_.OrderIndexGet(i, index);
        if (error)
        {
            response_.put("error", "Failed to get order index from ledger");
            return;
        }

        if (has_order && index.maker_ == maker && index.height_ == order_height)
        {
            continue;
        }

        rai::OrderInfo info;
        error = token_.ledger_.OrderInfoGet(transaction, index.maker_,
                                            index.height_, info);
        if (error)
        {
            response_.put("error", "Failed to get order info from ledger");
            return;
        }

        if (!include_inactive)
        {
            if (info.Finished()) continue;
            if (info.timeout_ < now) continue;

            rai::AccountSwapInfo account_swap_info;
            error = token_.ledger_.AccountSwapInfoGet(transaction, index.maker_,
                                                      account_swap_info);
            if (error)
            {
                response_.put("error", "Failed to get account swap info");
                return;
            }
            if (account_swap_info.pong_ < now - rai::MAX_TIMESTAMP_DIFF)
            {
                if (account_swap_info.pong_ + rai::SWAP_PING_PONG_INTERVAL
                    < account_swap_info.ping_)
                {
                    continue;
                }
            }
        }

        if (limit_by == "from_token")
        {
            if (from_type == rai::TokenType::_721
                || to_type == rai::TokenType::_721)
            {
                if (info.value_offer_ != limit_value)
                {
                    continue;
                }
            }
            else if (from_type == rai::TokenType::_20
                     && to_type == rai::TokenType::_20)
            {
                if (info.left_ != limit_value
                    && (limit_value < info.min_offer_
                        || limit_value > info.left_))
                {
                    continue;
                }
            }
        }
        else if (limit_by == "to_token")
        {
            if (from_type == rai::TokenType::_721
                || to_type == rai::TokenType::_721)
            {
                if (info.value_want_ != limit_value)
                {
                    continue;
                }
            }
            else if (from_type == rai::TokenType::_20
                     && to_type == rai::TokenType::_20)
            {
                rai::uint512_t value_offer_s(info.value_offer_.Number());
                rai::uint512_t value_want_s(info.value_want_.Number());
                rai::uint512_t limit_value_s(limit_value.Number());
                rai::uint512_t min_offer_s(info.min_offer_.Number());
                rai::uint512_t left_s(info.left_.Number());
                limit_value_s = limit_value_s * value_offer_s / value_want_s;
                if (left_s != limit_value_s
                    && (limit_value_s < min_offer_s || limit_value_s > left_s))
                {
                    continue;
                }
            }
        }

        std::string error_info;
        rai::Ptree order;
        error = token_.MakeOrderPtree(transaction, index.maker_, index.height_,
                                      error_info, order);
        if (error)
        {
            response_.put("error", error_info);
            return;
        }
        orders.push_back(std::make_pair("", order));
        --count;
    }

    response_.put_child("orders", orders);
    response_.put("more", rai::BoolToString(i != n));
}

void rai::TokenRpcHandler::SwapHelperStatus()
{
    response_.put("count", std::to_string(token_.swap_helper_.Size()));
}

void rai::TokenRpcHandler::SwapInfo()
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

    std::string error_info;
    rai::Ptree swap;
    error =
        token_.MakeSwapPtree(transaction, account, height, error_info, swap);
    if (error)
    {
        response_.put("error", error_info);
        return;
    }
    response_.put_child("swap", swap);
}

void rai::TokenRpcHandler::SubmitTakeNackBlock()
{
    rai::Account taker;
    bool error = GetTaker_(taker);
    IF_ERROR_RETURN_VOID(error);
    response_.put("taker", taker.StringAccount());

    uint64_t inquiry_height = 0;
    error = GetInquiryHeight_(inquiry_height);
    IF_ERROR_RETURN_VOID(error);
    response_.put("inquiry_height", std::to_string(inquiry_height));

    boost::optional<rai::Ptree&> block_ptree =
        request_.get_child_optional("block");
    if (!block_ptree)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_BLOCK;
        return;
    }
    response_.put_child("block", *block_ptree);

    std::shared_ptr<rai::Block> block =
        rai::DeserializeBlockJson(error_code_, *block_ptree);
    if (error_code_ != rai::ErrorCode::SUCCESS || block == nullptr)
    {
        return;
    }

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    bool existing =
        token_.ledger_.TakeNackBlockExists(transaction, taker, inquiry_height);
    if (existing)
    {
        response_.put("status", "submitted");
        return;
    }

    rai::SwapInfo info;
    error =
        token_.ledger_.SwapInfoGet(transaction, taker, inquiry_height, info);
    if (error)
    {
        response_.put("error", "swap missing");
        return;
    }

    if (info.status_ != rai::SwapInfo::Status::INQUIRY_ACK)
    {
        response_.put("error", "swap status");
        return;
    }

    rai::AccountInfo maker_info;
    error = token_.ledger_.AccountInfoGet(transaction, info.maker_, maker_info);
    if (error)
    {
        response_.put("error", "maker missing");
        return;
    }

    if (maker_info.head_ != info.trade_previous_
        || !maker_info.Confirmed(maker_info.head_height_))
    {
        response_.put("error", "maker status");
        return;
    }

    token_.SubmitTakeNackBlock(taker, inquiry_height, block);
    response_.put("status", "pending");
}

void rai::TokenRpcHandler::SwapMainAccount()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::Account main_account;
    error =
        token_.ledger_.SwapMainAccountGet(transaction, account, main_account);
    if (error)
    {
        response_.put("error", "missing");
        return;
    }

    response_.put("main_account", main_account.StringAccount());
}

void rai::TokenRpcHandler::TakerSwaps()
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

    rai::Ptree swaps;
    rai::Iterator i =
        token_.ledger_.SwapInfoLowerBound(transaction, account, height);
    rai::Iterator n = token_.ledger_.SwapInfoUpperBound(transaction, account);
    for (; i != n && count > 0; ++i, --count)
    {
        rai::SwapInfo info;
        error = token_.ledger_.SwapInfoGet(i, account, height, info);
        if (error)
        {
            response_.put("error", "Failed to get swap info");
            return;
        }

        std::string error_info;
        rai::Ptree swap;
        error = token_.MakeSwapPtree(transaction, account, height, error_info,
                                     swap);
        if (error)
        {
            response_.put("error", error_info);
            return;
        }
        swaps.push_back(std::make_pair("", swap));
    }

    response_.put_child("swaps", swaps);
    response_.put("more", rai::BoolToString(i != n));
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

void rai::TokenRpcHandler::TokenIdOwner()
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

    rai::Account owner = token_.GetTokenIdOwner(transaction, key, id);
    response_.put("owner", owner.StringAccount());
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
    response_.put("account", account.StringAccount());

    uint64_t count = 0;
    error = GetCount_(count);
    IF_ERROR_RETURN_VOID(error);
    if (count == 0) count = 100;
    if (count > 1000) count = 1000;

    std::vector<rai::TokenKey> tokens;
    error = GetTokens_(tokens);
    IF_ERROR_RETURN_VOID(error);

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::Ptree receivables;
    for (const auto& token : tokens)
    {
        rai::Iterator i = token_.ledger_.TokenReceivableLowerBound(
            transaction, account, token);
        rai::Iterator n = token_.ledger_.TokenReceivableUpperBound(
            transaction, account, token);
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
                response_.put(
                    "error",
                    rai::ToString("Get token info failed, token.chain=",
                                  rai::ChainToString(key.token_.chain_),
                                  ", token.address_raw=",
                                  key.token_.address_.StringHex()));
                return;
            }

            std::shared_ptr<rai::Block> block(nullptr);
            if (rai::IsLocalSource(receivable.source_))
            {
                error =
                    token_.ledger_.BlockGet(transaction, key.tx_hash_, block);
                if (error)
                {
                    response_.put("error",
                                  rai::ToString("Get block failed, hash=",
                                                key.tx_hash_.StringHex()));
                    return;
                }
            }

            rai::Ptree entry;
            token_.MakeReceivablePtree(key, receivable, info, block, entry);
            receivables.push_back(std::make_pair("", entry));
        }
        if (count == 0) break;
    }

    response_.put_child("receivables", receivables);
}

void rai::TokenRpcHandler::TokenReceivablesAll()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    uint64_t count = 0;
    error = GetCount_(count);
    IF_ERROR_RETURN_VOID(error);
    if (count == 0) count = 100;
    if (count > 1000) count = 1000;

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::Ptree receivables;
    rai::Iterator i =
        token_.ledger_.TokenReceivableLowerBound(transaction, account);
    rai::Iterator n =
        token_.ledger_.TokenReceivableUpperBound(transaction, account);
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

void rai::TokenRpcHandler::TokenReceivablesSummary()
{
    rai::Account account;
    bool error = GetAccount_(account);
    IF_ERROR_RETURN_VOID(error);
    response_.put("account", account.StringAccount());

    rai::Transaction transaction(error_code_, token_.ledger_, false);
    IF_NOT_SUCCESS_RETURN_VOID(error_code_);

    rai::Ptree tokens;
    rai::Iterator i =
        token_.ledger_.TokenReceivableLowerBound(transaction, account);
    rai::Iterator n =
        token_.ledger_.TokenReceivableUpperBound(transaction, account);
    while (i != n)
    {
        rai::TokenReceivableKey key;
        rai::TokenReceivable receivable;
        error = token_.ledger_.TokenReceivableGet(i, key, receivable);
        if (error)
        {
            response_.put("error", "Get token receivable info failed");
            return;
        }
        rai::Ptree entry;
        token_.TokenKeyToPtree(key.token_, entry);
        tokens.push_back(std::make_pair("", entry));

        i = token_.ledger_.TokenReceivableUpperBound(transaction, account,
                                                     key.token_);
    }

    response_.put_child("tokens", tokens);
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

bool rai::TokenRpcHandler::GetTokenType_(rai::TokenType& type)
{
    auto type_o = request_.get_optional<std::string>("type");
    if (!type_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_TYPE;
        return true;
    }
    type = rai::StringToTokenType(*type_o);
    if (type == rai::TokenType::INVALID)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_TYPE;
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

bool rai::TokenRpcHandler::GetTokens_(std::vector<rai::TokenKey>& tokens)
{
    try
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_TOKENS;
        auto tokens_o = request_.get_child_optional("tokens");
        if (!tokens_o) return true;

        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_TOKENS;
        size_t count = 0;
        for (const auto& i : *tokens_o)
        {
            auto chain_o = i.second.get_optional<std::string>("chain");
            if (!chain_o) return true;
            rai::Chain chain = rai::StringToChain(*chain_o);
            if (chain == rai::Chain::INVALID) return true;

            rai::TokenAddress address;
            auto address_raw_o =
                i.second.get_optional<std::string>("address_raw");
            if (address_raw_o)
            {
                bool error = address.DecodeHex(*address_raw_o);
                IF_ERROR_RETURN(error, true);
            }
            else
            {
                auto address_o = i.second.get_optional<std::string>("address");
                if (!address_o) return true;
                bool error =
                    rai::StringToTokenAddress(chain, *address_o, address);
                IF_ERROR_RETURN(error, true);
            }

            tokens.emplace_back(chain, address);
            ++count;
            if (count >= 1000) break;
        }
    }
    catch (...)
    {
        return true;
    }
    error_code_ = rai::ErrorCode::SUCCESS;
    return false;
}

bool rai::TokenRpcHandler::GetToken_(const std::string& key,
                                     rai::ErrorCode missing,
                                     rai::ErrorCode invalid,
                                     rai::TokenKey& token,
                                     rai::TokenType& type)
{
    try
    {
        error_code_ = missing;
        auto token_o = request_.get_child_optional(key);
        if (!token_o) return true;

        error_code_ = invalid;
        auto chain_o = token_o->get_optional<std::string>("chain");
        if (!chain_o) return true;
        rai::Chain chain = rai::StringToChain(*chain_o);
        if (chain == rai::Chain::INVALID) return true;

        rai::TokenAddress address;
        auto address_raw_o = token_o->get_optional<std::string>("address_raw");
        if (address_raw_o)
        {
            bool error = address.DecodeHex(*address_raw_o);
            IF_ERROR_RETURN(error, true);
        }
        else
        {
            auto address_o = token_o->get_optional<std::string>("address");
            if (!address_o) return true;
            bool error = rai::StringToTokenAddress(chain, *address_o, address);
            IF_ERROR_RETURN(error, true);
        }
        token = rai::TokenKey(chain, address);

        auto type_o = token_o->get_optional<std::string>("type");
        if (!type_o)
        {
            return true;
        }
        type = rai::StringToTokenType(*type_o);
        if (type == rai::TokenType::INVALID)
        {
            return true;
        }
    }
    catch (...)
    {
        return true;
    }
    error_code_ = rai::ErrorCode::SUCCESS;
    return false;
}

bool rai::TokenRpcHandler::GetMaker_(rai::Account& maker)
{
    auto maker_o = request_.get_optional<std::string>("maker");
    if (!maker_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_MAKER;
        return true;
    }

    if (maker.DecodeAccount(*maker_o))
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_MAKER;
        return true;
    }

    return false;
}

bool rai::TokenRpcHandler::GetTradeHeight_(uint64_t& height)
{
    auto height_o = request_.get_optional<std::string>("trade_height");
    if (!height_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_TRADE_HEIGHT;
        return true;
    }

    bool error = rai::StringToUint(*height_o, height);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_TRADE_HEIGHT;
        return  true;
    }

    return false;
}

bool rai::TokenRpcHandler::GetTaker_(rai::Account& taker)
{
    auto taker_o = request_.get_optional<std::string>("taker");
    if (!taker_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_TAKER;
        return true;
    }

    if (taker.DecodeAccount(*taker_o))
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_TAKER;
        return true;
    }

    return false;
}

bool rai::TokenRpcHandler::GetInquiryHeight_(uint64_t& height)
{
    auto height_o = request_.get_optional<std::string>("inquiry_height");
    if (!height_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_INQUIRY_HEIGHT;
        return true;
    }

    bool error = rai::StringToUint(*height_o, height);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_INQUIRY_HEIGHT;
        return  true;
    }

    return false;
}

bool rai::TokenRpcHandler::GetOrder_(rai::Account& maker,
                                     uint64_t& order_height)
{
    bool error = GetMaker_(maker);
    IF_ERROR_RETURN(error, true);

    auto height_o = request_.get_optional<std::string>("order_height");
    if (!height_o)
    {
        error_code_ = rai::ErrorCode::RPC_MISS_FIELD_ORDER_HEIGHT;
        return true;
    }

    error = rai::StringToUint(*height_o, order_height);
    if (error)
    {
        error_code_ = rai::ErrorCode::RPC_INVALID_FIELD_ORDER_HEIGHT;
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

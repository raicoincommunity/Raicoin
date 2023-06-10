#include <rai/rai_wallet/qt.hpp>

#include <assert.h>
#include <rai/common/blocks.hpp>
#include <rai/common/parameters.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/extensions.hpp>
#include <rai/secure/util.hpp>

namespace
{
void ShowButtonDefault(QPushButton& button)
{
    button.setStyleSheet("QPushButton { color: black }");
}

void ShowButtonError(QPushButton& button)
{
    button.setStyleSheet("QPushButton { color: red }");
}

void ShowButtonSuccess(QPushButton& button)
{
    button.setStyleSheet("QPushButton { color: blue }");
}

void ShowLineError(QLineEdit& line)
{
    line.setStyleSheet("QLineEdit { color: red }");
}

void ShowLineDefault(QLineEdit& line)
{
    line.setStyleSheet("QLineEdit { color: black }");
}

void ShowLineSuccess(QLineEdit& line)
{
    line.setStyleSheet("QLineEdit { color: blue }");
}

void ShowLabelError(QLabel& label)
{
    label.setStyleSheet("QLabel { color: red }");
}

void ShowLabelDefault(QLabel& label)
{
    label.setStyleSheet("QLabel { color: black }");
}

void ShowLabelSuccess(QLabel& label)
{
    label.setStyleSheet("QLabel { color: blue }");
}

}  // namespace

bool rai::QtEventProcessor::event(QEvent* event)
{
    if (event != nullptr)
    {
        static_cast<rai::QtEvent*>(event)->action_();
    }
    else
    {
        assert(0);
    }

    return true;
}

rai::QtEvent::QtEvent(const std::function<void()>& action)
    : QEvent(QEvent::Type::User), action_(action)
{
}

rai::QtStatus::QtStatus(rai::QtMain& qt_main) : main_(qt_main)
{
}

void rai::QtStatus::Refresh()
{
    auto connection = main_.wallets_->websockets_[0]->Status();
    if (connection == rai::WebsocketStatus::CONNECTED)
    {
        status_set_.erase(rai::QtStatusType::DISCONNECTED);
    }
    else
    {
        status_set_.insert(rai::QtStatusType::DISCONNECTED);
    }

    if (main_.wallets_->Synced(main_.wallets_->SelectedAccount()))
    {
        status_set_.erase(rai::QtStatusType::SYNC);
    }
    else
    {
        status_set_.insert(rai::QtStatusType::SYNC);
    }

    auto wallet_id = main_.wallets_->SelectedWalletId();
    if (main_.wallets_->WalletLocked(wallet_id))
    {
        status_set_.insert(rai::QtStatusType::LOCKED);
    }
    else
    {
        status_set_.erase(rai::QtStatusType::LOCKED);
    }

    if (main_.wallets_->WalletVulnerable(wallet_id))
    {
        status_set_.insert(rai::QtStatusType::VULNERABLE);
    }
    else
    {
        status_set_.erase(rai::QtStatusType::VULNERABLE);
    }

    main_.status_->setText(Text().c_str());
    main_.status_->setStyleSheet(
        (std::string("QLabel { ") + Color() + " }").c_str());
}

void rai::QtStatus::Start(const std::weak_ptr<rai::QtMain>& qt_main_w)
{
    main_.wallets_->observers_.connection_status_.Add(
        [qt_main_w](rai::WebsocketStatus status) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent(
                [](rai::QtMain& qt_main) { qt_main.active_status_.Refresh(); });
        });

    main_.wallets_->observers_.wallet_locked_.Add([qt_main_w](bool locked) {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        qt_main->PostEvent(
            [](rai::QtMain& qt_main) { qt_main.active_status_.Refresh(); });
    });

    main_.wallets_->observers_.wallet_password_set_.Add([qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        qt_main->PostEvent(
            [](rai::QtMain& qt_main) { qt_main.active_status_.Refresh(); });
    });

    main_.wallets_->observers_.selected_wallet_.Add([qt_main_w](uint32_t) {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        qt_main->PostEvent(
            [](rai::QtMain& qt_main) { qt_main.active_status_.Refresh(); });
    });

    main_.wallets_->observers_.synced_.Add(
        [qt_main_w](const rai::Account& account, bool) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            if (qt_main->wallets_->SelectedAccount() != account)
            {
                return;
            }
            qt_main->PostEvent(
                [](rai::QtMain& qt_main) { qt_main.active_status_.Refresh(); });
        });

    Refresh();
}

std::string rai::QtStatus::Text() const
{
    if (status_set_.empty())
    {
        return "Status: Running";
    }

    std::string result;
    switch (*status_set_.begin())
    {
        case rai::QtStatusType::DISCONNECTED:
        {
            result = "Status: Disconnected";
            break;
        }
        case rai::QtStatusType::SYNC:
        {
            result = "Status: Synchronizing";
            break;
        }
        case rai::QtStatusType::ACTIVE:
        {
            result = "Status: Wallet active";
            break;
        }
        case rai::QtStatusType::LOCKED:
        {
            result = "Status: Wallet locked";
            break;
        }
        case rai::QtStatusType::VULNERABLE:
        {
            result = "Status: Wallet password empty";
            break;
        }
        default:
        {
            result = "Status: Error occurred";
        }
    }

    return result;
}

std::string rai::QtStatus::Color() const
{
    if (status_set_.empty())
    {
        return "color: black";
    }

    std::string result;
    switch (*status_set_.begin())
    {
        case rai::QtStatusType::DISCONNECTED:
        {
            result = "color: red";
            break;
        }
        case rai::QtStatusType::ACTIVE:
        {
            result = "color: green";
            break;
        }
        case rai::QtStatusType::LOCKED:
        {
            result = "color: orange";
            break;
        }
        case rai::QtStatusType::SYNC:
        case rai::QtStatusType::VULNERABLE:
        {
            result = "color: blue";
            break;
        }
        default:
        {
            result = "color: black";
        }
    }

    return result;
}

rai::QtSelfPane::QtSelfPane(rai::QtMain& qt_main)
    : window_(new QWidget),
      layout_(new QVBoxLayout),
      self_window_(new QWidget),
      self_layout_(new QHBoxLayout),
      account_label_(new QLabel("Your Raicoin wallet/account:")),
      account_window_(new QWidget),
      account_layout_(new QHBoxLayout),
      wallet_text_(new QLineEdit),
      slash_(new QLabel("/")),
      account_text_(new QLineEdit),
      copy_button_(new QPushButton("Copy account")),
      balance_window_(new QWidget),
      balance_layout_(new QHBoxLayout),
      balance_label_(new QLabel),
      main_(qt_main)
{
    account_label_->setStyleSheet("font-weight: bold;");
    version_ = new QLabel(QString("%1 %2 network")
                              .arg(RAI_VERSION_STRING)
                              .arg(rai::NetworkString().c_str()));
    self_layout_->addWidget(account_label_);
    self_layout_->addStretch();
    self_layout_->addWidget(version_);
    self_layout_->setContentsMargins(0, 0, 0, 0);
    self_window_->setLayout(self_layout_);

    wallet_text_->setReadOnly(true);
    wallet_text_->setStyleSheet("QLineEdit { background: #ddd; }");
    wallet_text_->setAlignment(Qt::AlignRight);
    account_text_->setReadOnly(true);
    account_text_->setStyleSheet("QLineEdit { background: #ddd; }");
    account_layout_->addWidget(wallet_text_, 1),
        account_layout_->addWidget(slash_),
        account_layout_->addWidget(account_text_, 18);
    account_layout_->addWidget(copy_button_, 1);
    account_layout_->setContentsMargins(0, 0, 0, 0);
    account_window_->setLayout(account_layout_);

    balance_label_->setStyleSheet("font-weight: bold;");
    balance_layout_->addWidget(balance_label_);
    balance_layout_->addStretch();
    balance_layout_->setContentsMargins(0, 0, 0, 0);
    balance_window_->setLayout(balance_layout_);

    layout_->addWidget(self_window_);
    layout_->addWidget(account_window_);
    layout_->addWidget(balance_window_);
    layout_->setContentsMargins(5, 5, 5, 5);
    window_->setLayout(layout_);
}

void rai::QtSelfPane::Refresh()
{
    rai::Account account = main_.wallets_->SelectedAccount();
    rai::Amount balance;
    rai::Amount confirmed;
    rai::Amount receivable;
    main_.wallets_->AccountBalanceAll(account, balance, confirmed, receivable);

    std::string final_text =
        std::string("Balance: ") + main_.FormatBalance(confirmed);

    if (balance > confirmed)
    {
        final_text += "\nPending: ";
        final_text += main_.FormatBalance(balance - confirmed);
    }
    else if (confirmed > balance)
    {
        final_text += "\nPending: -";
        final_text += main_.FormatBalance(confirmed - balance);
    }

    if (!receivable.IsZero())
    {
        final_text += "\nReceivable: ";
        final_text += main_.FormatBalance(receivable);
    }

    auto wallet_id = main_.wallets_->SelectedWalletId();
    wallet_text_->setText(std::to_string(wallet_id).c_str());
    account_text_->setText(account.StringAccount().c_str());
    balance_label_->setText(final_text.c_str());
}

void rai::QtSelfPane::Start(const std::weak_ptr<rai::QtMain>& qt_main_w)
{
    QObject::connect(copy_button_, &QPushButton::clicked, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        qt_main->application_.clipboard()->setText(
            qt_main->wallets_->SelectedAccount().StringAccount().c_str());
        qt_main->self_pane_.copy_button_->setText("Copied!");

        qt_main->PostEvent(2, [](rai::QtMain& qt_main) {
            qt_main.self_pane_.copy_button_->setText("Copy account");
        });
    });

    main_.wallets_->observers_.block_.Add(
        [qt_main_w](const std::shared_ptr<rai::Block>& block, bool rollback) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent([block](rai::QtMain& qt_main) {
                if (block->Account() == qt_main.wallets_->SelectedAccount())
                {
                    qt_main.self_pane_.Refresh();
                }
            });
        });

    main_.wallets_->observers_.selected_account_.Add(
        [qt_main_w](const rai::Account& account) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent([account](rai::QtMain& qt_main) {
                qt_main.self_pane_.Refresh();
            });
        });

    main_.wallets_->observers_.receivable_.Add(
        [qt_main_w](const rai::Account& account) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent([account](rai::QtMain& qt_main) {
                if (account == qt_main.wallets_->SelectedAccount())
                {
                    qt_main.self_pane_.Refresh();
                }
            });
        });

    Refresh();
}

namespace
{
class QtHistoryVisitor : public rai::BlockVisitor
{
public:
    QtHistoryVisitor(rai::Transaction& transaction, rai::Ledger& ledger,
                     uint64_t confirmed_height, bool fork)
        : transaction_(transaction),
          ledger_(ledger),
          confirmed_height_(confirmed_height),
          fork_(fork)
    {
    }

    rai::ErrorCode Send(const rai::Block& block) override
    {
        record_.height_   = block.Height();
        record_.type_     = "send";
        record_.link_     = block.Link().StringAccount();
        record_.hash_     = block.Hash();
        record_.previous_ = block.Previous();

        if (confirmed_height_ == rai::Block::INVALID_HEIGHT
            || block.Height() > confirmed_height_)
        {
            record_.status_ = "pending";
        }
        else
        {
            record_.status_ = "confirmed";
        }
        if (fork_)
        {
			record_.status_ += ", fork";
        }

        std::shared_ptr<rai::Block> previous(nullptr);
        bool error = ledger_.BlockGet(transaction_, block.Previous(), previous);
        if (error)
        {
            // log
            record_.amount_.Clear();
        }
        else
        {
            record_.amount_ = previous->Balance() - block.Balance();
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Receive(const rai::Block& block) override
    {
        record_.height_   = block.Height();
        record_.type_     = "receive";
        record_.link_     = block.Link().StringHex();
        record_.hash_     = block.Hash();
        record_.previous_ = block.Previous();

        if (confirmed_height_ == rai::Block::INVALID_HEIGHT
            || block.Height() > confirmed_height_)
        {
            record_.status_ = "pending";
        }
        else
        {
            record_.status_ = "confirmed";
        }
        if (fork_)
        {
			record_.status_ += ", fork";
        }

        if (record_.height_ == 0)
        {
            rai::Amount price = rai::CreditPrice(block.Timestamp());
            record_.amount_ =
                block.Balance() + rai::Amount(price.Number() * block.Credit());
        }
        else
        {
            std::shared_ptr<rai::Block> previous(nullptr);
            bool error =
                ledger_.BlockGet(transaction_, block.Previous(), previous);
            if (error)
            {
                // log
                record_.amount_.Clear();
            }
            else
            {
                record_.amount_ = block.Balance() - previous->Balance();
            }
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Change(const rai::Block& block) override
    {
        record_.height_   = block.Height();
        record_.type_     = "change";
        record_.link_     = block.Representative().StringAccount();
        record_.amount_   = 0;
        record_.hash_     = block.Hash();
        record_.previous_ = block.Previous();

        if (confirmed_height_ == rai::Block::INVALID_HEIGHT
            || block.Height() > confirmed_height_)
        {
            record_.status_ = "pending";
        }
        else
        {
            record_.status_ = "confirmed";
        }
        if (fork_)
        {
			record_.status_ += ", fork";
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Credit(const rai::Block& block) override
    {
        record_.height_   = block.Height();
        record_.type_     = "credit";
        record_.link_     = block.Link().StringHex();
        record_.hash_     = block.Hash();
        record_.previous_ = block.Previous();

        if (confirmed_height_ == rai::Block::INVALID_HEIGHT
            || block.Height() > confirmed_height_)
        {
            record_.status_ = "pending";
        }
        else
        {
            record_.status_ = "confirmed";
        }
        if (fork_)
        {
			record_.status_ += ", fork";
        }

        std::shared_ptr<rai::Block> previous(nullptr);
        bool error = ledger_.BlockGet(transaction_, block.Previous(), previous);
        if (error)
        {
            // log
            record_.amount_.Clear();
        }
        else
        {
            record_.amount_ = previous->Balance() - block.Balance();
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Reward(const rai::Block& block) override
    {
        record_.height_   = block.Height();
        record_.type_     = "reward";
        record_.link_     = block.Link().StringHex();
        record_.hash_     = block.Hash();
        record_.previous_ = block.Previous();

        if (confirmed_height_ == rai::Block::INVALID_HEIGHT
            || block.Height() > confirmed_height_)
        {
            record_.status_ = "pending";
        }
        else
        {
            record_.status_ = "confirmed";
        }
        if (fork_)
        {
			record_.status_ += ", fork";
        }

        std::shared_ptr<rai::Block> previous(nullptr);
        bool error = ledger_.BlockGet(transaction_, block.Previous(), previous);
        if (error)
        {
            // log
            record_.amount_.Clear();
        }
        else
        {
            record_.amount_ = block.Balance() - previous->Balance();
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Destroy(const rai::Block& block) override
    {
        record_.height_   = block.Height();
        record_.type_     = "destroy";
        record_.link_     = block.Link().StringHex();
        record_.hash_     = block.Hash();
        record_.previous_ = block.Previous();

        if (confirmed_height_ == rai::Block::INVALID_HEIGHT
            || block.Height() > confirmed_height_)
        {
            record_.status_ = "pending";
        }
        else
        {
            record_.status_ = "confirmed";
        }
        if (fork_)
        {
			record_.status_ += ", fork";
        }

        std::shared_ptr<rai::Block> previous(nullptr);
        bool error = ledger_.BlockGet(transaction_, block.Previous(), previous);
        if (error)
        {
            // log
            record_.amount_.Clear();
        }
        else
        {
            record_.amount_ = previous->Balance() - block.Balance();
        }

        return rai::ErrorCode::SUCCESS;
    }

    rai::ErrorCode Bind(const rai::Block& block) override
    {
        record_.height_   = block.Height();
        record_.type_     = "bind";
        record_.link_     = block.Representative().StringAccount();
        record_.amount_   = 0;
        record_.hash_     = block.Hash();
        record_.previous_ = block.Previous();

        if (confirmed_height_ == rai::Block::INVALID_HEIGHT
            || block.Height() > confirmed_height_)
        {
            record_.status_ = "pending";
        }
        else
        {
            record_.status_ = "confirmed";
        }
        if (fork_)
        {
			record_.status_ += ", fork";
        }

        return rai::ErrorCode::SUCCESS;

    }

    rai::Transaction& transaction_;
    rai::Ledger& ledger_;
    uint64_t confirmed_height_;
    bool fork_;

    rai::QtHistoryRecord record_;
};
}  // namespace

rai::QtHistoryRecord::QtHistoryRecord() : height_(rai::Block::INVALID_HEIGHT)
{
}

rai::QtHistoryModel::QtHistoryModel(rai::QtHistory& history, QObject* parent)
    : QAbstractTableModel(parent),
      history_(history),
      head_height_(rai::Block::INVALID_HEIGHT)

{
}

int rai::QtHistoryModel::rowCount(const QModelIndex& index) const
{
    if (head_height_ == rai::Block::INVALID_HEIGHT)
    {
        return 0;
    }

    return static_cast<int>(head_height_ + 1);
}

int rai::QtHistoryModel::columnCount(const QModelIndex& index) const
{
    return 6;
}

QVariant rai::QtHistoryModel::data(const QModelIndex& index, int role) const
{
    if (index.column() == 3 && role == Qt::TextAlignmentRole)
    {
        return static_cast<Qt::Alignment::Int>(Qt::AlignVCenter
                                               | Qt::AlignRight);
    }

    if (!index.isValid() || role != Qt::DisplayRole
        || head_height_ == rai::Block::INVALID_HEIGHT
        || index.row() > head_height_)
    {
        return QVariant();
    }

    if (current_record_.height_ != head_height_ - index.row())
    {
        bool error = Record(index.row());
        if (error)
        {
            return QVariant();
        }
    }

    return Field(index.column());
}

QVariant rai::QtHistoryModel::headerData(int section,
                                         Qt::Orientation orientation,
                                         int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return QVariant();
    }

    switch (section)
    {
        case 0:
        {
            return QString("Height");
        }
        case 1:
        {
            return QString("Type");
        }
        case 2:
        {
            return QString("Link");
        }
        case 3:
        {
            return QString("Amount");
        }
        case 4:
        {
            return QString("Hash");
        }
        case 5:
        {
            return QString("Status");
        }
        default:
        {
            return QVariant();
        }
    }
}

QVariant rai::QtHistoryModel::Field(int index) const
{
    const rai::QtHistoryRecord& r = current_record_;
    switch (index)
    {
        case 0:
        {
            return QString(std::to_string(r.height_).c_str());
        }
        case 1:
        {
            return QString(r.type_.c_str());
        }
        case 2:
        {
            return QString(r.link_.c_str());
        }
        case 3:
        {
            return QString(history_.main_.FormatBalance(r.amount_).c_str());
        }
        case 4:
        {
            return QString(r.hash_.StringHex().c_str());
        }
        case 5:
        {
            return QString(r.status_.c_str());
        }
        default:
        {
            return QVariant();
        }
    }
}

bool rai::QtHistoryModel::Record(int row) const
{
    std::cout << "Record: row=" << row << std::endl;
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, history_.main_.wallets_->ledger_,
                                 false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return true;
    }

    if (row > head_height_)
    {
        return true;
    }

    bool error          = false;
    rai::Ledger& ledger = history_.main_.wallets_->ledger_;
    std::shared_ptr<rai::Block> block(nullptr);
    if (row == 0)
    {
        error = ledger.BlockGet(transaction, head_, block);
    }
    else if (current_record_.height_ == head_height_ - row + 1)
    {
        error = ledger.BlockGet(transaction, current_record_.previous_, block);
    }
    else
    {
        error =
            ledger.BlockGet(transaction, account_, head_height_ - row, block);
    }
    IF_ERROR_RETURN(error, true);

    bool fork =
        ledger.ForkExists(transaction, block->Account(), block->Height());
    QtHistoryVisitor visitor(transaction, ledger, confirmed_height_, fork);
    block->Visit(visitor);
    current_record_ = visitor.record_;

    return false;
}

uint64_t rai::QtHistoryModel::HeadHeight() const
{
    return head_height_;
}

rai::Account rai::QtHistoryModel::Account() const
{
    return account_;
}

rai::QtHistory::QtHistory(rai::QtMain& qt_main)
    : window_(new QWidget),
      layout_(new QVBoxLayout),
      model_(new QtHistoryModel(*this)),
      view_(new QTableView),
      main_(qt_main)
{
#if 0
    model_->setHorizontalHeaderItem(0, new QStandardItem("Height"));
    model_->setHorizontalHeaderItem(1, new QStandardItem("Type"));
    model_->setHorizontalHeaderItem(2, new QStandardItem("Link"));
    model_->setHorizontalHeaderItem(3, new QStandardItem("Amount"));
    model_->setHorizontalHeaderItem(4, new QStandardItem("Hash"));
    model_->setHorizontalHeaderItem(5, new QStandardItem("Status"));
#endif

    view_->setModel(model_);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->verticalHeader()->hide();
    view_->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);

    layout_->addWidget(view_);
    layout_->setContentsMargins(0, 0, 0, 0);
    window_->setLayout(layout_);
}

void rai::QtHistory::OnMenuRequested(const QPoint& pos)
{
    std::weak_ptr<rai::QtMain> qt_main_w(main_.Shared());
    auto menu = std::make_shared<QMenu>(view_);
    std::vector<std::shared_ptr<QAction>> actions;
    actions.push_back(std::make_shared<QAction>("Copy block json"));
    actions.push_back(std::make_shared<QAction>("Copy block raw"));

    QModelIndex index(main_.history_.view_->indexAt(pos));
    model_->data(index, Qt::DisplayRole);
    auto status = model_->Field(5).toString();
    if (status.contains("fork"))
    {
        actions.push_back(std::make_shared<QAction>("Copy fork detail"));
    }

    for (const auto& action : actions)
    {
        menu->addAction(action.get());
    }

    QObject::connect(
        menu.get(), &QMenu::triggered,
        [pos, menu, actions, qt_main_w](QAction* action) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;
            QModelIndex index(qt_main->history_.view_->indexAt(pos));
            rai::Account account = qt_main->history_.model_->Account();
            uint64_t head_height = qt_main->history_.model_->HeadHeight();
            if (index.row() > head_height)
            {
                return;
            }
            uint64_t height = head_height - index.row();

            rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
            rai::Ledger& ledger       = qt_main->wallets_->ledger_;
            rai::Transaction transaction(error_code, ledger, false);
            IF_NOT_SUCCESS_RETURN_VOID(error_code);

            std::string copy_str;
            bool error;
            if (action->text() == "Copy block json")
            {
                std::shared_ptr<rai::Block> block(nullptr);
                error = ledger.BlockGet(transaction, account, height, block);
                IF_ERROR_RETURN_VOID(error);
                copy_str = block->Json();
            }
            else if (action->text() == "Copy block raw")
            {
                std::shared_ptr<rai::Block> block(nullptr);
                error = ledger.BlockGet(transaction, account, height, block);
                IF_ERROR_RETURN_VOID(error);
                std::vector<uint8_t> bytes;
                {
                    rai::VectorStream stream(bytes);
                    block->Serialize(stream);
                }
                copy_str = rai::BytesToHex(bytes.data(), bytes.size());
            }
            else if (action->text() == "Copy fork detail")
            {
                rai::Ptree ptree;
                std::shared_ptr<rai::Block> first(nullptr);
                std::shared_ptr<rai::Block> second(nullptr);
                error =
                    ledger.ForkGet(transaction, account, height, first, second);
                IF_ERROR_RETURN_VOID(error);
                rai::Ptree ptree_first;
                rai::Ptree ptree_second;
                first->SerializeJson(ptree_first);
                second->SerializeJson(ptree_second);
                ptree.put_child("block_first", ptree_first);
                ptree.put_child("block_second", ptree_second);
                std::stringstream ostream;
                boost::property_tree::write_json(ostream, ptree);
                ostream.flush();
                copy_str = ostream.str();
            }
            else
            {
                return;
            }

            qt_main->application_.clipboard()->setText(copy_str.c_str());
        });
    menu->popup(view_->viewport()->mapToGlobal(pos));
}

void rai::QtHistory::Refresh()
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, main_.wallets_->ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    rai::Account account = main_.wallets_->SelectedAccount();
    rai::AccountInfo info;
    main_.wallets_->ledger_.AccountInfoGet(transaction, account, info);

    model_->beginResetModel();

    model_->account_          = account;
    model_->head_             = info.head_;
    model_->head_height_      = info.head_height_;
    model_->confirmed_height_ = info.confirmed_height_;
    model_->current_record_   = rai::QtHistoryRecord();

    model_->endResetModel();
}

void rai::QtHistory::Start(const std::weak_ptr<rai::QtMain>& qt_main_w)
{
    QObject::connect(view_, &QTableView::customContextMenuRequested,
                     [qt_main_w](const QPoint& pos) {
                         if (auto qt_main = qt_main_w.lock())
                         {
                             qt_main->history_.OnMenuRequested(pos);
                         }
                     });

    main_.wallets_->observers_.block_.Add(
        [qt_main_w](const std::shared_ptr<rai::Block>& block, bool rollback) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent([block](rai::QtMain& qt_main) {
                if (qt_main.wallets_->SelectedAccount() == block->Account())
                {
                    qt_main.history_.Refresh();
                }
            });
        });

    main_.wallets_->observers_.selected_account_.Add(
        [qt_main_w](const rai::Account& account) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent(
                [](rai::QtMain& qt_main) { qt_main.history_.Refresh(); });
        });

    main_.wallets_->observers_.fork_.Add(
        [qt_main_w](const rai::Account& account) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            if (account != qt_main->wallets_->SelectedAccount())
            {
                return;
            }

            qt_main->PostEvent(
                [](rai::QtMain& qt_main) { qt_main.history_.Refresh(); });
        });

    Refresh();
}

rai::QtSend::QtSend(rai::QtMain& qt_main)
    : window_(new QWidget),
      layout_(new QVBoxLayout),
      destination_label_(new QLabel("Destination account:")),
      destination_(new QLineEdit),
      amount_label_(new QLabel("Amount:")),
      amount_(new QLineEdit),
      note_label_(new QLabel("Note/Memo:")),
      note_(new QLineEdit),
      send_(new QPushButton("Send")),
      back_(new QPushButton("Back")),
      main_(qt_main)
{
    std::string place_text = rai::Account().StringAccount() + "[_SubAccount]";
    destination_->setPlaceholderText(place_text.c_str());

    note_->setPlaceholderText("optional");

    layout_->addWidget(destination_label_);
    layout_->addWidget(destination_);
    layout_->addWidget(amount_label_);
    layout_->addWidget(amount_);
    layout_->addWidget(note_label_);
    layout_->addWidget(note_);
    layout_->addWidget(send_);
    layout_->addStretch();
    layout_->addWidget(back_);
    layout_->setContentsMargins(0, 0, 0, 0);

    window_->setLayout(layout_);
}

void rai::QtSend::Start(const std::weak_ptr<rai::QtMain>& qt_main_w)
{
    QObject::connect(back_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Pop();
        }
    });

    QObject::connect(send_, &QPushButton::released, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        ShowLineDefault(*qt_main->send_.destination_);
        ShowLineDefault(*qt_main->send_.amount_);
        qt_main->send_.send_->setEnabled(false);

        bool error = false;
        std::string error_info;
        do
        {
            std::string destination_str =
                qt_main->send_.destination_->text().toStdString();
            rai::StringTrim(destination_str, " \r\n\t");
            rai::AccountParser parser(destination_str);
            if (parser.Error())
            {
                ShowLineError(*qt_main->send_.destination_);
                error_info = "Bad destination account";
                break;
            }

            rai::Account destination = parser.Account();
            std::string sub_account = parser.SubAccount();

            std::vector<uint8_t> extensions;
            if (!sub_account.empty())
            {
                error = rai::ExtensionAppend(
                    rai::ExtensionSubAccount(sub_account), extensions);
                if (error)
                {
                    ShowLineError(*qt_main->send_.destination_);
                    error_info = "Bad destination sub account";
                    break;
                }
            }

            std::string amount_str =
                qt_main->send_.amount_->text().toStdString();
            rai::StringTrim(amount_str, "\r\n\t ");
            rai::Amount amount;
            error = amount.DecodeBalance(qt_main->rendering_ratio_, amount_str);
            if (error)
            {
                ShowLineError(*qt_main->send_.amount_);
                error_info = "Bad amount number";
                break;
            }

            rai::Amount balance;
            qt_main->wallets_->AccountBalance(
                qt_main->wallets_->SelectedAccount(), balance);
            if (balance < amount)
            {
                ShowLineError(*qt_main->send_.amount_);
                error_info = "Not enough balance";
                break;
            }

            std::string note = qt_main->send_.note_->text().toStdString();
            if (!note.empty())
            {
                error =
                    rai::ExtensionAppend(rai::ExtensionNote(note), extensions);
                if (error)
                {
                    ShowLineError(*qt_main->send_.note_);
                    error_info = "Bad note";
                    break;
                }
            }

            rai::ErrorCode error_code = qt_main->wallets_->AccountSend(
                destination, amount, extensions,
                [qt_main_w](rai::ErrorCode error_code,
                            const std::shared_ptr<rai::Block>& block) {
                    auto qt_main = qt_main_w.lock();
                    if (qt_main == nullptr) return;

                    qt_main->PostEvent([error_code](rai::QtMain& qt_main) {
                        if (error_code == rai::ErrorCode::SUCCESS)
                        {
                            qt_main.send_.amount_->clear();
                            qt_main.send_.destination_->clear();
                            qt_main.send_.note_->clear(); 
                            qt_main.send_.send_->setEnabled(true);
                        }
                        else
                        {
                            ShowButtonError(*qt_main.send_.send_);
                            qt_main.send_.send_->setText(
                                rai::ErrorString(error_code).c_str());
                            qt_main.PostEvent(5, [](rai::QtMain& qt_main) {
                                ShowButtonDefault(*qt_main.send_.send_);
                                qt_main.send_.send_->setText("Send");
                                qt_main.send_.send_->setEnabled(true);
                            });
                        }
                    });
                });
            if (error_code != rai::ErrorCode::SUCCESS)
            {
                error_info = rai::ErrorString(error_code);
            }
        } while (0);

        if (!error_info.empty())
        {
            ShowButtonError(*qt_main->send_.send_);
            qt_main->send_.send_->setText(error_info.c_str());
            qt_main->PostEvent(5, [](rai::QtMain& qt_main) {
                ShowButtonDefault(*qt_main.send_.send_);
                qt_main.send_.send_->setText("Send");
                qt_main.send_.send_->setEnabled(true);
            });
        }
    });
}

rai::QtReceive::QtReceive(rai::QtMain& qt_main)
    : window_(new QWidget),
      layout_(new QVBoxLayout),
      label_(new QLabel("Account receivables:")),
      model_(new QStandardItemModel),
      view_(new QTableView),
      receive_(new QPushButton("Receive")),
      receive_all_(new QPushButton("Receive all")),
      separator_(new QFrame),
      group_(new QButtonGroup),
      group_layout_(new QHBoxLayout),
      auto_receive_(new QLabel("Auto receive:")),
      yes_(new QRadioButton("Yes")),
      no_(new QRadioButton("No")),
      separator_2_(new QFrame),
      back_(new QPushButton("Back")),
      main_(qt_main)
{
    model_->setHorizontalHeaderItem(0, new QStandardItem("From"));
    model_->setHorizontalHeaderItem(1, new QStandardItem("Amount"));
    model_->setHorizontalHeaderItem(2, new QStandardItem("Hash"));
    view_->setModel(model_);
    view_->verticalHeader()->hide();
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);

    layout_->addWidget(label_);
    layout_->addWidget(view_);
    layout_->addWidget(receive_);
    layout_->addWidget(receive_all_);

    separator_->setFrameShape(QFrame::HLine);
    separator_->setFrameShadow(QFrame::Sunken);
    layout_->addWidget(separator_);

    group_->addButton(yes_);
    group_->addButton(no_);
    group_->setId(yes_, 0);
    group_->setId(no_, 1);
    group_layout_->addWidget(auto_receive_);
    group_layout_->addWidget(yes_);
    group_layout_->addWidget(no_);
    layout_->addLayout(group_layout_);

    separator_2_->setFrameShape(QFrame::HLine);
    separator_2_->setFrameShadow(QFrame::Sunken);
    layout_->addWidget(separator_2_);

    layout_->addWidget(back_);

    layout_->setContentsMargins(0, 0, 0, 0);
    window_->setLayout(layout_);
}

void rai::QtReceive::Receive(
    const rai::Account& account, const rai::BlockHash& hash,
    QPushButton* button,
    const std::function<void(rai::ErrorCode,
                             const std::shared_ptr<rai::Block>&)>& callback)
{
    receive_->setEnabled(false);
    receive_all_->setEnabled(false);
    std::weak_ptr<rai::QtMain> qt_main_w(main_.Shared());
    rai::ErrorCode error_code = main_.wallets_->AccountReceive(
        account, hash,
        [qt_main_w, button, callback](
            rai::ErrorCode error_code,
            const std::shared_ptr<rai::Block>& block) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent(
                [error_code, block, button, callback](rai::QtMain& qt_main) {
                    if (error_code == rai::ErrorCode::SUCCESS
                        || error_code
                               == rai::ErrorCode::WALLET_NOT_SELECTED_ACCOUNT)
                    {
                        callback(error_code, block);
                        qt_main.receive_.receive_->setEnabled(true);
                        qt_main.receive_.receive_all_->setEnabled(true);
                        return;
                    }

                    std::string error_info = rai::ErrorString(error_code);
                    if (error_code
                        == rai::ErrorCode::WALLET_RECEIVABLE_LESS_THAN_CREDIT)
                    {
                        error_info = "Minimum receivable amount is ";
                        error_info += qt_main.FormatBalance(
                            rai::CreditPrice(qt_main.CurrentTimestamp()));
                        error_info += " for account's first block";
                    }
                    ShowButtonError(*button);
                    button->setText(error_info.c_str());
                    qt_main.PostEvent(5, [button](rai::QtMain& qt_main) {
                        ShowButtonDefault(*button);
                        button->setText(button == qt_main.receive_.receive_
                                            ? "Receive"
                                            : "Receive all");
                        qt_main.receive_.receive_->setEnabled(true);
                        qt_main.receive_.receive_all_->setEnabled(true);
                    });
                    callback(error_code, block);
                });
        });

    if (error_code != rai::ErrorCode::SUCCESS
        && error_code != rai::ErrorCode::WALLET_NOT_SELECTED_ACCOUNT)
    {
        ShowButtonError(*button);
        button->setText(rai::ErrorString(error_code).c_str());
        main_.PostEvent(5, [button](rai::QtMain& qt_main) {
            ShowButtonDefault(*button);
            button->setText(button == qt_main.receive_.receive_
                                ? "Receive"
                                : "Receive all");
            qt_main.receive_.receive_->setEnabled(true);
            qt_main.receive_.receive_all_->setEnabled(true);
        });
    }
}

void rai::QtReceive::ReceiveNext(
    rai::ErrorCode error_code, const std::shared_ptr<rai::Block>& block,
    const rai::Account& account,
    const std::shared_ptr<std::vector<rai::BlockHash>>& hashs)
{
    IF_NOT_SUCCESS_RETURN_VOID(error_code);
    if (block == nullptr) return;

    hashs->erase(hashs->begin());
    if (hashs->empty()) return;

    main_.PostEvent([account, hashs](rai::QtMain& qt_main) {
        std::weak_ptr<rai::QtMain> qt_main_w(qt_main.Shared());
        qt_main.receive_.Receive(account, *hashs->begin(),
                                 qt_main.receive_.receive_all_,
                                 [qt_main_w, account, hashs](
                                     rai::ErrorCode error_code,
                                     const std::shared_ptr<rai::Block>& block) {
                                     auto qt_main = qt_main_w.lock();
                                     if (qt_main == nullptr) return;
                                     qt_main->receive_.ReceiveNext(
                                         error_code, block, account, hashs);
                                 });
    });
}

void rai::QtReceive::Refresh()
{
    model_->removeRows(0, model_->rowCount());

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, main_.wallets_->ledger_, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        // log
        return;
    }

    rai::Account account = main_.wallets_->SelectedAccount();
    rai::ReceivableInfos receivables;
    bool error = main_.wallets_->ledger_.ReceivableInfosGet(
        transaction, account, rai::ReceivableInfosType::ALL, receivables);
    if (error)
    {
        // log
        return;
    }

    for (const auto& i : receivables)
    {
        QList<QStandardItem*> items;
        items.push_back(new QStandardItem(
            QString(i.first.source_.StringAccount().c_str())));
        auto amount = new QStandardItem(
            QString(main_.FormatBalance(i.first.amount_).c_str()));
        amount->setData(static_cast<int>(Qt::AlignVCenter | Qt::AlignRight),
                        Qt::TextAlignmentRole);
        items.push_back(amount);
        items.push_back(
            new QStandardItem(QString(i.second.StringHex().c_str())));
        model_->appendRow(items);
    }
}

void rai::QtReceive::Start(const std::weak_ptr<rai::QtMain>& qt_main_w)
{
    QObject::connect(back_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Pop();
        }
    });

    QObject::connect(receive_, &QPushButton::released, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        auto selection =
            qt_main->receive_.view_->selectionModel()->selectedRows();
        if (selection.size() > 1)
        {
            return;
        }

        std::string hash_str;
        if (selection.size() == 0)
        {
            if (qt_main->receive_.model_->rowCount() == 0)
            {
                return;
            }
            hash_str =
                qt_main->receive_.model_->item(0, 2)->text().toStdString();
        }
        else
        {
            hash_str = qt_main->receive_.model_->item(selection[0].row(), 2)
                           ->text()
                           .toStdString();
        }

        rai::BlockHash hash;
        bool error = hash.DecodeHex(hash_str);
        IF_ERROR_RETURN_VOID(error);

        rai::Account account = qt_main->wallets_->SelectedAccount();
        qt_main->receive_.Receive(
            account, hash, qt_main->receive_.receive_,
            [](rai::ErrorCode, const std::shared_ptr<rai::Block>&) {});
    });

    QObject::connect(receive_all_, &QPushButton::released, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;
        if (qt_main->receive_.model_->rowCount() == 0)
        {
            return;
        }

        auto hashs = std::make_shared<std::vector<rai::BlockHash>>();
        for (int i = 0, n = qt_main->receive_.model_->rowCount(); i < n; ++i)
        {
            std::string hash_str =
                qt_main->receive_.model_->item(i, 2)->text().toStdString();
            rai::BlockHash hash;
            bool error = hash.DecodeHex(hash_str);
            IF_ERROR_RETURN_VOID(error);
            hashs->push_back(hash);
        }

        rai::Account account = qt_main->wallets_->SelectedAccount();
        qt_main->receive_.Receive(
            account, *hashs->begin(), qt_main->receive_.receive_all_,
            [qt_main_w, account, hashs](
                rai::ErrorCode error_code,
                const std::shared_ptr<rai::Block>& block) {
                auto qt_main = qt_main_w.lock();
                if (qt_main == nullptr) return;

                qt_main->receive_.ReceiveNext(error_code, block, account,
                                              hashs);
            });
    });

    QObject::connect(yes_, &QRadioButton::toggled, [this, qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;
        if (yes_->isChecked())
        {
            main_.auto_receive_ = true;
            main_.SaveConfig();
            main_.AutoReceiveNotify();
        }
    });

    QObject::connect(no_, &QRadioButton::toggled, [this, qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;
        if (no_->isChecked())
        {
            main_.auto_receive_ = false;
            main_.SaveConfig();
            main_.AutoReceiveNotify();
        }
    });

    main_.wallets_->observers_.block_.Add(
        [this, qt_main_w](const std::shared_ptr<rai::Block>& block,
                          bool rollback) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent([block](rai::QtMain& qt_main) {
                if (qt_main.wallets_->SelectedAccount() == block->Account()
                    && block->Opcode() == rai::BlockOpcode::RECEIVE)
                {
                    qt_main.receive_.Refresh();
                }
            });

            if (!rollback && block->Opcode() == rai::BlockOpcode::CREDIT)
            {
                if (main_.auto_receive_)
                {
                    main_.AutoReceiveNotify();
                }
            }
        });

    main_.wallets_->observers_.selected_account_.Add(
        [qt_main_w](const rai::Account& account) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent(
                [](rai::QtMain& qt_main) { qt_main.receive_.Refresh(); });
        });

    main_.wallets_->observers_.receivable_.Add(
        [this, qt_main_w](const rai::Account& account) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent([account](rai::QtMain& qt_main) {
                if (account == qt_main.wallets_->SelectedAccount())
                {
                    qt_main.receive_.Refresh();
                }
            });

            if (main_.auto_receive_)
            {
                main_.AutoReceiveNotify();
            }
        });

    main_.wallets_->observers_.wallet_locked_.Add(
        [this, qt_main_w](bool locked) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            if (!locked && main_.auto_receive_)
            {
                main_.AutoReceiveNotify();
            }
        });

    main_.wallets_->observers_.connection_status_.Add(
        [this, qt_main_w](rai::WebsocketStatus status) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            if (status == rai::WebsocketStatus::CONNECTED)
            {
                if (main_.auto_receive_)
                {
                    main_.AutoReceiveNotify();
                }
            }
        });

    main_.wallets_->observers_.synced_.Add(
        [this, qt_main_w](const rai::Account& account, bool synced) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            if (synced && main_.auto_receive_)
            {
                main_.AutoReceiveNotify();
            }
        });

    Refresh();

    if (main_.auto_receive_)
    {
        yes_->click();
    }
    else
    {
        no_->click();
    }
}

rai::QtSettings::QtSettings(rai::QtMain& qt_main)
    : window_(new QWidget),
      layout_(new QVBoxLayout),
      password_(new QLineEdit),
      lock_toggle_(new QPushButton),
      separator_1_(new QFrame),
      new_password_(new QLineEdit),
      retype_password_(new QLineEdit),
      change_password_(new QPushButton("Set/Change wallet password")),
      separator_2_(new QFrame),
      representative_(new QLabel("Account representative:")),
      current_representative_(new QLabel),
      new_representative_(new QLineEdit),
      change_representative_(new QPushButton("Change representative")),
      separator_3_(new QFrame),
      current_limit_(new QLabel),
      increase_window_(new QWidget),
      increase_layout_(new QHBoxLayout),
      increase_label_(new QLabel("Transactions:")),
      increase_(new QLineEdit),
      cost_label_(new QLabel("Cost:")),
      cost_(new QLineEdit),
      increase_button_(new QPushButton("Increase daily transactions limit")),
      back_(new QPushButton("Back")),
      main_(qt_main)
{
    password_->setPlaceholderText("Password"),
        password_->setEchoMode(QLineEdit::EchoMode::Password);

    separator_1_->setFrameShape(QFrame::HLine);
    separator_1_->setFrameShadow(QFrame::Sunken);

    new_password_->setEchoMode(QLineEdit::EchoMode::Password);
    new_password_->setPlaceholderText("New password");

    retype_password_->setEchoMode(QLineEdit::EchoMode::Password);
    retype_password_->setPlaceholderText("Retype password");

    separator_2_->setFrameShape(QFrame::HLine);
    separator_2_->setFrameShadow(QFrame::Sunken);

    current_representative_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    new_representative_->setPlaceholderText(
        rai::Account().StringAccount().c_str());

    separator_3_->setFrameShape(QFrame::HLine);
    separator_3_->setFrameShadow(QFrame::Sunken);

    current_limit_->setToolTip(
        "The transactions limit will be reset at 0 o'clock UTC every day");

    increase_->setPlaceholderText("Must be multiples of 20");
    cost_->setEnabled(false);
    increase_layout_->setContentsMargins(0, 0, 0, 0);
    increase_layout_->addWidget(increase_label_);
    increase_layout_->addWidget(increase_);
    increase_layout_->addWidget(cost_label_);
    increase_layout_->addWidget(cost_);
    increase_window_->setLayout(increase_layout_);

    layout_->addWidget(password_);
    layout_->addWidget(lock_toggle_);
    layout_->addWidget(separator_1_);
    layout_->addWidget(new_password_);
    layout_->addWidget(retype_password_);
    layout_->addWidget(change_password_);
    layout_->addWidget(separator_2_);
    layout_->addWidget(representative_);
    layout_->addWidget(current_representative_);
    layout_->addWidget(new_representative_);
    layout_->addWidget(change_representative_);
    layout_->addWidget(separator_3_);
    layout_->addWidget(current_limit_);
    layout_->addWidget(increase_window_);
    layout_->addWidget(increase_button_);
    layout_->addStretch();
    layout_->addWidget(back_);

    window_->setLayout(layout_);
}

void rai::QtSettings::DelayResume(void* widget)
{
    main_.PostEvent(5, [widget](rai::QtMain& qt_main) {
        if (widget == qt_main.settings_.increase_button_)
        {
            ShowButtonDefault(*qt_main.settings_.increase_button_);
            qt_main.settings_.increase_button_->setText(
                "Increase daily transactions limit");
            qt_main.settings_.increase_button_->setEnabled(true);
        }

        if (widget == qt_main.settings_.change_representative_)
        {
            ShowButtonDefault(*qt_main.settings_.change_representative_);
            qt_main.settings_.change_representative_->setText(
                "Change representative");
            qt_main.settings_.change_representative_->setEnabled(true);
        }
    });
}

void rai::QtSettings::Refresh()
{
    RefreshLock();
    RefreshRepresentative();
    RefreshTransactionsLimit();
}

void rai::QtSettings::RefreshLock()
{
    uint32_t wallet_id = main_.wallets_->SelectedWalletId();
    if (main_.wallets_->WalletLocked(wallet_id))
    {
        password_->setEnabled(true);
        lock_toggle_->setText("Unlock wallet");
        lock_toggle_->setEnabled(true);
    }
    else
    {
        password_->setEnabled(false);
        lock_toggle_->setText("Lock wallet");
        if (main_.wallets_->WalletVulnerable(wallet_id))
        {
            lock_toggle_->setEnabled(false);
        }
        else
        {
            lock_toggle_->setEnabled(true);
        }
    }
}

void rai::QtSettings::RefreshRepresentative()
{
    new_representative_->clear();
    ShowLineDefault(*new_representative_);
    rai::Account rep;
    bool error = main_.wallets_->AccountRepresentative(
        main_.wallets_->SelectedAccount(), rep);
    if (error)
    {
        current_representative_->setText("");
        new_representative_->setPlaceholderText(
            rai::Account().StringAccount().c_str());
        new_representative_->setEnabled(false);
        change_representative_->setEnabled(false);
    }
    else
    {
        current_representative_->setText(rep.StringAccount().c_str());
        new_representative_->setPlaceholderText(rep.StringAccount().c_str());
        new_representative_->setEnabled(true);
        change_representative_->setEnabled(true);
    }
}

void rai::QtSettings::RefreshTransactionsLimit()
{
    uint32_t total = 0;
    uint32_t used  = 0;
    main_.wallets_->AccountTransactionsLimit(main_.wallets_->SelectedAccount(),
                                             total, used);
    std::string limit = "Account daily transactions limit: "
                        + std::to_string(used) + " / " + std::to_string(total);
    current_limit_->setText(limit.c_str());
}

void rai::QtSettings::Start(const std::weak_ptr<rai::QtMain>& qt_main_w)
{
    QObject::connect(lock_toggle_, &QPushButton::released, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        if (qt_main->settings_.lock_toggle_->text() == QString("Lock wallet"))
        {
            qt_main->wallets_->LockWallet(
                qt_main->wallets_->SelectedWalletId());
            qt_main->settings_.RefreshLock();
        }
        else if (qt_main->settings_.lock_toggle_->text()
                 == QString("Unlock wallet"))
        {
            std::string password(
                qt_main->settings_.password_->text().toStdString());
            if (!qt_main->wallets_->EnterPassword(password))
            {
                qt_main->settings_.password_->clear();
                qt_main->settings_.RefreshLock();
            }
            else
            {
                ShowLineError(*qt_main->settings_.password_);
                ShowButtonError(*qt_main->settings_.lock_toggle_);
                qt_main->settings_.lock_toggle_->setText("Invalid password");

                qt_main->PostEvent(5, [](rai::QtMain& qt_main) {
                    ShowLineDefault(*qt_main.settings_.password_);
                    ShowButtonDefault(*qt_main.settings_.lock_toggle_);
                    qt_main.settings_.RefreshLock();
                });
            }

            rai::SecureClearString(password);
        }
        else
        {
            // do nothing
        }
    });

    QObject::connect(change_password_, &QPushButton::released, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        if (qt_main->settings_.new_password_->text().isEmpty())
        {
            qt_main->settings_.new_password_->clear();
            qt_main->settings_.new_password_->setPlaceholderText(
                "Empty Password - try again: New password");
            qt_main->settings_.retype_password_->clear();
            qt_main->settings_.retype_password_->setPlaceholderText(
                "Empty Password - try again: Retype password");
            return;
        }

        if (qt_main->settings_.new_password_->text()
            != qt_main->settings_.retype_password_->text())
        {
            qt_main->settings_.retype_password_->clear();
            qt_main->settings_.retype_password_->setPlaceholderText(
                "Password mismatch");
            return;
        }

        std::string password(
            qt_main->settings_.new_password_->text().toStdString());
        rai::ErrorCode error_code = qt_main->wallets_->ChangePassword(password);
        rai::SecureClearString(password);
        if (error_code == rai::ErrorCode::SUCCESS)
        {
            qt_main->settings_.new_password_->clear();
            qt_main->settings_.new_password_->setPlaceholderText(
                "New password");
            qt_main->settings_.retype_password_->clear();
            qt_main->settings_.retype_password_->setPlaceholderText(
                "Retype password");
            ShowButtonSuccess(*qt_main->settings_.change_password_);
            qt_main->settings_.change_password_->setText(
                "Password was changed");
            qt_main->settings_.RefreshLock();
        }
        else
        {
            ShowButtonError(*qt_main->settings_.change_password_);
            qt_main->settings_.change_password_->setText(
                rai::ErrorString(error_code).c_str());
        }

        qt_main->PostEvent(5, [](rai::QtMain& qt_main) {
            ShowButtonDefault(*qt_main.settings_.change_password_);
            qt_main.settings_.change_password_->setText(
                "Set/Change wallet password");
        });
    });

    QObject::connect(
        change_representative_, &QPushButton::released, [qt_main_w]() {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;
            ShowLineDefault(*qt_main->settings_.new_representative_);
            qt_main->settings_.change_representative_->setEnabled(false);

            bool error = false;
            std::string error_info;
            do
            {
                std::string rep_str =
                    qt_main->settings_.new_representative_->text()
                        .toStdString();
                rai::StringTrim(rep_str, " \r\n\t");
                rai::Account rep;
                error = rep.DecodeAccount(rep_str);
                if (error)
                {
                    ShowLineError(*qt_main->settings_.new_representative_);
                    error_info = "Invalid account";
                    break;
                }

                rai::ErrorCode error_code = qt_main->wallets_->AccountChange(
                    rep, [qt_main_w](rai::ErrorCode error_code,
                                     const std::shared_ptr<rai::Block>& block) {
                        auto qt_main = qt_main_w.lock();
                        if (qt_main == nullptr) return;

                        qt_main->PostEvent([error_code](rai::QtMain& qt_main) {
                            if (error_code == rai::ErrorCode::SUCCESS)
                            {
                                qt_main.settings_.RefreshRepresentative();
                            }
                            else
                            {
                                ShowButtonError(
                                    *qt_main.settings_.change_representative_);
                                qt_main.settings_.change_representative_
                                    ->setText(
                                        rai::ErrorString(error_code).c_str());
                                qt_main.settings_.DelayResume(
                                    qt_main.settings_.change_representative_);
                            }
                        });
                    });
                if (error_code != rai::ErrorCode::SUCCESS)
                {
                    error_info = rai::ErrorString(error_code);
                    break;
                }
            } while (0);

            if (!error_info.empty())
            {
                ShowButtonError(*qt_main->settings_.change_representative_);
                qt_main->settings_.change_representative_->setText(
                    error_info.c_str());
                qt_main->settings_.DelayResume(
                    qt_main->settings_.change_representative_);
            }
        });

    QObject::connect(
        increase_, &QLineEdit::textChanged, [qt_main_w](const QString& text) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            uint32_t tx = 0;
            bool error  = rai::StringToUint(text.toStdString(), tx);
            if (error || tx >= rai::MAX_ACCOUNT_DAILY_TRANSACTIONS
                || tx % rai::TRANSACTIONS_PER_CREDIT != 0)
            {
                qt_main->settings_.cost_->clear();
                return;
            }

            rai::Amount price = rai::CreditPrice(qt_main->CurrentTimestamp());
            rai::Amount cost(price.Number()
                             * (tx / rai::TRANSACTIONS_PER_CREDIT));
            qt_main->settings_.cost_->setText(
                qt_main->FormatBalance(cost).c_str());
        });

    QObject::connect(increase_button_, &QPushButton::released, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;
        ShowLineDefault(*qt_main->settings_.increase_);
        qt_main->settings_.increase_button_->setEnabled(false);

        bool error = false;
        std::string error_info;
        do
        {
            std::string increase_str =
                qt_main->settings_.increase_->text().toStdString();

            uint32_t tx = 0;
            error = rai::StringToUint(increase_str, tx);
            if (error || tx == 0)
            {
                ShowLineError(*qt_main->settings_.increase_);
                error_info = "Invalid transactions";
                break;
            }
            if (tx >= rai::MAX_ACCOUNT_DAILY_TRANSACTIONS)
            {
                ShowLineError(*qt_main->settings_.increase_);
                error_info =
                    "Transactions must be less than "
                    + std::to_string(rai::MAX_ACCOUNT_DAILY_TRANSACTIONS);
                break;
            }
            if (tx % rai::TRANSACTIONS_PER_CREDIT != 0)
            {
                ShowLineError(*qt_main->settings_.increase_);
                error_info = "Transactions must be multiples of 20";
                break;
            }

            rai::ErrorCode error_code = qt_main->wallets_->AccountCredit(
                tx / rai::TRANSACTIONS_PER_CREDIT,
                [qt_main_w](rai::ErrorCode error_code,
                            const std::shared_ptr<rai::Block>& block) {
                    auto qt_main = qt_main_w.lock();
                    if (qt_main == nullptr) return;

                    qt_main->PostEvent([error_code](rai::QtMain& qt_main) {
                        if (error_code == rai::ErrorCode::SUCCESS)
                        {
                            qt_main.settings_.increase_->clear();
                            qt_main.settings_.increase_button_->setEnabled(
                                true);
                        }
                        else
                        {
                            ShowButtonError(
                                *qt_main.settings_.increase_button_);
                            qt_main.settings_.increase_button_->setText(
                                rai::ErrorString(error_code).c_str());
                            qt_main.settings_.DelayResume(
                                qt_main.settings_.increase_button_);
                        }
                    });
                });

            if (error_code != rai::ErrorCode::SUCCESS)
            {
                error_info = rai::ErrorString(error_code);
            }
        } while (0);

        if (!error_info.empty())
        {
            ShowButtonError(*qt_main->settings_.increase_button_);
            qt_main->settings_.increase_button_->setText(error_info.c_str());
            qt_main->settings_.DelayResume(qt_main->settings_.increase_button_);
        }
    });

    QObject::connect(back_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Pop();
        }
    });

    main_.wallets_->observers_.block_.Add(
        [qt_main_w](const std::shared_ptr<rai::Block>& block, bool rollback) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent([block](rai::QtMain& qt_main) {
                if (block->Account() != qt_main.wallets_->SelectedAccount())
                {
                    return;
                }
                if (block->Height() == 0
                    || block->Opcode() == rai::BlockOpcode::CHANGE)
                {
                    qt_main.settings_.RefreshRepresentative();
                }
                qt_main.settings_.RefreshTransactionsLimit();
            });
        });

    main_.wallets_->observers_.selected_account_.Add(
        [qt_main_w](const rai::Account& account) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent([](rai::QtMain& qt_main) {
                qt_main.settings_.RefreshRepresentative();
                qt_main.settings_.RefreshTransactionsLimit();
            });
        });

    main_.wallets_->observers_.selected_wallet_.Add([qt_main_w](uint32_t) {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        qt_main->PostEvent(
            [](rai::QtMain& qt_main) { qt_main.settings_.RefreshLock(); });
    });

    main_.wallets_->observers_.time_synced_.Add([qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        qt_main->PostEvent([](rai::QtMain& qt_main) {
            qt_main.settings_.RefreshTransactionsLimit();
        });
    });

    Refresh();
}

rai::QtAccounts::QtAccounts(rai::QtMain& qt_main)
    : window_(new QWidget),
      layout_(new QVBoxLayout),
      accounts_label_(new QLabel),
      model_(new QStandardItemModel),
      view_(new QTableView),
      use_account_(new QPushButton("Use account")),
      create_account_(new QPushButton("Create account")),
      separator_(new QFrame),
      key_file_window_(new QWidget),
      key_file_layout_(new QHBoxLayout),
      key_file_label_(new QLabel("Key file:")),
      key_file_(new QLineEdit),
      key_file_button_(new QPushButton("choose...")),
      password_window_(new QWidget),
      password_layout_(new QHBoxLayout),
      password_label_(new QLabel("Password:")),
      password_(new QLineEdit),
      import_account_(new QPushButton("Import adhoc key")),
      separator_2_(new QFrame),
      back_(new QPushButton("Back")),
      main_(qt_main)
{
    model_->setHorizontalHeaderItem(0, new QStandardItem("Account ID"));
    model_->setHorizontalHeaderItem(1, new QStandardItem("Balance"));
    model_->setHorizontalHeaderItem(2, new QStandardItem("Receivable"));
    model_->setHorizontalHeaderItem(3, new QStandardItem("Account"));

    view_->setModel(model_);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->verticalHeader()->hide();
    view_->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    view_->horizontalHeader()->setStretchLastSection(true);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);

    separator_->setFrameShape(QFrame::HLine);
    separator_->setFrameShadow(QFrame::Sunken);

    key_file_layout_->addWidget(key_file_label_);
    key_file_layout_->addWidget(key_file_);
    key_file_layout_->addWidget(key_file_button_);
    key_file_window_->setLayout(key_file_layout_);

    password_layout_->addWidget(password_label_);
    password_->setEchoMode(QLineEdit::EchoMode::Password);
    password_layout_->addWidget(password_);
    password_window_->setLayout(password_layout_);

    separator_2_->setFrameShape(QFrame::HLine);
    separator_2_->setFrameShadow(QFrame::Sunken);

    layout_->addWidget(accounts_label_);
    layout_->addWidget(view_);
    layout_->addWidget(use_account_);
    layout_->addWidget(create_account_);
    layout_->addWidget(separator_);
    layout_->addWidget(key_file_window_), layout_->addWidget(password_window_),
        layout_->addWidget(import_account_);
    layout_->addWidget(separator_2_);
    layout_->addWidget(back_);

    window_->setLayout(layout_);
}

void rai::QtAccounts::OnMenuRequested(const QPoint& pos)
{
    std::weak_ptr<rai::QtMain> qt_main_w(main_.Shared());
    auto menu   = std::make_shared<QMenu>(view_);
    auto action = std::make_shared<QAction>("Copy Account");
    menu->addAction(action.get());
    QObject::connect(
        menu.get(), &QMenu::triggered,
        [pos, menu, action, qt_main_w](QAction*) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;
            QModelIndex index(qt_main->accounts_.view_->indexAt(pos));
            QStandardItem* item =
                qt_main->accounts_.model_->item(index.row(), 3);
            qt_main->application_.clipboard()->setText(item->text());
        });
    menu->popup(view_->viewport()->mapToGlobal(pos));
}

void rai::QtAccounts::Refresh()
{
    model_->removeRows(0, model_->rowCount());

    std::string label_str = "Accounts in current wallet (Wallet ID: "
                            + std::to_string(main_.wallets_->SelectedWalletId())
                            + "):";
    accounts_label_->setText(label_str.c_str());

    QBrush brush;
    QFont font;
    font.setBold(true);

    rai::Account selected_account = main_.wallets_->SelectedAccount();
    auto accounts = main_.wallets_->SelectedWallet()->Accounts();
    for (const auto& i : accounts)
    {
        rai::Amount ignore;
        rai::Amount balance;
        rai::Amount receivable;
        main_.wallets_->AccountBalanceAll(i.second.first, ignore, balance,
                                          receivable);
        bool selected = selected_account == i.second.first;
        QList<QStandardItem*> items;
        QStandardItem* item_id =
            new QStandardItem(std::to_string(i.first).c_str());
        if (selected) item_id->setFont(font);
        items.push_back(item_id);

        std::string balance_str     = main_.FormatBalance(balance);
        QStandardItem* item_balance = new QStandardItem(balance_str.c_str());
        item_balance->setData(
            static_cast<int>(Qt::AlignVCenter | Qt::AlignRight),
            Qt::TextAlignmentRole);
        if (selected) item_balance->setFont(font);
        items.push_back(item_balance);

        std::string receivable_str = main_.FormatBalance(receivable);
        QStandardItem* item_receivable =
            new QStandardItem(receivable_str.c_str());
        item_receivable->setData(
            static_cast<int>(Qt::AlignVCenter | Qt::AlignRight),
            Qt::TextAlignmentRole);
        if (selected) item_receivable->setFont(font);
        items.push_back(item_receivable);

        std::string account_str     = i.second.first.StringAccount();
        QStandardItem* item_account = new QStandardItem(account_str.c_str());
        i.second.second ? brush.setColor("red") : brush.setColor("black");
        item_account->setForeground(brush);
        if (selected) item_account->setFont(font);
        items.push_back(item_account);

        model_->appendRow(items);
    }

    view_->resizeColumnsToContents();
}

void rai::QtAccounts::Start(const std::weak_ptr<rai::QtMain>& qt_main_w)
{
    QObject::connect(use_account_, &QPushButton::released, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        auto selection =
            qt_main->accounts_.view_->selectionModel()->selectedRows();
        if (selection.size() != 1)
        {
            return;
        }

        std::string id = qt_main->accounts_.model_->item(selection[0].row())
                             ->text()
                             .toStdString();
        uint32_t account_id = 0;
        bool error          = rai::StringToUint(id, account_id);
        IF_ERROR_RETURN_VOID(error);
        qt_main->wallets_->SelectAccount(account_id);
    });

    QObject::connect(create_account_, &QPushButton::released, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        rai::ErrorCode error_code = qt_main->wallets_->CreateAccount();
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            ShowButtonError(*qt_main->accounts_.create_account_);
            qt_main->accounts_.create_account_->setText(
                rai::ErrorString(error_code).c_str());
        }
        else
        {
            ShowButtonSuccess(*qt_main->accounts_.create_account_);
            qt_main->accounts_.create_account_->setText(
                "New account was created");
        }

        qt_main->accounts_.Refresh();

        qt_main->PostEvent(5, [](rai::QtMain& qt_main) {
            ShowButtonDefault(*qt_main.accounts_.create_account_);
            qt_main.accounts_.create_account_->setText("Create account");
        });
    });

    QObject::connect(key_file_button_, &QPushButton::released, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        QString key_file_path = QFileDialog::getOpenFileName(qt_main->window_);
        ShowLineDefault(*qt_main->accounts_.key_file_);
        qt_main->accounts_.key_file_->setText(key_file_path);
    });

    QObject::connect(import_account_, &QPushButton::released, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        std::string path = qt_main->accounts_.key_file_->text().toStdString();
        rai::StringLeftTrim(path, " \t\r\n");
        rai::StringRightTrim(path, " \t\r\n");
        if (path.empty())
        {
            ShowLineError(*qt_main->accounts_.key_file_);
            return;
        }

        std::string password;
        password.reserve(1024);
        password = qt_main->accounts_.password_->text().toStdString();

        rai::KeyPair key_pair;
        rai::ErrorCode error_code = rai::DecryptKey(key_pair, path, password);
        rai::SecureClearString(password);

        if (error_code != rai::ErrorCode::SUCCESS)
        {
            if (error_code == rai::ErrorCode::PASSWORD_ERROR)
            {
                ShowLineError(*qt_main->accounts_.password_);
            }
            else
            {
                ShowLineError(*qt_main->accounts_.key_file_);
            }

            return;
        }
        ShowLineDefault(*qt_main->accounts_.password_);
        qt_main->accounts_.key_file_->clear();
        qt_main->accounts_.password_->clear();

        error_code = qt_main->wallets_->ImportAccount(key_pair);
        if (error_code == rai::ErrorCode::SUCCESS)
        {
            qt_main->accounts_.Refresh();
            return;
        }

        ShowButtonError(*qt_main->accounts_.import_account_);
        std::string error_str = rai::ErrorString(error_code);
        qt_main->accounts_.import_account_->setText(error_str.c_str());
        qt_main->PostEvent(5, [](rai::QtMain& qt_main) {
            ShowButtonDefault(*qt_main.accounts_.import_account_);
            qt_main.accounts_.import_account_->setText("Import adhoc key");
        });
    });

    QObject::connect(back_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Pop();
        }
    });

    QObject::connect(view_, &QTableView::customContextMenuRequested,
                     [qt_main_w](const QPoint& pos) {
                         if (auto qt_main = qt_main_w.lock())
                         {
                             qt_main->accounts_.OnMenuRequested(pos);
                         }
                     });

    main_.wallets_->observers_.block_.Add(
        [qt_main_w](const std::shared_ptr<rai::Block>& block, bool rollback) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent([block](rai::QtMain& qt_main) {
                auto wallet = qt_main.wallets_->SelectedWallet();
                if (wallet != nullptr && wallet->IsMyAccount(block->Account()))
                {
                    qt_main.accounts_.Refresh();
                }
            });
        });

    main_.wallets_->observers_.receivable_.Add(
        [qt_main_w](const rai::Account& account) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent([account](rai::QtMain& qt_main) {
                auto wallet = qt_main.wallets_->SelectedWallet();
                if (wallet != nullptr && wallet->IsMyAccount(account))
                {
                    qt_main.accounts_.Refresh();
                }
            });
        });

    main_.wallets_->observers_.selected_account_.Add(
        [qt_main_w](const rai::Account& account) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent(
                [](rai::QtMain& qt_main) { qt_main.accounts_.Refresh(); });
        });

    Refresh();
}

rai::QtWallets::QtWallets(rai::QtMain& qt_main)
    : window_(new QWidget),
      layout_(new QVBoxLayout),
      wallets_label_(new QLabel("Wallets:")),
      model_(new QStandardItemModel),
      view_(new QTableView),
      use_wallet_(new QPushButton("Use wallet")),
      create_wallet_(new QPushButton("Create wallet")),
      copy_wallet_seed_(new QPushButton("Copy wallet seed")),
      separator_(new QFrame),
      seed_(new QLineEdit),
      import_(new QPushButton("Import wallet")),
      separator_2_(new QFrame),
      back_(new QPushButton("Back")),
      main_(qt_main)
{
    model_->setHorizontalHeaderItem(0, new QStandardItem("Wallet ID"));
    model_->setHorizontalHeaderItem(1, new QStandardItem("Accounts"));
    model_->setHorizontalHeaderItem(2, new QStandardItem("Total Balance"));
    model_->setHorizontalHeaderItem(3, new QStandardItem("Total Receivable"));

    view_->setModel(model_);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->verticalHeader()->hide();
    view_->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);

    separator_->setFrameShape(QFrame::HLine);
    separator_->setFrameShadow(QFrame::Sunken);

    seed_->setPlaceholderText("Wallet seed");

    separator_2_->setFrameShape(QFrame::HLine);
    separator_2_->setFrameShadow(QFrame::Sunken);

    layout_->addWidget(wallets_label_);
    layout_->addWidget(view_);
    layout_->addWidget(use_wallet_);
    layout_->addWidget(create_wallet_);
    layout_->addWidget(copy_wallet_seed_);
    layout_->addWidget(separator_);
    layout_->addWidget(seed_);
    layout_->addWidget(import_);
    layout_->addWidget(separator_2_);
    layout_->addWidget(back_);

    window_->setLayout(layout_);
}

void rai::QtWallets::Refresh()
{
    model_->removeRows(0, model_->rowCount());

    QBrush brush;
    QFont font;
    font.setBold(true);

    uint32_t selected_wallet = main_.wallets_->SelectedWalletId();
    auto ids                 = main_.wallets_->WalletIds();
    for (const auto& i : ids)
    {
        rai::Amount ignore;
        rai::Amount balance;
        rai::Amount receivable;
        main_.wallets_->WalletBalanceAll(i, ignore, balance, receivable);
        bool selected = selected_wallet == i;
        QList<QStandardItem*> items;
        QStandardItem* item_id = new QStandardItem(std::to_string(i).c_str());
        if (selected) item_id->setFont(font);
        items.push_back(item_id);

        std::string wallets_str =
            std::to_string(main_.wallets_->WalletAccounts(i));
        QStandardItem* item_wallets = new QStandardItem(wallets_str.c_str());
        item_wallets->setData(static_cast<int>(Qt::AlignCenter),
                              Qt::TextAlignmentRole);
        if (selected) item_wallets->setFont(font);
        items.push_back(item_wallets);

        std::string balance_str     = main_.FormatBalance(balance);
        QStandardItem* item_balance = new QStandardItem(balance_str.c_str());
        item_balance->setData(
            static_cast<int>(Qt::AlignVCenter | Qt::AlignRight),
            Qt::TextAlignmentRole);
        if (selected) item_balance->setFont(font);
        items.push_back(item_balance);

        std::string receivable_str = main_.FormatBalance(receivable);
        QStandardItem* item_receivable =
            new QStandardItem(receivable_str.c_str());
        item_receivable->setData(
            static_cast<int>(Qt::AlignVCenter | Qt::AlignRight),
            Qt::TextAlignmentRole);
        if (selected) item_receivable->setFont(font);
        items.push_back(item_receivable);

        model_->appendRow(items);
    }

    view_->resizeColumnsToContents();
}

void rai::QtWallets::Start(const std::weak_ptr<rai::QtMain>& qt_main_w)
{
    QObject::connect(use_wallet_, &QPushButton::released, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        auto selection =
            qt_main->qt_wallets_.view_->selectionModel()->selectedRows();
        if (selection.size() != 1)
        {
            return;
        }

        std::string id = qt_main->qt_wallets_.model_->item(selection[0].row())
                             ->text()
                             .toStdString();
        uint32_t wallet_id = 0;
        bool error         = rai::StringToUint(id, wallet_id);
        IF_ERROR_RETURN_VOID(error);
        qt_main->wallets_->SelectWallet(wallet_id);
    });

    QObject::connect(create_wallet_, &QPushButton::released, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        rai::ErrorCode error_code = qt_main->wallets_->CreateWallet();
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            ShowButtonError(*qt_main->qt_wallets_.create_wallet_);
            qt_main->qt_wallets_.create_wallet_->setText(
                rai::ErrorString(error_code).c_str());
        }
        else
        {
            ShowButtonSuccess(*qt_main->qt_wallets_.create_wallet_);
            qt_main->qt_wallets_.create_wallet_->setText(
                "New wallet was created");
        }

        qt_main->qt_wallets_.Refresh();

        qt_main->PostEvent(5, [](rai::QtMain& qt_main) {
            ShowButtonDefault(*qt_main.qt_wallets_.create_wallet_);
            qt_main.qt_wallets_.create_wallet_->setText("Create wallet");
        });
    });

    QObject::connect(copy_wallet_seed_, &QPushButton::clicked, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        auto wallet_id = qt_main->wallets_->SelectedWalletId();
        rai::RawKey seed;
        rai::ErrorCode error_code =
            qt_main->wallets_->WalletSeed(wallet_id, seed);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            ShowButtonError(*qt_main->qt_wallets_.copy_wallet_seed_);
            qt_main->qt_wallets_.copy_wallet_seed_->setText(
                rai::ErrorString(error_code).c_str());
        }
        else
        {
            ShowButtonSuccess(*qt_main->qt_wallets_.copy_wallet_seed_);
            qt_main->application_.clipboard()->setText(
                seed.data_.StringHex().c_str());
            std::string info =
                "Seed copied (Wallet ID: " + std::to_string(wallet_id) + ")";
            qt_main->qt_wallets_.copy_wallet_seed_->setText(info.c_str());
        }

        qt_main->PostEvent(3, [](rai::QtMain& qt_main) {
            ShowButtonDefault(*qt_main.qt_wallets_.copy_wallet_seed_);
            qt_main.qt_wallets_.copy_wallet_seed_->setText("Copy wallet seed");
        });
    });

    QObject::connect(import_, &QPushButton::clicked, [qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        bool error = false;
        std::string show_info;
        do
        {
            rai::RawKey seed;
            error = seed.data_.DecodeHex(
                qt_main->qt_wallets_.seed_->text().toStdString());
            if (error)
            {
                ShowLineError(*qt_main->qt_wallets_.seed_);
                show_info = "Bad wallet seed";
                break;
            }

            uint32_t wallet_id = 0;
            rai::ErrorCode error_code =
                qt_main->wallets_->ImportWallet(seed, wallet_id);
            if (error_code == rai::ErrorCode::SUCCESS)
            {
                show_info = "Import successfully (Wallet ID: "
                            + std::to_string(wallet_id) + ")";
            }
            else if (error_code == rai::ErrorCode::WALLET_EXISTS)
            {
                error     = true;
                show_info = "The wallet already exists (Wallet ID: "
                            + std::to_string(wallet_id) + ")";
            }
            else
            {
                error     = true;
                show_info = rai::ErrorString(error_code);
            }
        } while (0);

        if (error)
        {
            ShowButtonError(*qt_main->qt_wallets_.import_);
            qt_main->qt_wallets_.import_->setText(show_info.c_str());
        }
        else
        {
            ShowLineDefault(*qt_main->qt_wallets_.seed_);
            ShowButtonSuccess(*qt_main->qt_wallets_.import_);
            qt_main->qt_wallets_.seed_->clear();
            qt_main->qt_wallets_.import_->setText(show_info.c_str());
        }

        qt_main->qt_wallets_.Refresh();

        qt_main->PostEvent(5, [](rai::QtMain& qt_main) {
            ShowButtonDefault(*qt_main.qt_wallets_.import_);
            qt_main.qt_wallets_.import_->setText("Import wallet");
        });
    });

    QObject::connect(back_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Pop();
        }
    });

    main_.wallets_->observers_.block_.Add(
        [qt_main_w](const std::shared_ptr<rai::Block>& block, bool rollback) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent([block](rai::QtMain& qt_main) {
                qt_main.qt_wallets_.Refresh();
            });
        });

    main_.wallets_->observers_.receivable_.Add(
        [qt_main_w](const rai::Account& account) {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->PostEvent(
                [](rai::QtMain& qt_main) { qt_main.qt_wallets_.Refresh(); });
        });

    main_.wallets_->observers_.selected_wallet_.Add([qt_main_w](uint32_t) {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        qt_main->PostEvent(
            [](rai::QtMain& qt_main) { qt_main.qt_wallets_.Refresh(); });
    });

    Refresh();
}

rai::QtSignVerify::QtSignVerify(rai::QtMain& qt_main)
    : window_(new QWidget),
      layout_(new QVBoxLayout),
      message_label_(new QLabel("Message:")),
      message_(new QPlainTextEdit),
      account_label_(new QLabel("Account:")),
      account_(new QLineEdit),
      signature_label_(new QLabel("Signature:")),
      signature_(new QPlainTextEdit),
      sign_(new QPushButton("Sign")),
      verify_(new QPushButton("Verify")),
      back_(new QPushButton("Back")),
      main_(qt_main)
{
    layout_->addWidget(message_label_),
    layout_->addWidget(message_),
    layout_->addWidget(account_label_);
    layout_->addWidget(account_),
    layout_->addWidget(signature_label_),
    layout_->addWidget(signature_);
    layout_->addWidget(sign_);
    layout_->addWidget(verify_);
    layout_->addWidget(back_);

    window_->setLayout(layout_);
}

void rai::QtSignVerify::Start(const std::weak_ptr<rai::QtMain>& qt_main_w)
{
    QObject::connect(sign_, &QPushButton::released, [qt_main_w, this]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        bool error = false;
        std::string show_info;
        do
        {
            std::string message = message_->toPlainText().toStdString();
            if (message.empty())
            {
                error = true;
                show_info = "Error: message empty";
                break;
            }

            std::string account_str = account_->text().toStdString();
            rai::StringTrim(account_str, " \r\n\t");
            if (account_str.empty())
            {
                error = true;
                show_info = "Error: account empty";
                break;
            }

            rai::Account account;
            error = account.DecodeAccount(account_str);
            if (error)
            {
                error = true;
                show_info = "Error: invalid account";
                break;
            }

            auto wallet = qt_main->wallets_->SelectedWallet();
            if (!wallet->IsMyAccount(account))
            {
                error = true;
                show_info = "Error: this account doesn't exist";
                break;
            }

            if (qt_main->wallets_->WalletLocked(
                    qt_main->wallets_->SelectedWalletId()))
            {
                error     = true;
                show_info = "Error: wallet is locked, unlock it first";
                break;
            }

            rai::BlockHash hash;
            blake2b_state state;
            blake2b_init(&state, sizeof(hash));
            blake2b_update(&state, message.data(), message.size());
            blake2b_final(&state, hash.bytes.data(), hash.bytes.size());

            rai::Signature signature;
            error = wallet->Sign(account, hash, signature);
            if (error)
            {
                show_info = "Error: failed to sign message";
                break;
            }

            signature_->setPlainText(signature.StringHex().c_str());
        } while (0);

        if (error)
        {
            ShowButtonError(*sign_);
            sign_->setText(show_info.c_str());
        }
        else
        {
            ShowButtonSuccess(*sign_);
            sign_->setText("Success");
        }

        qt_main->PostEvent(5, [this](rai::QtMain& qt_main) {
            ShowButtonDefault(*sign_);
            sign_->setText("Sign");
        });
    });

    QObject::connect(verify_, &QPushButton::released, [qt_main_w, this]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;

        bool error = false;
        std::string show_info;
        do
        {
            std::string message = message_->toPlainText().toStdString();
            if (message.empty())
            {
                error = true;
                show_info = "Error: message empty";
                break;
            }

            std::string account_str = account_->text().toStdString();
            rai::StringTrim(account_str, " \r\n\t");
            if (account_str.empty())
            {
                error = true;
                show_info = "Error: account empty";
                break;
            }

            rai::Account account;
            error = account.DecodeAccount(account_str);
            if (error)
            {
                error = true;
                show_info = "Error: invalid account";
                break;
            }


            std::string signature_str = signature_->toPlainText().toStdString();
            rai::StringTrim(signature_str, " \r\n\t");
            if (signature_str.empty())
            {
                error = true;
                show_info = "Error: signature empty";
                break;
            }

            rai::Signature signature;
            error = signature.DecodeHex(signature_str);
            if (error)
            {
                error = true;
                show_info = "Error: invalid signature";
                break;
            }

            rai::BlockHash hash;
            blake2b_state state;
            blake2b_init(&state, sizeof(hash));
            blake2b_update(&state, message.data(), message.size());
            blake2b_final(&state, hash.bytes.data(), hash.bytes.size());

            error = rai::ValidateMessage(account, hash, signature);
            if (error)
            {
                error = true;
                show_info = "Wrong signature";
                break;
            }
        } while (0);

        if (error)
        {
            ShowButtonError(*verify_);
            verify_->setText(show_info.c_str());
        }
        else
        {
            ShowButtonSuccess(*verify_);
            verify_->setText("Signature verified");
        }

        qt_main->PostEvent(5, [this](rai::QtMain& qt_main) {
            ShowButtonDefault(*verify_);
            verify_->setText("Verify");
        });
    });
    
    QObject::connect(back_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Pop();
        } 
    });
}

rai::QtCreateBlock::QtCreateBlock(rai::QtMain& qt_main)
    : window_(new QWidget),
      layout_(new QVBoxLayout),
      group_(new QButtonGroup),
      group_layout_(new QHBoxLayout),
      send_(new QRadioButton("Send")),
      receive_(new QRadioButton("Receive")),
      change_(new QRadioButton("Change")),
      credit_(new QRadioButton("Credit")),
      source_label_(new QLabel ("Source:")),
      source_(new QLineEdit),
      destination_label_(new QLabel ("Destination:")),
      destination_(new QLineEdit),
      amount_label_(new QLabel ("Amount:")),
      amount_(new QLineEdit),
      representative_label_(new QLabel ("Representative:")),
      representative_(new QLineEdit),
      credit_label_(new QLabel("Credit To Increase:")),
      credit_amount_(new QLineEdit),
      block_(new QPlainTextEdit),
      status_(new QLabel),
      create_json_(new QPushButton("Create Json")),
      create_raw_(new QPushButton("Create Raw")),
      back_(new QPushButton("Back")),
      main_(qt_main)
{
    group_->addButton(send_);
    group_->addButton(receive_);
    group_->addButton(change_);
    group_->addButton(credit_);
    group_->setId(send_, 0);
    group_->setId(receive_, 1);
    group_->setId(change_, 2);
    group_->setId(credit_, 3);
    group_layout_->addWidget(send_);
    group_layout_->addWidget(receive_);
    group_layout_->addWidget(change_);
    group_layout_->addWidget(credit_);

    layout_->addLayout(group_layout_);
    layout_->addWidget(source_label_);
    layout_->addWidget(source_);
    layout_->addWidget(destination_label_);
    layout_->addWidget(destination_);
    layout_->addWidget(amount_label_);
    layout_->addWidget(amount_);
    layout_->addWidget(representative_label_);
    layout_->addWidget(representative_);
    layout_->addWidget(credit_label_);
    layout_->addWidget(credit_amount_);
    layout_->addWidget(block_);
    layout_->addWidget(status_);
    layout_->addWidget(create_json_);
    layout_->addWidget(create_raw_);
    layout_->addWidget(back_);

    window_->setLayout(layout_);
}

void rai::QtCreateBlock::Start(const std::weak_ptr<rai::QtMain>& qt_main_w)
{
    QObject::connect(send_, &QRadioButton::toggled, [this, qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;
        if (send_->isChecked())
        {
            HideAll();
            ShowSend();
        }
    });

    QObject::connect(receive_, &QRadioButton::toggled, [this, qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;
        if (receive_->isChecked())
        {
            HideAll();
            ShowReceive();
        }
    });

    QObject::connect(change_, &QRadioButton::toggled, [this, qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;
        if (change_->isChecked())
        {
            HideAll();
            ShowChange();
        }
    });

    QObject::connect(credit_, &QRadioButton::toggled, [this, qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;
        if (credit_->isChecked())
        {
            HideAll();
            ShowCredit();
        }
    });

    QObject::connect(create_json_, &QPushButton::released, [this, qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;
        Create(false);
    });

    QObject::connect(create_raw_, &QPushButton::released, [this, qt_main_w]() {
        auto qt_main = qt_main_w.lock();
        if (qt_main == nullptr) return;
        Create(true);
    });

    QObject::connect(back_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Pop();
        } 
    });

    send_->click();
}

void rai::QtCreateBlock::HideAll()
{
    source_label_->hide();
    source_->hide();
    destination_label_->hide();
    destination_->hide();
    amount_label_->hide();
    amount_->hide();
    representative_label_->hide();
    representative_->hide();
    credit_label_->hide();
    credit_amount_->hide();
    block_->setPlainText("");
    ShowDefault("");
}

void rai::QtCreateBlock::ShowSend()
{
    destination_label_->show();
    destination_->show();
    amount_label_->show();
    amount_->show();
}

void rai::QtCreateBlock::ShowReceive()
{
    source_label_->show();
    source_->show();
    amount_label_->show();
    amount_->show();
}

void rai::QtCreateBlock::ShowChange()
{
    representative_label_->show();
    representative_->show();
}

void rai::QtCreateBlock::ShowCredit()
{
    credit_label_->show();
    credit_amount_->show();
}

void rai::QtCreateBlock::Create(bool raw)
{
    auto wallet_id = main_.wallets_->SelectedWalletId();
    if (main_.wallets_->WalletLocked(wallet_id))
    {
        ShowError(rai::ErrorCode::WALLET_LOCKED);
        return;
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    auto& ledger = main_.wallets_->ledger_;
    rai::Transaction transaction(error_code, ledger, false);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ShowError(error_code);
        return;
    }

    rai::AccountInfo info;
    rai::Account account = main_.wallets_->SelectedAccount();
    ledger.AccountInfoGet(transaction, account, info);

    std::shared_ptr<rai::Block> head(nullptr);
    if (info.Valid())
    {
        bool error = ledger.BlockGet(transaction, info.head_, head);
        if (error)
        {
            ShowError(rai::ErrorCode::LEDGER_BLOCK_GET);
            return;
        }
    }

    if (!info.Valid() && group_->checkedId() != 1)
    {
        ShowError("The account's first block should be a receive block");
        return;
    }

    std::unique_ptr<rai::Block> block(nullptr);
    switch (group_->checkedId())
    {
        case 0:
        {
            block = CreateSend(account, info, head);
            break;
        }
        case 1:
        {
            block = CreateReceive(account, info, head);
            break;
        }
        case 2:
        {
            block = CreateChange(account, info, head);
            break;
        }
        case 3:
        {
            block = CreateCredit(account, info, head);
            break;
        }
        default:
        {
            break;
        }
    }

    if (block == nullptr)
    {
        return;
    }

    std::string show;
    if (raw)
    {
        std::vector<uint8_t> bytes;
        {
            rai::VectorStream stream(bytes);
            block->Serialize(stream);
        }
        show = rai::BytesToHex(bytes.data(), bytes.size());
    }
    else
    {
        show = block->Json();
    }
    block_->setPlainText(show.c_str());
    ShowSuccess("Block Created");
}

std::unique_ptr<rai::Block> rai::QtCreateBlock::CreateSend(
    const rai::Account& account, const rai::AccountInfo& info,
    std::shared_ptr<rai::Block>& head)
{
    std::string destination_str = destination_->text().toStdString();
    rai::StringTrim(destination_str, " \r\n\t");
    rai::AccountParser parser(destination_str);
    if (parser.Error())
    {
        ShowError("Bad destination account");
        return nullptr;
    }

    rai::Account destination = parser.Account();
    std::string sub_account  = parser.SubAccount();

    std::vector<uint8_t> extensions;
    if (!sub_account.empty())
    {
        bool error = rai::ExtensionAppend(rai::ExtensionSubAccount(sub_account),
                                          extensions);
        if (error)
        {
            ShowError("Bad sub account");
            return nullptr;
        }
    }

    std::string amount_str = amount_->text().toStdString();
    rai::StringTrim(amount_str, "\r\n\t ");
    rai::Amount amount;
    bool error = amount.DecodeBalance(main_.rendering_ratio_, amount_str);
    if (error)
    {
        ShowError("Bad amount number");
        return nullptr;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::SEND;
    uint16_t credit = head->Credit();

    uint64_t now = main_.CurrentTimestamp();
    uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
    if (timestamp > now + 60)
    {
        ShowError(rai::ErrorCode::BLOCK_TIMESTAMP);
        return nullptr;
    }
    if (info.forks_ > rai::MaxAllowedForks(timestamp, credit))
    {
        ShowError(rai::ErrorCode::ACCOUNT_RESTRICTED);
        return nullptr;
    }
    
    uint32_t counter =
        rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        ShowError(rai::ErrorCode::ACCOUNT_ACTION_CREDIT);
        return nullptr;
    }
    uint64_t height = head->Height() + 1;
    rai::BlockHash previous = info.head_;
    if (head->Balance() < amount)
    {
        ShowError(rai::ErrorCode::ACCOUNT_ACTION_BALANCE);
        return nullptr;
    }
    rai::Amount balance = head->Balance() - amount;
    rai::uint256_union link = destination;

    if (extensions.size() > rai::TxBlock::MaxExtensionsLength())
    {
        ShowError(rai::ErrorCode::EXTENSIONS_LENGTH);
        return nullptr;
    }

    rai::Ptree extensions_ptree;
    if (rai::ExtensionsToPtree(extensions, extensions_ptree))
    {
        ShowError(rai::ErrorCode::GENERIC);
        return nullptr;
    }

    rai::RawKey private_key;
    auto wallet = main_.wallets_->SelectedWallet();
    rai::ErrorCode error_code  = wallet->PrivateKey(account, private_key);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ShowError(error_code);
        return nullptr;
    }

    if (info.type_ == rai::BlockType::TX_BLOCK)
    {
        return std::make_unique<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account, previous,
            head->Representative(), balance, link, extensions.size(),
            extensions, private_key, account);
    }
    else
    {
        ShowError(rai::ErrorCode::BLOCK_TYPE);
        return nullptr;
    }
}

std::unique_ptr<rai::Block> rai::QtCreateBlock::CreateReceive(
    const rai::Account& account, const rai::AccountInfo& info,
    std::shared_ptr<rai::Block>& head)
{
    std::string source_str = source_->text().toStdString();
    rai::StringTrim(source_str, " \r\n\t");
    rai::BlockHash source;
    bool error = source.DecodeHex(source_str);
    if (error)
    {
        ShowError("Invalid source");
        return nullptr;
    }

    std::string amount_str = amount_->text().toStdString();
    rai::StringTrim(amount_str, "\r\n\t ");
    rai::Amount amount;
    error = amount.DecodeBalance(main_.rendering_ratio_, amount_str);
    if (error)
    {
        ShowError("Bad amount number");
        return nullptr;
    }

    if (!info.Valid())
    {
        rai::BlockOpcode opcode = rai::BlockOpcode::RECEIVE;
        uint16_t credit         = 1;
        uint64_t timestamp      = main_.CurrentTimestamp();
        uint32_t counter        = 1;
        uint64_t height         = 0;
        rai::BlockHash previous(0);
        if (amount < rai::CreditPrice(timestamp))
        {
            ShowError(rai::ErrorCode::WALLET_RECEIVABLE_LESS_THAN_CREDIT);
            return nullptr;
        }
        rai::Amount balance     = amount - rai::CreditPrice(timestamp);
        rai::uint256_union link = source;
        rai::Account rep        = main_.wallets_->RandomRepresentative();
        rai::RawKey private_key;
        auto wallet               = main_.wallets_->SelectedWallet();
        rai::ErrorCode error_code = wallet->PrivateKey(account, private_key);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            ShowError(error_code);
            return nullptr;
        }

        return std::make_unique<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account, previous, rep,
            balance, link, 0, std::vector<uint8_t>(), private_key, account);
    }
    else
    {
        rai::BlockOpcode opcode = rai::BlockOpcode::RECEIVE;
        uint16_t credit         = head->Credit();

        uint64_t timestamp = main_.CurrentTimestamp();

        if (info.forks_ > rai::MaxAllowedForks(timestamp, credit))
        {
            ShowError(rai::ErrorCode::ACCOUNT_RESTRICTED);
            return nullptr;
        }

        uint32_t counter = rai::SameDay(timestamp, head->Timestamp())
                               ? head->Counter() + 1
                               : 1;
        if (counter
            > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
        {
            ShowError(rai::ErrorCode::ACCOUNT_ACTION_CREDIT);
            return nullptr;
        }
        uint64_t height         = head->Height() + 1;
        rai::BlockHash previous = info.head_;
        rai::Amount balance     = head->Balance() + amount;
        rai::uint256_union link = source;
        rai::RawKey private_key;
        auto wallet = main_.wallets_->SelectedWallet();
        rai::ErrorCode error_code = wallet->PrivateKey(account, private_key);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            ShowError(error_code);
            return nullptr;
        }

        if (info.type_ == rai::BlockType::TX_BLOCK)
        {
            return std::make_unique<rai::TxBlock>(
                opcode, credit, counter, timestamp, height, account, previous,
                head->Representative(), balance, link, 0, std::vector<uint8_t>(),
                private_key, account);
        }
        else
        {
            ShowError(rai::ErrorCode::BLOCK_TYPE);
            return nullptr;
        }
    }
}

std::unique_ptr<rai::Block> rai::QtCreateBlock::CreateChange(
    const rai::Account& account, const rai::AccountInfo& info,
    std::shared_ptr<rai::Block>& head)
{
    std::string rep_str = representative_->text().toStdString();
    rai::StringTrim(rep_str, " \r\n\t");
    rai::Account rep;
    bool error = rep.DecodeAccount(rep_str);
    if (error)
    {
        ShowError("Invalid representative account");
        return nullptr;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::CHANGE;
    uint16_t credit = head->Credit();

    uint64_t now = main_.CurrentTimestamp();
    uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
    if (timestamp > now + 60)
    {
        ShowError(rai::ErrorCode::BLOCK_TIMESTAMP);
        return nullptr;
    }
    if (info.forks_ > rai::MaxAllowedForks(timestamp, credit))
    {
        ShowError(rai::ErrorCode::ACCOUNT_RESTRICTED);
        return  nullptr;
    }

    uint32_t counter =
        rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        ShowError(rai::ErrorCode::ACCOUNT_ACTION_CREDIT);
        return nullptr;
    }
    uint64_t height = head->Height() + 1;
    rai::BlockHash previous = info.head_;
    rai::Amount balance = head->Balance();
    rai::uint256_union link(0);
    rai::RawKey private_key;
    auto wallet = main_.wallets_->SelectedWallet();
    rai::ErrorCode error_code = wallet->PrivateKey(account, private_key);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ShowError(error_code);
        return nullptr;
    }

    if (info.type_ == rai::BlockType::TX_BLOCK)
    {
        return std::make_unique<rai::TxBlock>(
            opcode, credit, counter, timestamp, height, account, previous, rep,
            balance, link, 0, std::vector<uint8_t>(), private_key, account);
    }
    else
    {
        ShowError(rai::ErrorCode::BLOCK_TYPE);
        return nullptr;
    }
}

std::unique_ptr<rai::Block> rai::QtCreateBlock::CreateCredit(
    const rai::Account& account, const rai::AccountInfo& info,
    std::shared_ptr<rai::Block>& head)
{
    std::string credit_str = credit_amount_->text().toStdString();
    rai::StringTrim(credit_str, " \r\n\t");
    uint16_t credit_inc = 0;
    bool error = rai::StringToUint(credit_str, credit_inc);
    if (error || 0 == credit_inc)
    {
        ShowError("Invalid credit number");
        return nullptr;
    }

    rai::BlockOpcode opcode = rai::BlockOpcode::CREDIT;
    uint16_t credit = head->Credit() + credit_inc;
    if (credit <= head->Credit())
    {
        ShowError(rai::ErrorCode::ACCOUNT_MAX_CREDIT);
        return nullptr;
    }

    uint64_t now = main_.CurrentTimestamp();
    uint64_t timestamp = now > head->Timestamp() ? now : head->Timestamp();
    if (timestamp > now + 60)
    {
        ShowError(rai::ErrorCode::BLOCK_TIMESTAMP);
        return nullptr;
    }
    if (info.forks_ > rai::MaxAllowedForks(timestamp, credit))
    {
        ShowError(rai::ErrorCode::ACCOUNT_RESTRICTED);
        return nullptr;
    }

    uint32_t counter =
        rai::SameDay(timestamp, head->Timestamp()) ? head->Counter() + 1 : 1;
    if (counter > static_cast<uint32_t>(credit) * rai::TRANSACTIONS_PER_CREDIT)
    {
        ShowError(rai::ErrorCode::ACCOUNT_ACTION_CREDIT);
        return nullptr;
    }

    uint64_t height = head->Height() + 1;
    rai::BlockHash previous = info.head_;

    rai::Amount cost(rai::CreditPrice(timestamp).Number() * credit_inc);
    if (cost > head->Balance())
    {
        ShowError(rai::ErrorCode::ACCOUNT_ACTION_BALANCE);
        return nullptr;
    }
    rai::Amount balance = head->Balance() - cost;

    rai::uint256_union link(0);
    rai::RawKey private_key;
    auto wallet = main_.wallets_->SelectedWallet();
    rai::ErrorCode error_code = wallet->PrivateKey(account, private_key);
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        ShowError(error_code);
        return nullptr;
    }

    return std::make_unique<rai::TxBlock>(
        opcode, credit, counter, timestamp, height, account, previous,
        head->Representative(), balance, link, 0, std::vector<uint8_t>(),
        private_key, account);
}

void rai::QtCreateBlock::ShowError(const std::string& show)
{
    ShowLabelError(*status_);
    status_->setText(show.c_str());
}

void rai::QtCreateBlock::ShowError(rai::ErrorCode error_code)
{
    ShowError(rai::ErrorString(error_code));
}

void rai::QtCreateBlock::ShowSuccess(const std::string& show)
{
    ShowLabelSuccess(*status_);
    status_->setText(show.c_str());
}

void rai::QtCreateBlock::ShowDefault(const std::string& show)
{
    ShowLabelDefault(*status_);
    status_->setText(show.c_str());
}

rai::QtAdvanced::QtAdvanced(rai::QtMain& qt_main)
    : window_(new QWidget),
      layout_(new QVBoxLayout),
      sign_verify_button_(new QPushButton("Sign/verify message")),
      create_block_button_(new QPushButton("Create Block")),
      back_(new QPushButton("Back")),
      main_(qt_main),
      sign_verify_(qt_main),
      create_block_(qt_main)
{
    layout_->addWidget(sign_verify_button_);
    layout_->addWidget(create_block_button_);
    layout_->addStretch();
    layout_->addWidget(back_);

    window_->setLayout(layout_);
}

void rai::QtAdvanced::Start(const std::weak_ptr<rai::QtMain>& qt_main_w)
{
    sign_verify_.Start(qt_main_w);
    create_block_.Start(qt_main_w);

    QObject::connect(
        sign_verify_button_, &QPushButton::released, [qt_main_w]() {
            if (auto qt_main = qt_main_w.lock())
            {
                qt_main->Push(qt_main->advanced_.sign_verify_.window_);
            }
        });

    QObject::connect(
        create_block_button_, &QPushButton::released, [this, qt_main_w]() {
            if (auto qt_main = qt_main_w.lock())
            {
                qt_main->Push(create_block_.window_);
            }
        });

    QObject::connect(back_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Pop();
        } 
    });
}

rai::QtMain::QtMain(QApplication& application, rai::QtEventProcessor& processor,
                    const std::shared_ptr<rai::Wallets>& wallets,
                    const boost::filesystem::path& path,
                    const rai::QtWalletConfig& config)
    : wallets_(wallets),
      config_path_(path),
      config_(config),
      application_(application),
      window_(new QWidget),
      layout_(new QVBoxLayout),
      status_(new QLabel),
      separator_(new QFrame),
      history_label_(new QLabel("Account history:")),
      main_stack_(new QStackedWidget),
      entry_window_(new QWidget),
      entry_window_layout_(new QVBoxLayout),
      send_button_(new QPushButton("Send")),
      receive_button_(new QPushButton("Receive")),
      settings_button_(new QPushButton("Settings")),
      accounts_button_(new QPushButton("Accounts")),
      wallets_button_(new QPushButton("Wallets")),
      advanced_button_(new QPushButton("Advanced")),
      hint_(new QLabel),
      active_status_(*this),
      self_pane_(*this),
      history_(*this),
      send_(*this),
      receive_(*this),
      settings_(*this),
      accounts_(*this),
      qt_wallets_(*this),
      advanced_(*this),
      processor_(processor),
      rendering_ratio_(rai::RAI),
      auto_receive_(config.auto_receive_),
      stopped_(false),
      waiting_(false),
      empty_(false),
      error_code_(rai::ErrorCode::SUCCESS),
      thread_([this]() { Run(); })
{
    entry_window_layout_->addWidget(history_label_);
    entry_window_layout_->addWidget(history_.window_);
    entry_window_layout_->addWidget(send_button_);
    entry_window_layout_->addWidget(receive_button_);
    entry_window_layout_->addWidget(settings_button_);
    entry_window_layout_->addWidget(accounts_button_);
    entry_window_layout_->addWidget(wallets_button_);
    entry_window_layout_->addWidget(advanced_button_);
    entry_window_layout_->setSpacing(5);
    entry_window_->setLayout(entry_window_layout_);
    main_stack_->addWidget(entry_window_);

    status_->setContentsMargins(5, 5, 5, 5);
    status_->setAlignment(Qt::AlignHCenter);

    separator_->setFrameShape(QFrame::HLine);
    separator_->setFrameShadow(QFrame::Sunken);

    hint_->setText("Remember - Back Up Your Wallet Seed And Keep It Secret");
    hint_->setStyleSheet("QLabel { color:red; }");
    hint_->setAlignment(Qt::AlignCenter);

    layout_->addWidget(status_);
    layout_->addWidget(self_pane_.window_);
    layout_->addWidget(separator_);
    layout_->addWidget(main_stack_);
    layout_->addWidget(hint_);

    layout_->setSpacing(0);
    layout_->setContentsMargins(0, 0, 0, 0);
    window_->setLayout(layout_);
    window_->resize(640, 640);
    window_->setStyleSheet("QLineEdit { padding:3px;}");
}

uint64_t rai::QtMain::CurrentTimestamp() const
{
    uint64_t now = 0;
    bool error = wallets_->CurrentTimestamp(now); // server time
    if (!error)
    {
        return now;
    }

    return rai::CurrentTimestamp(); // local time
}

std::string rai::QtMain::FormatBalance(const rai::Amount& balance) const
{
    std::string balance_str = balance.StringBalance(rendering_ratio_);
    std::string unit        = "RAI";
    if (rendering_ratio_ == rai::mRAI)
    {
        unit = std::string("mRAI");
    }
    else if (rendering_ratio_ == rai::uRAI)
    {
        unit = std::string("uRAI");
    }
    return balance_str + " " + unit;
}

void rai::QtMain::Start(const std::weak_ptr<rai::QtMain>& qt_main_w)
{
    QObject::connect(send_button_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Push(qt_main->send_.window_);
        }
    });

    QObject::connect(receive_button_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Push(qt_main->receive_.window_);
        }
    });

    QObject::connect(settings_button_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Push(qt_main->settings_.window_);
        }
    });

    QObject::connect(accounts_button_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Push(qt_main->accounts_.window_);
        }
    });

    QObject::connect(wallets_button_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Push(qt_main->qt_wallets_.window_);
        }
    });

    QObject::connect(advanced_button_, &QPushButton::released, [qt_main_w]() {
        if (auto qt_main = qt_main_w.lock())
        {
            qt_main->Push(qt_main->advanced_.window_);
        }
    });
}

void rai::QtMain::Pop()
{
    main_stack_->removeWidget(main_stack_->currentWidget());
}

void rai::QtMain::PostEvent(const std::function<void(rai::QtMain&)>& process)
{
    std::weak_ptr<rai::QtMain> qt_main_w(Shared());
    application_.postEvent(&processor_,
                           new rai::QtEvent([qt_main_w, process]() {
                               auto qt_main = qt_main_w.lock();
                               if (qt_main == nullptr) return;

                               process(*qt_main);
                           }));
}

void rai::QtMain::PostEvent(uint32_t delay,
                            const std::function<void(rai::QtMain&)>& process)
{
    std::weak_ptr<rai::QtMain> qt_main_w(Shared());
    wallets_->alarm_.Add(
        std::chrono::steady_clock::now() + std::chrono::seconds(delay),
        [qt_main_w, process]() {
            auto qt_main = qt_main_w.lock();
            if (qt_main == nullptr) return;

            qt_main->application_.postEvent(
                &qt_main->processor_, new rai::QtEvent([qt_main_w, process]() {
                    auto qt_main = qt_main_w.lock();
                    if (qt_main == nullptr) return;

                    process(*qt_main);
                }));
        });
}

void rai::QtMain::Push(QWidget* window)
{
    main_stack_->addWidget(window);
    main_stack_->setCurrentIndex(main_stack_->count() - 1);
}

std::shared_ptr<rai::QtMain> rai::QtMain::Shared()
{
    return shared_from_this();
}

void rai::QtMain::Start()
{
    std::weak_ptr<rai::QtMain> this_w(Shared());
    Start(this_w);
    active_status_.Start(this_w);
    self_pane_.Start(this_w);
    history_.Start(this_w);
    send_.Start(this_w);
    receive_.Start(this_w);
    settings_.Start(this_w);
    accounts_.Start(this_w);
    qt_wallets_.Start(this_w);
    advanced_.Start(this_w);
}

void rai::QtMain::SaveConfig()
{
    config_.auto_receive_ = auto_receive_;
    std::fstream config_file;
    rai::WriteObject(config_, config_path_, config_file);
    config_file.close();
}

void rai::QtMain::Run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_)
    {
        if (!auto_receive_)
        {
            std::cout << "Auto receive: false\n";
            condition_.wait(lock);
            continue;
        }

        if (waiting_)
        {
            std::cout << "Auto receive: waiting\n";
            condition_.wait_for(lock, std::chrono::seconds(5));
            continue;
        }

        if (error_code_ != rai::ErrorCode::SUCCESS)
        {
            std::cout << "Auto receive: error:" << static_cast<uint32_t>(error_code_) << std::endl;
            error_code_ = rai::ErrorCode::SUCCESS;
            condition_.wait_for(lock, std::chrono::seconds(5));
            continue;
        }

        ProcessAutoReceive_(lock);

        if (empty_)
        {
            std::cout << "Auto receive: empty\n";
            condition_.wait_for(lock, std::chrono::seconds(300));
        }
    }
}

void rai::QtMain::Stop()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
    }

    condition_.notify_all();
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void rai::QtMain::AutoReceiveNotify()
{
    condition_.notify_all();
}

void rai::QtMain::AutoReceiveCallback(rai::ErrorCode error_code,
                                      const std::shared_ptr<rai::Block>& block)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        error_code_ = error_code;
        waiting_ = false;
    }

    condition_.notify_all();
}

void rai::QtMain::ProcessAutoReceive_(std::unique_lock<std::mutex>& lock)
{
    empty_ = true;
    lock.unlock();

    bool receive = false;
    rai::BlockHash link;
    rai::Account account;
    std::shared_ptr<rai::Wallet> wallet(nullptr);
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    do
    {
        if (!wallets_->Connected())
        {
            break;
        }

        rai::Ledger& ledger = wallets_->ledger_;
        rai::Transaction transaction(error_code, ledger, false);
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            break;
        }

        auto wallets = wallets_->SharedWallets();
        for (auto& w : wallets)
        {
            wallet = w;
            auto accounts = wallet->Accounts();
            for (auto& i : accounts)
            {
                account = i.second.first;
                error_code = wallets_->AccountActionPreCheck(wallet, account);
                if (error_code != rai::ErrorCode::SUCCESS)
                {
                    continue;
                }

                rai::ReceivableInfos receivables;
                bool error = ledger.ReceivableInfosGet(
                    transaction, account, rai::ReceivableInfosType::ALL,
                    receivables, 1);
                if (error)
                {
                    std::cout << "QtMain::ProcessAutoReceive_: failed to get "
                                 "receivables"
                              << std::endl;
                    continue;
                }

                auto it = receivables.begin();
                if (it == receivables.end())
                {
                    continue;
                }

                uint64_t now = CurrentTimestamp();
                rai::Amount receive_mininum(rai::CreditPrice(now).Number()
                                            / 10);
                if (it->first.amount_ < receive_mininum)
                {
                    continue;
                }

                rai::AccountInfo info;
                error = ledger.AccountInfoGet(transaction, account, info);
                if (error || !info.Valid())
                {
                    if (it->first.amount_ < rai::CreditPrice(now))
                    {
                        continue;
                    }
                }
                else
                {
                    std::shared_ptr<rai::Block> block(nullptr);
                    error = ledger.BlockGet(transaction, info.head_, block);
                    if (error)
                    {
                        std::cout << "QtMain::ProcessAutoReceive_: failed to "
                                     "get block, hash="
                                  << info.head_.StringHex() << std::endl;
                        continue;
                    }

                    if (info.Restricted(block->Credit()))
                    {
                        continue;
                    }

                    if (block->Limited())
                    {
                        continue;
                    }
                }

                receive = true;
                link = it->second;
                break;
            }

            if (receive)
            {
                break;
            }
        }
    } while(0);
    lock.lock();

    if (!receive)
    {
        return;
    }
    empty_ = false;
    waiting_ = true;

    lock.unlock();

    error_code = wallets_->AccountReceive(
        wallet, account, link,
        [this](rai::ErrorCode error_code,
               const std::shared_ptr<rai::Block>& block) {
            AutoReceiveCallback(error_code, block);
        });
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        AutoReceiveCallback(error_code, nullptr);
    }
    std::cout << "Auto receive: account=" << account.StringAccount()
              << ", source=" << link.StringHex() << std::endl;

    lock.lock();
}
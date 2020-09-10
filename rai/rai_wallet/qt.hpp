#pragma once

#include <set>
#include <atomic>
#include <memory>
#include <Qt>
#include <QtGui>
#include <QtWidgets>
#include <boost/filesystem.hpp>
#include <rai/common/errors.hpp>
#include <rai/common/numbers.hpp>
#include <rai/common/alarm.hpp>
#include <rai/wallet/wallet.hpp>
#include <rai/rai_wallet/config.hpp>

namespace rai
{

class QtMain;

class QtEventProcessor : public QObject
{
public:
    bool event(QEvent*) override;
};

class QtEvent : public QEvent
{
public:
    QtEvent(const std::function<void()>&);
    std::function<void()> action_;
};

enum class QtStatusType
{
    INVALID      = 0,
    DISCONNECTED = 1,
    SYNC         = 2,
    ACTIVE       = 3,
    LOCKED       = 4,
    VULNERABLE   = 5,
};

class QtStatus
{
public:
    QtStatus(rai::QtMain&);
    void Refresh();
    void Start(const std::weak_ptr<rai::QtMain>&);
    std::string Text() const;
    std::string Color() const;

    rai::QtMain& main_;
    std::set<rai::QtStatusType> status_set_;
};

class QtSelfPane
{
public:
    QtSelfPane(rai::QtMain&);
    void Refresh();
    void Start(const std::weak_ptr<rai::QtMain>&);

    QWidget* window_;
    QVBoxLayout* layout_;
    QWidget* self_window_;
    QHBoxLayout* self_layout_;
    QLabel* account_label_;
    QLabel* version_;
    QWidget* account_window_;
    QHBoxLayout* account_layout_;
    QLineEdit* wallet_text_;
    QLabel* slash_;
    QLineEdit* account_text_;
    QPushButton* copy_button_;
    QWidget* balance_window_;
    QHBoxLayout* balance_layout_;
    QLabel* balance_label_;

    rai::QtMain& main_;
};

class QtHistoryRecord
{
public:
    QtHistoryRecord();

    uint64_t height_;
    std::string type_;
    std::string link_;
    rai::Amount amount_;
    rai::BlockHash hash_;
    std::string status_;
    rai::BlockHash previous_;
};

class QtHistory;
class QtHistoryModel : public QAbstractTableModel
{
public:
    QtHistoryModel(rai::QtHistory&, QObject* = nullptr);
    int rowCount(const QModelIndex &) const override;
    int columnCount(const QModelIndex &) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int, Qt::Orientation, int) const override;

    QVariant Field(int) const;
    bool Record(int) const;

    uint64_t HeadHeight() const;
    rai::Account Account() const;

private:
    friend class QtHistory;

    rai::QtHistory& history_;
    rai::Account account_;
    rai::BlockHash head_;
    uint64_t head_height_;
    uint64_t confirmed_height_;
    mutable rai::QtHistoryRecord current_record_;
};

class QtHistory
{
public: 
    QtHistory(rai::QtMain&);

    void OnMenuRequested(const QPoint& pos);
    void Refresh();
    void Start(const std::weak_ptr<rai::QtMain>&);

    QWidget* window_;
    QVBoxLayout* layout_;
    QtHistoryModel* model_;
    QTableView* view_;

    rai::QtMain& main_;
};

class QtSend
{
public:
    QtSend(rai::QtMain&);
    void Refresh();
    void Start(const std::weak_ptr<rai::QtMain>&);

    QWidget* window_;
    QVBoxLayout* layout_;
    QLabel* destination_label_;
    QLineEdit* destination_;
    QLabel* amount_label_;
    QLineEdit* amount_;
    QLabel* note_label_;
    QLineEdit* note_;
    QPushButton* send_;
    QPushButton* back_;

    rai::QtMain& main_;
};

class QtReceive
{
public:
    QtReceive(rai::QtMain&);
    void Receive(const rai::Account&, const rai::BlockHash&, QPushButton*,
                 const std::function<void(
                     rai::ErrorCode, const std::shared_ptr<rai::Block>&)>&);
    void ReceiveNext(rai::ErrorCode, const std::shared_ptr<rai::Block>&,
                     const rai::Account&,
                     const std::shared_ptr<std::vector<rai::BlockHash>>&);
    void Refresh();
    void Start(const std::weak_ptr<rai::QtMain>&);

    QWidget* window_;
    QVBoxLayout* layout_;
    QLabel* label_;
    QStandardItemModel* model_;
    QTableView* view_;
    QPushButton* receive_;
    QPushButton* receive_all_;
    QFrame* separator_;
    QButtonGroup* group_;
    QHBoxLayout* group_layout_;
    QLabel* auto_receive_;
    QRadioButton* yes_;
    QRadioButton* no_;
    QFrame* separator_2_;
    QPushButton* back_;

    rai::QtMain& main_;
};

class QtSettings
{
public:
    QtSettings(rai::QtMain&);

    void DelayResume(void*);
    void Refresh();
    void RefreshLock();
    void RefreshRepresentative();
    void RefreshTransactionsLimit();
    void Start(const std::weak_ptr<rai::QtMain>&);

    QWidget* window_;
    QVBoxLayout* layout_;
    QLineEdit* password_;
    QPushButton* lock_toggle_;
    QFrame* separator_1_;
    QLineEdit* new_password_;
    QLineEdit* retype_password_;
    QPushButton* change_password_;
    QFrame* separator_2_;
    QLabel* representative_;
    QLabel* current_representative_;
    QLineEdit* new_representative_;
    QPushButton* change_representative_;
    QFrame* separator_3_;
    QLabel* current_limit_;
    QWidget* increase_window_;
    QHBoxLayout* increase_layout_;
    QLabel* increase_label_;
    QLineEdit* increase_;
    QLabel* cost_label_;
    QLineEdit* cost_;
    QPushButton* increase_button_;

    QPushButton* back_;

    rai::QtMain& main_;
};

class QtAccounts
{
public:
    QtAccounts(rai::QtMain&);

    void OnMenuRequested(const QPoint& pos);
    void Refresh();
    void Start(const std::weak_ptr<rai::QtMain>&);

    QWidget* window_;
    QVBoxLayout* layout_;
    QLabel* accounts_label_;
    QStandardItemModel* model_;
    QTableView* view_;
    QPushButton* use_account_;
    QPushButton* create_account_;
    QFrame* separator_;
    QWidget* key_file_window_;
    QHBoxLayout* key_file_layout_;
    QLabel* key_file_label_;
    QLineEdit* key_file_;
    QPushButton* key_file_button_;
    QWidget* password_window_;
    QHBoxLayout* password_layout_;
    QLabel* password_label_;
    QLineEdit* password_;
    QPushButton* import_account_;
    QFrame* separator_2_;
    QPushButton* back_;

    rai::QtMain& main_;
};


class QtWallets
{
public:
    QtWallets(rai::QtMain&);

    void Refresh();
    void Start(const std::weak_ptr<rai::QtMain>&);

    QWidget* window_;
    QVBoxLayout* layout_;
    QLabel* wallets_label_;
    QStandardItemModel* model_;
    QTableView* view_;
    QPushButton* use_wallet_;
    QPushButton* create_wallet_;
    QPushButton* copy_wallet_seed_;
    QFrame* separator_;
    QLineEdit* seed_;
    QPushButton* import_;
    QFrame* separator_2_;
    QPushButton* back_;

    rai::QtMain& main_;
};

class QtSignVerify
{
public:
    QtSignVerify(rai::QtMain&);

    void Start(const std::weak_ptr<rai::QtMain>&);

    QWidget* window_;
    QVBoxLayout* layout_;
    QLabel* message_label_;
    QPlainTextEdit* message_;
    QLabel* account_label_;
    QLineEdit* account_;
    QLabel* signature_label_;
    QPlainTextEdit* signature_;
    QPushButton* sign_;
    QPushButton* verify_;
    QPushButton* back_;

    rai::QtMain& main_;
};

class QtCreateBlock
{
public:
    QtCreateBlock(rai::QtMain&);

    void Start(const std::weak_ptr<rai::QtMain>&);
    void HideAll();
    void ShowSend();
    void ShowReceive();
    void ShowChange();
    void ShowCredit();
    void Create(bool);
    std::unique_ptr<rai::Block> CreateSend(const rai::Account&,
                                           const rai::AccountInfo&,
                                           std::shared_ptr<rai::Block>&);
    std::unique_ptr<rai::Block> CreateReceive(const rai::Account&,
                                              const rai::AccountInfo&,
                                              std::shared_ptr<rai::Block>&);
    std::unique_ptr<rai::Block> CreateChange(const rai::Account&,
                                             const rai::AccountInfo&,
                                             std::shared_ptr<rai::Block>&);
    std::unique_ptr<rai::Block> CreateCredit(const rai::Account&,
                                             const rai::AccountInfo&,
                                             std::shared_ptr<rai::Block>&);

    void ShowError(const std::string&);
    void ShowError(rai::ErrorCode);
    void ShowSuccess(const std::string&);
    void ShowDefault(const std::string&);

    QWidget* window_;
    QVBoxLayout* layout_;
    QButtonGroup* group_;
    QHBoxLayout* group_layout_;
    QRadioButton* send_;
    QRadioButton* receive_;
    QRadioButton* change_;
    QRadioButton* credit_;
    QLabel* source_label_;
    QLineEdit* source_;
    QLabel* destination_label_;
    QLineEdit* destination_;
    QLabel* amount_label_;
    QLineEdit* amount_;
    QLabel* representative_label_;
    QLineEdit* representative_;
    QLabel* credit_label_;
    QLineEdit* credit_amount_;
    QPlainTextEdit* block_;
    QLabel* status_;
    QPushButton* create_json_;
    QPushButton* create_raw_;
    QPushButton* back_;

    rai::QtMain& main_;
};

class QtAdvanced
{
public:
    QtAdvanced(rai::QtMain&);

    void Start(const std::weak_ptr<rai::QtMain>&);

    QWidget* window_;
    QVBoxLayout* layout_;
    QPushButton* sign_verify_button_;
    QPushButton* create_block_button_;
    QPushButton* back_;

    rai::QtMain& main_;
    rai::QtSignVerify sign_verify_;
    rai::QtCreateBlock create_block_;
};

class QtMain : public std::enable_shared_from_this<rai::QtMain>
{
public:
    QtMain(QApplication& application, rai::QtEventProcessor&,
           const std::shared_ptr<rai::Wallets>&, const boost::filesystem::path&,
           const rai::QtWalletConfig&);
    QtMain(const rai::QtMain&) = delete;

    std::string FormatBalance(const rai::Amount&) const;
    void Start(const std::weak_ptr<rai::QtMain>&);
    void Pop();
    void PostEvent(const std::function<void(rai::QtMain&)>&);
    void PostEvent(uint32_t, const std::function<void(rai::QtMain&)>&);
    void Push(QWidget*);
    std::shared_ptr<rai::QtMain> Shared();
    void Start();
    void SaveConfig();
    void Run();
    void Stop();
    void AutoReceiveNotify();
    void AutoReceiveCallback(rai::ErrorCode,
                             const std::shared_ptr<rai::Block>&);

    std::shared_ptr<rai::Wallets> wallets_;
    boost::filesystem::path config_path_;
    rai::QtWalletConfig config_;
    QApplication& application_;
    QWidget* window_;
    QVBoxLayout* layout_;
    QLabel* status_;
	QFrame * separator_;
    QLabel* history_label_;
    QStackedWidget* main_stack_;
    QWidget* entry_window_;
    QVBoxLayout* entry_window_layout_;
    QPushButton* send_button_;
    QPushButton* receive_button_;
    QPushButton* settings_button_;
    QPushButton* accounts_button_;
    QPushButton* wallets_button_;
    QPushButton* advanced_button_;
    QLabel* hint_;

    rai::QtStatus active_status_;
    rai::QtSelfPane self_pane_;
    rai::QtHistory history_;
    rai::QtSend send_;
    rai::QtReceive receive_;
    rai::QtSettings settings_;
    rai::QtAccounts accounts_;
    rai::QtWallets qt_wallets_;
    rai::QtAdvanced advanced_;

    rai::QtEventProcessor& processor_;
    rai::uint128_t rendering_ratio_;
    std::atomic<bool> auto_receive_;

private:
    void ProcessAutoReceive_(std::unique_lock<std::mutex>&);

    std::condition_variable condition_;
    mutable std::mutex mutex_;
    bool stopped_;
    bool waiting_;
    bool empty_;
    rai::ErrorCode error_code_;
    std::thread thread_;
};
}
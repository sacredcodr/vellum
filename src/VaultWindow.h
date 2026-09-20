#pragma once
#include "VaultStore.h"
#include "SecretClipboard.h"
#include <QElapsedTimer>
#include <QMainWindow>
#include <QTimer>
class QStackedWidget;
class QLineEdit;
class QPlainTextEdit;
class QListWidget;
class QLabel;
class QPushButton;
class QCheckBox;
class VaultWindow : public QMainWindow
{
    Q_OBJECT
    friend class VaultUiTests;
public:
    explicit VaultWindow(bool fixture = false);
    ~VaultWindow();
    void loadFixture(const QString& directory);
    void loadCreationFixture();
    void lockVault(bool automatic = false);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool nativeEvent(const QByteArray& type, void* message, qintptr* result) override;
    void closeEvent(QCloseEvent* event) override;
private:
    void buildUi();
    void updateWindowPresentation();
    void chooseVault(bool create);
    void unlock();
    void generateMasterPassphrase();
    void showSetupStep(bool recovery);
    void exportRecoveryCopy();
    void refreshList();
    void selectEntry(const QString& id);
    void newEntry(const QString& type);
    bool saveEntry();
    bool resolveDraft();
    void clearEditor();
    void deleteEntry();
    void backup();
    void showError(const QString& message);
    VaultStore m_store;
    SecretClipboard m_clipboard;
    QElapsedTimer m_activity;
    QTimer m_idleTimer;
    QStackedWidget* m_pages = nullptr;
    QLineEdit* m_path = nullptr;
    QLineEdit* m_master = nullptr;
    QLineEdit* m_confirm = nullptr;
    QWidget* m_creationActions = nullptr;
    QWidget* m_fileActions = nullptr;
    QLabel* m_intro = nullptr;
    QLabel* m_masterLabel = nullptr;
    QLabel* m_confirmLabel = nullptr;
    QLabel* m_phraseCheck = nullptr;
    QPushButton* m_changeLocation = nullptr;
    QLabel* m_footer = nullptr;
    QPushButton* m_setupBack = nullptr;
    QPushButton* m_generateMaster = nullptr;
    QPushButton* m_exportMaster = nullptr;
    bool m_recoveryStep = false;
    QCheckBox* m_masterReveal = nullptr;
    QCheckBox* m_recoveryAcknowledged = nullptr;
    QLabel* m_unlockTitle = nullptr;
    QLabel* m_unlockError = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_recordType = nullptr;
    QLabel* m_empty = nullptr;
    QLineEdit* m_search = nullptr;
    QListWidget* m_list = nullptr;
    QLineEdit* m_title = nullptr;
    QLineEdit* m_username = nullptr;
    QLineEdit* m_url = nullptr;
    QLineEdit* m_password = nullptr;
    QPlainTextEdit* m_notes = nullptr;
    QCheckBox* m_reveal = nullptr;
    QPushButton* m_save = nullptr;
    QWidget* m_loginFields = nullptr;
    QWidget* m_editor = nullptr;
    QString m_selected;
    QString m_filter;
    QSize m_workspaceSize{1240, 820};
    bool m_create = false;
    bool m_loading = false;
    bool m_dirty = false;
    bool m_fixture = false;
};

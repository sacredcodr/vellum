#include "VaultWindow.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QJsonObject>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QAbstractButton>
#include <QFileDialog>
#include <QMessageBox>
#include <wtsapi32.h>
class VaultUiTests : public QObject
{
    Q_OBJECT
private slots:
    void automaticLockSavesAndClears();
    void windowsLockNotificationClears();
    void listNavigationAndEditing();
    void creationResetsFilter();
    void dialogsAreFramelessAndCancelSafely();
    void setupMismatchAndFailedSaveAreRetryable();
    void recoveryExportThroughDialogsAndReopen();
    void generatedMasterRequiresAcknowledgement();
};
void VaultUiTests::automaticLockSavesAndClears()
{
    QTemporaryDir directory;
    VaultWindow window(true);
    QCOMPARE(window.width(), 470);
    QVERIFY(window.windowFlags() & Qt::FramelessWindowHint);
    window.loadFixture(directory.path());
    QVERIFY(window.windowFlags() & Qt::FramelessWindowHint);
    QVERIFY(window.width() >= 1000);
    window.m_notes->setPlainText("Unsaved fictional note before auto lock.");
    window.m_search->setText("Personal");
    QVERIFY(window.m_dirty);
    window.lockVault(true);
    QVERIFY(!window.m_store.unlocked()); QVERIFY(window.m_store.entries().isEmpty());
    QCOMPARE(window.m_pages->currentIndex(), 0); QCOMPARE(window.m_list->count(), 0);
    QCOMPARE(window.width(), 470);
    QVERIFY(window.windowFlags() & Qt::FramelessWindowHint);
    for (auto* field : {window.m_title, window.m_username, window.m_url, window.m_password, window.m_master, window.m_confirm}) QVERIFY(field->text().isEmpty());
    QVERIFY(window.m_notes->toPlainText().isEmpty());
    VaultStore reopened; QByteArray passphrase("fixture-only-master-passphrase"); reopened.open(directory.filePath("fixture.vault"), passphrase);
    QCOMPARE(reopened.entries().first().toObject()["notes"].toString(), QString("Unsaved fictional note before auto lock."));
}
void VaultUiTests::windowsLockNotificationClears()
{
    QTemporaryDir directory; VaultWindow window(true); window.loadFixture(directory.path());
    MSG message{}; message.message = WM_WTSSESSION_CHANGE; message.wParam = WTS_SESSION_LOCK;
    qintptr result = 0; window.nativeEvent("windows_generic_MSG", &message, &result);
    QVERIFY(!window.m_store.unlocked()); QVERIFY(window.m_password->text().isEmpty()); QCOMPARE(window.m_list->count(), 0);
}
void VaultUiTests::listNavigationAndEditing()
{
    QTemporaryDir directory; VaultWindow window(true); window.loadFixture(directory.path());
    window.m_list->setCurrentRow(1);
    QCOMPARE(window.m_title->text(), QString("Recovery checklist"));
    QVERIFY(window.m_loginFields->isHidden());
    window.m_notes->setPlainText("Edited fixture note.");
    QVERIFY(window.saveEntry());
    window.m_list->setCurrentRow(0);
    QCOMPARE(window.m_title->text(), QString("Personal email"));
    QCOMPARE(window.m_password->echoMode(), QLineEdit::Password);
    window.m_list->setCurrentRow(1);
    QCOMPARE(window.m_notes->toPlainText(), QString("Edited fixture note."));
    window.lockVault(true);
}
void VaultUiTests::creationResetsFilter()
{
    QTemporaryDir directory;
    VaultWindow window(true);
    window.loadFixture(directory.path());
    for (auto* filter : window.findChildren<QPushButton*>("navButton"))
    {
        if (filter->property("filter").toString() == "note")
        {
            filter->click();
        }
    }
    QCOMPARE(window.m_list->count(), 2);
    window.newEntry("login");
    QCOMPARE(window.m_list->count(), 5);
    QCOMPARE(window.m_title->text(), QString("Untitled password"));
    QVERIFY(window.m_filter.isEmpty());
    for (auto* filter : window.findChildren<QPushButton*>("navButton"))
    {
        QCOMPARE(filter->isChecked(), filter->property("filter").toString().isEmpty());
    }
    window.lockVault(true);
}
void VaultUiTests::generatedMasterRequiresAcknowledgement()
{
    QTemporaryDir directory;
    VaultWindow window(true);
    window.loadCreationFixture();
    window.m_path->setText(directory.filePath("generated.vault"));
    QCOMPARE(window.m_master->text().split(' ').size(), 7);
    QCOMPARE(window.m_master->text(), window.m_confirm->text());
    QCOMPARE(window.m_master->echoMode(), QLineEdit::Password);
    window.unlock();
    QVERIFY(!window.m_store.unlocked());
    QVERIFY(!QFile::exists(window.m_path->text()));
    window.m_recoveryAcknowledged->setChecked(true);
    window.generateMasterPassphrase();
    QVERIFY(!window.m_recoveryAcknowledged->isChecked());
    window.m_recoveryAcknowledged->setChecked(true);
    window.unlock();
    QVERIFY(window.m_store.unlocked());
    QVERIFY(window.m_master->text().isEmpty());
    QVERIFY(window.m_confirm->text().isEmpty());
    window.lockVault(true);
    QVERIFY(window.m_creationActions->isHidden());
    QVERIFY(!window.m_masterReveal->isChecked());
    window.loadCreationFixture();
    window.m_masterReveal->setChecked(true);
    window.lockVault(true);
    QVERIFY(window.m_master->text().isEmpty());
    QVERIFY(window.m_confirm->text().isEmpty());
    QVERIFY(!window.m_masterReveal->isChecked());
}
void VaultUiTests::dialogsAreFramelessAndCancelSafely()
{
    QTemporaryDir directory;
    VaultWindow window(true);
    bool fileFrameless = false;
    QTimer::singleShot(0, &window, [&]
    {
        auto* picker = window.findChild<QFileDialog*>();
        if (picker)
        {
            fileFrameless = (picker->windowFlags() & Qt::FramelessWindowHint) && picker->testOption(QFileDialog::DontUseNativeDialog);
            picker->reject();
        }
    });
    window.chooseVault(false);
    QVERIFY(fileFrameless);
    QVERIFY(window.m_path->text().isEmpty());
    window.loadFixture(directory.path());
    window.m_notes->setPlainText("Preserve draft on cancel");
    bool confirmationFrameless = false;
    QTimer::singleShot(0, &window, [&]
    {
        auto* confirmation = window.findChild<QMessageBox*>();
        if (confirmation)
        {
            confirmationFrameless = confirmation->windowFlags() & Qt::FramelessWindowHint;
            confirmation->done(QMessageBox::Cancel);
        }
    });
    QVERIFY(!window.resolveDraft());
    QVERIFY(confirmationFrameless);
    QVERIFY(window.m_dirty);
    QCOMPARE(window.m_notes->toPlainText(), QString("Preserve draft on cancel"));
    window.lockVault(true);
}
void VaultUiTests::setupMismatchAndFailedSaveAreRetryable()
{
    QTemporaryDir directory;
    VaultWindow window(true);
    window.loadCreationFixture();
    const auto phrase = window.m_master->text();
    window.m_confirm->setText("incorrect confirmation");
    window.unlock();
    QCOMPARE(window.m_master->text(), phrase);
    QCOMPARE(window.m_confirm->text(), QString("incorrect confirmation"));
    QVERIFY(!window.m_recoveryStep);
    window.m_confirm->setText(phrase);
    QCOMPARE(window.m_phraseCheck->text(), QString("Passphrases match."));
    const auto existing = directory.filePath("existing.vault");
    QFile file(existing);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("must not be overwritten");
    file.close();
    window.m_path->setText(existing);
    window.unlock();
    window.m_recoveryAcknowledged->setChecked(true);
    window.unlock();
    QVERIFY(!window.m_store.unlocked());
    QCOMPARE(window.m_master->text(), phrase);
    QCOMPARE(window.m_confirm->text(), phrase);
    QVERIFY(window.m_recoveryAcknowledged->isChecked());
    QVERIFY(window.findChild<QPushButton*>("unlockButton")->isEnabled());
    window.m_path->setText(directory.filePath("retry.vault"));
    window.unlock();
    QVERIFY(window.m_store.unlocked());
    window.lockVault(true);
}
void VaultUiTests::recoveryExportThroughDialogsAndReopen()
{
    QTemporaryDir directory;
    VaultWindow window(true);
    window.loadCreationFixture();
    const auto phrase = window.m_master->text();
    window.m_path->setText(directory.filePath("generated.vault"));
    window.unlock();
    int stage = 0;
    QTimer automation;
    connect(&automation, &QTimer::timeout, &window, [&]
    {
        if (stage == 0)
        {
            if (auto* warning = window.findChild<QMessageBox*>())
            {
                stage = 1;
                warning->button(QMessageBox::Save)->click();
            }
        }
        else if (stage == 1)
        {
            if (auto* picker = window.findChild<QFileDialog*>())
            {
                stage = 2;
                picker->selectFile(directory.filePath("recovery"));
                QMetaObject::invokeMethod(picker, "accept", Qt::QueuedConnection);
            }
        }
    });
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &window, [&]
    {
        for (auto* dialog : window.findChildren<QDialog*>()) dialog->reject();
    });
    automation.start(10);
    timeout.start(3000);
    window.m_exportMaster->click();
    automation.stop();
    timeout.stop();
    QCOMPARE(stage, 2);
    QFile recovery(directory.filePath("recovery.txt"));
    QVERIFY(recovery.open(QIODevice::ReadOnly));
    QVERIFY(recovery.readAll().endsWith(phrase.toUtf8() + "\n"));
    QVERIFY(window.m_recoveryAcknowledged->isChecked());
    auto* create = window.findChild<QPushButton*>("unlockButton");
    QVERIFY(create->isEnabled());
    create->click();
    QVERIFY(window.m_store.unlocked());
    window.lockVault(true);
    window.m_master->setText(phrase);
    create->click();
    QVERIFY(window.m_store.unlocked());
    QVERIFY(window.m_master->text().isEmpty());
    window.lockVault(true);
}
QTEST_MAIN(VaultUiTests)
#include "VaultUiTests.moc"

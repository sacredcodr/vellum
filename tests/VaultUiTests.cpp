#include "VaultWindow.h"
#include "NoteEditor.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QTextDocument>
#include <QTextList>
#include <QToolButton>
#include <QComboBox>
#include <QMenu>
#include <QAction>
#include <QMimeData>
#include <QClipboard>
#include <memory>
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
    void searchSavedContentAndFeedback();
    void searchKeyboardPreservesDraft();
    void noteFormattingAndEncryptedReopen();
    void noteEditorCompatibilityAndResources();
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
    VaultStore reopened; QByteArray passphrase("Fixture!7Quartz-Cobalt9Maple"); reopened.open(directory.filePath("fixture.vault"), passphrase);
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
void VaultUiTests::searchSavedContentAndFeedback()
{
    QTemporaryDir directory;
    VaultWindow window(true);
    window.loadFixture(directory.path());
    window.m_search->setText("  INVALID\tpersonal\n ");
    QCOMPARE(window.m_list->count(), 1);
    QCOMPARE(window.m_resultCount->text(), QString("1 of 4 items"));
    window.m_search->setText("https://example.invalid");
    QCOMPARE(window.m_list->count(), 2);
    window.m_search->setText("backup fictional");
    QCOMPARE(window.m_list->count(), 4);
    window.m_search->setText("EXAMPLE-NOT-A-REAL-PASSWORD");
    QCOMPARE(window.m_list->count(), 0);
    QVERIFY(!window.m_listEmpty->isHidden());
    QVERIFY(!window.m_clearSearch->isHidden());
    const auto selected = window.m_selected;
    window.m_notes->setPlainText("unsaved-search-sentinel");
    window.m_search->setText("unsaved-search-sentinel");
    QCOMPARE(window.m_list->count(), 0);
    QCOMPARE(window.m_selected, selected);
    QVERIFY(window.m_dirty);
    QVERIFY(window.saveEntry());
    QCOMPARE(window.m_list->count(), 1);
    window.m_clearSearch->click();
    QCOMPARE(window.m_list->count(), 4);
    QVERIFY(window.m_listEmpty->isHidden());
    window.m_search->setText(selected);
    QCOMPARE(window.m_list->count(), 0);
    window.m_search->setText("2026-09-20");
    QCOMPARE(window.m_list->count(), 0);
    window.m_search->setText(" \t\n");
    QCOMPARE(window.m_list->count(), 4);
    for (auto* filter : window.findChildren<QPushButton*>("navButton"))
    {
        if (filter->property("filter").toString() == "note") filter->click();
    }
    QCOMPARE(window.m_resultCount->text(), QString("2 of 2 items"));
    window.m_store.save(QJsonArray{});
    window.refreshList();
    QCOMPARE(window.m_resultCount->text(), QString("0 of 0 items"));
    QVERIFY(window.m_listEmpty->text().startsWith("No notes yet."));
    QVERIFY(window.m_clearSearch->isHidden());
    window.lockVault(true);
    QVERIFY(window.m_resultCount->text().isEmpty());
}
void VaultUiTests::searchKeyboardPreservesDraft()
{
    QTemporaryDir directory;
    VaultWindow window(true);
    window.loadFixture(directory.path());
    window.show();
    window.activateWindow();
    QApplication::processEvents();
    const auto original = window.m_selected;
    window.m_notes->setPlainText("Keep this draft");
    window.m_search->setText("Recovery");
    window.m_search->setFocus();
    QTimer::singleShot(0, &window, [&window]
    {
        if (auto* dialog = window.findChild<QMessageBox*>()) dialog->done(QMessageBox::Cancel);
    });
    QTest::keyClick(window.m_search, Qt::Key_Return);
    QCOMPARE(window.m_selected, original);
    QCOMPARE(window.m_notes->toPlainText(), QString("Keep this draft"));
    QVERIFY(window.m_dirty);
    QCOMPARE(window.focusWidget(), window.m_search);

    window.m_title->clear();
    QTimer::singleShot(0, &window, [&window]
    {
        if (auto* dialog = window.findChild<QMessageBox*>()) dialog->done(QMessageBox::Save);
    });
    QTest::keyClick(window.m_search, Qt::Key_Down);
    QCOMPARE(window.m_selected, original);
    QVERIFY(window.m_dirty);
    QCOMPARE(window.focusWidget(), window.m_search);

    QTimer::singleShot(0, &window, [&window]
    {
        if (auto* dialog = window.findChild<QMessageBox*>()) dialog->done(QMessageBox::Discard);
    });
    QTest::keyClick(window.m_search, Qt::Key_Down);
    QCOMPARE(window.m_title->text(), QString("Recovery checklist"));
    QCOMPARE(window.focusWidget(), window.m_list);
    QTest::keyClick(window.m_list, Qt::Key_Return);
    QCOMPARE(window.focusWidget(), window.m_title);
    window.m_search->setFocus();
    QTest::keyClick(window.m_search, Qt::Key_Escape);
    QVERIFY(window.m_search->text().isEmpty());
    QCOMPARE(window.m_list->count(), 4);
    QTest::keyClick(window.m_search, Qt::Key_Down);
    QCOMPARE(window.m_title->text(), QString("Personal email"));
    window.m_search->setText("no-such-item");
    window.m_search->setFocus();
    QTest::keyClick(window.m_search, Qt::Key_Return);
    QCOMPARE(window.focusWidget(), window.m_search);
    QCOMPARE(window.m_title->text(), QString("Personal email"));
    window.lockVault(true);
}
void VaultUiTests::noteFormattingAndEncryptedReopen()
{
    QTemporaryDir directory;
    VaultWindow window(true);
    window.loadFixture(directory.path());
    window.m_list->setCurrentRow(1);
    window.show();
    window.activateWindow();
    QApplication::processEvents();
    auto* editor = window.m_notes;
    editor->setPlainText("Project plan\nFirst task\nSecond task");
    QVERIFY(window.saveEntry());
    const auto id = window.m_selected;
    auto cursor = editor->textCursor();
    cursor.setPosition(0);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
    editor->setFocus();
    QTest::keyClick(editor, Qt::Key_B, Qt::ControlModifier);
    QVERIFY(editor->fontWeight() >= QFont::Bold);
    QVERIFY(window.m_dirty);
    QTest::keyClick(editor, Qt::Key_I, Qt::ControlModifier);
    QVERIFY(editor->fontItalic());
    QTest::keyClick(editor, Qt::Key_U, Qt::ControlModifier);
    QVERIFY(editor->fontUnderline());
    QVERIFY(window.saveEntry());
    const auto rich = window.m_store.entries()[1].toObject()["notesHtml"].toString();
    QVERIFY(!rich.isEmpty());
    window.m_list->setCurrentRow(0);
    QVERIFY(!editor->document()->isUndoAvailable());
    window.m_list->setCurrentRow(1);
    cursor = editor->textCursor();
    cursor.setPosition(2);
    editor->setTextCursor(cursor);
    QVERIFY(editor->fontWeight() >= QFont::Bold);
    QVERIFY(editor->fontItalic());
    QVERIFY(editor->fontUnderline());

    auto* paragraph = window.findChild<QComboBox*>("noteParagraph");
    paragraph->setCurrentIndex(1);
    paragraph->activated(1);
    QCOMPARE(editor->textCursor().blockFormat().headingLevel(), 1);
    QToolButton* bullets = nullptr;
    QToolButton* numbered = nullptr;
    QToolButton* colors = nullptr;
    QToolButton* clear = nullptr;
    for (auto* control : window.findChildren<QToolButton*>())
    {
        if (control->accessibleName() == "Bulleted list") bullets = control;
        if (control->accessibleName() == "Numbered list") numbered = control;
        if (control->accessibleName() == "Text color") colors = control;
        if (control->accessibleName() == "Clear inline formatting") clear = control;
    }
    QVERIFY(bullets && numbered && colors && clear);
    cursor.movePosition(QTextCursor::NextBlock);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
    bullets->click();
    QVERIFY(editor->textCursor().currentList());
    QCOMPARE(editor->textCursor().currentList()->count(), 2);
    QCOMPARE(editor->textCursor().currentList()->format().style(), QTextListFormat::ListDisc);
    numbered->click();
    QCOMPARE(editor->textCursor().currentList()->format().style(), QTextListFormat::ListDecimal);
    numbered->click();
    QVERIFY(!editor->textCursor().currentList());
    editor->undo();
    QVERIFY(editor->textCursor().currentList());
    editor->redo();
    QVERIFY(!editor->textCursor().currentList());
    cursor.setPosition(0);
    cursor.movePosition(QTextCursor::NextBlock);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
    bullets->click();
    colors->menu()->actions()[1]->trigger();
    QCOMPARE(editor->textColor(), QColor("#f2a6b3"));
    clear->click();
    QVERIFY(!editor->currentCharFormat().hasProperty(QTextFormat::ForegroundBrush));
    colors->menu()->actions()[3]->trigger();
    QCOMPARE(editor->textColor(), QColor("#92d5b5"));
    QVERIFY(window.saveEntry());
    const auto text = editor->toPlainText();
    window.lockVault(true);
    QVERIFY(editor->toPlainText().isEmpty());
    QVERIFY(!editor->document()->isUndoAvailable());
    editor->undo();
    QVERIFY(editor->toPlainText().isEmpty());
    window.m_master->setText("Fixture!7Quartz-Cobalt9Maple");
    window.unlock();
    QVERIFY(window.m_store.unlocked());
    window.selectEntry(id);
    QCOMPARE(editor->toPlainText(), text);
    cursor = editor->textCursor();
    cursor.setPosition(0);
    editor->setTextCursor(cursor);
    QCOMPARE(cursor.blockFormat().headingLevel(), 1);
    cursor.movePosition(QTextCursor::NextBlock);
    editor->setTextCursor(cursor);
    QVERIFY(cursor.currentList());
    QCOMPARE(editor->textColor(), QColor("#92d5b5"));
    window.m_search->setText("Second plan");
    QCOMPARE(window.m_list->count(), 1);
    window.lockVault(true);
}

void VaultUiTests::noteEditorCompatibilityAndResources()
{
    class TestNoteEditor : public NoteEditor
    {
    public:
        using NoteEditor::insertFromMimeData;
        using NoteEditor::createMimeDataFromSelection;
    } editor;
    editor.loadNote("<b>Literal legacy text</b>", {});
    QCOMPARE(editor.toPlainText(), QString("<b>Literal legacy text</b>"));
    editor.loadNote("Newer plain text", "<p>Stale rich text</p>");
    QCOMPARE(editor.toPlainText(), QString("Newer plain text"));
    QVERIFY(!editor.document()->isUndoAvailable());
    editor.selectAll();
    QMimeData source;
    source.setText("Pasted text");
    source.setHtml("<b>Pasted text</b><img src='file:///private.png'>");
    editor.insertFromMimeData(&source);
    QCOMPARE(editor.toPlainText(), QString("Pasted text"));
    QVERIFY(editor.fontWeight() < QFont::Bold);
    editor.selectAll();
    std::unique_ptr<QMimeData> exported(editor.createMimeDataFromSelection());
    QVERIFY(exported->formats().isEmpty());
    for (const auto& path : {QString("file:///C:/Windows/win.ini"), QString("https://example.invalid/image.png"), QString("data:image/png;base64,abcd")})
    {
        const auto resource = editor.document()->resource(QTextDocument::ImageResource, QUrl(path));
        QVERIFY(resource.isValid());
        QVERIFY(resource.toByteArray().isEmpty());
    }
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
    QString recoveryPath;
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
                picker->setDirectory(directory.path());
            }
        }
        else if (stage == 2)
        {
            if (auto* picker = window.findChild<QFileDialog*>())
            {
                stage = 3;
                picker->selectFile("recovery.txt");
                recoveryPath = picker->selectedFiles().value(0);
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
    QCOMPARE(stage, 3);
    QVERIFY(!recoveryPath.isEmpty());
    QFile recovery(recoveryPath);
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

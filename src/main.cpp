#include "VaultWindow.h"
#include "NoteEditor.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QTimer>
#include <QMessageBox>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QKeyEvent>
#include <QComboBox>
#include <QAbstractItemView>
#include <QMenu>
#include <QSplitter>

int main(int argc, char* argv[])
{
    bool fixture = false;
    for (int i = 1; i < argc; ++i) if (QByteArray(argv[i]) == "--self-test") fixture = true;
    if (fixture) qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    // Keep settings under the product identity used by current releases.
    app.setApplicationDisplayName("Vellum");
    app.setOrganizationName("Vellum"); app.setApplicationName("Vellum"); app.setStyle("Fusion");
    if (fixture)
    {
        for (const auto* font : {"segoeui.ttf", "segoeuib.ttf", "consola.ttf"}) QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/" + font);
    }
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#202020")); palette.setColor(QPalette::WindowText, QColor("#e4e4e4"));
    palette.setColor(QPalette::Base, QColor("#252525")); palette.setColor(QPalette::Text, QColor("#e4e4e4"));
    palette.setColor(QPalette::Button, QColor("#292929")); palette.setColor(QPalette::ButtonText, QColor("#e4e4e4"));
    palette.setColor(QPalette::Highlight, QColor("#514465")); palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor("#999999")); app.setPalette(palette);
    app.setStyleSheet(R"(
QWidget { font-family:'Segoe UI'; font-size:13px; color:#dedede; }
QMainWindow, QDialog, QMessageBox { background:#202020; }
QFrame#topbar { background:#191919; border-bottom:1px solid #303030; }
QWidget#nav { background:#191919; border-right:1px solid #303030; }
QWidget#collection { background:#191919; border-right:1px solid #303030; }
QFrame#unlockCard { background:#191919; border:1px solid #343e4b; border-radius:18px; }
QLabel#hero { font-size:24px; font-weight:600; color:#f1f4f8; }
QLabel#brand { font-size:19px; font-weight:600; }
QLabel#section { font-size:20px; font-weight:600; }
QLabel#eyebrow { color:#aaaaaa; font-size:11px; letter-spacing:1px; }
QLabel#muted { color:#a5a5a5; font-size:13px; }
QLabel#fine { color:#a0a0a0; font-size:11px; }
QLabel#error { color:#f3b8ac; font-size:12px; }
QPushButton { padding:9px 12px; border:1px solid #3a3a3a; border-radius:7px; background:#292929; }
QPushButton:hover { background:#353535; }
QPushButton:focus { border-color:#b5b5b5; }
QPushButton:disabled { background:#20262f; color:#748192; }
QPushButton#primary, QPushButton#unlockButton { background:#e3e1dc; color:#202020; border-color:#e3e1dc; font-weight:600; }
QPushButton#primary:disabled { background:#293a35; border-color:#364a43; color:#839c92; }
QPushButton#primary:hover, QPushButton#unlockButton:hover { background:#f4f2ec; }
QPushButton#danger { color:#e2a39b; background:transparent; border-color:transparent; }
QPushButton#navButton { background:transparent; border-color:transparent; padding:7px 9px; color:#a5a5a5; }
QPushButton#navButton:checked { background:#303030; color:#eeeeee; border-color:#444444; }
QLineEdit, QTextEdit { background:#252525; border:1px solid #383838; border-radius:7px; padding:10px; selection-background-color:#514465; }
QLineEdit:focus, QTextEdit:focus { border-color:#b5b5b5; }
QLineEdit#titleEdit { font-size:26px; font-weight:600; background:transparent; border:1px solid transparent; padding:8px 0; }
QLineEdit#titleEdit:focus { border-bottom-color:#b5b5b5; }
QTextEdit { font-size:15px; background:transparent; border:1px solid transparent; padding:14px 0; }
QTextEdit:focus { border-color:transparent; }
QFrame#noteToolbar { background:#282828; border:1px solid #3c3c3c; border-radius:8px; }
QFrame#formatSeparator { background:#424242; border:0; }
QToolButton { background:transparent; border:1px solid transparent; border-radius:5px; padding:0; }
QToolButton:hover { background:#3a3a3a; border-color:#494949; }
QToolButton:checked { background:#514465; border-color:#79678f; }
QToolButton:focus { border-color:#c5b3ef; }
QToolButton::menu-indicator { subcontrol-origin:padding; subcontrol-position:right center; right:1px; }
QComboBox#noteParagraph { background:transparent; border:1px solid transparent; border-radius:5px; padding:0 20px 0 8px; }
QComboBox#noteParagraph:hover { background:#3a3a3a; }
QComboBox#noteParagraph:focus { border-color:#c5b3ef; }
QComboBox#noteParagraph::drop-down { border:0; width:20px; }
QComboBox#noteParagraph::down-arrow { width:0; height:0; }
QComboBox#noteParagraph QAbstractItemView { background:#292929; border:1px solid #454545; padding:4px; selection-background-color:#514465; outline:0; }
QComboBox#noteParagraph QAbstractItemView::item { min-height:32px; padding-left:8px; }
QPushButton#itemActions, QPushButton#quiet, QPushButton#brandButton { background:transparent; border-color:transparent; color:#b8b8b8; padding:7px 9px; }
QPushButton#brandButton { font-size:16px; font-weight:600; color:#eeeeee; padding-left:4px; padding-right:18px; }
QPushButton#quiet:hover, QPushButton#brandButton:hover { background:#303030; }
QPushButton#quiet:focus, QPushButton#brandButton:focus { border-color:#b5b5b5; }
QPushButton::menu-indicator { subcontrol-position:right center; right:3px; }
QLabel#emptyState { font-size:16px; color:#a5a5a5; }
QFrame#appChrome { background:#191919; border-bottom:1px solid #303030; }
QLabel#chromeBrand { color:#b5b5b5; font-size:12px; }
QPushButton#windowControl { padding:3px; border:0; border-radius:4px; background:transparent; }
QPushButton#windowControl:hover { background:#404040; }
QPushButton#windowControl:focus { border:1px solid #a0a0a0; }
QDialog { border:1px solid #444444; }
QMenu { background:#292929; border:1px solid #454545; padding:5px; }
QMenu::item { padding:9px 24px 9px 12px; }
QMenu::item:selected { background:#454050; }
QPushButton#primary:disabled, QPushButton#unlockButton:disabled { background:#303030; border-color:#383838; color:#888888; }
QListWidget { border:0; background:transparent; outline:0; }
QListWidget::item { padding:12px 10px; border:1px solid transparent; border-radius:7px; margin-bottom:6px; }
QListWidget::item:selected { background:#333333; border-color:transparent; }
QListWidget::item:focus { border-color:#b5b5b5; }
QListWidget::item:hover { background:#292929; }
QCheckBox { spacing:7px; }
QCheckBox::indicator:unchecked { width:13px; height:13px; border:1px solid #7b899b; border-radius:3px; background:#252525; }
QSplitter::handle { background:#303030; width:1px; }
QScrollBar:vertical { background:transparent; width:8px; }
QScrollBar::handle:vertical { background:#465160; border-radius:4px; min-height:24px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
)");
    QCommandLineParser parser; parser.addHelpOption(); parser.addOption({"self-test", "Render isolated fictional data"}); parser.addOption({"data-dir", "Fixture output folder", "directory"}); parser.process(app);
    try
    {
        VaultWindow window(fixture); window.show();
        if (fixture)
        {
            if (!parser.isSet("data-dir")) return 2;
            const auto directory = QDir(parser.value("data-dir")).absolutePath(); QDir().mkpath(directory);
            QTimer::singleShot(100, &window, [&, directory]
            {
                try
                {
                    window.grab().save(directory + "/locked.png");
                    QTimer::singleShot(100, &window, [&window, directory]
                    {
                        if (auto* picker = window.findChild<QFileDialog*>())
                        {
                            picker->grab().save(directory + "/file-picker.png");
                            picker->reject();
                        }
                    });
                    window.findChild<QPushButton*>("openVault")->click();
                    window.loadFixture(directory);
                    app.processEvents(); window.grab().save(directory + "/workspace.png"); window.resize(1000, 700);
                    app.processEvents(); window.grab().save(directory + "/compact.png");
                    window.findChild<QListWidget*>("items")->setCurrentRow(1);
                    app.processEvents(); window.grab().save(directory + "/note.png");
                    window.findChild<QPushButton*>("navButton")->click();
                    app.processEvents(); window.grab().save(directory + "/empty.png");
                    auto* search = window.findChild<QLineEdit*>("vaultSearch");
                    search->setText("backup personal");
                    app.processEvents(); window.grab().save(directory + "/search-results.png");
                    search->setText("no-matching-fixture");
                    app.processEvents(); window.grab().save(directory + "/search-empty.png");
                    search->setText("recovery");
                    search->setFocus();
                    QKeyEvent openResult(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                    QApplication::sendEvent(search, &openResult);
                    app.processEvents(); window.grab().save(directory + "/search-keyboard.png");
                    search->clear();
                    auto* note = window.findChild<NoteEditor*>("noteEditor");
                    note->setHtml("<h1>A little space to think</h1><p>Collect the <b>important details</b>, then make room for <i>new ideas</i>.</p><h2>This week</h2><ul><li>Sketch the first draft</li><li>Keep useful references together</li><li><span style='color:#92d5b5'>Make time to review</span></li></ul><p><u>One thought to keep:</u> a good note makes the next step easier.</p>");
                    window.resize(1240, 820);
                    app.processEvents(); window.grab().save(directory + "/formatted-note.png");
                    window.resize(1000, 700);
                    note->setFocus();
                    auto selection = note->textCursor();
                    selection.setPosition(27);
                    selection.setPosition(53, QTextCursor::KeepAnchor);
                    note->setTextCursor(selection);
                    app.processEvents(); window.grab().save(directory + "/formatted-note-compact.png");
                    auto* paragraph = window.findChild<QComboBox*>("noteParagraph");
                    paragraph->showPopup();
                    app.processEvents(); paragraph->view()->window()->grab().save(directory + "/paragraph-menu.png");
                    paragraph->hidePopup();
                    auto* colors = window.findChild<QMenu*>("noteColors");
                    colors->popup(window.mapToGlobal(QPoint(600, 240)));
                    app.processEvents(); colors->grab().save(directory + "/color-menu.png");
                    colors->hide();
                    window.findChild<QSplitter*>()->setSizes({510, 490});
                    app.processEvents(); window.grab().save(directory + "/formatted-note-narrow-editor.png");
                    window.lockVault(true);
                    app.processEvents(); window.grab().save(directory + "/relocked.png");
                    window.loadCreationFixture();
                    app.processEvents(); window.grab().save(directory + "/create.png");
                    window.findChild<QPushButton*>("unlockButton")->click();
                    app.processEvents(); window.grab().save(directory + "/recovery.png");
                    QFile result(directory + "/result.json"); if (!result.open(QIODevice::WriteOnly)) { app.exit(1); return; } result.write("{\"fixture\":true,\"relocked\":true}"); app.quit();
                }
                catch (const std::exception&) { app.exit(1); }
            });
        }
        return app.exec();
    }
    catch (const std::exception& error) { QMessageBox failure(QMessageBox::Critical, "Vellum", QString::fromUtf8(error.what()), QMessageBox::Ok, nullptr, Qt::Dialog | Qt::FramelessWindowHint); failure.exec(); return 1; }
}

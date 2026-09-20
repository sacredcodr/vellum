#include "RecoveryCopy.h"
#include "VaultWindow.h"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QKeyEvent>
#include <QTextCursor>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMenu>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPainter>
#include <QScreen>
#include <QWindow>
#include <QStyle>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <wtsapi32.h>

namespace
{
QLabel* label(const QString& text, const char* name = "caption")
{
    auto* result = new QLabel(text); result->setObjectName(name); result->setWordWrap(true); return result;
}
QPushButton* button(const QString& text, const char* name = "")
{
    auto* result = new QPushButton(text); result->setObjectName(name); result->setCursor(Qt::PointingHandCursor); return result;
}
QPushButton* windowButton(QWidget* owner, QStyle::StandardPixmap icon, const QString& name)
{
    auto* result = button("", "windowControl");
    Q_UNUSED(owner);
    QPixmap glyph(16, 16);
    glyph.fill(Qt::transparent);
    QPainter painter(&glyph);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#c4c4c4"), 1.3));
    if (icon == QStyle::SP_TitleBarCloseButton)
    {
        painter.drawLine(QPointF(4, 4), QPointF(12, 12));
        painter.drawLine(QPointF(12, 4), QPointF(4, 12));
    }
    else if (icon == QStyle::SP_TitleBarMinButton)
    {
        painter.drawLine(QPointF(3, 10), QPointF(13, 10));
    }
    else
    {
        painter.drawRect(QRectF(4, 4, 8, 8));
    }
    painter.end();
    result->setIcon(QIcon(glyph));
    result->setFixedSize(30, 28);
    result->setAccessibleName(name);
    result->setToolTip(name);
    return result;
}
QString pickFile(QWidget* parent, const QString& title, const QString& initial, bool save, const QString& filter = "Encrypted vault (*.vault)")
{
    QFileDialog dialog(parent, title, {}, filter);
    dialog.setOption(QFileDialog::DontUseNativeDialog);
    dialog.setOption(QFileDialog::DontConfirmOverwrite);
    dialog.setWindowFlag(Qt::FramelessWindowHint);
    if (save) dialog.setDefaultSuffix(filter.contains("*.txt") ? "txt" : "vault");
    dialog.setAcceptMode(save ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
    dialog.setFileMode(save ? QFileDialog::AnyFile : QFileDialog::ExistingFile);
    dialog.selectFile(initial);
    dialog.resize(760, 480);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
    {
        return {};
    }
    return dialog.selectedFiles().first();
}
QMessageBox::StandardButton confirmAction(QWidget* parent, const QString& title, const QString& message,
    QMessageBox::StandardButtons choices, QMessageBox::StandardButton defaultChoice)
{
    QMessageBox dialog(QMessageBox::NoIcon, title, title, choices, parent, Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setInformativeText(message);
    dialog.setDefaultButton(defaultChoice);
    return static_cast<QMessageBox::StandardButton>(dialog.exec());
}

}
VaultWindow::VaultWindow(bool fixture) : m_fixture(fixture)
{
    setWindowTitle("Vellum — local notes & passwords");
    resize(1240, 820);
    buildUi();
    updateWindowPresentation();
    if (!fixture) m_path->setText(QSettings().value("vaultPath").toString());
    qApp->installEventFilter(this);
    m_activity.start();
    m_idleTimer.setInterval(1000);
    connect(&m_idleTimer, &QTimer::timeout, this, [this]
    {
        if ((m_store.unlocked() || !m_master->text().isEmpty()) && m_activity.elapsed() >= 5 * 60 * 1000) lockVault(true);
    });
    m_idleTimer.start();
    auto* shortcut = new QShortcut(QKeySequence("Ctrl+L"), this);
    connect(shortcut, &QShortcut::activated, this, [this] { lockVault(); });
    auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, [this] { if (m_store.unlocked()) saveEntry(); });
}
VaultWindow::~VaultWindow()
{
    qApp->removeEventFilter(this);
    if (!m_fixture) WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(winId()));
}
void VaultWindow::buildUi()
{
    m_pages = new QStackedWidget; setCentralWidget(m_pages);
    auto* locked = new QWidget;
    auto* lockedLayout = new QVBoxLayout(locked);
    lockedLayout->setContentsMargins(0, 0, 0, 0);
    auto* card = new QFrame; card->setObjectName("unlockCard"); card->setFixedWidth(470);
    auto* form = new QVBoxLayout(card); form->setContentsMargins(28, 22, 28, 24); form->setSpacing(10);
    auto* windowActions = new QHBoxLayout;
    auto* dragHandle = label("VELLUM", "eyebrow");
    dragHandle->setProperty("windowDragHandle", true);
    dragHandle->setToolTip("Drag to move window");
    windowActions->addWidget(dragHandle, 1);
    auto* minimizeButton = windowButton(this, QStyle::SP_TitleBarMinButton, "Minimize");
    auto* closeButton = windowButton(this, QStyle::SP_TitleBarCloseButton, "Close");
    connect(minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    windowActions->addWidget(minimizeButton);
    windowActions->addWidget(closeButton);
    form->addLayout(windowActions);
    m_setupBack = button("Back", "quiet");
    m_setupBack->hide();
    form->addWidget(m_setupBack, 0, Qt::AlignLeft);
    connect(m_setupBack, &QPushButton::clicked, this, [this]
    {
        if (m_recoveryStep)
        {
            showSetupStep(false);
            return;
        }
        m_create = false;
        m_master->clear();
        m_confirm->clear();
        m_path->clear();
        showSetupStep(false);
    });
    m_unlockTitle = label("Your private space.", "hero"); form->addWidget(m_unlockTitle);
    m_intro = label("Notes and passwords. Encrypted on this computer.", "muted"); form->addWidget(m_intro);
    m_path = new QLineEdit; m_path->setPlaceholderText("Select a vault file below"); m_path->setReadOnly(true); m_path->setAccessibleName("Vault file"); form->addWidget(m_path);
    m_fileActions = new QWidget;
    auto* fileActions = new QHBoxLayout(m_fileActions); fileActions->setContentsMargins(0, 0, 0, 0);
    auto* open = button("Open vault…"); auto* create = button("Create new…");
    open->setObjectName("openVault");
    fileActions->addWidget(open); fileActions->addWidget(create); form->addWidget(m_fileActions);
    connect(open, &QPushButton::clicked, this, [this] { chooseVault(false); });
    connect(create, &QPushButton::clicked, this, [this] { chooseVault(true); });
    m_masterLabel = label("Passphrase", "fine"); form->addWidget(m_masterLabel);
    m_master = new QLineEdit; m_master->setEchoMode(QLineEdit::Password); m_master->setPlaceholderText("Master passphrase"); m_master->setMaxLength(512); m_master->setAccessibleName("Master passphrase"); form->addWidget(m_master);
    m_confirmLabel = label("Confirm passphrase", "fine"); m_confirmLabel->hide(); form->addWidget(m_confirmLabel);
    m_confirm = new QLineEdit; m_confirm->setEchoMode(QLineEdit::Password); m_confirm->setPlaceholderText("Confirm master passphrase"); m_confirm->setMaxLength(512); m_confirm->hide(); form->addWidget(m_confirm);
    m_phraseCheck = label("", "fine"); m_phraseCheck->hide(); form->addWidget(m_phraseCheck);
    m_creationActions = new QWidget;
    auto* creationLayout = new QVBoxLayout(m_creationActions);
    creationLayout->setContentsMargins(0, 0, 0, 0);
    creationLayout->setSpacing(10);
    auto* generatorRow = new QHBoxLayout;
    m_generateMaster = button("Generate passphrase", "quiet");
    m_exportMaster = button("Save recovery copy", "quiet");
    generatorRow->addWidget(m_generateMaster);
    generatorRow->addWidget(m_exportMaster);
    creationLayout->addLayout(generatorRow);
    m_changeLocation = button("Change vault location", "quiet");
    creationLayout->addWidget(m_changeLocation);
    connect(m_changeLocation, &QPushButton::clicked, this, [this]
    {
        const auto destination = pickFile(this, "Choose a new vault file", "Vellum.vault", true);
        if (!destination.isEmpty())
        {
            m_path->setText(destination);
            m_unlockError->clear();
        }
    });
    m_masterReveal = new QCheckBox("Show passphrase");
    creationLayout->addWidget(m_masterReveal);
    m_recoveryAcknowledged = new QCheckBox("I've kept my passphrase somewhere safe");
    creationLayout->addWidget(m_recoveryAcknowledged);
    connect(m_generateMaster, &QPushButton::clicked, this, &VaultWindow::generateMasterPassphrase);
    connect(m_exportMaster, &QPushButton::clicked, this, &VaultWindow::exportRecoveryCopy);
    connect(m_masterReveal, &QCheckBox::toggled, this, [this](bool show)
    {
        m_master->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
        m_confirm->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
    });
    auto checkPhrase = [this]
    {
        m_recoveryAcknowledged->setChecked(false);
        if (m_master->text().isEmpty()) m_phraseCheck->clear();
        else if (m_master->text().size() < 16) m_phraseCheck->setText("Use at least 16 characters.");
        else if (m_confirm->text().isEmpty()) m_phraseCheck->setText("Repeat your passphrase to confirm it.");
        else if (m_master->text() == m_confirm->text()) m_phraseCheck->setText("Passphrases match.");
        else m_phraseCheck->setText("Passphrases do not match yet.");
    };
    connect(m_master, &QLineEdit::textChanged, this, checkPhrase);
    connect(m_confirm, &QLineEdit::textChanged, this, checkPhrase);
    m_master->setContextMenuPolicy(Qt::NoContextMenu);
    m_confirm->setContextMenuPolicy(Qt::NoContextMenu);
    m_creationActions->hide();
    form->addWidget(m_creationActions);
    auto* unlockButton = button("Unlock vault", "primary"); unlockButton->setObjectName("unlockButton"); form->addWidget(unlockButton);
    connect(m_recoveryAcknowledged, &QCheckBox::toggled, this, [this, unlockButton](bool checked)
    {
        unlockButton->setEnabled(!m_recoveryStep || checked);
    });
    connect(unlockButton, &QPushButton::clicked, this, &VaultWindow::unlock);
    connect(m_master, &QLineEdit::returnPressed, this, &VaultWindow::unlock);
    connect(m_confirm, &QLineEdit::returnPressed, this, &VaultWindow::unlock);
    m_unlockError = label("", "error"); form->addWidget(m_unlockError);
    m_footer = label("No cloud account. No password reset.", "fine"); form->addWidget(m_footer);
    lockedLayout->addWidget(card);
    m_pages->addWidget(locked);

    auto* workspace = new QWidget; auto* outer = new QVBoxLayout(workspace); outer->setContentsMargins(0, 0, 0, 0); outer->setSpacing(0);
    auto* chrome = new QFrame;
    chrome->setObjectName("appChrome");
    auto* chromeLayout = new QHBoxLayout(chrome);
    chromeLayout->setContentsMargins(20, 5, 8, 5);
    auto* workspaceDrag = label("Vellum", "chromeBrand");
    workspaceDrag->setProperty("windowDragHandle", true);
    chromeLayout->addWidget(workspaceDrag, 1);
    auto* minimize = windowButton(this, QStyle::SP_TitleBarMinButton, "Minimize");
    auto* maximize = windowButton(this, QStyle::SP_TitleBarMaxButton, "Maximize or restore");
    auto* close = windowButton(this, QStyle::SP_TitleBarCloseButton, "Close");
    connect(minimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(maximize, &QPushButton::clicked, this, [this]
    {
        if (isMaximized()) showNormal(); else showMaximized();
    });
    connect(close, &QPushButton::clicked, this, &QWidget::close);
    chromeLayout->addWidget(minimize);
    chromeLayout->addWidget(maximize);
    chromeLayout->addWidget(close);
    outer->addWidget(chrome);
    auto* split = new QSplitter;
    split->setChildrenCollapsible(false);
    outer->addWidget(split, 1);
    auto* collection = new QWidget;
    collection->setMinimumWidth(260);
    collection->setMaximumWidth(340);
    collection->setObjectName("collection");
    auto* collectionLayout = new QVBoxLayout(collection);
    collectionLayout->setContentsMargins(18, 20, 18, 16);
    collectionLayout->setSpacing(16);

    auto* sidebarHeader = new QHBoxLayout;
    auto* vaultMenuButton = button("Library", "brandButton");
    auto* vaultMenu = new QMenu(vaultMenuButton);
    vaultMenu->addAction("Create encrypted backup...", this, &VaultWindow::backup);
    vaultMenuButton->setMenu(vaultMenu);
    sidebarHeader->addWidget(vaultMenuButton);
    sidebarHeader->addStretch();
    auto* newButton = button("New", "quiet");
    auto* newMenu = new QMenu(newButton);
    newMenu->addAction("Password", this, [this] { newEntry("login"); });
    newMenu->addAction("Secure note", this, [this] { newEntry("note"); });
    newButton->setMenu(newMenu);
    sidebarHeader->addWidget(newButton);
    collectionLayout->addLayout(sidebarHeader);

    m_search = new QLineEdit;
    m_search->setPlaceholderText("Search vault...");
    m_search->setAccessibleName("Search vault");
    m_search->setClearButtonEnabled(true);
    collectionLayout->addWidget(m_search);
    connect(m_search, &QLineEdit::textChanged, this, &VaultWindow::refreshList);
    auto* searchShortcut = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]
    {
        if (m_store.unlocked())
        {
            m_search->setFocus();
            m_search->selectAll();
        }
    });
    auto* filters = new QHBoxLayout;
    filters->setSpacing(4);
    for (const auto& item : QList<QPair<QString, QString>>{{"All", ""}, {"Passwords", "login"}, {"Notes", "note"}})
    {
        auto* action = button(item.first, "navButton");
        action->setCheckable(true);
        action->setAutoExclusive(true);
        action->setChecked(item.second.isEmpty());
        action->setProperty("filter", item.second);
        connect(action, &QPushButton::clicked, this, [this, filter = item.second]
        {
            if (!resolveDraft())
            {
                for (auto* candidate : findChildren<QPushButton*>("navButton"))
                {
                    candidate->setChecked(candidate->property("filter").toString() == m_filter);
                }
                return;
            }
            m_filter = filter;
            m_selected.clear();
            clearEditor();
            refreshList();
        });
        filters->addWidget(action);
    }
    collectionLayout->addLayout(filters);
    m_list = new QListWidget; m_list->setObjectName("items"); collectionLayout->addWidget(m_list, 1);
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item)
    {
        if (item && !m_loading) selectEntry(item->data(Qt::UserRole).toString());
    }); split->addWidget(collection);
    auto* sidebarFooter = new QHBoxLayout;
    sidebarFooter->addWidget(label("Local vault", "fine"));
    sidebarFooter->addStretch();
    auto* lockButton = button("Lock", "quiet");
    lockButton->setToolTip("Lock vault (Ctrl+L)");
    connect(lockButton, &QPushButton::clicked, this, [this] { lockVault(); });
    sidebarFooter->addWidget(lockButton);
    collectionLayout->addLayout(sidebarFooter);
    auto* detail = new QWidget; detail->setMinimumWidth(490); auto* detailLayout = new QVBoxLayout(detail); detailLayout->setContentsMargins(40, 18, 40, 18); detailLayout->setSpacing(16);
    auto* documentHeader = new QHBoxLayout;
    m_recordType = label("Your library", "muted");
    documentHeader->addWidget(m_recordType);
    documentHeader->addStretch();
    auto* itemMenuButton = button("More", "quiet");
    itemMenuButton->setAccessibleName("Item actions");
    itemMenuButton->setObjectName("itemActions");
    auto* itemMenu = new QMenu(itemMenuButton);
    itemMenu->addAction("Delete item...", this, &VaultWindow::deleteEntry);
    itemMenuButton->setMenu(itemMenu);
    documentHeader->addWidget(itemMenuButton);
    detailLayout->addLayout(documentHeader);
    detailLayout->addSpacing(8);
    m_editor = new QWidget; auto* editorLayout = new QVBoxLayout(m_editor); editorLayout->setContentsMargins(0, 0, 0, 0); editorLayout->setSpacing(14);
    m_title = new QLineEdit; m_title->setObjectName("titleEdit"); m_title->setAccessibleName("Item title"); m_title->setPlaceholderText("Title"); m_title->setMaxLength(512); editorLayout->addWidget(m_title);
    m_loginFields = new QWidget; auto* loginLayout = new QFormLayout(m_loginFields); loginLayout->setContentsMargins(0, 0, 0, 0); loginLayout->setVerticalSpacing(14);
    m_username = new QLineEdit; m_url = new QLineEdit; m_password = new QLineEdit; m_password->setEchoMode(QLineEdit::Password);
    for (auto* input : {m_username, m_url, m_password}) input->setMaxLength(4096);
    loginLayout->addRow("Username", m_username); loginLayout->addRow("Website", m_url); loginLayout->addRow("Password", m_password);
    auto* passwordActions = new QHBoxLayout; passwordActions->setSpacing(8); m_reveal = new QCheckBox("Show"); auto* generate = button("Generate", "quiet"); auto* copy = button("Copy password", "quiet");
    passwordActions->addWidget(m_reveal); passwordActions->addStretch(); passwordActions->addWidget(generate); passwordActions->addWidget(copy); loginLayout->addRow(passwordActions);
    connect(m_reveal, &QCheckBox::toggled, this, [this](bool reveal) { m_password->setEchoMode(reveal ? QLineEdit::Normal : QLineEdit::Password); });
    connect(generate, &QPushButton::clicked, this, [this] { m_password->setText(VaultStore::generatePassword()); });
    connect(copy, &QPushButton::clicked, this, [this]
    {
        if (!m_store.unlocked() || m_password->text().isEmpty()) return;
        try { m_clipboard.copy(m_password->text(), reinterpret_cast<HWND>(winId())); m_status->setText("Password copied. Clears in 20 seconds if the clipboard is unchanged."); }
        catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
    }); editorLayout->addWidget(m_loginFields);
    m_notes = new QPlainTextEdit; m_notes->setPlaceholderText("Write something worth keeping private…"); m_notes->setAccessibleName("Private notes"); m_notes->setUndoRedoEnabled(false); editorLayout->addWidget(m_notes, 1);
    auto* actions = new QHBoxLayout; actions->addStretch();
    m_save = button("Save changes", "primary"); actions->addWidget(m_save); editorLayout->addLayout(actions);
    connect(m_save, &QPushButton::clicked, this, [this] { saveEntry(); });
    m_empty = label("A space for what matters.\n\nChoose an item or use New to add a note or password.", "emptyState");
    m_empty->setAlignment(Qt::AlignCenter);
    detailLayout->addWidget(m_empty, 1);
    detailLayout->addWidget(m_editor, 1); m_status = label("Select an item, or create a password or note.", "muted"); detailLayout->addWidget(m_status); split->addWidget(detail); split->setSizes({290, 950});
    m_pages->addWidget(workspace);
    auto markDirty = [this] { if (!m_loading) { m_dirty = true; m_save->show(); m_save->setEnabled(true); m_save->setText("Save changes"); m_status->setText("Unsaved changes  ·  Ctrl+S to save"); } };
    for (auto* input : {m_title, m_username, m_url, m_password}) connect(input, &QLineEdit::textChanged, this, markDirty);
    connect(m_notes, &QPlainTextEdit::textChanged, this, markDirty);
    for (auto* input : {m_title, m_username, m_url, m_password}) input->setContextMenuPolicy(Qt::NoContextMenu);
    m_notes->setContextMenuPolicy(Qt::NoContextMenu);
    clearEditor();
}
void VaultWindow::updateWindowPresentation()
{
    const bool locked = !m_store.unlocked();
    const bool visible = isVisible();
    const QPoint center = visible ? frameGeometry().center() : screen()->availableGeometry().center();
    if (locked && width() >= 1000)
    {
        m_workspaceSize = size();
    }
    if (!m_fixture)
    {
        WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(winId()));
    }
    setWindowFlag(Qt::FramelessWindowHint, true);
    if (locked && isMaximized()) setWindowState(Qt::WindowNoState);
    layout()->setSizeConstraint(QLayout::SetNoConstraint);
    m_pages->setMinimumSize(0, 0);
    if (locked)
    {
        auto* card = findChild<QFrame*>("unlockCard");
        const int height = qMax(440, card->sizeHint().height());
        setFixedSize(470, height);
        QPainterPath outline;
        outline.addRoundedRect(QRectF(rect()), 18, 18);
        setMask(QRegion(outline.toFillPolygon().toPolygon()));
    }
    else
    {
        clearMask();
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        setMinimumSize(1000, 700);
        resize(m_workspaceSize);
    }
    const QRect available = screen()->availableGeometry();
    move(qBound(available.left(), center.x() - width() / 2, qMax(available.left(), available.right() - width() + 1)),
         qBound(available.top(), center.y() - height() / 2, qMax(available.top(), available.bottom() - height() + 1)));
    if (visible)
    {
        show();
    }
    if (!m_fixture && !WTSRegisterSessionNotification(reinterpret_cast<HWND>(winId()), NOTIFY_FOR_THIS_SESSION))
    {
        m_unlockError->setText("Windows session-lock notifications are unavailable. Lock manually before leaving.");
    }
}
void VaultWindow::chooseVault(bool create)
{
    const auto path = pickFile(this, create ? "Create a new vault" : "Open encrypted vault", create ? "Vellum.vault" : "", create);
    if (path.isEmpty()) return;
    m_create = create; m_path->setText(path); m_master->clear(); m_confirm->clear(); m_confirm->setVisible(create);
    m_creationActions->setVisible(create);
    m_masterReveal->setChecked(false);
    m_recoveryAcknowledged->setChecked(false);
    m_unlockTitle->setText(create ? "Create your vault." : "Welcome back.");
    findChild<QPushButton*>("unlockButton")->setText(create ? "Save encrypted vault" : "Unlock vault");
    m_unlockError->setText(create ? "Choose a long, unique passphrase of at least 16 characters. Losing it means losing access." : ""); m_master->setFocus();
    showSetupStep(false);
}
void VaultWindow::showSetupStep(bool recovery)
{
    m_recoveryStep = m_create && recovery;
    m_path->setVisible(!m_create || m_recoveryStep);
    m_changeLocation->setVisible(m_recoveryStep);
    m_phraseCheck->setVisible(m_create && !m_recoveryStep);
    m_fileActions->setVisible(!m_create);
    m_setupBack->setVisible(m_create);
    m_masterLabel->setVisible(!m_recoveryStep);
    m_confirmLabel->setVisible(m_create && !m_recoveryStep);
    m_master->setVisible(!m_recoveryStep);
    m_confirm->setVisible(m_create && !m_recoveryStep);
    m_creationActions->setVisible(m_create);
    m_generateMaster->setVisible(!m_recoveryStep);
    m_masterReveal->setVisible(!m_recoveryStep);
    m_exportMaster->setVisible(m_recoveryStep);
    m_recoveryAcknowledged->setVisible(m_recoveryStep);
    m_masterReveal->setChecked(false);
    m_unlockError->clear();
    m_unlockTitle->setText(m_recoveryStep ? "Keep your key safe." : m_create ? "Your passphrase." : "Your private space.");
    m_intro->setText(m_recoveryStep
        ? "Save a recovery copy, or write your passphrase down somewhere safe."
        : m_create ? "Use your own, or generate one below." : "Notes and passwords. Encrypted on this computer.");
    m_footer->setText(m_recoveryStep ? "Recovery copies are unencrypted text. Keep yours separate from the vault." : m_create ? "1 of 2  /  Passphrase" : "No cloud account. No password reset.");
    findChild<QPushButton*>("unlockButton")->setText(m_recoveryStep ? "Create vault" : m_create ? "Continue" : "Unlock vault");
    findChild<QPushButton*>("unlockButton")->setEnabled(!m_recoveryStep || m_recoveryAcknowledged->isChecked());
    updateWindowPresentation();
}
void VaultWindow::generateMasterPassphrase()
{
    if (!m_create || m_store.unlocked())
    {
        return;
    }
    const auto passphrase = VaultStore::generatePassphrase();
    m_master->setText(passphrase);
    m_confirm->setText(passphrase);
    m_masterReveal->setChecked(false);
    m_unlockError->clear();
    m_intro->setText("Seven random words, generated on this computer.");
    updateWindowPresentation();
}
void VaultWindow::exportRecoveryCopy()
{
    if (!m_create || m_store.unlocked())
    {
        return;
    }
    if (m_master->text().size() < 16 || m_master->text() != m_confirm->text())
    {
        m_unlockError->setText("Enter matching passphrases of at least 16 characters first.");
        updateWindowPresentation();
        return;
    }
    const auto answer = confirmAction(this, "Save readable recovery copy",
        "This text file will contain your master passphrase without encryption. Anyone with it and your vault can unlock your data. Keep it separate, for example on removable storage kept in a safe place.",
        QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Save)
    {
        return;
    }
    const auto destination = pickFile(this, "Save recovery copy (not encrypted)", "Vellum-recovery.txt", true, "Text file (*.txt)");
    if (destination.isEmpty() || !m_create || m_master->text().size() < 16)
    {
        return;
    }
    if (QFileInfo(destination).absoluteFilePath().compare(QFileInfo(m_path->text()).absoluteFilePath(), Qt::CaseInsensitive) == 0)
    {
        m_unlockError->setText("Choose a different location from the encrypted vault file.");
        updateWindowPresentation();
        return;
    }
    try
    {
        RecoveryCopy::save(destination, m_master->text());
        m_recoveryAcknowledged->setChecked(true);
        m_intro->setText("Recovery copy saved. You can now create your vault.");
        m_unlockError->clear();
    }
    catch (const std::exception& error)
    {
        m_unlockError->setText(QString::fromUtf8(error.what()));
    }
    updateWindowPresentation();
}
void VaultWindow::unlock()
{
    if (m_path->text().isEmpty()) { m_unlockError->setText("Choose a vault file first."); return; }
    if (m_create && m_master->text() != m_confirm->text())
    {
        showSetupStep(false);
        m_unlockError->setText("The passphrases do not match. Correct the confirmation below.");
        m_confirm->setFocus();
        m_confirm->selectAll();
        updateWindowPresentation();
        return;
    }
    if (m_create && !m_recoveryStep)
    {
        if (m_master->text().size() < 16)
        {
            m_unlockError->setText("Use at least 16 characters, or generate a passphrase.");
            updateWindowPresentation();
            return;
        }
        showSetupStep(true);
        return;
    }
    if (m_create && !m_recoveryAcknowledged->isChecked())
    {
        m_unlockError->setText("Save a recovery copy or confirm that you have kept your passphrase somewhere safe.");
        updateWindowPresentation();
        return;
    }
    QByteArray passphrase = m_master->text().toUtf8();
    const bool creating = m_create;
    if (!creating)
    {
        m_master->clear();
        m_confirm->clear();
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    try
    {
        if (m_create) m_store.create(m_path->text(), passphrase); else m_store.open(m_path->text(), passphrase);
        QApplication::restoreOverrideCursor(); m_master->clear(); m_confirm->clear(); m_phraseCheck->clear(); m_create = false; m_confirm->hide(); m_creationActions->hide(); m_masterReveal->setChecked(false); m_unlockError->clear();
        if (!m_fixture) QSettings().setValue("vaultPath", m_store.path());
        m_selected.clear(); clearEditor(); refreshList(); m_pages->setCurrentIndex(1); updateWindowPresentation(); m_activity.restart();
        m_status->setText("Vault unlocked. All saved items are encrypted on disk.");
    }
    catch (const std::exception& error)
    {
        QApplication::restoreOverrideCursor();
        // Creation remains retryable with the same phrase and its existing recovery copy.
        m_unlockError->setText(QString::fromUtf8(error.what()));
        updateWindowPresentation();
    }
}
void VaultWindow::refreshList()
{
    const QSignalBlocker block(m_list); m_list->clear();
    const auto query = m_search->text();
    for (const auto& value : m_store.entries())
    {
        const auto item = value.toObject();
        if (!m_filter.isEmpty() && item["type"] != m_filter) continue;
        if (!(item["title"].toString() + " " + item["username"].toString()).contains(query, Qt::CaseInsensitive)) continue;
        auto* row = new QListWidgetItem(item["title"].toString() + "\n" + (item["type"] == "login" ? "Password" : "Secure note"), m_list);
        row->setData(Qt::UserRole, item["id"].toString()); row->setToolTip(item["title"].toString());
        if (item["id"] == m_selected) m_list->setCurrentItem(row);
    }
}
void VaultWindow::clearEditor()
{
    m_loading = true;
    for (auto* field : {m_title, m_username, m_url, m_password}) field->clear();
    m_notes->clear(); m_reveal->setChecked(false); m_dirty = false; m_save->setEnabled(false); m_save->hide(); m_editor->setEnabled(false); m_editor->hide(); m_empty->show();
    m_recordType->setText("Your library");
    findChild<QPushButton*>("itemActions")->setEnabled(false);
    m_status->setText("Select an item, or create a password or note.");
    m_loading = false;
}
bool VaultWindow::resolveDraft()
{
    if (!m_dirty) return true;
    const auto answer = confirmAction(this, "Unsaved changes", "Save changes before continuing?", QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (!m_store.unlocked()) return false;
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) return saveEntry();
    const auto selected = m_selected;
    m_dirty = false; m_selected.clear();
    selectEntry(selected);
    return true;
}
void VaultWindow::selectEntry(const QString& id)
{
    if (id == m_selected) return;
    if (!resolveDraft()) { refreshList(); return; }
    for (const auto& value : m_store.entries())
    {
        const auto item = value.toObject(); if (item["id"] != id) continue;
        clearEditor(); m_loading = true; m_selected = id;
        m_recordType->setText(item["type"] == "login" ? "Password" : "Secure note");
        m_title->setText(item["title"].toString()); m_username->setText(item["username"].toString()); m_url->setText(item["url"].toString());
        m_password->setText(item["password"].toString()); m_notes->setPlainText(item["notes"].toString());
        m_loginFields->setVisible(item["type"] == "login"); m_editor->setEnabled(true); m_editor->show(); m_empty->hide(); findChild<QPushButton*>("itemActions")->setEnabled(true); m_status->setText("Saved locally · encrypted at rest"); m_loading = false;
        return;
    }
}
void VaultWindow::newEntry(const QString& type)
{
    if (!resolveDraft() || !m_store.unlocked()) return;
    auto entries = m_store.entries();
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entries.append(QJsonObject{{"id", id}, {"type", type}, {"title", type == "login" ? "Untitled password" : "Untitled note"}, {"username", ""}, {"url", ""}, {"password", ""}, {"notes", ""}, {"updated", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
    try { m_store.save(entries); m_dirty = false; m_filter.clear();
        for (auto* candidate : findChildren<QPushButton*>("navButton"))
        {
            candidate->setChecked(candidate->property("filter").toString().isEmpty());
        }
        m_search->clear(); selectEntry(id); refreshList(); m_title->setFocus(); m_title->selectAll(); }
    catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}
bool VaultWindow::saveEntry()
{
    if (!m_store.unlocked() || m_selected.isEmpty()) return false;
    if (!m_dirty) return true;
    if (m_title->text().trimmed().isEmpty()) { showError("Give this item a title before saving."); return false; }
    if (m_notes->toPlainText().size() > 1024 * 1024) { showError("Notes are limited to one million characters."); return false; }
    auto entries = m_store.entries();
    for (qsizetype i = 0; i < entries.size(); ++i)
    {
        auto item = entries[i].toObject(); if (item["id"] != m_selected) continue;
        item["title"] = m_title->text(); item["username"] = m_username->text(); item["url"] = m_url->text(); item["password"] = m_password->text(); item["notes"] = m_notes->toPlainText(); item["updated"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate); entries[i] = item;
        try { m_store.save(entries); m_dirty = false; m_save->setEnabled(false); m_save->hide(); m_status->setText("Saved locally · encrypted at rest"); refreshList(); return true; }
        catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); return false; }
    }
    return false;
}
void VaultWindow::deleteEntry()
{
    if (m_selected.isEmpty()) return;
    if (confirmAction(this, "Delete item", "Permanently delete this item from this vault? Existing backups may still contain it.", QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes || !m_store.unlocked()) return;
    auto entries = m_store.entries();
    for (qsizetype i = 0; i < entries.size(); ++i) if (entries[i].toObject()["id"] == m_selected) { entries.removeAt(i); break; }
    try { m_store.save(entries); m_selected.clear(); clearEditor(); refreshList(); m_status->setText("Item deleted from this vault."); }
    catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}
void VaultWindow::backup()
{
    if (!resolveDraft() || !m_store.unlocked()) return;
    const auto path = pickFile(this, "Create encrypted backup", "Vellum-backup-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".vault", true);
    if (path.isEmpty() || !m_store.unlocked()) return;
    try { m_store.backup(path); m_status->setText("Encrypted backup created. Open it with the same master passphrase to verify recovery."); }
    catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
}
void VaultWindow::lockVault(bool automatic)
{
    if (!m_store.unlocked())
    {
        m_master->clear();
        m_confirm->clear();
        m_masterReveal->setChecked(false);
        m_clipboard.clear();
        if (m_create) showSetupStep(false);
        for (auto* dialog : findChildren<QDialog*>())
        {
            dialog->reject();
        }
        return;
    }
    if (!automatic && !resolveDraft()) return;
    const bool failedSave = automatic && m_dirty && !saveEntry();
    m_clipboard.clear(); m_selected.clear(); clearEditor(); m_store.lock(); m_list->clear(); m_search->clear();
    m_status->clear(); m_recordType->setText("Your library");
    m_master->clear(); m_confirm->clear(); m_confirm->hide(); m_creationActions->hide(); m_masterReveal->setChecked(false); m_create = false;
    showSetupStep(false);
    m_unlockTitle->setText("Your vault is locked."); findChild<QPushButton*>("unlockButton")->setText("Unlock vault");
    m_unlockError->setText(failedSave ? "Automatic lock could not save your draft. The last successfully saved vault is unchanged; unsaved edits were discarded." : "");
    m_pages->setCurrentIndex(0);
    updateWindowPresentation();
    for (auto* menu : findChildren<QMenu*>())
    {
        menu->close();
    }
    for (auto* dialog : findChildren<QDialog*>()) dialog->reject();
}
void VaultWindow::showError(const QString& message) { m_status->setText(message); }
bool VaultWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Polish)
    {
        if (auto* dialog = qobject_cast<QDialog*>(watched))
        {
            dialog->setWindowFlag(Qt::FramelessWindowHint);
        }
    }
    if (watched->property("windowDragHandle").toBool() && event->type() == QEvent::MouseButtonPress)
    {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton && windowHandle())
        {
            windowHandle()->startSystemMove();
            return true;
        }
    }
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::MouseButtonPress || event->type() == QEvent::Wheel) m_activity.restart();
    if ((m_store.unlocked() || m_create) && event->type() == QEvent::KeyPress)
    {
        const auto* key = static_cast<QKeyEvent*>(event);
        const bool copying = key->matches(QKeySequence::Copy);
        const bool cutting = key->matches(QKeySequence::Cut);
        const bool secretField = watched == m_master || watched == m_confirm || watched == m_title || watched == m_username || watched == m_url || watched == m_password || watched == m_notes;
        if ((copying || cutting) && secretField)
        {
            auto* line = qobject_cast<QLineEdit*>(watched);
            if (line && line->echoMode() == QLineEdit::Password) return true;
            const QString selection = line ? line->selectedText() : m_notes->textCursor().selectedText();
            if (selection.isEmpty()) return true;
            try
            {
                m_clipboard.copy(selection, reinterpret_cast<HWND>(winId()));
                if (cutting)
                {
                    if (line) line->insert("");
                    else { auto cursor = m_notes->textCursor(); cursor.removeSelectedText(); m_notes->setTextCursor(cursor); }
                }
                m_status->setText("Copied with a 20-second clipboard timeout.");
            }
            catch (const std::exception& error) { showError(QString::fromUtf8(error.what())); }
            return true;
        }
    }
    return false;
}
bool VaultWindow::nativeEvent(const QByteArray& type, void* message, qintptr* result)
{
    const auto* native = static_cast<MSG*>(message);
    if (native->message == WM_NCHITTEST && m_store.unlocked() && !isMaximized())
    {
        RECT bounds{};
        GetWindowRect(reinterpret_cast<HWND>(winId()), &bounds);
        const int x = static_cast<short>(LOWORD(native->lParam));
        const int y = static_cast<short>(HIWORD(native->lParam));
        constexpr int edge = 7;
        const bool left = x < bounds.left + edge;
        const bool right = x >= bounds.right - edge;
        const bool top = y < bounds.top + edge;
        const bool bottom = y >= bounds.bottom - edge;
        if (top || bottom || left || right)
        {
            *result = top ? (left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP)
                : bottom ? (left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM)
                : left ? HTLEFT : HTRIGHT;
            return true;
        }
    }

    if ((native->message == WM_WTSSESSION_CHANGE && native->wParam == WTS_SESSION_LOCK) || (native->message == WM_POWERBROADCAST && native->wParam == PBT_APMSUSPEND)) lockVault(true);
    return QMainWindow::nativeEvent(type, message, result);
}
void VaultWindow::closeEvent(QCloseEvent* event)
{
    if (m_store.unlocked() && !resolveDraft()) { event->ignore(); return; }
    m_clipboard.clear(); clearEditor(); m_store.lock(); event->accept();
}
void VaultWindow::loadFixture(const QString& directory)
{
    QByteArray passphrase("fixture-only-master-passphrase");
    m_store.create(directory + "/fixture.vault", passphrase);
    QJsonArray entries;
    for (const auto& pair : QList<QPair<QString, QString>>{{"login", "Personal email"}, {"note", "Recovery checklist"}, {"login", "Design workspace"}, {"note", "Ideas for next week"}})
    {
        entries.append(QJsonObject{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"type", pair.first}, {"title", pair.second}, {"username", pair.first == "login" ? "alex@example.invalid" : ""}, {"url", pair.first == "login" ? "https://example.invalid" : ""}, {"password", pair.first == "login" ? "EXAMPLE-NOT-A-REAL-PASSWORD" : ""}, {"notes", "A private place for the details that matter.\n\nEverything in this example is fictional.\n\nKeep an encrypted backup somewhere you trust, and verify that you can open it."}, {"updated", "2026-09-20"}});
    }
    m_store.save(entries); m_path->setText(m_store.path()); refreshList(); selectEntry(entries.first().toObject()["id"].toString()); refreshList(); m_pages->setCurrentIndex(1); updateWindowPresentation();
}

void VaultWindow::loadCreationFixture()
{
    if (!m_fixture || m_store.unlocked())
    {
        return;
    }
    m_create = true;
    m_path->setText("Vellum-example.vault");
    m_confirm->show();
    m_creationActions->show();
    m_unlockTitle->setText("Create your vault.");
    findChild<QPushButton*>("unlockButton")->setText("Save encrypted vault");
    showSetupStep(false);
    generateMasterPassphrase();
}

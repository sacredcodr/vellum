#include "NoteEditor.h"

#include <QAction>
#include <QComboBox>
#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QSignalBlocker>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextList>
#include <QToolButton>

namespace
{
class ParagraphCombo : public QComboBox
{
public:
    explicit ParagraphCombo(QWidget* parent) : QComboBox(parent) {}

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QComboBox::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#bdbdbd"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const qreal x = width() - 15;
        const qreal y = height() / 2.0;
        painter.drawPolyline(QPolygonF{{x - 3, y - 1.5}, {x, y + 1.5}, {x + 3, y - 1.5}});
    }
};

enum class FormatIcon { Bold, Italic, Underline, Bullets, Numbers, Color, Clear };

QIcon formatIcon(FormatIcon kind, const QColor& color = QColor("#dedede"))
{
    QPixmap image(40, 40);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#e1e1e1"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QFont font("Segoe UI");
    font.setPixelSize(18);
    if (kind == FormatIcon::Bullets || kind == FormatIcon::Numbers)
    {
        font.setPixelSize(8);
        painter.setFont(font);
        for (int row = 0; row < 3; ++row)
        {
            const int y = 4 + row * 6;
            painter.drawLine(QPointF(9, y), QPointF(18, y));
            if (kind == FormatIcon::Bullets)
            {
                painter.setBrush(QColor("#e1e1e1"));
                painter.drawEllipse(QPointF(3, y), 1, 1);
            }
            else
            {
                painter.drawText(QRectF(0, y - 5, 6, 10), Qt::AlignCenter, QString::number(row + 1));
            }
        }
    }
    else if (kind == FormatIcon::Clear)
    {
        painter.drawPolygon(QPolygonF{{2, 12}, {11, 3}, {18, 10}, {10, 18}, {8, 18}});
        painter.drawLine(QPointF(7, 7), QPointF(14, 14));
        painter.drawLine(QPointF(9, 18), QPointF(19, 18));
    }
    else
    {
        font.setBold(kind == FormatIcon::Bold);
        font.setItalic(kind == FormatIcon::Italic);
        font.setUnderline(kind == FormatIcon::Underline);
        painter.setFont(font);
        const QString text = kind == FormatIcon::Bold ? "B" : kind == FormatIcon::Italic ? "I" : kind == FormatIcon::Underline ? "U" : "A";
        painter.drawText(QRectF(0, -2, 20, 22), Qt::AlignCenter, text);
        if (kind == FormatIcon::Color)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawRoundedRect(QRectF(2, 17, 16, 3), 1, 1);
        }
    }
    return QIcon(image);
}

QIcon colorSwatch(const QColor& color)
{
    QPixmap image(32, 32);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(2, 2, 12, 12), 3, 3);
    return QIcon(image);
}

class LocalNoteDocument : public QTextDocument
{
public:
    explicit LocalNoteDocument(QObject* parent) : QTextDocument(parent) {}

protected:
    QVariant loadResource(int, const QUrl&) override
    {
        // A valid empty value prevents fallback to a global resource provider.
        return QByteArray{};
    }
};
}

NoteEditor::NoteEditor(QWidget* parent) : QTextEdit(parent)
{
    setDocument(new LocalNoteDocument(this));
    setAcceptRichText(false);
    setAcceptDrops(false);
    setContextMenuPolicy(Qt::NoContextMenu);
    setObjectName("noteEditor");
    setAccessibleName("Private notes");
    setPlaceholderText("Write something worth keeping private...");
    connect(this, &QTextEdit::currentCharFormatChanged, this, &NoteEditor::updateToolbar);
    connect(this, &QTextEdit::cursorPositionChanged, this, &NoteEditor::updateToolbar);
}

QWidget* NoteEditor::createToolbar(QWidget* parent)
{
    auto* toolbar = new QFrame(parent);
    toolbar->setObjectName("noteToolbar");
    toolbar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    toolbar->setFixedHeight(50);
    auto* layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(2);

    auto addSeparator = [toolbar, layout]
    {
        auto* separator = new QFrame(toolbar);
        separator->setObjectName("formatSeparator");
        separator->setFixedSize(1, 20);
        layout->addWidget(separator);
    };

    m_paragraph = new ParagraphCombo(toolbar);
    m_paragraph->setObjectName("noteParagraph");
    m_paragraph->setAccessibleName("Paragraph style");
    m_paragraph->setToolTip("Paragraph style");
    m_paragraph->addItems({"Normal text", "Heading 1", "Heading 2", "Heading 3"});
    for (int index = 0; index < m_paragraph->count(); ++index)
    {
        m_paragraph->setItemData(index, QSize(180, 34), Qt::SizeHintRole);
    }
    m_paragraph->setFixedSize(120, 34);
    m_paragraph->view()->setMinimumWidth(180);
    layout->addWidget(m_paragraph);
    connect(m_paragraph, &QComboBox::activated, this, &NoteEditor::applyHeading);
    addSeparator();

    auto addButton = [this, toolbar, layout](FormatIcon icon, const QString& name, const QString& shortcut, bool checkable)
    {
        auto* action = new QAction(name, this);
        action->setIcon(formatIcon(icon));
        action->setToolTip(shortcut.isEmpty() ? name : name + " (" + shortcut + ")");
        action->setCheckable(checkable);
        if (!shortcut.isEmpty())
        {
            action->setShortcut(QKeySequence(shortcut));
            action->setShortcutContext(Qt::WidgetShortcut);
            addAction(action);
        }
        auto* control = new QToolButton(toolbar);
        control->setDefaultAction(action);
        control->setToolButtonStyle(Qt::ToolButtonIconOnly);
        control->setIconSize(QSize(20, 20));
        control->setFixedSize(34, 34);
        control->setAccessibleName(name);
        control->setToolTip(shortcut.isEmpty() ? name : name + " (" + shortcut + ")");
        control->setFocusPolicy(Qt::StrongFocus);
        layout->addWidget(control);
        return control;
    };
    m_bold = addButton(FormatIcon::Bold, "Bold", "Ctrl+B", true);
    m_italic = addButton(FormatIcon::Italic, "Italic", "Ctrl+I", true);
    m_underline = addButton(FormatIcon::Underline, "Underline", "Ctrl+U", true);
    addSeparator();
    m_bullets = addButton(FormatIcon::Bullets, "Bulleted list", "", true);
    m_numbered = addButton(FormatIcon::Numbers, "Numbered list", "", true);
    connect(m_bold->defaultAction(), &QAction::triggered, this, [this](bool checked)
    {
        setFontWeight(checked ? QFont::Bold : QFont::Normal);
        setFocus();
    });
    connect(m_italic->defaultAction(), &QAction::triggered, this, [this](bool checked) { setFontItalic(checked); setFocus(); });
    connect(m_underline->defaultAction(), &QAction::triggered, this, [this](bool checked) { setFontUnderline(checked); setFocus(); });
    connect(m_bullets->defaultAction(), &QAction::triggered, this, [this] { toggleList(false); });
    connect(m_numbered->defaultAction(), &QAction::triggered, this, [this] { toggleList(true); });

    addSeparator();
    m_color = addButton(FormatIcon::Color, "Text color", "", false);
    m_color->setFixedWidth(40);
    auto* colors = new QMenu(m_color);
    colors->setObjectName("noteColors");
    for (const auto& color : QList<QPair<QString, QString>>{{"Default", ""}, {"Rose", "#f2a6b3"}, {"Gold", "#e8c57a"}, {"Mint", "#92d5b5"}, {"Sky", "#99c9f5"}, {"Lavender", "#c5b3ef"}})
    {
        const QColor swatch = color.second.isEmpty() ? palette().color(QPalette::Text) : QColor(color.second);
        auto* action = colors->addAction(colorSwatch(swatch), color.first, this, [this, value = color.second]
        {
            setTextColor(value.isEmpty() ? palette().color(QPalette::Text) : QColor(value));
            setFocus();
            updateToolbar();
        });
        action->setCheckable(true);
        action->setData(swatch);
    }
    m_color->setMenu(colors);
    m_color->setPopupMode(QToolButton::InstantPopup);
    auto* clear = addButton(FormatIcon::Clear, "Clear inline formatting", "", false);
    connect(clear->defaultAction(), &QAction::triggered, this, [this]
    {
        setCurrentCharFormat(QTextCharFormat{});
        setFocus();
    });
    updateToolbar();
    return toolbar;
}

void NoteEditor::loadNote(const QString& plainText, const QString& html)
{
    clear();
    setCurrentCharFormat(QTextCharFormat{});
    if (html.isEmpty())
    {
        setPlainText(plainText);
    }
    else
    {
        setHtml(html);
        // Older versions update only plain text. Never resurrect stale rich content.
        if (toPlainText() != plainText) setPlainText(plainText);
    }
    document()->clearUndoRedoStacks();
    document()->setModified(false);
    updateToolbar();
}

void NoteEditor::insertFromMimeData(const QMimeData* source)
{
    if (source->hasText()) insertPlainText(source->text());
}

QMimeData* NoteEditor::createMimeDataFromSelection() const
{
    // Prevent QTextEdit's drag export from bypassing the protected clipboard path.
    return new QMimeData;
}

void NoteEditor::applyHeading(int level)
{
    auto cursor = textCursor();
    cursor.beginEditBlock();
    QTextBlockFormat block;
    block.setHeadingLevel(level);
    block.setTopMargin(level ? 12 : 0);
    block.setBottomMargin(level ? 8 : 0);
    cursor.mergeBlockFormat(block);
    const int first = cursor.selectionStart();
    const int last = cursor.selectionEnd();
    QTextCursor paragraphs(document());
    paragraphs.setPosition(first);
    paragraphs.movePosition(QTextCursor::StartOfBlock);
    paragraphs.setPosition(last, QTextCursor::KeepAnchor);
    if (last > first && paragraphs.atBlockStart()) paragraphs.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
    paragraphs.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    QTextCharFormat format;
    format.setFontPointSize(level == 1 ? 22 : level == 2 ? 18 : level == 3 ? 15 : 11);
    format.setFontWeight(level ? QFont::Bold : QFont::Normal);
    paragraphs.mergeCharFormat(format);
    cursor.endEditBlock();
    setTextCursor(cursor);
    if (!cursor.hasSelection()) mergeCurrentCharFormat(format);
    setFocus();
    updateToolbar();
}

void NoteEditor::toggleList(bool numbered)
{
    auto cursor = textCursor();
    cursor.beginEditBlock();
    const auto style = numbered ? QTextListFormat::ListDecimal : QTextListFormat::ListDisc;
    if (cursor.currentList() && cursor.currentList()->format().style() == style)
    {
        auto block = document()->findBlock(cursor.selectionStart());
        const int end = cursor.selectionEnd();
        while (block.isValid() && (block.position() < end || block.position() == cursor.selectionStart()))
        {
            if (auto* list = block.textList()) list->remove(block);
            QTextCursor paragraph(block);
            auto format = block.blockFormat();
            format.setIndent(0);
            paragraph.setBlockFormat(format);
            block = block.next();
        }
    }
    else
    {
        QTextListFormat format;
        format.setStyle(style);
        format.setIndent(1);
        cursor.createList(format);
    }
    cursor.endEditBlock();
    setTextCursor(cursor);
    setFocus();
    updateToolbar();
}

void NoteEditor::updateToolbar()
{
    if (!m_paragraph) return;
    const QSignalBlocker block(m_paragraph);
    m_paragraph->setCurrentIndex(qBound(0, textCursor().blockFormat().headingLevel(), 3));
    m_bold->defaultAction()->setChecked(fontWeight() >= QFont::Bold);
    m_italic->defaultAction()->setChecked(fontItalic());
    m_underline->defaultAction()->setChecked(fontUnderline());
    const auto* list = textCursor().currentList();
    m_bullets->defaultAction()->setChecked(list && list->format().style() == QTextListFormat::ListDisc);
    m_numbered->defaultAction()->setChecked(list && list->format().style() == QTextListFormat::ListDecimal);
    const auto color = currentCharFormat().foreground().style() == Qt::NoBrush ? palette().color(QPalette::Text) : textColor();
    m_color->defaultAction()->setIcon(formatIcon(FormatIcon::Color, color));
    for (auto* action : m_color->menu()->actions()) action->setChecked(action->data().value<QColor>() == color);
}

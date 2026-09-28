#pragma once

#include <QTextEdit>

class QComboBox;
class QToolButton;

class NoteEditor : public QTextEdit
{
    Q_OBJECT
public:
    explicit NoteEditor(QWidget* parent = nullptr);
    QWidget* createToolbar(QWidget* parent);
    void loadNote(const QString& plainText, const QString& html);

protected:
    void insertFromMimeData(const QMimeData* source) override;
    QMimeData* createMimeDataFromSelection() const override;

private:
    void applyHeading(int level);
    void toggleList(bool numbered);
    void updateToolbar();
    QComboBox* m_paragraph = nullptr;
    QToolButton* m_bold = nullptr;
    QToolButton* m_italic = nullptr;
    QToolButton* m_underline = nullptr;
    QToolButton* m_bullets = nullptr;
    QToolButton* m_numbered = nullptr;
    QToolButton* m_color = nullptr;
};

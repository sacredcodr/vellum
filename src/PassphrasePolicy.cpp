#include "PassphrasePolicy.h"
#include <QRegularExpression>
#include <QSet>

namespace
{
bool isRepeated(const QString& value)
{
    for (qsizetype unitLength = 1; unitLength * 3 <= value.size(); ++unitLength)
    {
        if (value.size() % unitLength != 0)
        {
            continue;
        }
        const auto unit = value.first(unitLength);
        bool matches = true;
        for (qsizetype offset = unitLength; offset < value.size(); offset += unitLength)
        {
            if (value.sliced(offset, unitLength) != unit)
            {
                matches = false;
                break;
            }
        }
        if (matches)
        {
            return true;
        }
    }
    return false;
}

bool hasSequence(const QString& value)
{
    const auto lower = value.toLower();
    for (qsizetype start = 0; start + 5 <= lower.size(); ++start)
    {
        bool ascending = true;
        bool descending = true;
        for (qsizetype index = start + 1; index < start + 5; ++index)
        {
            ascending = ascending && lower[index].unicode() == lower[index - 1].unicode() + 1;
            descending = descending && lower[index].unicode() == lower[index - 1].unicode() - 1;
        }
        if (ascending || descending)
        {
            return true;
        }
    }
    return false;
}
}

QString PassphrasePolicy::error(const QString& passphrase)
{
    const auto normalized = passphrase.normalized(QString::NormalizationForm_KC);
    if (normalized != normalized.trimmed())
    {
        return "Remove spaces from the beginning or end.";
    }
    if (normalized.toUtf8().size() > 1024)
    {
        return "Use no more than 1,024 UTF-8 bytes.";
    }

    auto compact = normalized.toLower();
    compact.remove(QRegularExpression("[^a-z0-9]"));
    for (const auto* weak : {"password", "passphrase", "letmein", "qwerty", "123456", "abcdef", "vellum"})
    {
        if (compact.contains(QLatin1String(weak)))
        {
            return "Avoid names, common passwords, and keyboard sequences.";
        }
    }
    if (isRepeated(compact) || hasSequence(compact))
    {
        return "Avoid repeated or sequential patterns.";
    }

    const auto words = normalized.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QSet<QString> uniqueWords;
    bool wordsAreLongEnough = true;
    for (const auto& word : words)
    {
        uniqueWords.insert(word.toCaseFolded());
        wordsAreLongEnough = wordsAreLongEnough && word.size() >= 3;
    }
    if (normalized.size() >= 24 && words.size() >= 6 && uniqueWords.size() >= 5 && wordsAreLongEnough)
    {
        return {};
    }

    bool lower = false;
    bool upper = false;
    bool digit = false;
    bool symbol = false;
    QSet<QChar> uniqueCharacters;
    for (const auto character : normalized)
    {
        lower = lower || character.isLower();
        upper = upper || character.isUpper();
        digit = digit || character.isDigit();
        symbol = symbol || (!character.isLetterOrNumber() && !character.isSpace());
        uniqueCharacters.insert(character.toCaseFolded());
    }
    if (normalized.size() >= 24 && lower && upper && digit && symbol && uniqueCharacters.size() >= 12)
    {
        return {};
    }
    return "Use six or more unrelated words, or at least 24 mixed characters. The generator is recommended.";
}

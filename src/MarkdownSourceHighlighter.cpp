#include "MarkdownSourceHighlighter.h"

#include <QFont>
#include <QRegularExpression>
#include <QTextDocument>

namespace {

QTextCharFormat makeFormat(const QColor &color)
{
    QTextCharFormat format;
    format.setForeground(color);
    return format;
}

} // namespace

MarkdownSourceHighlighter::MarkdownSourceHighlighter(QTextDocument *document,
                                                     bool dark,
                                                     QObject *parent)
    : QSyntaxHighlighter(document)
    , m_dark(dark)
{
    if (parent) {
        setParent(parent);
    }
    rebuildFormats();
}

void MarkdownSourceHighlighter::setDark(bool dark)
{
    if (m_dark == dark) {
        return;
    }
    m_dark = dark;
    rebuildFormats();
    rehighlight();
}

void MarkdownSourceHighlighter::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    rehighlight();
}

bool MarkdownSourceHighlighter::isEnabled() const
{
    return m_enabled;
}

void MarkdownSourceHighlighter::rebuildFormats()
{
    if (m_dark) {
        m_heading = makeFormat(QColor(QStringLiteral("#8ab4f8")));
        m_heading.setFontWeight(QFont::Bold);
        m_bold = makeFormat(QColor(QStringLiteral("#e6e6e6")));
        m_bold.setFontWeight(QFont::Bold);
        m_emphasis = makeFormat(QColor(QStringLiteral("#e6e6e6")));
        m_emphasis.setFontItalic(true);
        m_inlineCode = makeFormat(QColor(QStringLiteral("#d7ba7d")));
        m_link = makeFormat(QColor(QStringLiteral("#8ab4f8")));
        m_quote = makeFormat(QColor(QStringLiteral("#9a9a9a")));
        m_marker = makeFormat(QColor(QStringLiteral("#c586c0")));
        m_fence = makeFormat(QColor(QStringLiteral("#6aab73")));
        m_math = makeFormat(QColor(QStringLiteral("#b5a642")));
    } else {
        m_heading = makeFormat(QColor(QStringLiteral("#0b57d0")));
        m_heading.setFontWeight(QFont::Bold);
        m_bold = makeFormat(QColor(QStringLiteral("#111111")));
        m_bold.setFontWeight(QFont::Bold);
        m_emphasis = makeFormat(QColor(QStringLiteral("#111111")));
        m_emphasis.setFontItalic(true);
        m_inlineCode = makeFormat(QColor(QStringLiteral("#9e880d")));
        m_link = makeFormat(QColor(QStringLiteral("#0b57d0")));
        m_quote = makeFormat(QColor(QStringLiteral("#6a6a6a")));
        m_marker = makeFormat(QColor(QStringLiteral("#7a3e9d")));
        m_fence = makeFormat(QColor(QStringLiteral("#067d17")));
        m_math = makeFormat(QColor(QStringLiteral("#9e880d")));
    }
}

void MarkdownSourceHighlighter::applyRegex(const QString &text,
                                           const QRegularExpression &expression,
                                           const QTextCharFormat &format)
{
    auto it = expression.globalMatch(text);
    while (it.hasNext()) {
        const auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), format);
    }
}

void MarkdownSourceHighlighter::highlightBlock(const QString &text)
{
    if (!m_enabled) {
        return;
    }

    static const QRegularExpression heading(
        QStringLiteral("^\\s{0,3}#{1,6}(?:\\s+.*)?$"));
    static const QRegularExpression fence(
        QStringLiteral("^\\s{0,3}(?:`{3,}|~{3,}).*$"));
    static const QRegularExpression quote(QStringLiteral("^\\s{0,3}>.*$"));
    static const QRegularExpression listMarker(
        QStringLiteral("^\\s*(?:[-+*]|\\d+[.)])\\s+"));
    static const QRegularExpression bold(
        QStringLiteral("\\*\\*[^*]+\\*\\*|__[^_]+__"));
    static const QRegularExpression emphasis(
        QStringLiteral("(?<!\\*)\\*[^*]+\\*(?!\\*)|(?<!_)_[^_]+_(?!_)"));
    static const QRegularExpression link(
        QStringLiteral("\\[[^\\]]+\\]\\([^)]+\\)"));
    static const QRegularExpression inlineCode(QStringLiteral("`[^`]+`"));
    static const QRegularExpression math(
        QStringLiteral("\\$\\$[^$]+\\$\\$|\\$[^$]+\\$"));

    applyRegex(text, heading, m_heading);
    applyRegex(text, fence, m_fence);
    applyRegex(text, quote, m_quote);
    applyRegex(text, listMarker, m_marker);
    applyRegex(text, bold, m_bold);
    applyRegex(text, emphasis, m_emphasis);
    applyRegex(text, link, m_link);
    applyRegex(text, math, m_math);
    applyRegex(text, inlineCode, m_inlineCode);
}

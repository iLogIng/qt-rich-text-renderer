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
    , m_palette(defaultMarkdownSourcePalette(dark))
{
    if (parent) {
        setParent(parent);
    }
    rebuildFormats();
}

void MarkdownSourceHighlighter::setDark(bool dark)
{
    m_palette = defaultMarkdownSourcePalette(dark);
    rebuildFormats();
    rehighlight();
}

void MarkdownSourceHighlighter::setSyntaxPalette(const MarkdownSourcePalette &palette)
{
    m_palette = palette;
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

void MarkdownSourceHighlighter::rebuildFormats()
{
    m_heading = makeFormat(m_palette.heading);
    m_heading.setFontWeight(QFont::Bold);
    m_bold = makeFormat(m_palette.bold);
    m_bold.setFontWeight(QFont::Bold);
    m_emphasis = makeFormat(m_palette.emphasis);
    m_emphasis.setFontItalic(true);
    m_inlineCode = makeFormat(m_palette.inlineCode);
    m_link = makeFormat(m_palette.link);
    m_quote = makeFormat(m_palette.quote);
    m_marker = makeFormat(m_palette.marker);
    m_fence = makeFormat(m_palette.fence);
    m_math = makeFormat(m_palette.math);
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

    if (previousBlockState() == 1) {
        setFormat(0, text.size(), m_inlineCode);
        setCurrentBlockState(fence.match(text).hasMatch() ? 0 : 1);
        return;
    }

    if (fence.match(text).hasMatch()) {
        setFormat(0, text.size(), m_fence);
        setCurrentBlockState(1);
        return;
    }
    setCurrentBlockState(0);
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
    applyRegex(text, quote, m_quote);
    applyRegex(text, listMarker, m_marker);
    applyRegex(text, bold, m_bold);
    applyRegex(text, emphasis, m_emphasis);
    applyRegex(text, link, m_link);
    applyRegex(text, math, m_math);
    applyRegex(text, inlineCode, m_inlineCode);
}

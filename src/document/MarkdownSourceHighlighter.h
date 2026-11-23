#pragma once

#include "SyntaxPalette.h"

#include <QTextCharFormat>
#include <QSyntaxHighlighter>

class QRegularExpression;

class MarkdownSourceHighlighter : public QSyntaxHighlighter {
public:
    explicit MarkdownSourceHighlighter(QTextDocument *document,
                                       bool dark = false,
                                       QObject *parent = nullptr);

    void setDark(bool dark);
    void setSyntaxPalette(const MarkdownSourcePalette &palette);
    void setEnabled(bool enabled);

protected:
    void highlightBlock(const QString &text) override;

private:
    void rebuildFormats();
    void applyRegex(const QString &text,
                    const QRegularExpression &expression,
                    const QTextCharFormat &format);

    bool m_enabled = true;
    MarkdownSourcePalette m_palette;
    QTextCharFormat m_heading;
    QTextCharFormat m_bold;
    QTextCharFormat m_emphasis;
    QTextCharFormat m_inlineCode;
    QTextCharFormat m_link;
    QTextCharFormat m_quote;
    QTextCharFormat m_marker;
    QTextCharFormat m_fence;
    QTextCharFormat m_math;
};

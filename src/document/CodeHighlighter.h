#pragma once

#include <QRegularExpression>
#include <QTextCharFormat>
#include <QSyntaxHighlighter>

class CodeHighlighter : public QSyntaxHighlighter {
public:
    explicit CodeHighlighter(QTextDocument *document, bool dark = false, QObject *parent = nullptr);

    void setDark(bool dark);

protected:
    void highlightBlock(const QString &text) override;

private:
    void rebuildFormats();
    void highlightCpp(const QString &text);
    void highlightPython(const QString &text);
    void highlightJson(const QString &text);
    void highlightJavaScript(const QString &text);
    void highlightBash(const QString &text);
    void highlightLatex(const QString &text);
    void applyRegex(const QString &text, const QRegularExpression &expression, const QTextCharFormat &format);

    bool m_dark = false;
    QTextCharFormat m_keyword;
    QTextCharFormat m_string;
    QTextCharFormat m_number;
    QTextCharFormat m_comment;
    QTextCharFormat m_preprocessor;
    QTextCharFormat m_type;
    QTextCharFormat m_function;
};

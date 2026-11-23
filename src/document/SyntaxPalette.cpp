#include "SyntaxPalette.h"

CodeSyntaxPalette defaultCodeSyntaxPalette(bool dark)
{
    CodeSyntaxPalette palette;
    if (dark) {
        palette.keyword = QColor(QStringLiteral("#cf8e6d"));
        palette.string = QColor(QStringLiteral("#6aab73"));
        palette.number = QColor(QStringLiteral("#2aacb8"));
        palette.comment = QColor(QStringLiteral("#7a7a7a"));
        palette.preprocessor = QColor(QStringLiteral("#b5a642"));
        palette.type = QColor(QStringLiteral("#56a8f5"));
        palette.function = QColor(QStringLiteral("#56a8f5"));
    } else {
        palette.keyword = QColor(QStringLiteral("#0033b3"));
        palette.string = QColor(QStringLiteral("#067d17"));
        palette.number = QColor(QStringLiteral("#1750eb"));
        palette.comment = QColor(QStringLiteral("#8c8c8c"));
        palette.preprocessor = QColor(QStringLiteral("#9e880d"));
        palette.type = QColor(QStringLiteral("#00627a"));
        palette.function = QColor(QStringLiteral("#00627a"));
    }
    return palette;
}

MarkdownSourcePalette defaultMarkdownSourcePalette(bool dark)
{
    MarkdownSourcePalette palette;
    if (dark) {
        palette.heading = QColor(QStringLiteral("#8ab4f8"));
        palette.bold = QColor(QStringLiteral("#e6e6e6"));
        palette.emphasis = QColor(QStringLiteral("#e6e6e6"));
        palette.inlineCode = QColor(QStringLiteral("#d7ba7d"));
        palette.link = QColor(QStringLiteral("#8ab4f8"));
        palette.quote = QColor(QStringLiteral("#9a9a9a"));
        palette.marker = QColor(QStringLiteral("#c586c0"));
        palette.fence = QColor(QStringLiteral("#6aab73"));
        palette.math = QColor(QStringLiteral("#b5a642"));
    } else {
        palette.heading = QColor(QStringLiteral("#0b57d0"));
        palette.bold = QColor(QStringLiteral("#111111"));
        palette.emphasis = QColor(QStringLiteral("#111111"));
        palette.inlineCode = QColor(QStringLiteral("#9e880d"));
        palette.link = QColor(QStringLiteral("#0b57d0"));
        palette.quote = QColor(QStringLiteral("#6a6a6a"));
        palette.marker = QColor(QStringLiteral("#7a3e9d"));
        palette.fence = QColor(QStringLiteral("#067d17"));
        palette.math = QColor(QStringLiteral("#9e880d"));
    }
    return palette;
}

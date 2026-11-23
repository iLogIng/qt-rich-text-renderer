#pragma once

#include <QColor>

// Palette data lives in the document layer so highlighters do not depend on
// the UI theme model.
struct CodeSyntaxPalette {
    QColor keyword;
    QColor string;
    QColor number;
    QColor comment;
    QColor preprocessor;
    QColor type;
    QColor function;
};

struct MarkdownSourcePalette {
    QColor heading;
    QColor bold;
    QColor emphasis;
    QColor inlineCode;
    QColor link;
    QColor quote;
    QColor marker;
    QColor fence;
    QColor math;
};

CodeSyntaxPalette defaultCodeSyntaxPalette(bool dark);
MarkdownSourcePalette defaultMarkdownSourcePalette(bool dark);

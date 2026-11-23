#pragma once

#include <QColor>

class QTextDocument;

struct PreviewStyle {
    int maxTextWidth = 0;
    QColor codeBackground;
    int codeBlockMargin = 0;
};

// Applies code-block background, outer block margins and an optional code font
// family while preserving the code language property and syntax colors.
void applyPreviewStyle(QTextDocument *document,
                       const PreviewStyle &style,
                       const QString &codeFontFamily = {});

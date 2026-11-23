#include "PreviewStyle.h"

#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>

namespace {

bool isCodeBlock(const QTextBlock &block)
{
    return block.isValid()
        && !block.blockFormat()
                .stringProperty(QTextFormat::BlockCodeLanguage)
                .trimmed()
                .isEmpty();
}

} // namespace

void applyPreviewStyle(QTextDocument *document,
                       const PreviewStyle &style,
                       const QString &codeFontFamily)
{
    if (!document) {
        return;
    }
    if (!style.codeBackground.isValid() && style.codeBlockMargin <= 0
        && codeFontFamily.isEmpty()) {
        return;
    }

    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        if (!isCodeBlock(block)) {
            continue;
        }

        QTextBlockFormat format = block.blockFormat();
        if (style.codeBackground.isValid()) {
            format.setBackground(style.codeBackground);
        }
        if (style.codeBlockMargin > 0) {
            if (!isCodeBlock(block.previous())) {
                format.setTopMargin(style.codeBlockMargin);
            }
            if (!isCodeBlock(block.next())) {
                format.setBottomMargin(style.codeBlockMargin);
            }
        }

        if (format != block.blockFormat()) {
            QTextCursor cursor(block);
            cursor.setBlockFormat(format);
        }

        if (!codeFontFamily.isEmpty()) {
            QTextCursor cursor(block);
            cursor.select(QTextCursor::BlockUnderCursor);
            QTextCharFormat characterFormat;
            characterFormat.setFontFamilies({codeFontFamily});
            cursor.mergeCharFormat(characterFormat);
        }
    }
}

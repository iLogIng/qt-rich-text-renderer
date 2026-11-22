#include "MarkdownDocumentBuilder.h"

#include "SvgObjectHandler.h"
#include "SvgSanitizer.h"

#include <QSvgRenderer>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>

namespace {

QString replaceTokenWithError(QTextDocument *document, const QString &token, const QString &error)
{
    QTextCursor cursor = document->find(token);
    if (cursor.isNull()) {
        return QStringLiteral("token not found: %1").arg(token);
    }
    if (!cursor.hasSelection()) {
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, token.size());
    }
    cursor.insertText(QStringLiteral("[render error: %1]").arg(error));
    return {};
}

} // namespace

bool MarkdownDocumentBuilder::embedSvg(QTextDocument *document,
                                       const QString &token,
                                       const QString &svg,
                                       const QString &error,
                                       bool mermaid,
                                       bool dark,
                                       const QColor &foreground,
                                       int maxWidth,
                                       QString *errorOut)
{
    if (!error.isEmpty() || svg.isEmpty()) {
        const QString message = error.isEmpty() ? QStringLiteral("empty SVG") : error;
        const QString failure = replaceTokenWithError(document, token, message);
        if (errorOut) {
            *errorOut = failure.isEmpty() ? message : failure;
        }
        return false;
    }

    const QString safeSvg = sanitizeSvgForQt(svg, mermaid, foreground, dark);
    QSvgRenderer renderer(safeSvg.toUtf8());
    if (!renderer.isValid()) {
        const QString message = QStringLiteral("QSvgRenderer rejected SVG");
        const QString failure = replaceTokenWithError(document, token, message);
        if (errorOut) {
            *errorOut = failure.isEmpty() ? message : failure;
        }
        return false;
    }

    QSizeF size = renderer.defaultSize();
    if (size.isEmpty()) {
        size = renderer.viewBoxF().size();
    }
    if (size.isEmpty()) {
        size = QSizeF(400, 200);
    }
    if (maxWidth > 0 && size.width() > maxWidth) {
        size.setHeight(size.height() * maxWidth / size.width());
        size.setWidth(maxWidth);
    }

    QTextCursor cursor = document->find(token);
    if (cursor.isNull()) {
        if (errorOut) {
            *errorOut = QStringLiteral("token not found: %1").arg(token);
        }
        return false;
    }
    if (!cursor.hasSelection()) {
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, token.size());
    }

    QTextCharFormat objectFormat;
    objectFormat.setObjectType(SvgObjectHandler::Type);
    objectFormat.setProperty(SvgObjectHandler::SvgProperty, safeSvg);
    objectFormat.setProperty(SvgObjectHandler::WidthProperty, size.width());
    objectFormat.setProperty(SvgObjectHandler::HeightProperty, size.height());

    cursor.insertText(QString(QChar::ObjectReplacementCharacter), objectFormat);
    return true;
}

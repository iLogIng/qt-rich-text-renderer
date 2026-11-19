#include "MarkdownRenderer.h"

#include "SvgObjectHandler.h"
#include "SvgSanitizer.h"

#include <QRegularExpression>
#include <QSvgRenderer>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QUuid>

namespace {

QString makeToken(const QString &prefix, int index)
{
    return QStringLiteral("LOCVASBLOCK%1%2TOKEN").arg(prefix).arg(index);
}

QString replaceInlineMath(QString line, const QString &prefix, QVector<RenderBlock> &blocks)
{
    int searchFrom = 0;
    while (true) {
        const int start = line.indexOf(QStringLiteral("\\("), searchFrom);
        if (start < 0) {
            break;
        }
        const int end = line.indexOf(QStringLiteral("\\)"), start + 2);
        if (end < 0) {
            break;
        }
        const QString source = line.mid(start + 2, end - start - 2);
        RenderBlock block;
        block.kind = RenderBlock::Kind::Math;
        block.source = source;
        block.display = false;
        block.token = makeToken(prefix, blocks.size());
        blocks.push_back(block);
        line.replace(start, end - start + 2, block.token);
        searchFrom = start + block.token.size();
    }
    return line;
}

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

PreparedMarkdown MarkdownRenderer::prepare(const QString &source)
{
    PreparedMarkdown prepared;
    const QString prefix = QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-')).left(8);
    const QStringList lines = source.split(QLatin1Char('\n'));

    bool inFence = false;
    QString fenceMarker;

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        const QString trimmed = line.trimmed();

        if (!inFence && (trimmed.startsWith(QStringLiteral("```")) ||
                         trimmed.startsWith(QStringLiteral("~~~")))) {
            fenceMarker = trimmed.left(3);
            const QString info = trimmed.mid(3).trimmed();
            if (info.startsWith(QStringLiteral("mermaid"), Qt::CaseInsensitive)) {
                QString content;
                ++i;
                while (i < lines.size() && !lines.at(i).trimmed().startsWith(fenceMarker)) {
                    content += lines.at(i);
                    content += QLatin1Char('\n');
                    ++i;
                }
                RenderBlock block;
                block.kind = RenderBlock::Kind::Mermaid;
                block.source = content;
                block.display = true;
                block.token = makeToken(prefix, prepared.blocks.size());
                prepared.blocks.push_back(block);
                prepared.markdown += QLatin1Char('\n') + block.token + QLatin1Char('\n');
                continue;
            }
            inFence = true;
            prepared.markdown += line + QLatin1Char('\n');
            continue;
        }

        if (inFence) {
            prepared.markdown += line + QLatin1Char('\n');
            if (trimmed.startsWith(fenceMarker)) {
                inFence = false;
            }
            continue;
        }

        if (trimmed.startsWith(QStringLiteral("$$"))) {
            QString content;
            if (trimmed.size() > 4 && trimmed.endsWith(QStringLiteral("$$"))) {
                content = trimmed.mid(2, trimmed.size() - 4);
            } else {
                content = trimmed.mid(2);
                if (!content.isEmpty()) {
                    content += QLatin1Char('\n');
                }
                ++i;
                while (i < lines.size() && !lines.at(i).trimmed().endsWith(QStringLiteral("$$"))) {
                    content += lines.at(i);
                    content += QLatin1Char('\n');
                    ++i;
                }
                if (i < lines.size()) {
                    const QString closing = lines.at(i).trimmed();
                    content += closing.left(closing.size() - 2);
                }
            }
            RenderBlock block;
            block.kind = RenderBlock::Kind::Math;
            block.source = content;
            block.display = true;
            block.token = makeToken(prefix, prepared.blocks.size());
            prepared.blocks.push_back(block);
            prepared.markdown += QLatin1Char('\n') + block.token + QLatin1Char('\n');
            continue;
        }

        prepared.markdown += replaceInlineMath(line, prefix, prepared.blocks) + QLatin1Char('\n');
    }

    return prepared;
}

bool MarkdownRenderer::embedSvg(QTextDocument *document,
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

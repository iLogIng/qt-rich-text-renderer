#include "MarkdownPreprocessor.h"

#include <QUuid>

namespace {

QString makeToken(const QString &prefix, int index)
{
    return QStringLiteral("LOCVASBLOCK%1%2TOKEN").arg(prefix).arg(index);
}

QString addMathBlock(PreparedMarkdown &prepared,
                     const QString &prefix,
                     const QString &source,
                     bool display)
{
    RenderBlock block;
    block.kind = RenderBlock::Kind::Math;
    block.source = source;
    block.display = display;
    block.token = makeToken(prefix, prepared.blocks.size());
    prepared.blocks.push_back(block);
    return block.token;
}

bool parseFenceStart(const QString &line, QString *marker, QString *info)
{
    const QString trimmed = line.trimmed();
    if (trimmed.size() < 3) {
        return false;
    }
    const QChar fenceChar = trimmed.at(0);
    if (fenceChar != QLatin1Char('`') && fenceChar != QLatin1Char('~')) {
        return false;
    }
    int length = 0;
    while (length < trimmed.size() && trimmed.at(length) == fenceChar) {
        ++length;
    }
    if (length < 3) {
        return false;
    }
    *marker = QString(length, fenceChar);
    *info = trimmed.mid(length).trimmed();
    return true;
}

bool isFenceClose(const QString &line, const QString &marker)
{
    const QString trimmed = line.trimmed();
    if (trimmed.size() < marker.size()) {
        return false;
    }
    if (!trimmed.startsWith(marker)) {
        return false;
    }
    return trimmed.mid(marker.size()).trimmed().isEmpty();
}

bool validInlineMath(const QString &source)
{
    return !source.isEmpty()
           && !source.front().isSpace()
           && !source.back().isSpace();
}

QString processInline(const QString &line,
                      PreparedMarkdown &prepared,
                      const QString &prefix)
{
    QString out;
    out.reserve(line.size());

    int i = 0;
    while (i < line.size()) {
        const QChar c = line.at(i);

        // Inline code spans: keep literal and never parse math inside.
        if (c == QLatin1Char('`')) {
            int run = 1;
            while (i + run < line.size() && line.at(i + run) == QLatin1Char('`')) {
                ++run;
            }
            const QString ticks = line.mid(i, run);
            const int end = line.indexOf(ticks, i + run);
            if (end >= 0) {
                out += line.mid(i, end - i + run);
                i = end + run;
                continue;
            }
            out += c;
            ++i;
            continue;
        }

        // Escaped dollar: literal.
        if (c == QLatin1Char('\\') && i + 1 < line.size()
            && line.at(i + 1) == QLatin1Char('$')) {
            out += QLatin1Char('\\');
            out += QLatin1Char('$');
            i += 2;
            continue;
        }

        // \( ... \)
        if (c == QLatin1Char('\\') && i + 1 < line.size()
            && line.at(i + 1) == QLatin1Char('(')) {
            const int end = line.indexOf(QStringLiteral("\\)"), i + 2);
            if (end > i + 2) {
                const QString source = line.mid(i + 2, end - i - 2);
                if (validInlineMath(source)) {
                    out += addMathBlock(prepared, prefix, source, false);
                    i = end + 2;
                    continue;
                }
            }
            out += c;
            ++i;
            continue;
        }

        // \[ ... \]
        if (c == QLatin1Char('\\') && i + 1 < line.size()
            && line.at(i + 1) == QLatin1Char('[')) {
            const int end = line.indexOf(QStringLiteral("\\]"), i + 2);
            if (end > i + 2) {
                const QString source = line.mid(i + 2, end - i - 2);
                if (validInlineMath(source)) {
                    out += addMathBlock(prepared, prefix, source, true);
                    i = end + 2;
                    continue;
                }
            }
            out += c;
            ++i;
            continue;
        }

        if (c == QLatin1Char('$')) {
            // $$ ... $$ inline display math.
            if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('$')) {
                const int end = line.indexOf(QStringLiteral("$$"), i + 2);
                if (end > i + 2) {
                    const QString source = line.mid(i + 2, end - i - 2);
                    if (validInlineMath(source)) {
                        out += addMathBlock(prepared, prefix, source, true);
                        i = end + 2;
                        continue;
                    }
                }
                out += QLatin1Char('$');
                out += QLatin1Char('$');
                i += 2;
                continue;
            }

            // $ ... $ inline math. Keep the heuristic conservative so "$5" and
            // "$10" are not treated as formulas.
            const int end = line.indexOf(QLatin1Char('$'), i + 1);
            if (end > i + 1) {
                const QString source = line.mid(i + 1, end - i - 1);
                const bool leftBoundary = i == 0
                                          || line.at(i - 1).isSpace()
                                          || QStringLiteral("([{<,;:").contains(line.at(i - 1));
                const bool rightBoundary = end + 1 >= line.size()
                                           || line.at(end + 1).isSpace()
                                           || QStringLiteral(")]}>,.;:!?").contains(line.at(end + 1));
                if (leftBoundary && rightBoundary && validInlineMath(source)) {
                    out += addMathBlock(prepared, prefix, source, false);
                    i = end + 1;
                    continue;
                }
            }
        }

        out += c;
        ++i;
    }
    return out;
}

} // namespace

PreparedMarkdown MarkdownPreprocessor::prepare(const QString &source)
{
    PreparedMarkdown prepared;
    const QString prefix = QUuid::createUuid().toString(QUuid::WithoutBraces)
                               .remove(QLatin1Char('-'))
                               .left(8);
    const QStringList lines = source.split(QLatin1Char('\n'));

    bool inFence = false;
    QString fenceMarker;

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        const QString trimmed = line.trimmed();

        if (!inFence) {
            QString marker;
            QString info;
            if (parseFenceStart(line, &marker, &info)) {
                const QString language = info.section(QLatin1Char(' '), 0, 0).toLower();
                if (language == QStringLiteral("mermaid")) {
                    QString content;
                    ++i;
                    while (i < lines.size() && !isFenceClose(lines.at(i), marker)) {
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
                fenceMarker = marker;
                prepared.markdown += line + QLatin1Char('\n');
                continue;
            }

            // Multi-line $$ ... $$ display math.
            if (trimmed.startsWith(QStringLiteral("$$"))
                && trimmed.indexOf(QStringLiteral("$$"), 2) < 0) {
                QString content = trimmed.mid(2);
                ++i;
                while (i < lines.size()
                       && lines.at(i).trimmed().indexOf(QStringLiteral("$$")) < 0) {
                    content += QLatin1Char('\n') + lines.at(i);
                    ++i;
                }
                if (i < lines.size()) {
                    const QString closing = lines.at(i).trimmed();
                    const int pos = closing.indexOf(QStringLiteral("$$"));
                    content += QLatin1Char('\n') + closing.left(pos);
                }
                const QString token = addMathBlock(prepared, prefix, content.trimmed(), true);
                prepared.markdown += QLatin1Char('\n') + token + QLatin1Char('\n');
                continue;
            }

            // Multi-line \[ ... \] display math.
            if (trimmed.startsWith(QStringLiteral("\\["))
                && trimmed.indexOf(QStringLiteral("\\]"), 2) < 0) {
                QString content = trimmed.mid(2);
                ++i;
                while (i < lines.size()
                       && lines.at(i).trimmed().indexOf(QStringLiteral("\\]")) < 0) {
                    content += QLatin1Char('\n') + lines.at(i);
                    ++i;
                }
                if (i < lines.size()) {
                    const QString closing = lines.at(i).trimmed();
                    const int pos = closing.indexOf(QStringLiteral("\\]"));
                    content += QLatin1Char('\n') + closing.left(pos);
                }
                const QString token = addMathBlock(prepared, prefix, content.trimmed(), true);
                prepared.markdown += QLatin1Char('\n') + token + QLatin1Char('\n');
                continue;
            }

            prepared.markdown += processInline(line, prepared, prefix) + QLatin1Char('\n');
            continue;
        }

        prepared.markdown += line + QLatin1Char('\n');
        if (isFenceClose(line, fenceMarker)) {
            inFence = false;
        }
    }

    return prepared;
}

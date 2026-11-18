#include "SvgSanitizer.h"

#include <QMap>
#include <QRegularExpression>
#include <QStringList>

namespace {

int findMatchingParen(const QString &text, int openPos)
{
    int depth = 0;
    for (int i = openPos; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('(')) {
            ++depth;
        } else if (c == QLatin1Char(')')) {
            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }
    return -1;
}

int firstTopLevelComma(const QString &text)
{
    int depth = 0;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('(')) {
            ++depth;
        } else if (c == QLatin1Char(')')) {
            --depth;
        } else if (c == QLatin1Char(',') && depth == 0) {
            return i;
        }
    }
    return -1;
}

QString cleanFallback(QString fallback)
{
    fallback = fallback.trimmed();
    if (fallback.startsWith(QStringLiteral("color(from"))) {
        return QStringLiteral("#333333");
    }
    return fallback;
}

QString resolveVars(QString text, const QMap<QString, QString> &variables)
{
    for (int pass = 0; pass < 12; ++pass) {
        bool changed = false;
        int pos = 0;
        while ((pos = text.indexOf(QStringLiteral("var("), pos)) >= 0) {
            const int open = pos + 3;
            const int close = findMatchingParen(text, open);
            if (close < 0) {
                break;
            }
            const QString expression = text.mid(open + 1, close - open - 1);
            const int comma = firstTopLevelComma(expression);
            const QString name = (comma < 0 ? expression : expression.left(comma)).trimmed();
            QString value = variables.value(name);
            if (value.isEmpty()) {
                value = comma < 0 ? QString() : cleanFallback(expression.mid(comma + 1));
            }
            if (value.isEmpty()) {
                value = QStringLiteral("inherit");
            }
            text.replace(pos, close - pos + 1, value);
            pos += value.size();
            changed = true;
        }
        if (!changed) {
            break;
        }
    }
    return text;
}

QMap<QString, QString> parseRootVariables(const QString &svg)
{
    QMap<QString, QString> variables;
    const QRegularExpression svgTag(QStringLiteral("<svg\\b[^>]*>"),
                                    QRegularExpression::CaseInsensitiveOption);
    const auto match = svgTag.match(svg);
    if (!match.hasMatch()) {
        return variables;
    }
    const QString tag = match.captured(0);
    const QRegularExpression styleAttr(QStringLiteral("\\bstyle=\"([^\"]*)\""),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto styleMatch = styleAttr.match(tag);
    if (!styleMatch.hasMatch()) {
        return variables;
    }
    const QStringList declarations = styleMatch.captured(1).split(QLatin1Char(';'),
                                                                 Qt::SkipEmptyParts);
    for (const QString &declaration : declarations) {
        const int colon = declaration.indexOf(QLatin1Char(':'));
        if (colon <= 0) {
            continue;
        }
        const QString name = declaration.left(colon).trimmed();
        if (name.startsWith(QStringLiteral("--"))) {
            variables.insert(name, declaration.mid(colon + 1).trimmed());
        }
    }
    return variables;
}

QString stripStyleAttributes(QString svg)
{
    static const QRegularExpression styleAttr(QStringLiteral("\\s+style=\"[^\"]*\""),
                                              QRegularExpression::CaseInsensitiveOption);
    svg.remove(styleAttr);
    return svg;
}

QString stripDangerousContent(QString svg)
{
    static const QRegularExpression script(
        QStringLiteral("<script\\b[^>]*>[\\s\\S]*?</script>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression eventAttr(
        QStringLiteral("\\s+on[a-zA-Z]+=\"[^\"]*\""),
        QRegularExpression::CaseInsensitiveOption);
    svg.remove(script);
    svg.remove(eventAttr);
    return svg;
}

QString normalizeRootSize(QString svg)
{
    const QRegularExpression svgTag(QStringLiteral("<svg\\b[^>]*>"),
                                    QRegularExpression::CaseInsensitiveOption);
    const auto match = svgTag.match(svg);
    if (!match.hasMatch()) {
        return svg;
    }
    QString tag = match.captured(0);
    const QString originalTag = tag;

    const QRegularExpression viewBoxRe(QStringLiteral("\\bviewBox=\"([^\"]+)\""),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto viewBoxMatch = viewBoxRe.match(tag);
    double viewWidth = 0;
    double viewHeight = 0;
    if (viewBoxMatch.hasMatch()) {
        const QStringList parts = viewBoxMatch.captured(1).split(
            QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() == 4) {
            viewWidth = parts.at(2).toDouble();
            viewHeight = parts.at(3).toDouble();
        }
    }

    auto replaceDimension = [&](const QString &name, double viewValue) {
        const QRegularExpression re(QStringLiteral("\\b%1=\"([^\"]*)\"").arg(name),
                                    QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(tag);
        QString value;
        if (m.hasMatch()) {
            value = m.captured(1).trimmed();
        }
        double numeric = 0;
        double factor = 1.0;
        bool valid = false;
        if (value.endsWith(QStringLiteral("ex"))) {
            numeric = value.left(value.size() - 2).toDouble(&valid);
            factor = 8.0;
        } else if (value.endsWith(QStringLiteral("em"))) {
            numeric = value.left(value.size() - 2).toDouble(&valid);
            factor = 16.0;
        } else if (value.endsWith(QStringLiteral("px"))) {
            numeric = value.left(value.size() - 2).toDouble(&valid);
        } else if (value.endsWith(QLatin1Char('%')) || value.isEmpty()) {
            valid = false;
        } else {
            numeric = value.toDouble(&valid);
        }
        double result = valid ? numeric * factor : viewValue;
        if (result <= 0) {
            result = viewValue > 0 ? viewValue : 400.0;
        }
        const QString replacement = QStringLiteral("%1=\"%2\"").arg(name).arg(result, 0, 'f', 2);
        if (m.hasMatch()) {
            tag.replace(m.captured(0), replacement);
        } else {
            tag.insert(tag.size() - 1, QLatin1Char(' ') + replacement);
        }
    };

    replaceDimension(QStringLiteral("width"), viewWidth);
    replaceDimension(QStringLiteral("height"), viewHeight);

    if (tag != originalTag) {
        svg.replace(originalTag, tag);
    }
    return svg;
}

} // namespace

QString sanitizeSvgForQt(const QString &svg, bool mermaid, const QColor &foreground, bool dark)
{
    if (svg.isEmpty()) {
        return svg;
    }

    QString result = svg;
    if (mermaid) {
        QMap<QString, QString> variables = parseRootVariables(result);
        variables.insert(QStringLiteral("--_ff"), QStringLiteral("sans-serif"));
        variables.insert(QStringLiteral("--_fs"), QStringLiteral("16px"));
        variables.insert(QStringLiteral("--_sw"), QStringLiteral("1.5px"));
        if (dark) {
            variables.insert(QStringLiteral("--_mb"), QStringLiteral("#2d2d30"));
            variables.insert(QStringLiteral("--_p"), QStringLiteral("#2d2d30"));
            variables.insert(QStringLiteral("--_nb"), QStringLiteral("#8ab4f8"));
            variables.insert(QStringLiteral("--_nt"), QStringLiteral("#e6e6e6"));
            variables.insert(QStringLiteral("--_l"), QStringLiteral("#e6e6e6"));
            variables.insert(QStringLiteral("--_ah"), QStringLiteral("#e6e6e6"));
            variables.insert(QStringLiteral("--_el"), QStringLiteral("#3a3a3a"));
            variables.insert(QStringLiteral("--_tx"), QStringLiteral("#e6e6e6"));
        } else {
            variables.insert(QStringLiteral("--_mb"), QStringLiteral("#ececff"));
            variables.insert(QStringLiteral("--_p"), QStringLiteral("#ececff"));
            variables.insert(QStringLiteral("--_nb"), QStringLiteral("#9370db"));
            variables.insert(QStringLiteral("--_nt"), QStringLiteral("#333333"));
            variables.insert(QStringLiteral("--_l"), QStringLiteral("#333333"));
            variables.insert(QStringLiteral("--_ah"), QStringLiteral("#333333"));
            variables.insert(QStringLiteral("--_el"), QStringLiteral("#e8e8e8"));
            variables.insert(QStringLiteral("--_tx"), QStringLiteral("#333333"));
        }
        result = resolveVars(result, variables);
    }

    result.replace(QStringLiteral("currentColor"), foreground.name(QColor::HexRgb));
    result = stripStyleAttributes(result);
    result = stripDangerousContent(result);
    result = normalizeRootSize(result);
    return result;
}

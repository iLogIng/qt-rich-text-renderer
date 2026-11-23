#include "RenderTheme.h"

#include <QStringList>

QStringList mermaidThemeNames()
{
    return {QStringLiteral("default"),
            QStringLiteral("dark"),
            QStringLiteral("forest"),
            QStringLiteral("neutral"),
            QStringLiteral("base")};
}

QStringList mermaidThemeVariableNames()
{
    return {QStringLiteral("primaryColor"),
            QStringLiteral("lineColor"),
            QStringLiteral("textColor"),
            QStringLiteral("mainBkg"),
            QStringLiteral("nodeBorder"),
            QStringLiteral("nodeTextColor"),
            QStringLiteral("arrowheadColor"),
            QStringLiteral("edgeLabelBackground")};
}

bool isAllowedMermaidTheme(const QString &name)
{
    return mermaidThemeNames().contains(name);
}

bool isAllowedMermaidVariable(const QString &name)
{
    return mermaidThemeVariableNames().contains(name);
}

QString MermaidTheme::cacheKey() const
{
    if (variables.isEmpty()) {
        return name;
    }
    QStringList entries;
    entries.reserve(variables.size());
    for (auto it = variables.constBegin(); it != variables.constEnd(); ++it) {
        entries.append(it.key() + QLatin1Char('=') + it.value());
    }
    entries.sort();
    return name + QLatin1Char('|') + entries.join(QLatin1Char(';'));
}

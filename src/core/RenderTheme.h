#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

struct MermaidTheme {
    QString name = QStringLiteral("default");
    QHash<QString, QString> variables;

    QString cacheKey() const;
};

QStringList mermaidThemeNames();
QStringList mermaidThemeVariableNames();
bool isAllowedMermaidTheme(const QString &name);
bool isAllowedMermaidVariable(const QString &name);

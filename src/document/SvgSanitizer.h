#pragma once

#include <QColor>
#include <QHash>
#include <QString>

QString sanitizeSvgForQt(const QString &svg,
                         bool mermaid,
                         const QColor &foreground = QColor(QStringLiteral("#111111")),
                         bool dark = false,
                         const QHash<QString, QString> &mermaidVariables = {});

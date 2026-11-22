#pragma once

#include <QColor>
#include <QString>

QString sanitizeSvgForQt(const QString &svg,
                         bool mermaid,
                         const QColor &foreground = QColor(QStringLiteral("#111111")),
                         bool dark = false);

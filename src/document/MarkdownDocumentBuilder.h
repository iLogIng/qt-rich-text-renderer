#pragma once

#include <QColor>
#include <QHash>
#include <QString>

class QTextDocument;

class MarkdownDocumentBuilder {
public:
    static bool embedSvg(QTextDocument *document,
                         const QString &token,
                         const QString &svg,
                         const QString &error,
                         bool mermaid,
                         bool dark,
                         const QColor &foreground,
                         int maxWidth,
                         QString *errorOut = nullptr,
                         const QHash<QString, QString> &mermaidVariables = {});
};

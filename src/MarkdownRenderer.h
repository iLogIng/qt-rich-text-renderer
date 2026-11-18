#pragma once

#include <QColor>
#include <QString>
#include <QVector>

class QTextDocument;

struct RenderBlock {
    enum class Kind {
        Math,
        Mermaid,
    };

    Kind kind = Kind::Math;
    QString source;
    QString token;
    bool display = true;
};

struct PreparedMarkdown {
    QString markdown;
    QVector<RenderBlock> blocks;
};

class MarkdownRenderer {
public:
    static PreparedMarkdown prepare(const QString &source);
    static bool embedSvg(QTextDocument *document,
                         const QString &token,
                         const QString &svg,
                         const QString &error,
                         bool mermaid,
                         bool dark,
                         const QColor &foreground,
                         int maxWidth,
                         QString *errorOut = nullptr);
};

#pragma once

#include <QString>
#include <QVector>

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

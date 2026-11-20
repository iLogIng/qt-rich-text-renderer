#pragma once

#include <QHash>
#include <QString>
#include <QVector>

class QTextDocument;

struct OutlineEntry {
    int level = 1;
    QString title;
    int blockNumber = 0;
};

class MarkdownOutline {
public:
    static QVector<OutlineEntry> fromDocument(
        const QTextDocument &document,
        const QHash<QString, QString> &tokenLabels = {});
};

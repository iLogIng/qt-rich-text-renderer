#pragma once

#include <QString>

struct LocalFileContent {
    bool ok = false;
    QString path;
    QString text;
    QString error;
};

class LocalFileLoader {
public:
    static constexpr qint64 kMaxFileBytes = 32 * 1024 * 1024;

    static LocalFileContent load(const QString &path);
};

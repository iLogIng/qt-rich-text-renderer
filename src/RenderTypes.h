#pragma once

#include <QString>

struct RenderResult {
    QString svg;
    QString error;
    bool fromCache = false;
    qint64 elapsedMs = 0;
};

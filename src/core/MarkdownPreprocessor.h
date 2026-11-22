#pragma once

#include "MarkdownTypes.h"

#include <QString>

class MarkdownPreprocessor {
public:
    static PreparedMarkdown prepare(const QString &source);
};

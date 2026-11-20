#pragma once

#include <QString>
#include <QUrl>

class LinkResolver {
public:
    enum class Kind {
        Anchor,
        Remote,
        LocalFile,
        Mail,
        Empty,
        Rejected,
    };

    struct Resolved {
        Kind kind = Kind::Empty;
        QUrl url;
        QString path;
        QString error;
    };

    static Resolved resolve(const QUrl &url, const QUrl &baseUrl);
};

#include "LinkResolver.h"

#include <QFileInfo>

LinkResolver::Resolved LinkResolver::resolve(const QUrl &url, const QUrl &baseUrl)
{
    Resolved result;
    if (url.isEmpty()) {
        result.kind = Kind::Empty;
        return result;
    }

    if (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) {
        result.kind = Kind::Remote;
        result.url = url;
        return result;
    }
    if (url.scheme() == QStringLiteral("mailto")) {
        result.kind = Kind::Mail;
        result.url = url;
        return result;
    }
    if (!url.scheme().isEmpty() && url.scheme() != QStringLiteral("file")) {
        result.kind = Kind::Rejected;
        result.error = QStringLiteral("unsupported URL scheme: %1").arg(url.scheme());
        return result;
    }

    if (url.path().isEmpty() && !url.fragment().isEmpty()) {
        result.kind = Kind::Anchor;
        result.url = url;
        return result;
    }

    QUrl absolute = url;
    if (url.isRelative()) {
        if (baseUrl.isEmpty()) {
            result.kind = Kind::Rejected;
            result.error = QStringLiteral("relative link has no base directory");
            return result;
        }
        absolute = baseUrl.resolved(url);
    }

    if (!absolute.isLocalFile()) {
        result.kind = Kind::Rejected;
        result.error = QStringLiteral("not a local file URL");
        return result;
    }

    const QString rawPath = absolute.toLocalFile();
    if (rawPath.isEmpty()) {
        result.kind = Kind::Rejected;
        result.error = QStringLiteral("empty local path");
        return result;
    }

    const QFileInfo info(rawPath);
    if (!info.exists()) {
        result.kind = Kind::LocalFile;
        result.url = absolute;
        result.path = info.absoluteFilePath();
        result.error = QStringLiteral("file does not exist: %1").arg(result.path);
        return result;
    }

    result.kind = Kind::LocalFile;
    result.url = absolute;
    result.path = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath()
                                                      : info.canonicalFilePath();
    return result;
}

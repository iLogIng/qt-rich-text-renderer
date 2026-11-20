#include "SvgCache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

void SvgCache::configure(const QString &directory, const QString &version)
{
    m_directory = directory;
    m_version = version;
    QDir().mkpath(m_directory);
}

QString SvgCache::key(const QString &kind,
                      const QString &source,
                      bool display,
                      const QString &theme) const
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData("rich-text-render-service-v2");
    hash.addData(kind.toUtf8());
    hash.addData(m_version.toUtf8());
    hash.addData(source.toUtf8());
    if (kind == QStringLiteral("math")) {
        hash.addData(display ? "display" : "inline");
    } else {
        hash.addData(theme.toUtf8());
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool SvgCache::read(const QString &key, QString *svg) const
{
    const QString path = pathFor(key);
    if (!QFileInfo::exists(path)) {
        return false;
    }

    QFile cache(path);
    if (!cache.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QString content = QString::fromUtf8(cache.readAll());
    if (content.isEmpty()) {
        return false;
    }

    if (svg) {
        *svg = content;
    }
    return true;
}

void SvgCache::write(const QString &key, const QString &svg) const
{
    QFile cache(pathFor(key));
    if (cache.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cache.write(svg.toUtf8());
    }
}

QString SvgCache::pathFor(const QString &key) const
{
    return QDir(m_directory).filePath(key + QStringLiteral(".svg"));
}

#pragma once

#include <QString>

class SvgCache {
public:
    void configure(const QString &directory, const QString &version);

    QString key(const QString &kind,
                const QString &source,
                bool display,
                const QString &theme) const;

    bool read(const QString &key, QString *svg) const;
    void write(const QString &key, const QString &svg) const;

private:
    QString pathFor(const QString &key) const;

    QString m_directory;
    QString m_version;
};

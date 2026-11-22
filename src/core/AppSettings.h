#pragma once

#include <QByteArray>
#include <QString>

struct AppSettingsData {
    QString themeId = QStringLiteral("light");
    int uiFontPointSize = 10;
    int sourceFontPointSize = 10;
    bool sourceHighlighting = true;
    bool outlineVisible = true;
    bool sourceVisible = true;
    int maxSvgWidth = 860;
    int rendererIdleTimeoutMs = 60000;
    int mathRequestTimeoutMs = 10000;
    int mermaidRequestTimeoutMs = 15000;
    QString lastDirectory;
    QByteArray windowGeometry;
};

class AppSettings {
public:
    AppSettings(const QString &userPath, const QString &defaultsPath);

    static QString defaultUserPath();

    AppSettingsData load() const;
    AppSettingsData defaults() const;
    void save(const AppSettingsData &data);

private:
    QString m_userPath;
    QString m_defaultsPath;
};

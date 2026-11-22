#include "AppSettings.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QVariant>

namespace {

constexpr int kMinUiFontPointSize = 8;
constexpr int kMaxUiFontPointSize = 18;
constexpr int kMinSourceFontPointSize = 8;
constexpr int kMaxSourceFontPointSize = 24;
constexpr int kMinSvgWidth = 320;
constexpr int kMaxSvgWidth = 2000;
constexpr int kMinIdleTimeoutMs = 1000;
constexpr int kMaxIdleTimeoutMs = 60 * 60 * 1000;
constexpr int kMinRequestTimeoutMs = 1000;
constexpr int kMaxRequestTimeoutMs = 5 * 60 * 1000;

AppSettingsData::Theme parseTheme(const QString &value)
{
    return value.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0
               ? AppSettingsData::Theme::Dark
               : AppSettingsData::Theme::Light;
}

QString themeName(AppSettingsData::Theme theme)
{
    return theme == AppSettingsData::Theme::Dark ? QStringLiteral("dark")
                                                  : QStringLiteral("light");
}

struct SettingsSource {
    const QSettings *user = nullptr;
    const QSettings &defaults;

    QVariant value(const QString &key, const QVariant &fallback) const
    {
        if (user && user->contains(key)) {
            return user->value(key);
        }
        return defaults.contains(key) ? defaults.value(key) : fallback;
    }
};

int clampedInt(const SettingsSource &source,
               const QString &key,
               int fallback,
               int minimum,
               int maximum)
{
    return qBound(minimum, source.value(key, fallback).toInt(), maximum);
}

AppSettingsData readSettings(const QSettings *user, const QSettings &defaults)
{
    const SettingsSource source{user, defaults};
    const AppSettingsData fallback;
    AppSettingsData data;

    data.theme = parseTheme(source.value(QStringLiteral("appearance/theme"),
                                         themeName(fallback.theme))
                                .toString());
    data.uiFontPointSize = clampedInt(source,
                                      QStringLiteral("appearance/uiFontPointSize"),
                                      fallback.uiFontPointSize,
                                      kMinUiFontPointSize,
                                      kMaxUiFontPointSize);
    data.sourceFontPointSize = clampedInt(source,
                                          QStringLiteral("appearance/sourceFontPointSize"),
                                          fallback.sourceFontPointSize,
                                          kMinSourceFontPointSize,
                                          kMaxSourceFontPointSize);
    data.sourceHighlighting = source.value(QStringLiteral("appearance/sourceHighlighting"),
                                           fallback.sourceHighlighting)
                                  .toBool();
    data.outlineVisible = source.value(QStringLiteral("view/outlineVisible"),
                                       fallback.outlineVisible)
                              .toBool();
    data.sourceVisible = source.value(QStringLiteral("view/sourceVisible"),
                                      fallback.sourceVisible)
                             .toBool();
    data.maxSvgWidth = clampedInt(source,
                                  QStringLiteral("preview/maxSvgWidth"),
                                  fallback.maxSvgWidth,
                                  kMinSvgWidth,
                                  kMaxSvgWidth);
    data.rendererIdleTimeoutMs = clampedInt(source,
                                            QStringLiteral("renderer/idleTimeoutMs"),
                                            fallback.rendererIdleTimeoutMs,
                                            kMinIdleTimeoutMs,
                                            kMaxIdleTimeoutMs);
    data.mathRequestTimeoutMs = clampedInt(source,
                                           QStringLiteral("renderer/mathRequestTimeoutMs"),
                                           fallback.mathRequestTimeoutMs,
                                           kMinRequestTimeoutMs,
                                           kMaxRequestTimeoutMs);
    data.mermaidRequestTimeoutMs =
        clampedInt(source,
                   QStringLiteral("renderer/mermaidRequestTimeoutMs"),
                   fallback.mermaidRequestTimeoutMs,
                   kMinRequestTimeoutMs,
                   kMaxRequestTimeoutMs);
    data.lastDirectory = source.value(QStringLiteral("files/lastDirectory"),
                                      fallback.lastDirectory)
                             .toString();
    data.windowGeometry = source.value(QStringLiteral("window/geometry"),
                                       fallback.windowGeometry)
                              .toByteArray();
    return data;
}

} // namespace

AppSettings::AppSettings(const QString &userPath, const QString &defaultsPath)
    : m_userPath(userPath)
    , m_defaultsPath(defaultsPath)
{
}

QString AppSettings::defaultUserPath()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(directory).filePath(QStringLiteral("settings.ini"));
}

AppSettingsData AppSettings::load() const
{
    const QSettings user(m_userPath, QSettings::IniFormat);
    const QSettings defaults(m_defaultsPath, QSettings::IniFormat);
    return readSettings(&user, defaults);
}

AppSettingsData AppSettings::defaults() const
{
    const QSettings defaults(m_defaultsPath, QSettings::IniFormat);
    return readSettings(nullptr, defaults);
}

void AppSettings::save(const AppSettingsData &data)
{
    QDir().mkpath(QFileInfo(m_userPath).absolutePath());
    QSettings user(m_userPath, QSettings::IniFormat);
    user.setValue(QStringLiteral("meta/version"), 1);
    user.setValue(QStringLiteral("appearance/theme"), themeName(data.theme));
    user.setValue(QStringLiteral("appearance/uiFontPointSize"), data.uiFontPointSize);
    user.setValue(QStringLiteral("appearance/sourceFontPointSize"), data.sourceFontPointSize);
    user.setValue(QStringLiteral("appearance/sourceHighlighting"), data.sourceHighlighting);
    user.setValue(QStringLiteral("view/outlineVisible"), data.outlineVisible);
    user.setValue(QStringLiteral("view/sourceVisible"), data.sourceVisible);
    user.setValue(QStringLiteral("preview/maxSvgWidth"), data.maxSvgWidth);
    user.setValue(QStringLiteral("renderer/idleTimeoutMs"), data.rendererIdleTimeoutMs);
    user.setValue(QStringLiteral("renderer/mathRequestTimeoutMs"),
                  data.mathRequestTimeoutMs);
    user.setValue(QStringLiteral("renderer/mermaidRequestTimeoutMs"),
                  data.mermaidRequestTimeoutMs);
    user.setValue(QStringLiteral("files/lastDirectory"), data.lastDirectory);
    user.setValue(QStringLiteral("window/geometry"), data.windowGeometry);
    user.sync();
}

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

QVariant readSetting(const QSettings &user,
                     const QSettings &defaults,
                     const QString &key,
                     const QVariant &fallback)
{
    if (user.contains(key)) {
        return user.value(key);
    }
    if (defaults.contains(key)) {
        return defaults.value(key);
    }
    return fallback;
}

QVariant readDefault(const QSettings &defaults,
                     const QString &key,
                     const QVariant &fallback)
{
    return defaults.contains(key) ? defaults.value(key) : fallback;
}

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

    AppSettingsData data;
    data.theme = parseTheme(readSetting(user,
                                        defaults,
                                        QStringLiteral("appearance/theme"),
                                        themeName(data.theme))
                                .toString());
    data.uiFontPointSize = qBound(kMinUiFontPointSize,
                                  readSetting(user,
                                              defaults,
                                              QStringLiteral("appearance/uiFontPointSize"),
                                              data.uiFontPointSize)
                                      .toInt(),
                                  kMaxUiFontPointSize);
    data.sourceFontPointSize = qBound(kMinSourceFontPointSize,
                                      readSetting(user,
                                                  defaults,
                                                  QStringLiteral("appearance/sourceFontPointSize"),
                                                  data.sourceFontPointSize)
                                          .toInt(),
                                      kMaxSourceFontPointSize);
    data.sourceHighlighting = readSetting(user,
                                          defaults,
                                          QStringLiteral("appearance/sourceHighlighting"),
                                          data.sourceHighlighting)
                                  .toBool();
    data.outlineVisible = readSetting(user,
                                      defaults,
                                      QStringLiteral("view/outlineVisible"),
                                      data.outlineVisible)
                              .toBool();
    data.sourceVisible = readSetting(user,
                                     defaults,
                                     QStringLiteral("view/sourceVisible"),
                                     data.sourceVisible)
                             .toBool();
    data.maxSvgWidth = qBound(kMinSvgWidth,
                              readSetting(user,
                                          defaults,
                                          QStringLiteral("preview/maxSvgWidth"),
                                          data.maxSvgWidth)
                                  .toInt(),
                              kMaxSvgWidth);
    data.rendererIdleTimeoutMs = qBound(
        kMinIdleTimeoutMs,
        readSetting(user,
                    defaults,
                    QStringLiteral("renderer/idleTimeoutMs"),
                    data.rendererIdleTimeoutMs)
            .toInt(),
        kMaxIdleTimeoutMs);
    data.mathRequestTimeoutMs = qBound(
        kMinRequestTimeoutMs,
        readSetting(user,
                    defaults,
                    QStringLiteral("renderer/mathRequestTimeoutMs"),
                    data.mathRequestTimeoutMs)
            .toInt(),
        kMaxRequestTimeoutMs);
    data.mermaidRequestTimeoutMs = qBound(
        kMinRequestTimeoutMs,
        readSetting(user,
                    defaults,
                    QStringLiteral("renderer/mermaidRequestTimeoutMs"),
                    data.mermaidRequestTimeoutMs)
            .toInt(),
        kMaxRequestTimeoutMs);
    data.lastDirectory = readSetting(user,
                                     defaults,
                                     QStringLiteral("files/lastDirectory"),
                                     data.lastDirectory)
                             .toString();
    data.windowGeometry = readSetting(user,
                                      defaults,
                                      QStringLiteral("window/geometry"),
                                      data.windowGeometry)
                              .toByteArray();
    return data;
}

AppSettingsData AppSettings::defaults() const
{
    const QSettings defaults(m_defaultsPath, QSettings::IniFormat);
    const AppSettingsData fallback;

    AppSettingsData data;
    data.theme = parseTheme(readDefault(defaults, QStringLiteral("appearance/theme"),
                                        themeName(fallback.theme))
                                .toString());
    data.uiFontPointSize = qBound(kMinUiFontPointSize,
                                  readDefault(defaults, QStringLiteral("appearance/uiFontPointSize"),
                                              fallback.uiFontPointSize)
                                      .toInt(),
                                  kMaxUiFontPointSize);
    data.sourceFontPointSize = qBound(
        kMinSourceFontPointSize,
        readDefault(defaults, QStringLiteral("appearance/sourceFontPointSize"),
                    fallback.sourceFontPointSize)
            .toInt(),
        kMaxSourceFontPointSize);
    data.sourceHighlighting = readDefault(defaults, QStringLiteral("appearance/sourceHighlighting"),
                                          fallback.sourceHighlighting)
                                  .toBool();
    data.outlineVisible = readDefault(defaults, QStringLiteral("view/outlineVisible"),
                                      fallback.outlineVisible)
                              .toBool();
    data.sourceVisible = readDefault(defaults, QStringLiteral("view/sourceVisible"),
                                     fallback.sourceVisible)
                             .toBool();
    data.maxSvgWidth = qBound(kMinSvgWidth,
                              readDefault(defaults, QStringLiteral("preview/maxSvgWidth"),
                                          fallback.maxSvgWidth)
                                  .toInt(),
                              kMaxSvgWidth);
    data.rendererIdleTimeoutMs = qBound(
        kMinIdleTimeoutMs,
        readDefault(defaults, QStringLiteral("renderer/idleTimeoutMs"),
                    fallback.rendererIdleTimeoutMs)
            .toInt(),
        kMaxIdleTimeoutMs);
    data.mathRequestTimeoutMs = qBound(
        kMinRequestTimeoutMs,
        readDefault(defaults, QStringLiteral("renderer/mathRequestTimeoutMs"),
                    fallback.mathRequestTimeoutMs)
            .toInt(),
        kMaxRequestTimeoutMs);
    data.mermaidRequestTimeoutMs = qBound(
        kMinRequestTimeoutMs,
        readDefault(defaults, QStringLiteral("renderer/mermaidRequestTimeoutMs"),
                    fallback.mermaidRequestTimeoutMs)
            .toInt(),
        kMaxRequestTimeoutMs);
    data.lastDirectory = readDefault(defaults, QStringLiteral("files/lastDirectory"),
                                     fallback.lastDirectory)
                             .toString();
    data.windowGeometry = readDefault(defaults, QStringLiteral("window/geometry"),
                                      fallback.windowGeometry)
                              .toByteArray();
    return data;
}

void AppSettings::save(const AppSettingsData &data)
{
    QDir().mkpath(QFileInfo(m_userPath).absolutePath());
    QSettings user(m_userPath, QSettings::IniFormat);
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

QString AppSettings::userPath() const
{
    return m_userPath;
}

QString AppSettings::defaultsPath() const
{
    return m_defaultsPath;
}

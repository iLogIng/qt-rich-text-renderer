#include "Theme.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPalette>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStyleFactory>

namespace {

constexpr qint64 kMaxThemeBytes = 256 * 1024;

ThemeSpec lightFallback()
{
    ThemeSpec theme;
    theme.id = QStringLiteral("light");
    theme.name = QStringLiteral("Light");
    theme.dark = false;
    theme.useQss = false;

    theme.window = QColor(QStringLiteral("#ffffff"));
    theme.windowText = QColor(QStringLiteral("#111111"));
    theme.base = QColor(QStringLiteral("#ffffff"));
    theme.alternateBase = QColor(QStringLiteral("#f5f5f5"));
    theme.text = QColor(QStringLiteral("#111111"));
    theme.mutedText = QColor(QStringLiteral("#666666"));
    theme.button = QColor(QStringLiteral("#f0f0f0"));
    theme.buttonText = QColor(QStringLiteral("#111111"));
    theme.brightText = QColor(QStringLiteral("#ff0000"));
    theme.highlight = QColor(QStringLiteral("#308cc6"));
    theme.highlightedText = QColor(QStringLiteral("#ffffff"));
    theme.link = QColor(QStringLiteral("#0b57d0"));
    theme.linkVisited = QColor(QStringLiteral("#6b3fa0"));
    theme.placeholder = QColor(QStringLiteral("#888888"));
    theme.tooltipBase = QColor(QStringLiteral("#ffffdc"));
    theme.tooltipText = QColor(QStringLiteral("#111111"));
    theme.border = QColor(QStringLiteral("#d0d0d0"));
    return theme;
}


ThemeSpec darkFallback()
{
    ThemeSpec theme;
    theme.id = QStringLiteral("dark");
    theme.name = QStringLiteral("Dark");
    theme.dark = true;
    theme.useQss = false;

    theme.window = QColor(QStringLiteral("#1e1e1e"));
    theme.windowText = QColor(QStringLiteral("#e6e6e6"));
    theme.base = QColor(QStringLiteral("#1e1e1e"));
    theme.alternateBase = QColor(QStringLiteral("#252526"));
    theme.text = QColor(QStringLiteral("#e6e6e6"));
    theme.mutedText = QColor(QStringLiteral("#a0a0a0"));
    theme.button = QColor(QStringLiteral("#2d2d30"));
    theme.buttonText = QColor(QStringLiteral("#e6e6e6"));
    theme.brightText = QColor(QStringLiteral("#ffffff"));
    theme.highlight = QColor(QStringLiteral("#264f78"));
    theme.highlightedText = QColor(QStringLiteral("#ffffff"));
    theme.link = QColor(QStringLiteral("#8ab4f8"));
    theme.linkVisited = QColor(QStringLiteral("#c58af9"));
    theme.placeholder = QColor(QStringLiteral("#8c8c8c"));
    theme.tooltipBase = QColor(QStringLiteral("#252526"));
    theme.tooltipText = QColor(QStringLiteral("#e6e6e6"));
    theme.border = QColor(QStringLiteral("#3c3c3c"));
    return theme;
}

QString builtInPath(const QString &id)
{
    return QStringLiteral(":/themes/%1.json").arg(id);
}

bool readThemeFile(const QString &path, QByteArray *data, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("cannot read theme: %1").arg(file.errorString());
        }
        return false;
    }
    if (file.size() > kMaxThemeBytes) {
        if (error) {
            *error = QStringLiteral("theme file is too large");
        }
        return false;
    }
    *data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        if (error) {
            *error = QStringLiteral("cannot read theme: %1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

bool readColor(const QJsonObject &colors,
               const QString &key,
               const QColor &fallback,
               QColor *color,
               QString *error)
{
    if (!colors.contains(key)) {
        *color = fallback;
        return true;
    }
    const QColor value(colors.value(key).toString());
    if (!value.isValid()) {
        if (error) {
            *error = QStringLiteral("invalid color: colors.%1").arg(key);
        }
        return false;
    }
    *color = value;
    return true;
}

bool readTheme(const QString &path,
               const QString &requestedId,
               bool builtIn,
               ThemeSpec *theme,
               QString *error)
{
    QByteArray data;
    if (!readThemeFile(path, &data, error)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("invalid theme JSON: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    if (root.contains(QStringLiteral("schemaVersion"))
        && root.value(QStringLiteral("schemaVersion")).toInt() != 1) {
        if (error) {
            *error = QStringLiteral("unsupported theme schema");
        }
        return false;
    }

    const QString id = root.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty() || id != requestedId) {
        if (error) {
            *error = QStringLiteral("theme id does not match its location");
        }
        return false;
    }

    const bool dark = root.value(QStringLiteral("dark")).toBool(false);
    ThemeSpec parsed = dark ? darkFallback() : lightFallback();
    parsed.id = id;
    parsed.name = root.value(QStringLiteral("name")).toString().trimmed();
    if (parsed.name.isEmpty()) {
        parsed.name = id;
    }
    parsed.dark = dark;
    parsed.useQss = root.value(QStringLiteral("qss")).toBool(!builtIn);

    const QJsonObject colors = root.value(QStringLiteral("colors")).toObject();
    if (!readColor(colors, QStringLiteral("window"), parsed.window, &parsed.window, error)
        || !readColor(colors, QStringLiteral("windowText"), parsed.windowText, &parsed.windowText, error)
        || !readColor(colors, QStringLiteral("base"), parsed.base, &parsed.base, error)
        || !readColor(colors, QStringLiteral("alternateBase"), parsed.alternateBase, &parsed.alternateBase, error)
        || !readColor(colors, QStringLiteral("text"), parsed.text, &parsed.text, error)
        || !readColor(colors, QStringLiteral("mutedText"), parsed.mutedText, &parsed.mutedText, error)
        || !readColor(colors, QStringLiteral("button"), parsed.button, &parsed.button, error)
        || !readColor(colors, QStringLiteral("buttonText"), parsed.buttonText, &parsed.buttonText, error)
        || !readColor(colors, QStringLiteral("brightText"), parsed.brightText, &parsed.brightText, error)
        || !readColor(colors, QStringLiteral("highlight"), parsed.highlight, &parsed.highlight, error)
        || !readColor(colors, QStringLiteral("highlightedText"), parsed.highlightedText, &parsed.highlightedText, error)
        || !readColor(colors, QStringLiteral("link"), parsed.link, &parsed.link, error)
        || !readColor(colors, QStringLiteral("linkVisited"), parsed.linkVisited, &parsed.linkVisited, error)
        || !readColor(colors, QStringLiteral("placeholder"), parsed.placeholder, &parsed.placeholder, error)
        || !readColor(colors, QStringLiteral("tooltipBase"), parsed.tooltipBase, &parsed.tooltipBase, error)
        || !readColor(colors, QStringLiteral("tooltipText"), parsed.tooltipText, &parsed.tooltipText, error)
        || !readColor(colors, QStringLiteral("border"), parsed.border, &parsed.border, error)) {
        return false;
    }

    const QJsonObject metrics = root.value(QStringLiteral("metrics")).toObject();
    parsed.radius = qBound(0, metrics.value(QStringLiteral("radius")).toInt(parsed.radius), 32);
    parsed.spacing = qBound(0, metrics.value(QStringLiteral("spacing")).toInt(parsed.spacing), 32);

    *theme = parsed;
    return true;
}

QString colorValue(const QColor &color)
{
    return color.alpha() == 255 ? color.name() : color.name(QColor::HexArgb);
}

} // namespace

void ensureThemeResources()
{
    static const bool initialized = [] {
        Q_INIT_RESOURCE(resources);
        return true;
    }();
    Q_UNUSED(initialized);
}

QString ThemeManager::themeDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/rich-text-renderer");
    }
    return QDir(base).filePath(QStringLiteral("themes"));
}

QVector<ThemeInfo> ThemeManager::availableThemes()
{
    ensureThemeResources();
    QVector<ThemeInfo> themes;
    themes.push_back({QStringLiteral("light"), QStringLiteral("Light"), true});
    themes.push_back({QStringLiteral("dark"), QStringLiteral("Dark"), true});

    const QDir directory(themeDirectory());
    const QStringList entries =
        directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &id : entries) {
        if (id == QStringLiteral("light") || id == QStringLiteral("dark")) {
            continue;
        }
        QString error;
        const ThemeSpec theme = loadTheme(id, &error);
        if (error.isEmpty()) {
            themes.push_back({theme.id, theme.name, false});
        }
    }
    return themes;
}

ThemeSpec ThemeManager::loadTheme(const QString &id, QString *error)
{
    ensureThemeResources();
    if (error) {
        error->clear();
    }
    if (id.isEmpty()) {
        if (error) {
            *error = QStringLiteral("empty theme id");
        }
        return fallbackTheme();
    }

    if (id == QStringLiteral("light") || id == QStringLiteral("dark")) {
        ThemeSpec theme;
        if (!readTheme(builtInPath(id), id, true, &theme, error)) {
            return fallbackTheme();
        }
        return theme;
    }

    static const QRegularExpression idPattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    if (!idPattern.match(id).hasMatch()) {
        if (error) {
            *error = QStringLiteral("invalid theme id");
        }
        return fallbackTheme();
    }

    const QString path = QDir(themeDirectory()).filePath(id + QStringLiteral("/theme.json"));
    ThemeSpec theme;
    if (!readTheme(path, id, false, &theme, error)) {
        return fallbackTheme();
    }
    return theme;
}

ThemeSpec ThemeManager::fallbackTheme()
{
    return lightFallback();
}

QString ThemeManager::fallbackThemeId()
{
    return QStringLiteral("light");
}

QString ThemeManager::buildStyleSheet(const ThemeSpec &theme, QString *error)
{
    ensureThemeResources();
    QFile file(QStringLiteral(":/styles/app.qss"));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("cannot read the QSS template");
        }
        return {};
    }
    const QString source = QString::fromUtf8(file.readAll());

    QHash<QString, QString> tokens;
    tokens.insert(QStringLiteral("window"), colorValue(theme.window));
    tokens.insert(QStringLiteral("windowText"), colorValue(theme.windowText));
    tokens.insert(QStringLiteral("base"), colorValue(theme.base));
    tokens.insert(QStringLiteral("alternateBase"), colorValue(theme.alternateBase));
    tokens.insert(QStringLiteral("text"), colorValue(theme.text));
    tokens.insert(QStringLiteral("mutedText"), colorValue(theme.mutedText));
    tokens.insert(QStringLiteral("button"), colorValue(theme.button));
    tokens.insert(QStringLiteral("buttonText"), colorValue(theme.buttonText));
    tokens.insert(QStringLiteral("brightText"), colorValue(theme.brightText));
    tokens.insert(QStringLiteral("highlight"), colorValue(theme.highlight));
    tokens.insert(QStringLiteral("highlightedText"), colorValue(theme.highlightedText));
    tokens.insert(QStringLiteral("link"), colorValue(theme.link));
    tokens.insert(QStringLiteral("linkVisited"), colorValue(theme.linkVisited));
    tokens.insert(QStringLiteral("placeholder"), colorValue(theme.placeholder));
    tokens.insert(QStringLiteral("tooltipBase"), colorValue(theme.tooltipBase));
    tokens.insert(QStringLiteral("tooltipText"), colorValue(theme.tooltipText));
    tokens.insert(QStringLiteral("border"), colorValue(theme.border));
    tokens.insert(QStringLiteral("accent"), colorValue(theme.highlight));
    tokens.insert(QStringLiteral("accentText"), colorValue(theme.highlightedText));
    tokens.insert(QStringLiteral("radius"), QString::number(theme.radius));
    tokens.insert(QStringLiteral("spacing"), QString::number(theme.spacing));

    const QRegularExpression tokenPattern(QStringLiteral("@([A-Za-z][A-Za-z0-9]*)"));
    QString result;
    int offset = 0;
    auto matches = tokenPattern.globalMatch(source);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        result += source.mid(offset, match.capturedStart() - offset);
        const QString name = match.captured(1);
        if (!tokens.contains(name)) {
            if (error) {
                *error = QStringLiteral("unknown QSS token: @%1").arg(name);
            }
            return {};
        }
        result += tokens.value(name);
        offset = match.capturedEnd();
    }
    result += source.mid(offset);
    return result;
}

void ThemeManager::apply(const ThemeSpec &theme)
{
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    palette.setColor(QPalette::Window, theme.window);
    palette.setColor(QPalette::WindowText, theme.windowText);
    palette.setColor(QPalette::Base, theme.base);
    palette.setColor(QPalette::AlternateBase, theme.alternateBase);
    palette.setColor(QPalette::ToolTipBase, theme.tooltipBase);
    palette.setColor(QPalette::ToolTipText, theme.tooltipText);
    palette.setColor(QPalette::Text, theme.text);
    palette.setColor(QPalette::Button, theme.button);
    palette.setColor(QPalette::ButtonText, theme.buttonText);
    palette.setColor(QPalette::BrightText, theme.brightText);
    palette.setColor(QPalette::Highlight, theme.highlight);
    palette.setColor(QPalette::HighlightedText, theme.highlightedText);
    palette.setColor(QPalette::Link, theme.link);
    palette.setColor(QPalette::LinkVisited, theme.linkVisited);
    palette.setColor(QPalette::PlaceholderText, theme.placeholder);
    QApplication::setPalette(palette);

    QString error;
    const QString styleSheet = theme.useQss ? buildStyleSheet(theme, &error) : QString();
    if (!error.isEmpty()) {
        qWarning("Theme style sheet error: %s", qPrintable(error));
    }
    qApp->setStyleSheet(styleSheet);
}

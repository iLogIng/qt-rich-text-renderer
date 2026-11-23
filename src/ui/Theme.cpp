#include "Theme.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPalette>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QStyle>
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
    theme.codeSyntax = defaultCodeSyntaxPalette(false);
    theme.sourceSyntax = defaultMarkdownSourcePalette(false);
    theme.mermaid.name = QStringLiteral("default");
    theme.documentMargin = 12;
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
    theme.codeSyntax = defaultCodeSyntaxPalette(true);
    theme.sourceSyntax = defaultMarkdownSourcePalette(true);
    theme.mermaid.name = QStringLiteral("dark");
    theme.documentMargin = 12;
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

bool readOptionalString(const QJsonObject &object,
                        const QString &key,
                        QString *value,
                        QString *error)
{
    if (!object.contains(key)) {
        return true;
    }
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isString()) {
        if (error) {
            *error = QStringLiteral("%1 must be a string").arg(key);
        }
        return false;
    }
    *value = jsonValue.toString().trimmed();
    return true;
}

bool readOptionalColor(const QJsonObject &object,
                       const QString &key,
                       const QString &context,
                       QColor *value,
                       QString *error)
{
    if (!object.contains(key)) {
        return true;
    }
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isString()) {
        if (error) {
            *error = QStringLiteral("%1.%2 must be a color string").arg(context, key);
        }
        return false;
    }
    const QColor color(jsonValue.toString());
    if (!color.isValid()) {
        if (error) {
            *error = QStringLiteral("invalid color: %1.%2").arg(context, key);
        }
        return false;
    }
    *value = color;
    return true;
}

bool readOptionalInt(const QJsonObject &object,
                     const QString &key,
                     const QString &context,
                     int minimum,
                     int maximum,
                     int *value,
                     QString *error)
{
    if (!object.contains(key)) {
        return true;
    }
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isDouble()) {
        if (error) {
            *error = QStringLiteral("%1.%2 must be an integer").arg(context, key);
        }
        return false;
    }
    const double number = jsonValue.toDouble();
    const int integer = jsonValue.toInt();
    if (number != static_cast<double>(integer) || integer < minimum || integer > maximum) {
        if (error) {
            *error = QStringLiteral("%1.%2 must be between %3 and %4")
                         .arg(context, key)
                         .arg(minimum)
                         .arg(maximum);
        }
        return false;
    }
    *value = integer;
    return true;
}

bool readTypography(const QJsonObject &root, ThemeSpec *theme, QString *error)
{
    if (!root.contains(QStringLiteral("typography"))) {
        return true;
    }
    if (!root.value(QStringLiteral("typography")).isObject()) {
        if (error) {
            *error = QStringLiteral("typography must be a JSON object");
        }
        return false;
    }
    const QJsonObject typography = root.value(QStringLiteral("typography")).toObject();
    return readOptionalString(typography,
                              QStringLiteral("uiFontFamily"),
                              &theme->uiFontFamily,
                              error)
        && readOptionalString(typography,
                              QStringLiteral("previewFontFamily"),
                              &theme->previewFontFamily,
                              error)
        && readOptionalString(typography,
                              QStringLiteral("codeFontFamily"),
                              &theme->codeFontFamily,
                              error);
}

bool readCodePalette(const QJsonObject &object,
                     CodeSyntaxPalette *palette,
                     QString *error)
{
    return readColor(object, QStringLiteral("keyword"), palette->keyword, &palette->keyword, error)
        && readColor(object, QStringLiteral("string"), palette->string, &palette->string, error)
        && readColor(object, QStringLiteral("number"), palette->number, &palette->number, error)
        && readColor(object, QStringLiteral("comment"), palette->comment, &palette->comment, error)
        && readColor(object,
                     QStringLiteral("preprocessor"),
                     palette->preprocessor,
                     &palette->preprocessor,
                     error)
        && readColor(object, QStringLiteral("type"), palette->type, &palette->type, error)
        && readColor(object,
                     QStringLiteral("function"),
                     palette->function,
                     &palette->function,
                     error);
}

bool readSourcePalette(const QJsonObject &object,
                       MarkdownSourcePalette *palette,
                       QString *error)
{
    return readColor(object, QStringLiteral("heading"), palette->heading, &palette->heading, error)
        && readColor(object, QStringLiteral("bold"), palette->bold, &palette->bold, error)
        && readColor(object,
                     QStringLiteral("emphasis"),
                     palette->emphasis,
                     &palette->emphasis,
                     error)
        && readColor(object,
                     QStringLiteral("inlineCode"),
                     palette->inlineCode,
                     &palette->inlineCode,
                     error)
        && readColor(object, QStringLiteral("link"), palette->link, &palette->link, error)
        && readColor(object, QStringLiteral("quote"), palette->quote, &palette->quote, error)
        && readColor(object, QStringLiteral("marker"), palette->marker, &palette->marker, error)
        && readColor(object, QStringLiteral("fence"), palette->fence, &palette->fence, error)
        && readColor(object, QStringLiteral("math"), palette->math, &palette->math, error);
}

bool readSyntax(const QJsonObject &root, ThemeSpec *theme, QString *error)
{
    if (!root.contains(QStringLiteral("syntax"))) {
        return true;
    }
    if (!root.value(QStringLiteral("syntax")).isObject()) {
        if (error) {
            *error = QStringLiteral("syntax must be a JSON object");
        }
        return false;
    }
    const QJsonObject syntax = root.value(QStringLiteral("syntax")).toObject();

    if (syntax.contains(QStringLiteral("code"))) {
        if (!syntax.value(QStringLiteral("code")).isObject()) {
            if (error) {
                *error = QStringLiteral("syntax.code must be a JSON object");
            }
            return false;
        }
        if (!readCodePalette(syntax.value(QStringLiteral("code")).toObject(),
                             &theme->codeSyntax,
                             error)) {
            return false;
        }
    }
    if (syntax.contains(QStringLiteral("source"))) {
        if (!syntax.value(QStringLiteral("source")).isObject()) {
            if (error) {
                *error = QStringLiteral("syntax.source must be a JSON object");
            }
            return false;
        }
        if (!readSourcePalette(syntax.value(QStringLiteral("source")).toObject(),
                               &theme->sourceSyntax,
                               error)) {
            return false;
        }
    }
    return true;
}

bool readPreview(const QJsonObject &root, ThemeSpec *theme, QString *error)
{
    if (!root.contains(QStringLiteral("preview"))) {
        return true;
    }
    if (!root.value(QStringLiteral("preview")).isObject()) {
        if (error) {
            *error = QStringLiteral("preview must be a JSON object");
        }
        return false;
    }
    const QJsonObject preview = root.value(QStringLiteral("preview")).toObject();
    return readOptionalInt(preview,
                           QStringLiteral("maxTextWidth"),
                           QStringLiteral("preview"),
                           0,
                           10000,
                           &theme->preview.maxTextWidth,
                           error)
        && readOptionalColor(preview,
                             QStringLiteral("codeBackground"),
                             QStringLiteral("preview"),
                             &theme->preview.codeBackground,
                             error)
        && readOptionalInt(preview,
                           QStringLiteral("codeBlockMargin"),
                           QStringLiteral("preview"),
                           0,
                           64,
                           &theme->preview.codeBlockMargin,
                           error);
}

bool readMermaid(const QJsonObject &root, ThemeSpec *theme, QString *error)
{
    if (!root.contains(QStringLiteral("mermaid"))) {
        return true;
    }
    if (!root.value(QStringLiteral("mermaid")).isObject()) {
        if (error) {
            *error = QStringLiteral("mermaid must be a JSON object");
        }
        return false;
    }

    const QJsonObject mermaid = root.value(QStringLiteral("mermaid")).toObject();
    if (mermaid.contains(QStringLiteral("theme"))) {
        if (!mermaid.value(QStringLiteral("theme")).isString()) {
            if (error) {
                *error = QStringLiteral("mermaid.theme must be a string");
            }
            return false;
        }
        const QString name = mermaid.value(QStringLiteral("theme")).toString().trimmed();
        if (!isAllowedMermaidTheme(name)) {
            if (error) {
                *error = QStringLiteral("unsupported Mermaid theme: %1").arg(name);
            }
            return false;
        }
        theme->mermaid.name = name;
    }

    if (mermaid.contains(QStringLiteral("variables"))) {
        if (!mermaid.value(QStringLiteral("variables")).isObject()) {
            if (error) {
                *error = QStringLiteral("mermaid.variables must be a JSON object");
            }
            return false;
        }
        const QJsonObject variables = mermaid.value(QStringLiteral("variables")).toObject();
        for (auto it = variables.constBegin(); it != variables.constEnd(); ++it) {
            if (!isAllowedMermaidVariable(it.key())) {
                if (error) {
                    *error = QStringLiteral("unsupported Mermaid variable: %1").arg(it.key());
                }
                return false;
            }
            if (!it.value().isString()) {
                if (error) {
                    *error = QStringLiteral("Mermaid variable must be a color string: %1")
                                 .arg(it.key());
                }
                return false;
            }
            const QColor color(it.value().toString());
            if (!color.isValid()) {
                if (error) {
                    *error = QStringLiteral("invalid Mermaid color: %1").arg(it.key());
                }
                return false;
            }
            theme->mermaid.variables.insert(it.key(), color.name(QColor::HexRgb));
        }
    }
    return true;
}

bool readSlotStyles(const QJsonObject &root,
                    QHash<QString, SlotStyle> *styles,
                    QString *error)
{
    if (!root.contains(QStringLiteral("slots"))) {
        return true;
    }
    if (!root.value(QStringLiteral("slots")).isObject()) {
        if (error) {
            *error = QStringLiteral("slots must be a JSON object");
        }
        return false;
    }

    static const QSet<QString> allowedSlots = {
        QString::fromLatin1(ThemeSlotStyle::Top),
        QString::fromLatin1(ThemeSlotStyle::Bottom),
        QString::fromLatin1(ThemeSlotStyle::Empty),
        QString::fromLatin1(ThemeSlotStyle::ToolbarLeading),
        QString::fromLatin1(ThemeSlotStyle::ToolbarTrailing),
        QString::fromLatin1(ThemeSlotStyle::Content),
        QString::fromLatin1(ThemeSlotStyle::Preview),
        QString::fromLatin1(ThemeSlotStyle::Source),
        QString::fromLatin1(ThemeSlotStyle::Outline),
    };
    static const QSet<QString> allowedFields = {
        QStringLiteral("background"),
        QStringLiteral("borderColor"),
        QStringLiteral("borderWidth"),
        QStringLiteral("radius"),
        QStringLiteral("padding"),
    };

    const QJsonObject slotObject = root.value(QStringLiteral("slots")).toObject();
    for (auto it = slotObject.constBegin(); it != slotObject.constEnd(); ++it) {
        if (!allowedSlots.contains(it.key())) {
            if (error) {
                *error = QStringLiteral("unknown slot style: %1").arg(it.key());
            }
            return false;
        }
        if (!it.value().isObject()) {
            if (error) {
                *error = QStringLiteral("slot style must be a JSON object: %1").arg(it.key());
            }
            return false;
        }
        const QJsonObject fields = it.value().toObject();
        for (auto field = fields.constBegin(); field != fields.constEnd(); ++field) {
            if (!allowedFields.contains(field.key())) {
                if (error) {
                    *error = QStringLiteral("unknown slot style field: %1.%2")
                                 .arg(it.key(), field.key());
                }
                return false;
            }
        }

        SlotStyle style;
        const QString context = QStringLiteral("slots.%1").arg(it.key());
        if (!readOptionalColor(fields,
                               QStringLiteral("background"),
                               context,
                               &style.background,
                               error)
            || !readOptionalColor(fields,
                                  QStringLiteral("borderColor"),
                                  context,
                                  &style.borderColor,
                                  error)
            || !readOptionalInt(fields,
                                QStringLiteral("borderWidth"),
                                context,
                                0,
                                8,
                                &style.borderWidth,
                                error)
            || !readOptionalInt(fields,
                                QStringLiteral("radius"),
                                context,
                                0,
                                32,
                                &style.radius,
                                error)
            || !readOptionalInt(fields,
                                QStringLiteral("padding"),
                                context,
                                0,
                                32,
                                &style.padding,
                                error)) {
            return false;
        }
        if (style.borderColor.isValid() && style.borderWidth < 0) {
            style.borderWidth = 1;
        }
        styles->insert(it.key(), style);
    }
    return true;
}

bool resolveLayoutFile(const QString &file,
                       const QString &packageDirectory,
                       QString *layoutPath,
                       QString *error)
{
    if (file.isEmpty()) {
        if (error) {
            *error = QStringLiteral("layout.file is required");
        }
        return false;
    }
    if (QDir::isAbsolutePath(file)
        || file.contains(QLatin1Char('\\'))
        || file.contains(QStringLiteral("://"))
        || file != QDir::cleanPath(file)
        || file == QStringLiteral("..")
        || file.startsWith(QStringLiteral("../"))) {
        if (error) {
            *error = QStringLiteral("layout.file must stay inside the theme package");
        }
        return false;
    }
    if (!file.endsWith(QStringLiteral(".ui"), Qt::CaseInsensitive)) {
        if (error) {
            *error = QStringLiteral("layout.file must use the .ui suffix");
        }
        return false;
    }

    const QFileInfo packageInfo(packageDirectory);
    const QString packagePath = packageInfo.canonicalFilePath();
    if (packagePath.isEmpty() || !packageInfo.isDir()) {
        if (error) {
            *error = QStringLiteral("theme package directory is not accessible");
        }
        return false;
    }

    const QFileInfo layoutInfo(QDir(packageDirectory).filePath(file));
    const QString canonical = layoutInfo.canonicalFilePath();
    if (!layoutInfo.isFile() || canonical.isEmpty()) {
        if (error) {
            *error = QStringLiteral("layout file is missing");
        }
        return false;
    }
    if (!canonical.startsWith(packagePath + QDir::separator())) {
        if (error) {
            *error = QStringLiteral("layout file is outside the theme package");
        }
        return false;
    }
    *layoutPath = canonical;
    return true;
}

bool readLayout(const QJsonObject &root,
                bool builtIn,
                const QString &packageDirectory,
                ThemeSpec *theme)
{
    if (root.contains(QStringLiteral("layout"))
        && root.contains(QStringLiteral("layouts"))) {
        theme->layoutError =
            QStringLiteral("layout and layouts are mutually exclusive");
        return false;
    }

    if (root.contains(QStringLiteral("layouts"))) {
        if (builtIn) {
            theme->layoutError =
                QStringLiteral("built-in themes cannot provide a .ui layout");
            return false;
        }
        if (!root.value(QStringLiteral("layouts")).isObject()) {
            theme->layoutError = QStringLiteral("layouts must be a JSON object");
            return false;
        }
        const QJsonObject layouts = root.value(QStringLiteral("layouts")).toObject();
        const int apiVersion = layouts.value(QStringLiteral("apiVersion")).toInt(0);
        if (apiVersion != 1) {
            theme->layoutError =
                QStringLiteral("unsupported layout API version: %1").arg(apiVersion);
            return false;
        }
        if (!layouts.value(QStringLiteral("variants")).isArray()) {
            theme->layoutError = QStringLiteral("layouts.variants must be an array");
            return false;
        }
        const QJsonArray variants = layouts.value(QStringLiteral("variants")).toArray();
        if (variants.isEmpty()) {
            theme->layoutError = QStringLiteral("layouts.variants must not be empty");
            return false;
        }
        QSet<QString> ids;
        for (const QJsonValue &value : variants) {
            if (!value.isObject()) {
                theme->layoutError =
                    QStringLiteral("each layout variant must be a JSON object");
                return false;
            }
            const QJsonObject variant = value.toObject();
            const QString id = variant.value(QStringLiteral("id")).toString().trimmed();
            if (id.isEmpty() || ids.contains(id)) {
                theme->layoutError =
                    QStringLiteral("layout variant id must be unique and non-empty");
                return false;
            }
            ids.insert(id);
            const QString contentMode = variant.value(QStringLiteral("contentMode"))
                                            .toString(QStringLiteral("tabs"))
                                            .trimmed();
            if (contentMode != QStringLiteral("tabs")
                && contentMode != QStringLiteral("slots")) {
                theme->layoutError =
                    QStringLiteral("layout variant contentMode must be tabs or slots");
                return false;
            }
            const QString file = variant.value(QStringLiteral("file")).toString().trimmed();
            QString layoutFile;
            if (!resolveLayoutFile(file, packageDirectory, &layoutFile, &theme->layoutError)) {
                return false;
            }
            theme->layoutVariants.push_back({id, layoutFile, contentMode});
        }
        theme->layoutApiVersion = apiVersion;
        return true;
    }

    if (!root.contains(QStringLiteral("layout"))) {
        return false;
    }
    if (builtIn) {
        theme->layoutError = QStringLiteral("built-in themes cannot provide a .ui layout");
        return false;
    }
    if (!root.value(QStringLiteral("layout")).isObject()) {
        theme->layoutError = QStringLiteral("layout must be a JSON object");
        return false;
    }

    const QJsonObject layout = root.value(QStringLiteral("layout")).toObject();
    const int apiVersion = layout.value(QStringLiteral("apiVersion")).toInt(0);
    if (apiVersion != 1) {
        theme->layoutError =
            QStringLiteral("unsupported layout API version: %1").arg(apiVersion);
        return false;
    }

    const QString contentMode = layout.value(QStringLiteral("contentMode"))
                                    .toString(QStringLiteral("tabs"))
                                    .trimmed();
    if (contentMode != QStringLiteral("tabs") && contentMode != QStringLiteral("slots")) {
        theme->layoutError = QStringLiteral("layout.contentMode must be tabs or slots");
        return false;
    }
    theme->contentMode = contentMode;

    const QString file = layout.value(QStringLiteral("file")).toString().trimmed();
    QString layoutPath;
    if (!resolveLayoutFile(file, packageDirectory, &layoutPath, &theme->layoutError)) {
        return false;
    }
    theme->layoutApiVersion = apiVersion;
    theme->layoutFile = layoutPath;
    return true;
}


bool readAssets(const QJsonObject &root,
                bool builtIn,
                const QString &packageDirectory,
                ThemeSpec *theme)
{
    if (!root.contains(QStringLiteral("assets"))) {
        return true;
    }
    if (builtIn) {
        theme->assetError = QStringLiteral("built-in themes cannot provide assets");
        return true;
    }
    if (!root.value(QStringLiteral("assets")).isObject()) {
        theme->assetError = QStringLiteral("assets must be a JSON object");
        return true;
    }
    const QJsonObject assets = root.value(QStringLiteral("assets")).toObject();
    if (assets.isEmpty()) {
        return true;
    }

    const QFileInfo packageInfo(packageDirectory);
    const QString packagePath = packageInfo.canonicalFilePath();
    if (packagePath.isEmpty() || !packageInfo.isDir()) {
        theme->assetError = QStringLiteral("theme package directory is not accessible");
        return true;
    }

    static const QRegularExpression idPattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    for (auto it = assets.constBegin(); it != assets.constEnd(); ++it) {
        const QString id = it.key().trimmed();
        if (!idPattern.match(id).hasMatch()) {
            theme->assetError = QStringLiteral("invalid asset id: %1").arg(it.key());
            return true;
        }
        if (!it.value().isString()) {
            theme->assetError = QStringLiteral("asset path must be a string: %1").arg(id);
            return true;
        }
        const QString file = it.value().toString().trimmed();
        if (file.isEmpty() || QDir::isAbsolutePath(file)
            || file.contains(QLatin1Char('\\'))
            || file.contains(QStringLiteral("://"))
            || file != QDir::cleanPath(file)
            || file == QStringLiteral("..")
            || file.startsWith(QStringLiteral("../"))) {
            theme->assetError =
                QStringLiteral("asset path must stay inside the theme package: %1").arg(id);
            return true;
        }
        if (!file.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)
            && !file.endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive)
            && !file.endsWith(QStringLiteral(".jpeg"), Qt::CaseInsensitive)) {
            theme->assetError =
                QStringLiteral("asset must be PNG or JPEG: %1").arg(id);
            return true;
        }

        const QFileInfo assetInfo(QDir(packageDirectory).filePath(file));
        const QString assetPath = assetInfo.canonicalFilePath();
        if (!assetInfo.isFile() || assetPath.isEmpty()) {
            theme->assetError = QStringLiteral("asset file is missing: %1").arg(id);
            return true;
        }
        if (!assetPath.startsWith(packagePath + QDir::separator())) {
            theme->assetError =
                QStringLiteral("asset file is outside the theme package: %1").arg(id);
            return true;
        }
        if (assetInfo.size() > 2 * 1024 * 1024) {
            theme->assetError = QStringLiteral("asset file is too large: %1").arg(id);
            return true;
        }

        QImageReader reader(assetPath);
        reader.setDecideFormatFromContent(true);
        const QString format = QString::fromLatin1(reader.format()).toLower();
        if (!reader.canRead()
            || (format != QStringLiteral("png")
                && format != QStringLiteral("jpeg")
                && format != QStringLiteral("jpg"))) {
            theme->assetError =
                QStringLiteral("asset format is not supported: %1").arg(id);
            return true;
        }
        const QSize size = reader.size();
        if (!size.isValid() || size.width() <= 0 || size.height() <= 0
            || size.width() > 4096 || size.height() > 4096) {
            theme->assetError =
                QStringLiteral("asset dimensions are not supported: %1").arg(id);
            return true;
        }
        if (reader.imageCount() > 1) {
            theme->assetError =
                QStringLiteral("animated assets are not supported: %1").arg(id);
            return true;
        }

        ThemeAsset asset;
        asset.id = id;
        asset.filePath = assetPath;
        asset.format = format;
        asset.size = size;
        theme->assets.insert(id, asset);
    }
    return true;
}

bool readTheme(const QString &path,
               const QString &requestedId,
               bool builtIn,
               const QString &packageDirectory,
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
    parsed.packageDirectory = packageDirectory;
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

    if (!readTypography(root, &parsed, error)
        || !readSyntax(root, &parsed, error)
        || !readPreview(root, &parsed, error)
        || !readMermaid(root, &parsed, error)
        || !readSlotStyles(root, &parsed.slotStyles, error)) {
        return false;
    }

    const QJsonObject metrics = root.value(QStringLiteral("metrics")).toObject();
    parsed.radius = qBound(0, metrics.value(QStringLiteral("radius")).toInt(parsed.radius), 32);
    parsed.spacing = qBound(0, metrics.value(QStringLiteral("spacing")).toInt(parsed.spacing), 32);
    parsed.documentMargin =
        qBound(0, metrics.value(QStringLiteral("documentMargin")).toInt(parsed.documentMargin), 64);

    readLayout(root, builtIn, packageDirectory, &parsed);
    readAssets(root, builtIn, packageDirectory, &parsed);

    *theme = parsed;
    return true;
}

QString colorValue(const QColor &color)
{
    return color.alpha() == 255 ? color.name() : color.name(QColor::HexArgb);
}

QString buildSlotRules(const ThemeSpec &theme)
{
    QString rules;
    for (auto it = theme.slotStyles.constBegin(); it != theme.slotStyles.constEnd(); ++it) {
        const SlotStyle &style = it.value();
        QStringList declarations;
        if (style.background.isValid()) {
            declarations.append(
                QStringLiteral("background-color: %1;").arg(colorValue(style.background)));
        }
        if (style.borderColor.isValid() && style.borderWidth > 0) {
            declarations.append(QStringLiteral("border: %1px solid %2;")
                                    .arg(style.borderWidth)
                                    .arg(colorValue(style.borderColor)));
        }
        if (style.radius >= 0) {
            declarations.append(QStringLiteral("border-radius: %1px;").arg(style.radius));
        }
        if (style.padding >= 0) {
            declarations.append(QStringLiteral("padding: %1px;").arg(style.padding));
        }
        if (declarations.isEmpty()) {
            continue;
        }
        rules += QStringLiteral("QWidget[%1=\"%2\"] {\n    %3\n}\n\n")
                     .arg(QString::fromLatin1(ThemeSlotStyle::Property),
                          it.key(),
                          declarations.join(QStringLiteral("\n    ")));
    }
    return rules;
}

} // namespace

ThemeLayout chooseThemeLayout(const ThemeSpec &theme, const QString &requestedId)
{
    if (!theme.layoutError.isEmpty()) {
        return {};
    }
    if (!theme.layoutVariants.isEmpty()) {
        for (const ThemeLayout &variant : theme.layoutVariants) {
            if (!requestedId.isEmpty() && variant.id == requestedId) {
                return variant;
            }
        }
        return theme.layoutVariants.first();
    }

    ThemeLayout layout;
    layout.file = theme.layoutFile;
    layout.contentMode = theme.contentMode;
    return layout;
}

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
        if (!readTheme(builtInPath(id), id, true, QString(), &theme, error)) {
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

    const QString packageDirectory = QDir(themeDirectory()).filePath(id);
    const QString path = QDir(packageDirectory).filePath(QStringLiteral("theme.json"));
    ThemeSpec theme;
    if (!readTheme(path, id, false, packageDirectory, &theme, error)) {
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

    const QString slotRules = buildSlotRules(theme);
    if (!slotRules.isEmpty()) {
        result += QLatin1Char('\n') + slotRules;
    }
    return result;
}

void ThemeManager::apply(const ThemeSpec &theme)
{
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // Start from the Fusion light palette so switching from a dark theme back
    // to a light theme cannot leak dark bevel roles into the new palette.
    QPalette palette = QApplication::style()->standardPalette();

    const auto setActiveAndInactive = [&palette](QPalette::ColorRole role, const QColor &color) {
        palette.setColor(QPalette::Active, role, color);
        palette.setColor(QPalette::Inactive, role, color);
    };
    const auto setAllGroups = [&palette](QPalette::ColorRole role, const QColor &color) {
        palette.setColor(QPalette::Active, role, color);
        palette.setColor(QPalette::Inactive, role, color);
        palette.setColor(QPalette::Disabled, role, color);
    };

    setActiveAndInactive(QPalette::Window, theme.window);
    setActiveAndInactive(QPalette::WindowText, theme.windowText);
    setActiveAndInactive(QPalette::Base, theme.base);
    setActiveAndInactive(QPalette::AlternateBase, theme.alternateBase);
    setActiveAndInactive(QPalette::ToolTipBase, theme.tooltipBase);
    setActiveAndInactive(QPalette::ToolTipText, theme.tooltipText);
    setActiveAndInactive(QPalette::Text, theme.text);
    setActiveAndInactive(QPalette::Button, theme.button);
    setActiveAndInactive(QPalette::ButtonText, theme.buttonText);
    setActiveAndInactive(QPalette::BrightText, theme.brightText);
    setActiveAndInactive(QPalette::Highlight, theme.highlight);
    setActiveAndInactive(QPalette::HighlightedText, theme.highlightedText);
    setActiveAndInactive(QPalette::Link, theme.link);
    setActiveAndInactive(QPalette::LinkVisited, theme.linkVisited);
    setActiveAndInactive(QPalette::PlaceholderText, theme.placeholder);
    setActiveAndInactive(QPalette::Accent, theme.highlight);

    if (theme.dark) {
        // Fusion paints grooves, frames and checked tool buttons from these
        // bevel roles. Leaving them at the light default produces bright
        // Outline buttons and edges in an otherwise dark window.
        const QColor light = theme.button.lighter(130);
        const QColor midlight = theme.button.lighter(115);
        const QColor dark = theme.button.darker(130);
        const QColor mid = theme.button.darker(115);
        const QColor shadow = theme.button.darker(160);

        setAllGroups(QPalette::Window, theme.window);
        setAllGroups(QPalette::Base, theme.base);
        setAllGroups(QPalette::AlternateBase, theme.alternateBase);
        setAllGroups(QPalette::ToolTipBase, theme.tooltipBase);
        setAllGroups(QPalette::ToolTipText, theme.tooltipText);
        setAllGroups(QPalette::Button, theme.button);
        setAllGroups(QPalette::BrightText, theme.brightText);
        setAllGroups(QPalette::Highlight, theme.highlight);
        setAllGroups(QPalette::HighlightedText, theme.highlightedText);
        setAllGroups(QPalette::Accent, theme.highlight);
        setAllGroups(QPalette::Light, light);
        setAllGroups(QPalette::Midlight, midlight);
        setAllGroups(QPalette::Dark, dark);
        setAllGroups(QPalette::Mid, mid);
        setAllGroups(QPalette::Shadow, shadow);

        setAllGroups(QPalette::WindowText, theme.mutedText);
        setAllGroups(QPalette::Text, theme.mutedText);
        setAllGroups(QPalette::ButtonText, theme.mutedText);
        setAllGroups(QPalette::Link, theme.mutedText);
        setAllGroups(QPalette::LinkVisited, theme.mutedText);
        setAllGroups(QPalette::PlaceholderText, theme.mutedText);

        palette.setColor(QPalette::Active, QPalette::WindowText, theme.windowText);
        palette.setColor(QPalette::Inactive, QPalette::WindowText, theme.windowText);
        palette.setColor(QPalette::Active, QPalette::Text, theme.text);
        palette.setColor(QPalette::Inactive, QPalette::Text, theme.text);
        palette.setColor(QPalette::Active, QPalette::ButtonText, theme.buttonText);
        palette.setColor(QPalette::Inactive, QPalette::ButtonText, theme.buttonText);
        palette.setColor(QPalette::Active, QPalette::Link, theme.link);
        palette.setColor(QPalette::Inactive, QPalette::Link, theme.link);
        palette.setColor(QPalette::Active, QPalette::LinkVisited, theme.linkVisited);
        palette.setColor(QPalette::Inactive, QPalette::LinkVisited, theme.linkVisited);
        palette.setColor(QPalette::Active, QPalette::PlaceholderText, theme.placeholder);
        palette.setColor(QPalette::Inactive, QPalette::PlaceholderText, theme.placeholder);
    }

    QApplication::setPalette(palette);

    QString error;
    const QString styleSheet = theme.useQss ? buildStyleSheet(theme, &error) : QString();
    if (!error.isEmpty()) {
        qWarning("Theme style sheet error: %s", qPrintable(error));
    }
    qApp->setStyleSheet(styleSheet);
}

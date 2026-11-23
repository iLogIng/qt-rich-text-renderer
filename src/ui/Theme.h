#pragma once

#include "SyntaxPalette.h"

#include <QColor>
#include <QHash>
#include <QString>
#include <QVector>

namespace ThemeSlotStyle {
inline constexpr char Property[] = "centralSlot";
inline constexpr char Top[] = "top";
inline constexpr char Bottom[] = "bottom";
inline constexpr char Empty[] = "empty";
inline constexpr char ToolbarLeading[] = "toolbarLeading";
inline constexpr char ToolbarTrailing[] = "toolbarTrailing";
inline constexpr char Content[] = "content";
inline constexpr char Preview[] = "preview";
inline constexpr char Source[] = "source";
} // namespace ThemeSlotStyle

struct SlotStyle {
    QColor background;
    QColor borderColor;
    int borderWidth = -1;
    int radius = -1;
    int padding = -1;
};

struct ThemeSpec {
    QString id;
    QString name;
    bool dark = false;
    bool useQss = false;

    QString uiFontFamily;
    QString previewFontFamily;
    QString codeFontFamily;

    QString packageDirectory;
    QString layoutFile;
    QString layoutError;
    int layoutApiVersion = 0;

    QColor window;
    QColor windowText;
    QColor base;
    QColor alternateBase;
    QColor text;
    QColor mutedText;
    QColor button;
    QColor buttonText;
    QColor brightText;
    QColor highlight;
    QColor highlightedText;
    QColor link;
    QColor linkVisited;
    QColor placeholder;
    QColor tooltipBase;
    QColor tooltipText;
    QColor border;

    CodeSyntaxPalette codeSyntax;
    MarkdownSourcePalette sourceSyntax;
    QHash<QString, SlotStyle> slotStyles;

    int radius = 6;
    int spacing = 6;
    int documentMargin = 12;
};

struct ThemeInfo {
    QString id;
    QString name;
    bool builtIn = false;
};

class ThemeManager {
public:
    static QString themeDirectory();
    static QVector<ThemeInfo> availableThemes();

    static ThemeSpec loadTheme(const QString &id, QString *error = nullptr);
    static ThemeSpec fallbackTheme();
    static QString fallbackThemeId();

    static QString buildStyleSheet(const ThemeSpec &theme, QString *error = nullptr);
    static void apply(const ThemeSpec &theme);
};

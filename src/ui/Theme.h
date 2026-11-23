#pragma once

#include <QColor>
#include <QString>
#include <QVector>

struct ThemeSpec {
    QString id;
    QString name;
    bool dark = false;
    bool useQss = false;

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

    int radius = 6;
    int spacing = 6;
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

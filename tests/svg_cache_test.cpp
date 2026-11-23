#include "RenderTheme.h"
#include "SvgCache.h"

#include <QTemporaryDir>
#include <QTextStream>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (!condition) {
        QTextStream(stderr) << "FAIL: " << message << "\n";
        ++failures;
    }
}

} // namespace

int main()
{
    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("temporary cache directory"));
    if (!directory.isValid()) {
        return 1;
    }

    SvgCache cache;
    cache.configure(directory.path(), QStringLiteral("version-a"));

    const QString mathDisplay =
        cache.key(QStringLiteral("math"), QStringLiteral("a+b"), true, QString());
    const QString mathInline =
        cache.key(QStringLiteral("math"), QStringLiteral("a+b"), false, QString());
    const QString mermaidDark =
        cache.key(QStringLiteral("mermaid"), QStringLiteral("graph TD"), true, QStringLiteral("dark"));
    const QString mermaidLight =
        cache.key(QStringLiteral("mermaid"), QStringLiteral("graph TD"), true, QStringLiteral("default"));

    expect(mathDisplay == cache.key(QStringLiteral("math"), QStringLiteral("a+b"), true, QString()),
           QStringLiteral("cache key is deterministic"));
    expect(mathDisplay != mathInline, QStringLiteral("display flag affects cache key"));
    expect(mermaidDark != mermaidLight, QStringLiteral("Mermaid theme affects cache key"));
    MermaidTheme themeA;
    themeA.name = QStringLiteral("base");
    themeA.variables.insert(QStringLiteral("primaryColor"), QStringLiteral("#111111"));
    MermaidTheme themeB = themeA;
    themeB.variables.insert(QStringLiteral("primaryColor"), QStringLiteral("#222222"));
    expect(themeA.cacheKey() != themeB.cacheKey(),
           QStringLiteral("Mermaid theme variables affect the theme signature"));
    expect(mathDisplay.size() == 64, QStringLiteral("cache key is a SHA-256 hex string"));

    QString svg;
    expect(!cache.read(mathDisplay, &svg), QStringLiteral("missing cache entry is not a hit"));

    const QString content = QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\"/>");
    cache.write(mathDisplay, content);
    expect(cache.read(mathDisplay, &svg), QStringLiteral("written cache entry is readable"));
    expect(svg == content, QStringLiteral("cache content round-trips"));

    if (failures == 0) {
        QTextStream(stdout) << "SVG cache tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}

#include "SvgSanitizer.h"

#include <QColor>
#include <QHash>
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
    const QString mermaidSvg = QStringLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 50\" "
        "style=\"--_p:#111111;--_tx:#222222;\" width=\"100%\" height=\"100%\" "
        "onclick=\"alert(1)\">"
        "<script>alert(1)</script>"
        "<rect width=\"100\" height=\"50\" fill=\"var(--_p)\"/>"
        "<text fill=\"var(--_tx)\">theme label</text>"
        "<text fill=\"currentColor\">label</text>"
        "</svg>");

    const QString light = sanitizeSvgForQt(mermaidSvg,
                                           true,
                                           QColor(QStringLiteral("#abcdef")),
                                           false);
    expect(!light.contains(QStringLiteral("<script")), QStringLiteral("script element removed"));
    expect(!light.contains(QStringLiteral("onclick")), QStringLiteral("event attribute removed"));
    expect(!light.contains(QStringLiteral("var(")), QStringLiteral("CSS variables resolved"));
    expect(light.contains(QStringLiteral("#ececff")), QStringLiteral("Mermaid light fill applied"));
    expect(light.contains(QStringLiteral("#abcdef")), QStringLiteral("currentColor replaced"));
    expect(light.contains(QStringLiteral("width=\"100.00\"")), QStringLiteral("percentage width normalized"));
    expect(light.contains(QStringLiteral("height=\"50.00\"")), QStringLiteral("percentage height normalized"));

    const QString dark = sanitizeSvgForQt(mermaidSvg,
                                          true,
                                          QColor(QStringLiteral("#abcdef")),
                                          true);
    expect(dark.contains(QStringLiteral("#2d2d30")), QStringLiteral("Mermaid dark fill applied"));
    expect(dark.contains(QStringLiteral("#e6e6e6")), QStringLiteral("Mermaid dark text color applied"));

    const QHash<QString, QString> variables = {
        {QStringLiteral("primaryColor"), QStringLiteral("#123456")},
        {QStringLiteral("textColor"), QStringLiteral("#abcdef")},
    };
    const QString custom = sanitizeSvgForQt(mermaidSvg,
                                            true,
                                            QColor(QStringLiteral("#abcdef")),
                                            false,
                                            variables);
    expect(custom.contains(QStringLiteral("#123456")),
           QStringLiteral("Mermaid primary color variable is applied"));
    expect(custom.contains(QStringLiteral("#abcdef")),
           QStringLiteral("Mermaid text color variable is applied"));

    if (failures == 0) {
        QTextStream(stdout) << "SVG sanitizer tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}

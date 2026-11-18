#include "MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QTimer>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("rich-text-render-spike"));
    app.setOrganizationName(QStringLiteral("locvas-spike"));

    QString screenshotPath;
    QString filePath;
    bool dark = false;
    bool toggleThemeAfterStart = false;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--screenshot") && i + 1 < argc) {
            screenshotPath = QString::fromLocal8Bit(argv[++i]);
        } else if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--file") && i + 1 < argc) {
            filePath = QString::fromLocal8Bit(argv[++i]);
        } else if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--dark")) {
            dark = true;
        } else if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--toggle-theme")) {
            toggleThemeAfterStart = true;
        }
    }

    MainWindow window(dark);
    window.show();
    if (toggleThemeAfterStart) {
        QTimer::singleShot(1500, &window, [&window]() {
            QMetaObject::invokeMethod(&window, "toggleTheme", Qt::DirectConnection);
        });
    }
    if (!filePath.isEmpty()) {
        window.loadFile(filePath);
    }

    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(8000, &window, [&window, screenshotPath, &app]() {
            const bool ok = window.grab().save(screenshotPath);
            QTextStream(stderr) << (ok ? "screenshot saved: " : "screenshot failed: ")
                                << screenshotPath << "\n";
            app.quit();
        });
    }

    return app.exec();
}

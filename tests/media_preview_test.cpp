#include "MediaPreviewWindow.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QGraphicsView>
#include <QImage>
#include <QLabel>
#include <QTemporaryDir>
#include <QTextStream>
#include <QWheelEvent>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (!condition) {
        QTextStream(stderr) << "FAIL: " << message << "\n";
        ++failures;
    }
}

QAction *action(MediaPreviewWindow *window, const char *name)
{
    return window ? window->findChild<QAction *>(QString::fromLatin1(name)) : nullptr;
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("temporary directory available"));
    if (!directory.isValid()) {
        return 1;
    }

    QImage sourceImage(64, 32, QImage::Format_ARGB32);
    sourceImage.fill(Qt::red);
    const QString sourcePath = directory.filePath(QStringLiteral("sample.png"));
    expect(sourceImage.save(sourcePath, "PNG"), QStringLiteral("write source image"));

    MediaPreviewWindow *imageWindow = MediaPreviewWindow::showImageFile(sourcePath, nullptr);
    expect(imageWindow != nullptr, QStringLiteral("image preview window created"));
    if (!imageWindow) {
        return 1;
    }
    QCoreApplication::processEvents();

    QAction *zoomOut = action(imageWindow, "zoomOutAction");
    QAction *zoomIn = action(imageWindow, "zoomInAction");
    QAction *fit = action(imageWindow, "fitAction");
    QAction *actualSize = action(imageWindow, "actualSizeAction");
    QAction *saveAs = action(imageWindow, "saveAsAction");
    auto *zoomLabel = imageWindow->findChild<QLabel *>(QStringLiteral("zoomLabel"));

    expect(zoomOut && zoomIn && fit && actualSize && saveAs && zoomLabel,
           QStringLiteral("preview controls found"));
    if (!zoomOut || !zoomIn || !fit || !actualSize || !saveAs || !zoomLabel) {
        delete imageWindow;
        return 1;
    }

    expect(zoomLabel->text() == QStringLiteral("100%"),
           QStringLiteral("small image starts at actual size"));

    auto *view = imageWindow->findChild<QGraphicsView *>();
    expect(view != nullptr, QStringLiteral("preview graphics view found"));
    if (view) {
        const QPoint center = view->viewport()->rect().center();
        QWheelEvent wheel(QPointF(center),
                          QPointF(view->viewport()->mapToGlobal(center)),
                          QPoint(),
                          QPoint(0, 120),
                          Qt::NoButton,
                          Qt::ControlModifier,
                          Qt::NoScrollPhase,
                          false);
        QApplication::sendEvent(view->viewport(), &wheel);
        expect(zoomLabel->text() == QStringLiteral("125%"),
               QStringLiteral("Ctrl+wheel zooms in"));
        fit->trigger();
    }

    zoomIn->trigger();
    expect(zoomLabel->text() == QStringLiteral("125%"),
           QStringLiteral("zoom in advances to 125 percent"));

    zoomOut->trigger();
    expect(zoomLabel->text() == QStringLiteral("100%"),
           QStringLiteral("zoom out returns to 100 percent"));

    actualSize->trigger();
    expect(zoomLabel->text() == QStringLiteral("100%"),
           QStringLiteral("actual size sets 100 percent"));

    fit->trigger();
    expect(zoomLabel->text() == QStringLiteral("100%"),
           QStringLiteral("fit keeps small images at 100 percent"));

    for (int i = 0; i < 24; ++i) {
        zoomIn->trigger();
    }
    expect(zoomLabel->text() == QStringLiteral("800%"),
           QStringLiteral("zoom in is clamped at 800 percent"));

    for (int i = 0; i < 80; ++i) {
        zoomOut->trigger();
    }
    expect(zoomLabel->text() == QStringLiteral("5%"),
           QStringLiteral("zoom out is clamped at 5 percent"));

    fit->trigger();
    expect(zoomLabel->text() == QStringLiteral("100%"),
           QStringLiteral("fit restores the preview"));

    const QString copiedPath = directory.filePath(QStringLiteral("sample-copy.png"));
    QString error;
    expect(imageWindow->saveTo(copiedPath, &error),
           QStringLiteral("raster image saved: %1").arg(error));
    expect(readAll(copiedPath) == readAll(sourcePath),
           QStringLiteral("same-format save preserves source bytes"));

    const QString reencodedPath = directory.filePath(QStringLiteral("sample-copy.jpg"));
    expect(imageWindow->saveTo(reencodedPath, &error),
           QStringLiteral("raster image re-encoded: %1").arg(error));
    expect(!QImage(reencodedPath).isNull(),
           QStringLiteral("re-encoded image can be decoded"));

    const QByteArray svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                           "width=\"120\" height=\"60\" viewBox=\"0 0 120 60\">"
                           "<rect width=\"120\" height=\"60\" fill=\"#ff0000\"/>"
                           "</svg>";
    MediaPreviewWindow *svgWindow =
        MediaPreviewWindow::showSvg(QString::fromUtf8(svg),
                                    QStringLiteral("SVG preview"),
                                    nullptr,
                                    QStringLiteral("diagram.svg"));
    expect(svgWindow != nullptr, QStringLiteral("SVG preview window created"));
    if (!svgWindow) {
        delete imageWindow;
        return 1;
    }
    QCoreApplication::processEvents();

    const QString svgPath = directory.filePath(QStringLiteral("diagram.svg"));
    expect(svgWindow->saveTo(svgPath, &error),
           QStringLiteral("SVG saved: %1").arg(error));
    expect(readAll(svgPath) == svg, QStringLiteral("SVG save preserves source bytes"));

    const QString pngPath = directory.filePath(QStringLiteral("diagram.png"));
    expect(svgWindow->saveTo(pngPath, &error),
           QStringLiteral("SVG exported as PNG: %1").arg(error));
    const QImage exportedPng(pngPath);
    expect(!exportedPng.isNull(), QStringLiteral("exported PNG can be decoded"));
    expect(exportedPng.size() == QSize(120, 60),
           QStringLiteral("exported PNG uses the SVG display size"));

    const QString unsupportedPath = directory.filePath(QStringLiteral("diagram.bmp"));
    expect(!svgWindow->saveTo(unsupportedPath, &error),
           QStringLiteral("unsupported SVG export format is rejected"));

    delete svgWindow;
    delete imageWindow;

    if (failures == 0) {
        QTextStream(stdout) << "PASS: media preview zoom and save controls\n";
    }
    return failures == 0 ? 0 : 1;
}

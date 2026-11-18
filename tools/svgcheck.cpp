#include "SvgSanitizer.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QFile>
#include <QTextStream>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 3) {
        QTextStream(stderr) << "usage: svgcheck <input.svg> <output.png> [math|mermaid]\n";
        return 2;
    }

    QFile file(QString::fromLocal8Bit(argv[1]));
    if (!file.open(QIODevice::ReadOnly)) {
        QTextStream(stderr) << "cannot open input\n";
        return 2;
    }
    QString svg = QString::fromUtf8(file.readAll());
    if (argc >= 4) {
        svg = sanitizeSvgForQt(svg, QString::fromLocal8Bit(argv[3]) == QStringLiteral("mermaid"));
    }

    QSvgRenderer renderer(svg.toUtf8());
    if (!renderer.isValid()) {
        QTextStream(stderr) << "QSvgRenderer: invalid SVG\n";
        return 3;
    }

    QSizeF size = renderer.defaultSize();
    if (size.isEmpty()) {
        size = renderer.viewBoxF().size();
    }
    if (size.isEmpty()) {
        size = QSizeF(800, 600);
    }
    const qreal scale = 2.0;
    QImage image((size * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&painter, QRectF(0, 0, image.width(), image.height()));
    }
    image.setDevicePixelRatio(scale);

    if (!image.save(QString::fromLocal8Bit(argv[2]))) {
        QTextStream(stderr) << "cannot save output\n";
        return 4;
    }
    QTextStream(stdout) << "ok size=" << size.width() << "x" << size.height()
                        << " image=" << image.width() << "x" << image.height() << "\n";
    return 0;
}

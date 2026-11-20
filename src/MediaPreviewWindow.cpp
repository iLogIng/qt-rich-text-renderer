#include "MediaPreviewWindow.h"

#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QScrollArea>
#include <QSvgRenderer>
#include <QVBoxLayout>

namespace {

QSize boundedPreviewSize(const QSizeF &source, const QWidget *parent)
{
    QScreen *screen = parent ? parent->screen() : QGuiApplication::primaryScreen();
    QSize limit(1200, 900);
    if (screen) {
        const QRect available = screen->availableGeometry();
        limit = QSize(qMax(320, static_cast<int>(available.width() * 0.8)),
                      qMax(240, static_cast<int>(available.height() * 0.8)));
    }

    QSize result = source.toSize();
    if (result.width() > limit.width() || result.height() > limit.height()) {
        result.scale(limit, Qt::KeepAspectRatio);
    }
    return result;
}

} // namespace

MediaPreviewWindow::MediaPreviewWindow(const QString &title, QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(title);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setAlignment(Qt::AlignCenter);

    m_label = new QLabel(m_scroll);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setBackgroundRole(QPalette::Base);
    m_label->setAutoFillBackground(true);
    m_scroll->setWidget(m_label);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_scroll);

    resize(800, 600);
}

bool MediaPreviewWindow::isPreviewableImageFile(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return false;
    }
    if (info.suffix().compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    QImageReader reader(path);
    return reader.canRead();
}

void MediaPreviewWindow::showImageFile(const QString &path, QWidget *parent)
{
    const QFileInfo info(path);
    if (!isPreviewableImageFile(path)) {
        QMessageBox::warning(parent,
                             tr("Image preview"),
                             tr("This file cannot be previewed as an image:\n\n%1")
                                 .arg(path));
        return;
    }

    if (info.suffix().compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(parent,
                                 tr("Image preview"),
                                 tr("Cannot read image file:\n\n%1").arg(path));
            return;
        }
        showSvg(QString::fromUtf8(file.readAll()),
                tr("SVG preview: %1").arg(info.fileName()),
                parent);
        return;
    }

    QImageReader reader(path);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(parent,
                             tr("Image preview"),
                             tr("Cannot decode image file:\n\n%1\n\n%2")
                                 .arg(path, reader.errorString()));
        return;
    }

    const QSize previewSize = boundedPreviewSize(image.size(), parent);
    const QImage scaled = image.scaled(previewSize,
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);
    auto *window = new MediaPreviewWindow(tr("Image preview: %1").arg(info.fileName()),
                                          parent);
    window->setPixmap(QPixmap::fromImage(scaled));
    window->show();
}

void MediaPreviewWindow::showSvg(const QString &svg, const QString &title, QWidget *parent)
{
    QSvgRenderer renderer(svg.toUtf8());
    if (!renderer.isValid()) {
        QMessageBox::warning(parent,
                             tr("SVG preview"),
                             tr("The rendered SVG is not valid and cannot be previewed."));
        return;
    }

    QSizeF sourceSize = renderer.defaultSize();
    if (sourceSize.isEmpty()) {
        sourceSize = renderer.viewBoxF().size();
    }
    if (sourceSize.isEmpty()) {
        sourceSize = QSizeF(800, 600);
    }

    const QSize previewSize = boundedPreviewSize(sourceSize, parent);
    QScreen *screen = parent ? parent->screen() : QGuiApplication::primaryScreen();
    const qreal ratio = screen ? screen->devicePixelRatio() : 1.0;
    const QSize pixelSize(qRound(previewSize.width() * ratio),
                          qRound(previewSize.height() * ratio));
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&painter, QRectF(0, 0, image.width(), image.height()));
    }
    image.setDevicePixelRatio(ratio);

    auto *window = new MediaPreviewWindow(title, parent);
    window->setPixmap(QPixmap::fromImage(image));
    window->show();
}

void MediaPreviewWindow::setPixmap(const QPixmap &pixmap)
{
    m_label->setPixmap(pixmap);
    m_label->resize(pixmap.size() / pixmap.devicePixelRatio());
    const QSize target = (pixmap.size() / pixmap.devicePixelRatio()).boundedTo(QSize(1200, 900));
    resize(target.expandedTo(QSize(320, 240)));
}

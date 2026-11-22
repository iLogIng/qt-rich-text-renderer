#include "MediaPreviewWindow.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsSvgItem>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSaveFile>
#include <QScreen>
#include <QScrollBar>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {

constexpr qreal kMinZoom = 0.05;
constexpr qreal kMaxZoom = 8.0;
constexpr qreal kZoomStep = 1.25;

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

QString normalizedSuffix(QString suffix)
{
    suffix = suffix.toLower();
    if (suffix == QStringLiteral("jpeg")) {
        return QStringLiteral("jpg");
    }
    if (suffix == QStringLiteral("tif")) {
        return QStringLiteral("tiff");
    }
    return suffix;
}

bool sameImageFormat(const QString &left, const QString &right)
{
    return !left.isEmpty() && normalizedSuffix(left) == normalizedSuffix(right);
}

bool writeBytes(const QByteArray &data, const QString &path, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    if (file.write(data) != data.size()) {
        if (error) {
            *error = file.errorString();
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
}

bool copyFileBytes(const QString &source, const QString &target, QString *error)
{
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = input.errorString();
        }
        return false;
    }

    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = output.errorString();
        }
        return false;
    }

    while (!input.atEnd()) {
        const QByteArray chunk = input.read(1024 * 1024);
        if (chunk.isEmpty() && input.error() != QFileDevice::NoError) {
            if (error) {
                *error = input.errorString();
            }
            output.cancelWriting();
            return false;
        }
        if (output.write(chunk) != chunk.size()) {
            if (error) {
                *error = output.errorString();
            }
            output.cancelWriting();
            return false;
        }
    }

    if (!output.commit()) {
        if (error) {
            *error = output.errorString();
        }
        return false;
    }
    return true;
}

QString firstFilterSuffix(const QString &filter)
{
    const int marker = filter.indexOf(QStringLiteral("*."));
    if (marker < 0) {
        return {};
    }
    int end = marker + 2;
    while (end < filter.size()
           && (filter.at(end).isLetterOrNumber() || filter.at(end) == QLatin1Char('_'))) {
        ++end;
    }
    return filter.mid(marker + 2, end - marker - 2).toLower();
}

} // namespace

MediaPreviewWindow::MediaPreviewWindow(const QString &title, QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(title);

    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto *zoomOutAction = new QAction(QStringLiteral("−"), this);
    zoomOutAction->setObjectName(QStringLiteral("zoomOutAction"));
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    zoomOutAction->setToolTip(tr("Zoom out (Ctrl+-)"));
    connect(zoomOutAction, &QAction::triggered, this, [this] {
        zoomBy(1.0 / kZoomStep);
    });
    m_toolbar->addAction(zoomOutAction);

    m_zoomLabel = new QLabel(QStringLiteral("100%"), this);
    m_zoomLabel->setObjectName(QStringLiteral("zoomLabel"));
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setMinimumWidth(56);
    m_toolbar->addWidget(m_zoomLabel);

    auto *zoomInAction = new QAction(QStringLiteral("+"), this);
    zoomInAction->setObjectName(QStringLiteral("zoomInAction"));
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    zoomInAction->setToolTip(tr("Zoom in (Ctrl++)"));
    connect(zoomInAction, &QAction::triggered, this, [this] {
        zoomBy(kZoomStep);
    });
    m_toolbar->addAction(zoomInAction);

    auto *fitAction = new QAction(tr("Fit"), this);
    fitAction->setObjectName(QStringLiteral("fitAction"));
    fitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    fitAction->setToolTip(tr("Fit image to window (Ctrl+0)"));
    connect(fitAction, &QAction::triggered, this, &MediaPreviewWindow::applyFit);
    m_toolbar->addAction(fitAction);

    auto *actualSizeAction = new QAction(QStringLiteral("1:1"), this);
    actualSizeAction->setObjectName(QStringLiteral("actualSizeAction"));
    actualSizeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+1")));
    actualSizeAction->setToolTip(tr("Actual size (Ctrl+1)"));
    connect(actualSizeAction, &QAction::triggered, this, [this] {
        setZoom(1.0);
    });
    m_toolbar->addAction(actualSizeAction);

    m_toolbar->addSeparator();

    auto *saveAsAction = new QAction(tr("Save As…"), this);
    saveAsAction->setObjectName(QStringLiteral("saveAsAction"));
    saveAsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    saveAsAction->setToolTip(tr("Save image as (Ctrl+S)"));
    connect(saveAsAction, &QAction::triggered, this, &MediaPreviewWindow::saveAs);
    m_toolbar->addAction(saveAsAction);

    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setAlignment(Qt::AlignCenter);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform, true);
    m_view->setBackgroundBrush(palette().brush(QPalette::Base));
    m_view->viewport()->installEventFilter(this);
    m_view->installEventFilter(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_toolbar);
    layout->addWidget(m_view);

    resize(800, 600);
}

MediaPreviewWindow::~MediaPreviewWindow()
{
    if (m_scene) {
        m_scene->clear();
    }
    delete m_svgRenderer;
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

MediaPreviewWindow *MediaPreviewWindow::showImageFile(const QString &path, QWidget *parent)
{
    const QFileInfo info(path);
    if (!isPreviewableImageFile(path)) {
        QMessageBox::warning(parent,
                             tr("Image preview"),
                             tr("This file cannot be previewed as an image:\n\n%1")
                                 .arg(path));
        return nullptr;
    }

    if (info.suffix().compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(parent,
                                 tr("Image preview"),
                                 tr("Cannot read image file:\n\n%1").arg(path));
            return nullptr;
        }
        return showSvg(QString::fromUtf8(file.readAll()),
                       tr("SVG preview: %1").arg(info.fileName()),
                       parent,
                       info.fileName());
    }

    QImageReader reader(path);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(parent,
                             tr("Image preview"),
                             tr("Cannot decode image file:\n\n%1\n\n%2")
                                 .arg(path, reader.errorString()));
        return nullptr;
    }

    auto *window = new MediaPreviewWindow(tr("Image preview: %1").arg(info.fileName()),
                                          parent);
    window->setImage(image, path);
    window->show();
    return window;
}

MediaPreviewWindow *MediaPreviewWindow::showSvg(const QString &svg,
                                                const QString &title,
                                                QWidget *parent,
                                                const QString &suggestedName)
{
    const QByteArray data = svg.toUtf8();
    QSvgRenderer renderer(data);
    if (!renderer.isValid()) {
        QMessageBox::warning(parent,
                             tr("SVG preview"),
                             tr("The rendered SVG is not valid and cannot be previewed."));
        return nullptr;
    }

    auto *window = new MediaPreviewWindow(title, parent);
    window->setSvg(data, suggestedName);
    window->show();
    return window;
}

void MediaPreviewWindow::setImage(const QImage &image, const QString &sourcePath)
{
    m_scene->clear();
    delete m_svgRenderer;
    m_svgRenderer = nullptr;
    m_svg.clear();

    m_sourceImage = image;
    m_sourcePath = sourcePath;
    m_suggestedName = QFileInfo(sourcePath).fileName();
    if (m_suggestedName.isEmpty()) {
        m_suggestedName = QStringLiteral("image.png");
    }

    auto *item = m_scene->addPixmap(QPixmap::fromImage(image));
    item->setTransformationMode(Qt::SmoothTransformation);
    m_item = item;
    m_scene->setSceneRect(item->boundingRect());

    resizeWindowForContent(QSizeF(image.size()));
    applyFit();
}

void MediaPreviewWindow::setSvg(const QByteArray &svg, const QString &suggestedName)
{
    m_scene->clear();
    delete m_svgRenderer;
    m_svgRenderer = new QSvgRenderer(svg);
    m_svg = svg;
    m_sourceImage = QImage();
    m_sourcePath.clear();
    m_suggestedName = suggestedName.isEmpty() ? QStringLiteral("diagram.svg") : suggestedName;

    auto *item = new QGraphicsSvgItem();
    item->setSharedRenderer(m_svgRenderer);
    m_scene->addItem(item);
    m_item = item;
    m_scene->setSceneRect(item->boundingRect());

    QSizeF sourceSize = m_svgRenderer->defaultSize();
    if (sourceSize.isEmpty()) {
        sourceSize = m_svgRenderer->viewBoxF().size();
    }
    if (sourceSize.isEmpty()) {
        sourceSize = item->boundingRect().size();
    }
    if (sourceSize.isEmpty()) {
        sourceSize = QSizeF(800, 600);
    }

    resizeWindowForContent(sourceSize);
    applyFit();
}

void MediaPreviewWindow::resizeWindowForContent(const QSizeF &sourceSize)
{
    const QSize previewSize =
        boundedPreviewSize(sourceSize, this).expandedTo(QSize(360, 260));
    const int toolbarHeight = m_toolbar ? m_toolbar->sizeHint().height() : 0;
    resize(previewSize.width(), previewSize.height() + toolbarHeight);
}

void MediaPreviewWindow::applyFit()
{
    if (!m_item || !m_view) {
        return;
    }

    m_fitMode = true;
    m_view->resetTransform();
    m_view->fitInView(m_item, Qt::KeepAspectRatio);
    if (m_view->transform().m11() > 1.0) {
        m_view->resetTransform();
        m_view->centerOn(m_item);
    }
    m_zoom = m_view->transform().m11();
    updateZoomUi();
}

void MediaPreviewWindow::setZoom(qreal zoom, bool anchorUnderMouse)
{
    if (!m_item || !m_view) {
        return;
    }

    const qreal target = qBound(kMinZoom, zoom, kMaxZoom);
    m_fitMode = false;
    if (qFuzzyCompare(target, m_zoom)) {
        updateZoomUi();
        return;
    }

    const auto previousAnchor = m_view->transformationAnchor();
    m_view->setTransformationAnchor(anchorUnderMouse ? QGraphicsView::AnchorUnderMouse
                                                     : QGraphicsView::AnchorViewCenter);
    m_zoom = target;
    m_view->setTransform(QTransform::fromScale(m_zoom, m_zoom));
    m_view->setTransformationAnchor(previousAnchor);
    updateZoomUi();
}

void MediaPreviewWindow::zoomBy(qreal factor, bool anchorUnderMouse)
{
    setZoom(m_zoom * factor, anchorUnderMouse);
}

void MediaPreviewWindow::updateZoomUi()
{
    if (m_zoomLabel) {
        m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(m_zoom * 100)));
    }
}

void MediaPreviewWindow::saveAs()
{
    QStringList filters;
    QString defaultBaseName;
    QString defaultSuffix;
    QString defaultDirectory;

    if (!m_svg.isEmpty()) {
        defaultDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        defaultBaseName = QFileInfo(m_suggestedName).completeBaseName();
        if (defaultBaseName.isEmpty()) {
            defaultBaseName = QStringLiteral("diagram");
        }
        defaultSuffix = QStringLiteral("svg");
        filters << tr("SVG image (*.svg)")
                << tr("PNG image (*.png)")
                << tr("All files (*)");
    } else {
        const QFileInfo sourceInfo(m_sourcePath);
        defaultDirectory = sourceInfo.absolutePath();
        defaultBaseName = sourceInfo.completeBaseName();
        if (defaultBaseName.isEmpty()) {
            defaultBaseName = QStringLiteral("image");
        }
        const QString sourceSuffix = sourceInfo.suffix().toLower();
        defaultSuffix = sourceSuffix.isEmpty() ? QStringLiteral("png") : sourceSuffix;
        if (!sourceSuffix.isEmpty()) {
            filters << tr("Original format (*.%1)").arg(sourceSuffix);
        }
        if (sourceSuffix != QStringLiteral("png")) {
            filters << tr("PNG image (*.png)");
        }
        filters << tr("All files (*)");
    }

    if (defaultDirectory.isEmpty()) {
        defaultDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }
    if (defaultDirectory.isEmpty()) {
        defaultDirectory = QDir::homePath();
    }

    QFileDialog dialog(this,
                       tr("Save image as"),
                       QDir(defaultDirectory).filePath(defaultBaseName));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.selectNameFilter(filters.first());
    dialog.setDefaultSuffix(defaultSuffix);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    connect(&dialog, &QFileDialog::filterSelected, &dialog, [&dialog](const QString &filter) {
        const QString suffix = firstFilterSuffix(filter);
        if (!suffix.isEmpty()) {
            dialog.setDefaultSuffix(suffix);
        }
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QStringList selectedFiles = dialog.selectedFiles();
    if (selectedFiles.isEmpty()) {
        return;
    }

    QString path = selectedFiles.first();
    if (QFileInfo(path).suffix().isEmpty() && !dialog.defaultSuffix().isEmpty()) {
        path += QLatin1Char('.') + dialog.defaultSuffix();
    }

    QString error;
    if (!saveTo(path, &error)) {
        QMessageBox::warning(this,
                             tr("Save image"),
                             tr("Cannot save image:\n\n%1\n\n%2").arg(path, error));
    }
}

bool MediaPreviewWindow::saveTo(const QString &path, QString *error) const
{
    if (path.isEmpty()) {
        if (error) {
            *error = tr("Empty file name.");
        }
        return false;
    }

    const QFileInfo target(path);
    if (!m_svg.isEmpty()) {
        const QString suffix = target.suffix().toLower();
        if (suffix == QStringLiteral("svg")) {
            return writeBytes(m_svg, path, error);
        }
        if (suffix == QStringLiteral("png")) {
            QSvgRenderer renderer(m_svg);
            if (!renderer.isValid()) {
                if (error) {
                    *error = tr("The SVG data is not valid.");
                }
                return false;
            }

            QSizeF sourceSize = renderer.defaultSize();
            if (sourceSize.isEmpty()) {
                sourceSize = renderer.viewBoxF().size();
            }
            if (sourceSize.isEmpty()) {
                sourceSize = QSizeF(800, 600);
            }
            const QSize imageSize(qMax(1, qCeil(sourceSize.width())),
                                  qMax(1, qCeil(sourceSize.height())));
            QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            {
                QPainter painter(&image);
                renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(imageSize)));
            }
            if (!image.save(path, "PNG")) {
                if (error) {
                    *error = tr("Cannot encode PNG image.");
                }
                return false;
            }
            return true;
        }

        if (error) {
            *error = tr("Unsupported image format: %1").arg(target.suffix());
        }
        return false;
    }

    if (!m_sourceImage.isNull()) {
        const QString sourceSuffix = QFileInfo(m_sourcePath).suffix();
        if (!m_sourcePath.isEmpty() && sameImageFormat(sourceSuffix, target.suffix())) {
            if (target.absoluteFilePath() == QFileInfo(m_sourcePath).absoluteFilePath()) {
                return true;
            }
            return copyFileBytes(m_sourcePath, path, error);
        }

        QImageWriter writer(path);
        writer.setQuality(95);
        if (!writer.write(m_sourceImage)) {
            if (error) {
                *error = writer.errorString();
            }
            return false;
        }
        return true;
    }

    if (error) {
        *error = tr("No image data is available.");
    }
    return false;
}

bool MediaPreviewWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view && event->type() == QEvent::Resize) {
        if (m_fitMode) {
            QTimer::singleShot(0, this, [this] {
                if (m_fitMode) {
                    applyFit();
                }
            });
        }
    } else if (watched == m_view->viewport() && event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            const int delta = wheel->angleDelta().y() != 0 ? wheel->angleDelta().y()
                                                           : wheel->pixelDelta().y();
            if (delta != 0) {
                zoomBy(delta > 0 ? kZoomStep : 1.0 / kZoomStep, true);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MediaPreviewWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_fitMode) {
        QTimer::singleShot(0, this, [this] {
            if (m_fitMode) {
                applyFit();
            }
        });
    }
}

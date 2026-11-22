#pragma once

#include <QByteArray>
#include <QImage>
#include <QWidget>

class QEvent;
class QGraphicsItem;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QShowEvent;
class QSvgRenderer;
class QToolBar;

class MediaPreviewWindow : public QWidget {
    Q_OBJECT

public:
    static bool isPreviewableImageFile(const QString &path);
    static MediaPreviewWindow *showImageFile(const QString &path, QWidget *parent);
    static MediaPreviewWindow *showSvg(const QString &svg,
                                       const QString &title,
                                       QWidget *parent,
                                       const QString &suggestedName = QString());

    bool saveTo(const QString &path, QString *error = nullptr) const;

    ~MediaPreviewWindow() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    explicit MediaPreviewWindow(const QString &title, QWidget *parent = nullptr);

    void setImage(const QImage &image, const QString &sourcePath);
    void setSvg(const QByteArray &svg, const QString &suggestedName);
    void resizeWindowForContent(const QSizeF &sourceSize);
    void applyFit();
    void setZoom(qreal zoom, bool anchorUnderMouse = false);
    void zoomBy(qreal factor, bool anchorUnderMouse = false);
    void updateZoomUi();
    void saveAs();

    QGraphicsView *m_view = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QGraphicsItem *m_item = nullptr;
    QSvgRenderer *m_svgRenderer = nullptr;
    QToolBar *m_toolbar = nullptr;
    QLabel *m_zoomLabel = nullptr;

    QImage m_sourceImage;
    QByteArray m_svg;
    QString m_sourcePath;
    QString m_suggestedName;

    qreal m_zoom = 1.0;
    bool m_fitMode = true;
};

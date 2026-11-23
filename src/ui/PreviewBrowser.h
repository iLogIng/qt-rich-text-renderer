#pragma once

#include <QMargins>
#include <QTextBrowser>
#include <QUrl>

class QMouseEvent;
class QResizeEvent;

class PreviewBrowser : public QTextBrowser {
    Q_OBJECT

public:
    explicit PreviewBrowser(QWidget *parent = nullptr);

    QMargins contentMargins() const;
    void setContentMargins(const QMargins &margins);

signals:
    void imagePreviewRequested(const QUrl &url);
    void svgPreviewRequested(const QString &svg);
    void viewportResized();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    bool tryPreviewMedia(const QPoint &position);
};

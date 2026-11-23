#pragma once

#include <QTextBrowser>
#include <QUrl>

class QMouseEvent;

class PreviewBrowser : public QTextBrowser {
    Q_OBJECT

public:
    explicit PreviewBrowser(QWidget *parent = nullptr);

signals:
    void imagePreviewRequested(const QUrl &url);
    void svgPreviewRequested(const QString &svg);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    bool tryPreviewMedia(const QPoint &position);
};

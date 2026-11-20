#pragma once

#include <QWidget>

class QLabel;
class QPixmap;
class QScrollArea;

class MediaPreviewWindow : public QWidget {
    Q_OBJECT

public:
    static bool isPreviewableImageFile(const QString &path);
    static void showImageFile(const QString &path, QWidget *parent);
    static void showSvg(const QString &svg, const QString &title, QWidget *parent);

private:
    explicit MediaPreviewWindow(const QString &title, QWidget *parent = nullptr);

    void setPixmap(const QPixmap &pixmap);

    QScrollArea *m_scroll = nullptr;
    QLabel *m_label = nullptr;
};

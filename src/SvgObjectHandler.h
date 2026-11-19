#pragma once

#include <QHash>
#include <QObject>
#include <QSizeF>
#include <QTextFormat>
#include <QTextObjectInterface>

class QPainter;
class QRectF;
class QSvgRenderer;
class QTextDocument;

class SvgObjectHandler : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    enum {
        Type = QTextFormat::UserObject + 0x5347,
        SvgProperty = QTextFormat::UserProperty + 0x100,
        WidthProperty = QTextFormat::UserProperty + 0x101,
        HeightProperty = QTextFormat::UserProperty + 0x102,
    };

    explicit SvgObjectHandler(QObject *parent = nullptr);
    ~SvgObjectHandler() override;

    QSizeF intrinsicSize(QTextDocument *document,
                         int positionInDocument,
                         const QTextFormat &format) override;

    void drawObject(QPainter *painter,
                    const QRectF &rect,
                    QTextDocument *document,
                    int positionInDocument,
                    const QTextFormat &format) override;

private:
    QHash<QString, QSvgRenderer *> m_renderers;
};

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QTextBrowser;
class QWidget;

class MarkdownLinkHandler : public QObject {
    Q_OBJECT

public:
    enum class Kind {
        Anchor,
        Remote,
        LocalFile,
        Mail,
        Empty,
        Rejected,
    };

    struct Resolved {
        Kind kind = Kind::Empty;
        QUrl url;
        QString path;
        QString error;
    };

    MarkdownLinkHandler(QTextBrowser *view, QWidget *dialogParent, QObject *parent = nullptr);

    void setBaseUrl(const QUrl &baseUrl);
    QUrl baseUrl() const;

    static Resolved resolve(const QUrl &url, const QUrl &baseUrl);
    static void scrollToAnchor(QTextBrowser *view, const QString &fragment);

signals:
    void localFileRequested(const QString &path, const QString &fragment);

public slots:
    void handleUrl(const QUrl &url);

private:
    QTextBrowser *m_view = nullptr;
    QWidget *m_dialogParent = nullptr;
    QUrl m_baseUrl;
};

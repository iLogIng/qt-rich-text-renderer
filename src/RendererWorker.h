#pragma once

#include "RenderTypes.h"

#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QString>
#include <functional>

class QProcess;
class QTimer;

struct RendererJob {
    QString id;
    QString source;
    bool display = true;
    QString theme;
    quint64 generation = 0;
    std::function<void(const RenderResult &)> callback;
};

class RendererWorker : public QObject {
    Q_OBJECT

public:
    explicit RendererWorker(QString kind, QObject *parent = nullptr);

    void configure(const QString &qjsPath,
                   const QString &scriptPath,
                   int requestTimeoutMs = 10000,
                   int idleMs = 60000);

    void submit(RendererJob job);
    void cancelGeneration(quint64 generation);
    void shutdown();

    bool isRunning() const;
    bool isReady() const;
    int pendingCount() const;

signals:
    void logLine(const QString &line);

private:
    void ensureStarted();
    void startNext();
    void writeCurrent();
    void handleStdout();
    void handleLine(const QByteArray &line);
    void finishCurrent(const RenderResult &result);
    void stopProcess();
    void scheduleIdle();
    void failCurrentAndQueue(const QString &error);

    QString m_kind;
    QString m_qjsPath;
    QString m_scriptPath;
    int m_requestTimeoutMs = 10000;
    int m_idleMs = 60000;

    QProcess *m_process = nullptr;
    QTimer *m_startTimer = nullptr;
    QTimer *m_requestTimer = nullptr;
    QTimer *m_idleTimer = nullptr;

    QQueue<RendererJob> m_queue;
    RendererJob m_current;
    bool m_hasCurrent = false;
    bool m_ready = false;
    bool m_stopping = false;
    bool m_timedOut = false;
    quint64 m_generation = 0;
    QByteArray m_stdoutBuffer;
};

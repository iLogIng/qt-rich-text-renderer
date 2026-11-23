#include "RendererWorker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <QDebug>

#include <utility>

#ifdef Q_OS_UNIX
#include <sys/resource.h>
#endif

namespace {

constexpr int kStartTimeoutMs = 5000;
constexpr int kDefaultRequestTimeoutMs = 10000;
constexpr int kDefaultIdleMs = 60000;
constexpr int kStopGraceMs = 1000;
constexpr int kAddressSpaceLimitBytes = 256 * 1024 * 1024;
constexpr int kCpuLimitSeconds = 60;

QByteArray makeRequestLine(const RendererJob &job)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), job.id);
    object.insert(QStringLiteral("source"), job.source);
    if (job.theme.isEmpty()) {
        object.insert(QStringLiteral("display"), job.display);
    } else {
        object.insert(QStringLiteral("theme"), job.theme);
        if (!job.themeVariables.isEmpty()) {
            QJsonObject variables;
            for (auto it = job.themeVariables.constBegin();
                 it != job.themeVariables.constEnd();
                 ++it) {
                variables.insert(it.key(), it.value());
            }
            object.insert(QStringLiteral("themeVariables"), variables);
        }
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

} // namespace

RendererWorker::RendererWorker(QString kind, QObject *parent)
    : QObject(parent)
    , m_kind(std::move(kind))
{
    m_startTimer = new QTimer(this);
    m_startTimer->setSingleShot(true);
    connect(m_startTimer, &QTimer::timeout, this, [this]() {
        if (m_ready) {
            return;
        }
        const QString error = QStringLiteral("%1 worker did not become ready in %2ms")
                                  .arg(m_kind)
                                  .arg(kStartTimeoutMs);
        failCurrentAndQueue(error);
        if (m_process) {
            m_stopping = true;
            m_process->kill();
        }
    });

    m_requestTimer = new QTimer(this);
    m_requestTimer->setSingleShot(true);
    connect(m_requestTimer, &QTimer::timeout, this, [this]() {
        if (!m_hasCurrent) {
            return;
        }
        m_timedOut = true;
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        } else {
            RenderResult result;
            result.error = QStringLiteral("%1 renderer timed out").arg(m_kind);
            finishCurrent(result);
        }
    });

    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    connect(m_idleTimer, &QTimer::timeout, this, [this]() {
        if (!m_hasCurrent && m_queue.isEmpty()) {
            stopProcess();
        }
    });
}

void RendererWorker::configure(const QString &qjsPath,
                               const QString &scriptPath,
                               int requestTimeoutMs,
                               int idleMs)
{
    m_qjsPath = qjsPath;
    m_scriptPath = scriptPath;
    m_requestTimeoutMs = requestTimeoutMs > 0 ? requestTimeoutMs : kDefaultRequestTimeoutMs;
    m_idleMs = idleMs > 0 ? idleMs : kDefaultIdleMs;
}

void RendererWorker::submit(RendererJob job)
{
    if (job.generation != m_generation) {
        return;
    }
    m_idleTimer->stop();
    m_queue.enqueue(std::move(job));
    startNext();
}

void RendererWorker::cancelGeneration(quint64 generation)
{
    m_generation = generation;

    QQueue<RendererJob> kept;
    while (!m_queue.isEmpty()) {
        RendererJob job = m_queue.dequeue();
        if (job.generation == generation) {
            kept.enqueue(std::move(job));
        }
    }
    m_queue = std::move(kept);

    // Do not kill the worker for a stale current request. Let it finish and
    // discard its callback; killing here would spawn a replacement while the
    // old qjs process is still dying, defeating the <=2 process bound.
    if (m_hasCurrent && m_current.generation != generation) {
        m_timedOut = false;
    }
}

void RendererWorker::shutdown()
{
    m_queue.clear();
    m_hasCurrent = false;
    m_stopping = true;
    if (m_process && m_process->state() != QProcess::NotRunning && m_ready) {
        m_process->write("{\"cmd\":\"shutdown\"}\n");
        m_process->closeWriteChannel();
    }
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}

void RendererWorker::ensureStarted()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        return;
    }
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }

    m_ready = false;
    m_stopping = false;
    m_timedOut = false;
    m_stdoutBuffer.clear();

    m_process = new QProcess(this);
    m_process->setProgram(m_qjsPath);
    m_process->setArguments({QStringLiteral("--std"), m_scriptPath});
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

#ifdef Q_OS_UNIX
    m_process->setChildProcessModifier([]() {
        struct rlimit addressSpace {};
        addressSpace.rlim_cur = kAddressSpaceLimitBytes;
        addressSpace.rlim_max = kAddressSpaceLimitBytes;
        setrlimit(RLIMIT_AS, &addressSpace);

        struct rlimit cpu {};
        cpu.rlim_cur = kCpuLimitSeconds;
        cpu.rlim_max = kCpuLimitSeconds;
        setrlimit(RLIMIT_CPU, &cpu);
    });
#endif

    connect(m_process, &QProcess::readyReadStandardOutput, this, &RendererWorker::handleStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        const QByteArray data = m_process->readAllStandardError();
        if (!data.trimmed().isEmpty()) {
            emit logLine(QString::fromUtf8(data).trimmed());
        }
    });
    connect(m_process, &QProcess::started, this, [this]() {
        m_startTimer->stop();
        m_startTimer->start(kStartTimeoutMs);
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
                Q_UNUSED(status);
                QProcess *finishedProcess = m_process;
                m_ready = false;
                m_startTimer->stop();
                m_requestTimer->stop();

                // Read any final protocol line before detaching the process.
                // finishCurrent() may start a new worker, so keep the old
                // pointer separate from m_process.
                handleStdout();

                if (m_process == finishedProcess) {
                    m_process = nullptr;
                }

                if (m_hasCurrent) {
                    RenderResult result;
                    if (m_timedOut) {
                        result.error = QStringLiteral("%1 renderer timed out").arg(m_kind);
                    } else {
                        const QByteArray err = finishedProcess ? finishedProcess->readAllStandardError()
                                                               : QByteArray();
                        result.error = QString::fromUtf8(err).trimmed();
                        if (result.error.isEmpty()) {
                            result.error = QStringLiteral("%1 renderer exited with code %2")
                                               .arg(m_kind)
                                               .arg(code);
                        }
                    }
                    finishCurrent(result);
                }

                m_timedOut = false;
                if (finishedProcess) {
                    finishedProcess->deleteLater();
                }

                m_stopping = false;
                if (!m_queue.isEmpty()) {
                    startNext();
                }
            });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            failCurrentAndQueue(QStringLiteral("%1 renderer failed to start: %2")
                                    .arg(m_kind, m_process ? m_process->errorString() : QString()));
            if (m_process) {
                m_stopping = true;
                m_process->kill();
            }
        }
    });

    m_startTimer->start(kStartTimeoutMs);
    m_process->start();
}

void RendererWorker::startNext()
{
    if (m_hasCurrent || m_queue.isEmpty()) {
        return;
    }

    m_current = m_queue.dequeue();
    m_hasCurrent = true;

    if (!m_process || m_process->state() == QProcess::NotRunning) {
        ensureStarted();
    }
    if (m_ready) {
        writeCurrent();
    }
}

void RendererWorker::writeCurrent()
{
    if (!m_hasCurrent || !m_ready || !m_process
        || m_process->state() == QProcess::NotRunning) {
        return;
    }

    m_timedOut = false;
    const QByteArray line = makeRequestLine(m_current);
    if (m_process->write(line) < 0) {
        RenderResult result;
        result.error = QStringLiteral("failed to write request to %1 worker").arg(m_kind);
        finishCurrent(result);
        return;
    }
    m_requestTimer->start(m_requestTimeoutMs);
}

void RendererWorker::handleStdout()
{
    if (!m_process) {
        return;
    }
    m_stdoutBuffer += m_process->readAllStandardOutput();

    int newline = -1;
    while ((newline = m_stdoutBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_stdoutBuffer.left(newline);
        m_stdoutBuffer.remove(0, newline + 1);
        if (!line.trimmed().isEmpty()) {
            handleLine(line);
        }
    }
}

void RendererWorker::handleLine(const QByteArray &line)
{
    QJsonParseError parseError {};
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit logLine(QStringLiteral("invalid worker output: %1")
                         .arg(QString::fromUtf8(line)));
        return;
    }

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("type")).toString() == QStringLiteral("ready")) {
        m_ready = true;
        m_startTimer->stop();
        if (m_hasCurrent) {
            writeCurrent();
        } else if (!m_queue.isEmpty()) {
            startNext();
        }
        return;
    }

    const QString id = object.value(QStringLiteral("id")).toString();
    if (!m_hasCurrent || id != m_current.id) {
        emit logLine(QStringLiteral("unexpected worker result id: %1").arg(id));
        return;
    }

    RenderResult result;
    result.svg = object.value(QStringLiteral("svg")).toString();
    result.error = object.value(QStringLiteral("error")).toString();
    result.elapsedMs = static_cast<qint64>(object.value(QStringLiteral("ms")).toDouble());
    finishCurrent(result);
}

void RendererWorker::finishCurrent(const RenderResult &result)
{
    if (!m_hasCurrent) {
        return;
    }
    m_requestTimer->stop();
    const RendererJob job = m_current;
    m_hasCurrent = false;
    m_current = {};

    if (job.generation == m_generation && job.callback) {
        job.callback(result);
    }

    if (!m_queue.isEmpty()) {
        startNext();
    } else {
        scheduleIdle();
    }
}

void RendererWorker::stopProcess()
{
    m_stopping = true;
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        if (m_process) {
            m_process->deleteLater();
            m_process = nullptr;
        }
        m_stopping = false;
        return;
    }

    if (m_ready) {
        m_process->write("{\"cmd\":\"shutdown\"}\n");
        m_process->closeWriteChannel();
    }
    QTimer::singleShot(kStopGraceMs, m_process, [process = m_process]() {
        if (process && process->state() != QProcess::NotRunning) {
            process->kill();
        }
    });
}

void RendererWorker::scheduleIdle()
{
    if (!m_hasCurrent && m_queue.isEmpty() && m_process
        && m_process->state() != QProcess::NotRunning) {
        m_idleTimer->start(m_idleMs);
    }
}

void RendererWorker::failCurrentAndQueue(const QString &error)
{
    QQueue<RendererJob> failed;
    failed.swap(m_queue);

    if (m_hasCurrent) {
        RenderResult result;
        result.error = error;
        finishCurrent(result);
    }

    while (!failed.isEmpty()) {
        RendererJob job = failed.dequeue();
        if (job.callback) {
            RenderResult result;
            result.error = error;
            job.callback(result);
        }
    }
}

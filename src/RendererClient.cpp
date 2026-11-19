#include "RendererClient.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QDebug>
#include <qobject.h>
#include <qprocess.h>
#include <qtimer.h>

RendererClient::RendererClient(QObject *parent)
    : QObject(parent)
{
}

void RendererClient::configure(const QString &qjsPath,
                               const QString &mathScript,
                               const QString &mermaidScript,
                               const QString &cacheDir,
                               const QString &version)
{
    m_qjsPath = qjsPath;
    m_mathScript = mathScript;
    m_mermaidScript = mermaidScript;
    m_cacheDir = cacheDir;
    m_version = version;
    QDir().mkpath(m_cacheDir);
}

void RendererClient::cancelAll()
{
    ++m_generation;
    const auto active = m_activeProcesses;
    for (QProcess *process : active) {
        process->setProperty("locvasCancelled", true);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
        }
    }
}

void RendererClient::renderMath(const QString &tex, bool display, Callback callback)
{
    render(QStringLiteral("math"), tex, m_mathScript,
           {display ? QStringLiteral("display") : QStringLiteral("inline")},
           std::move(callback));
}

void RendererClient::renderMermaid(const QString &source, bool dark, Callback callback)
{
    render(QStringLiteral("mermaid"), source, m_mermaidScript,
           {dark ? QStringLiteral("dark") : QStringLiteral("default")},
           std::move(callback));
}

QString RendererClient::cacheKey(const QString &kind,
                                 const QString &source,
                                 const QStringList &extraArgs) const
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData("rich-text-render-spike-v1");
    hash.addData(kind.toUtf8());
    hash.addData(m_version.toUtf8());
    hash.addData(source.toUtf8());
    for (const QString &arg : extraArgs) {
        hash.addData(arg.toUtf8());
    }
    return QString::fromLatin1(hash.result().toHex());
}

void RendererClient::render(const QString &kind,
                            const QString &source,
                            const QString &script,
                            const QStringList &extraArgs,
                            Callback callback)
{
    const quint64 generation = m_generation;
    const QString key = cacheKey(kind, source, extraArgs);
    const QString cachePath = QDir(m_cacheDir).filePath(key + QStringLiteral(".svg"));

    if (QFileInfo::exists(cachePath)) {
        QFile cache(cachePath);
        if (cache.open(QIODevice::ReadOnly)) {
            RenderResult result;
            result.svg = QString::fromUtf8(cache.readAll());
            result.fromCache = true;
            if (!result.svg.isEmpty()) {
                QTimer::singleShot(0, this, [this, generation, callback = std::move(callback), result]() {
                    if (generation == m_generation) {
                        callback(result);
                    }
                });
                return;
            }
        }
    }

    auto *temp = new QTemporaryFile(
        QDir::tempPath() + QStringLiteral("/locvas-render-XXXXXX") +
        (kind == QStringLiteral("mermaid") ? QStringLiteral(".mmd") : QStringLiteral(".tex")));
    temp->setAutoRemove(true);
    if (!temp->open()) {
        RenderResult result;
        result.error = QStringLiteral("cannot create temporary input file");
        QTimer::singleShot(0, this, [callback = std::move(callback), result]() {
            callback(result);
        });
        delete temp;
        return;
    }
    temp->write(source.toUtf8());
    temp->close();

    auto *process = new QProcess(this);
    m_activeProcesses.insert(process);
    process->setProgram(m_qjsPath);
    QStringList args;
    args << QStringLiteral("--std") << script << temp->fileName();
    args << extraArgs;
    process->setArguments(args);
    process->setProcessChannelMode(QProcess::SeparateChannels);

    auto completed = QSharedPointer<bool>::create(false);
    QElapsedTimer timer;
    timer.start();

    auto finish = [this, process, temp, cachePath, callback = std::move(callback), completed, timer, generation](const RenderResult &result) mutable {
        if (*completed) {
            return;
        }
        *completed = true;
        m_activeProcesses.remove(process);
        const bool cancelled = process->property("locvasCancelled").toBool()
                               || generation != m_generation;
        if (!cancelled) {
            RenderResult out = result;
            if (out.error.isEmpty() && !out.svg.isEmpty()) {
                QFile cache(cachePath);
                if (cache.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    cache.write(out.svg.toUtf8());
                }
            }
            out.elapsedMs = timer.elapsed();
            callback(out);
        }
        process->deleteLater();
        temp->deleteLater();
    };

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process, finish](int code, QProcess::ExitStatus status) mutable {
                const QByteArray out = process->readAllStandardOutput();
                const QByteArray err = process->readAllStandardError();
                RenderResult result;
                if (status != QProcess::NormalExit || code != 0) {
                    result.error = QString::fromUtf8(err).trimmed();
                    if (result.error.isEmpty()) {
                        result.error = QStringLiteral("renderer exited with code %1").arg(code);
                    }
                } else if (out.trimmed().isEmpty()) {
                    result.error = QStringLiteral("renderer produced no output");
                } else {
                    result.svg = QString::fromUtf8(out);
                }
                finish(result);
            });

    connect(process, &QProcess::errorOccurred, this,
            [process, finish](QProcess::ProcessError error) mutable {
                if (error == QProcess::FailedToStart) {
                    RenderResult result;
                    result.error = QStringLiteral("cannot start renderer: %1").arg(process->program());
                    finish(result);
                }
            });

    connect(process, &QProcess::started, process, [process]() {
        QTimer::singleShot(15000, process, [process]() {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
            }
        });
    });

    process->start();

    QTimer::singleShot(3000, process, [process, finish]() mutable {
        if (process->state() != QProcess::Starting) {
            process->kill();
            RenderResult result;
            result.error = QStringLiteral("render did not start within 3s");
            finish(result);
        }
    });
}

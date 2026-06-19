#include "ServerManager.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>
#include <QFileInfo>
#include <QDir>

ServerManager::ServerManager(QObject *parent) : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);

    m_healthTimer = new QTimer(this);
    m_healthTimer->setInterval(500);
    connect(m_healthTimer, &QTimer::timeout, this, &ServerManager::checkHealth);

    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    connect(m_idleTimer, &QTimer::timeout, this, &ServerManager::onIdleTimeout);
}

ServerManager::~ServerManager()
{
    stop();
}

void ServerManager::configure(const ServerConfig &cfg, int idleUnloadSeconds)
{
    m_cfg = cfg;
    m_idleMs = idleUnloadSeconds > 0 ? idleUnloadSeconds * 1000 : 0;
}

QString ServerManager::baseUrl() const
{
    return QStringLiteral("http://%1:%2").arg(m_cfg.host).arg(m_cfg.port);
}

void ServerManager::ensureRunning()
{
    if (m_state == Ready)    { emit ready(); return; }
    if (m_state == Starting) { return; }          // ready() will fire when healthy
    startProcess();
}

void ServerManager::startProcess()
{
    if (!QFileInfo::exists(m_cfg.binaryPath)) {
        emit failed(QStringLiteral("llama-server not found:\n%1").arg(m_cfg.binaryPath));
        return;
    }
    if (!QFileInfo::exists(m_cfg.modelPath)) {
        emit failed(QStringLiteral("Model file not found:\n%1").arg(m_cfg.modelPath));
        return;
    }

    m_state = Starting;
    emit starting();

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);
    m_proc->setWorkingDirectory(QFileInfo(m_cfg.binaryPath).absolutePath());
    connect(m_proc, &QProcess::readyReadStandardError,  this, &ServerManager::onReadyReadStdErr);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &ServerManager::onReadyReadStdOut);
    connect(m_proc, &QProcess::errorOccurred,           this, &ServerManager::onProcessError);
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        if (m_state == Starting) {
            m_state = Stopped;
            if (m_healthTimer) m_healthTimer->stop();
            emit failed(QStringLiteral("llama-server exited during startup (code %1).\n"
                                       "Check config.json \"extra_args\" and the model path.").arg(code));
        } else if (m_state == Ready) {
            m_state = Stopped;     // died after running; relaunch on next refine
        }
    });

    const int nSlots = m_cfg.parallelSlots > 0 ? m_cfg.parallelSlots : 1;
    QStringList args;
    args << "-m"         << QDir::toNativeSeparators(m_cfg.modelPath)
         << "--host"     << m_cfg.host
         << "--port"     << QString::number(m_cfg.port)
         << "-ngl"       << QString::number(m_cfg.nGpuLayers)
         << "-c"         << QString::number(m_cfg.ctxSize * nSlots)
         << "--parallel" << QString::number(nSlots);
    args << m_cfg.extraArgs;

    emit logMessage(QStringLiteral("Launching: %1 %2").arg(m_cfg.binaryPath, args.join(' ')));
    m_proc->start(m_cfg.binaryPath, args);

    if (!m_proc->waitForStarted(5000)) {
        m_state = Stopped;
        emit failed(QStringLiteral("Failed to start llama-server process."));
        return;
    }
    m_healthTimer->start();
}

void ServerManager::stop()
{
    if (m_healthTimer) m_healthTimer->stop();
    if (m_idleTimer)   m_idleTimer->stop();
    if (m_proc) {
        m_proc->disconnect(this);
        if (m_proc->state() != QProcess::NotRunning) {
            m_proc->terminate();
            if (!m_proc->waitForFinished(3000)) {
                m_proc->kill();
                m_proc->waitForFinished(2000);
            }
        }
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    m_state = Stopped;
}

void ServerManager::notifyActivity()
{
    if (m_idleMs > 0) m_idleTimer->start(m_idleMs);
}

void ServerManager::holdIdle()
{
    if (m_idleTimer) m_idleTimer->stop();
}

void ServerManager::onIdleTimeout()
{
    if (m_state == Ready) {
        stop();
        emit unloaded();
    }
}

void ServerManager::checkHealth()
{
    QNetworkRequest req{QUrl(baseUrl() + "/health")};
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_state != Starting) return;
        // While the model loads, /health returns 503 (an "error" to Qt).
        // A clean 200 means it is ready to serve.
        if (reply->error() == QNetworkReply::NoError) {
            m_state = Ready;
            m_healthTimer->stop();
            notifyActivity();        // begin the idle countdown
            emit ready();
        }
    });
}

void ServerManager::onReadyReadStdErr()
{
    if (!m_proc) return;
    const QString text = QString::fromLocal8Bit(m_proc->readAllStandardError());
    const auto lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines)
        emit logMessage(line.trimmed());
}

void ServerManager::onReadyReadStdOut()
{
    if (!m_proc) return;
    const QString text = QString::fromLocal8Bit(m_proc->readAllStandardOutput());
    const auto lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines)
        emit logMessage(line.trimmed());
}

void ServerManager::onProcessError(QProcess::ProcessError err)
{
    Q_UNUSED(err);
    if (!m_proc) return;
    const QString msg = m_proc->errorString();
    m_state = Stopped;          // next ensureRunning() will relaunch
    emit failed(QStringLiteral("Process error: %1").arg(msg));
}

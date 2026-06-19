#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QTimer;

// Configuration for a single llama-server instance.
struct ServerConfig {
    QString binaryPath;          // path to llama-server.exe
    QString modelPath;           // path to the .gguf model
    QString host = "127.0.0.1";  // keep loopback-only on an airgapped box
    int     port = 8080;
    int     nGpuLayers = 99;     // 99 = offload everything to the GPU; 0 = CPU only
    int     ctxSize = 4096;
    QStringList extraArgs;       // anything else to pass through verbatim
    int     parallelSlots = 1;   // concurrent request slots (= number of outputs)
};

// Owns the llama-server child process. Starts it lazily (on demand) and can
// unload it after an idle period so it isn't holding VRAM/RAM when unused.
class ServerManager : public QObject {
    Q_OBJECT
public:
    enum State { Stopped, Starting, Ready };

    explicit ServerManager(QObject *parent = nullptr);
    ~ServerManager() override;

    // Store config and idle policy. Does NOT start the server.
    // idleUnloadSeconds <= 0 means "never unload once loaded".
    void configure(const ServerConfig &cfg, int idleUnloadSeconds);

    void ensureRunning();   // start if needed; emits ready() when healthy
    void notifyActivity();  // (re)arm the idle-unload countdown
    void holdIdle();        // pause the countdown (e.g. during generation)
    void stop();            // stop the server now, freeing memory

    State   state() const   { return m_state; }
    bool    isReady() const { return m_state == Ready; }
    QString baseUrl() const;

signals:
    void starting();
    void ready();
    void unloaded();        // server stopped because it went idle
    void logMessage(const QString &line);
    void failed(const QString &reason);

private slots:
    void onReadyReadStdErr();
    void onReadyReadStdOut();
    void onProcessError(QProcess::ProcessError err);
    void checkHealth();
    void onIdleTimeout();

private:
    void startProcess();

    QProcess              *m_proc        = nullptr;
    QNetworkAccessManager *m_net         = nullptr;
    QTimer                *m_healthTimer = nullptr;
    QTimer                *m_idleTimer   = nullptr;
    ServerConfig           m_cfg;
    int                    m_idleMs      = 0;
    State                  m_state       = Stopped;
};

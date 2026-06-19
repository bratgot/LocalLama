#pragma once

#include <QMainWindow>
#include <QString>
#include <QVector>

class QPlainTextEdit;
class QComboBox;
class QPushButton;
class QLabel;
class ServerManager;
class LlamaClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onRefineClicked();
    void onServerStarting();
    void onServerReady();
    void onServerUnloaded();
    void onServerFailed(const QString &reason);

private:
    void readConfig();          // reads config.json, configures the server
    void setupUi();             // builds N output cards + N clients
    void wireSignals();
    void setBusy(bool busy);
    void flushPending();        // fire all candidates once the server is ready
    void appendOutput(int i, const QString &text);
    void onCandidateFinished(int i);

    QPlainTextEdit          *m_input    = nullptr;
    QComboBox               *m_mode     = nullptr;
    QPushButton             *m_refineBtn= nullptr;
    QLabel                  *m_status   = nullptr;
    QVector<QPlainTextEdit*> m_outputs;

    ServerManager           *m_server   = nullptr;
    QVector<LlamaClient*>    m_clients;

    QString m_baseUrl;
    double  m_temperature = 0.6;
    int     m_numOutputs  = 3;
    int     m_idleUnload  = 0;      // 0 = keep model loaded (fast); >0 = unload after N s idle

    bool    m_busy        = false;
    int     m_running     = 0;      // candidates still generating
    bool    m_hasPending  = false;
    QString m_pendingPrompt;
    QString m_pendingText;
};

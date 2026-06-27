#pragma once

#include <QMainWindow>
#include <QString>
#include <QVector>
#include <QJsonArray>

class QPlainTextEdit;
class QCheckBox;
class QPushButton;
class QGroupBox;
class QLabel;
class QListWidget;
class QFontComboBox;
class QComboBox;
class QSpinBox;
class QNetworkAccessManager;
class ServerManager;
class LlamaClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *e) override;   // persist settings + history on exit

private slots:
    void onRefineClicked();
    void onServerStarting();
    void onServerReady();
    void onServerUnloaded();
    void onServerFailed(const QString &reason);

private:
    void readConfig();          // reads config.json, configures the server
    void setupUi();             // builds header, mode checkboxes, cards, history
    void wireSignals();
    void setBusy(bool busy);
    void flushPending();        // fire all checked modes once the server is ready
    void appendOutput(int i, const QString &text);
    void onCandidateFinished(int i);

    // --- new behaviour --------------------------------------------------------
    void startGeneration();              // run current input through the checked modes
    void resetPreferences();             // restore defaults + re-center window
    void ensureOnScreen();               // pull the window back if it's off-screen
    void applyTheme(const QString &theme);   // "Light", "Dark", or "Wild"
    void applyFont();                    // push font family/size onto the editors
    void updateModeVisibility();         // show/hide each card to match its checkbox
    void loadPersistedState();           // restore dark mode + font + checked modes
    void persistState();                 // save dark mode + font + checked modes
    void updateModelInfo();              // refresh the model info panel text
    void fetchServerProps();             // GET /props to confirm the loaded model
    void recordHistory();                // push the just-finished run into history
    void refreshHistoryList();           // rebuild the side list from m_history
    void onHistorySelected(int row);     // reload a past conversation
    void historyStep(int delta);         // Prev/Next through previous generations
    void loadHistory();                  // read history.json
    void saveHistory();                  // write history.json
    QString settingsPath() const;
    QString historyPath() const;

    // One mode = one checkbox + one output card + one client.
    struct ModeUi {
        QCheckBox      *check  = nullptr;
        QGroupBox      *card   = nullptr;
        QPlainTextEdit *output = nullptr;
        LlamaClient    *client = nullptr;
        QString         label;
        QString         prompt;
    };
    QVector<ModeUi> m_modes;

    QPlainTextEdit *m_input        = nullptr;
    QPushButton    *m_refineBtn    = nullptr;   // primary "Generate" action
    QPushButton    *m_newBtn       = nullptr;   // secondary "Regenerate"
    QPushButton    *m_undoBtn      = nullptr;   // undo input edits
    QPushButton    *m_redoBtn      = nullptr;   // redo input edits
    QPushButton    *m_prevBtn      = nullptr;   // older generation
    QPushButton    *m_nextBtn      = nullptr;   // newer generation
    QPushButton    *m_clearHistBtn = nullptr;
    QPushButton    *m_resetBtn     = nullptr;
    QComboBox      *m_themeCombo   = nullptr;
    QFontComboBox  *m_fontCombo    = nullptr;
    QSpinBox       *m_fontSize     = nullptr;
    QLabel         *m_status       = nullptr;
    QLabel         *m_modelInfo    = nullptr;   // detailed panel
    QLabel         *m_modelLine    = nullptr;   // persistent one-line status-bar summary
    QListWidget    *m_historyList  = nullptr;

    ServerManager         *m_server  = nullptr;
    QNetworkAccessManager *m_infoNet = nullptr;

    QString m_baseUrl;
    double  m_temperature = 0.6;
    int     m_numOutputs  = 3;
    int     m_idleUnload  = 0;      // 0 = keep model loaded (fast); >0 = unload after N s idle
    QString m_defaultMode = "Make it concise";   // first-run default checkbox

    // model info panel
    QString m_modelName;            // model file name from config
    int     m_ctxSize     = 4096;   // configured context
    int     m_nGpuLayers  = 99;     // configured GPU offload
    QString m_liveModel;            // model name confirmed by the server (/props)
    int     m_liveCtx     = 0;      // context confirmed by the server (/props)

    bool    m_busy        = false;
    int     m_running     = 0;      // candidates still generating
    bool    m_hasPending  = false;
    QString m_pendingText;
    QVector<int> m_activeModes;     // mode indices generating in the current run
    int     m_genCounter  = 0;      // bumps each run so seeds (and answers) differ

    QJsonArray m_history;           // persisted conversation history (newest first)
    QJsonArray m_clearedHistory;    // one-step backup so "Clear history" can be undone
};

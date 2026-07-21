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
class QTabWidget;
class QTextEdit;
class QTimer;
class QProcess;
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
    void manageIntents();                // create/edit/delete reusable context presets
    void refreshIntentCombo();           // rebuild the Context combo from presets
    void onIntentComboChanged(int idx);  // apply the picked preset (or none)
    void loadIntentState();              // read presets + current context
    void saveIntentState();              // write presets + current context
    void toggleExpand();                 // grow the input box to fill the window
    void resetPreferences();             // restore defaults + re-center window
    void ensureOnScreen();               // pull the window back if it's off-screen
    void applyTheme(const QString &theme);   // "Light", "Dark", or "Wild"
    void applyFont();                    // push font family/size onto the editors
    void updateModeVisibility();         // show/hide each card to match its checkbox
    void loadPersistedState();           // restore dark mode + font + checked modes
    void persistState();                 // save dark mode + font + checked modes
    void updateModelInfo();              // refresh the model info panel text
    QWidget *buildHelpTab();             // the Help / Tech info tab (help + model info)
    void showHelpPopup();                // quick-help popup dialog
    void startVramMonitor();             // begin polling GPU VRAM usage
    void pollVram();                     // query nvidia-smi and update the VRAM label
    void fetchServerProps();             // GET /props to confirm the loaded model
    void recordHistory();                // push the just-finished run into history
    void refreshHistoryList();           // rebuild the side list from m_history
    void onHistorySelected(int row);     // reload a past conversation
    void historyStep(int delta);         // Prev/Next through previous generations
    void loadHistory();                  // read history.json
    void saveHistory();                  // write history.json
    QString configDir() const;           // per-user, writable config dir
    QString settingsPath() const;
    QString historyPath() const;

    // --- Chat tab -------------------------------------------------------------
    void buildChatTab();                 // build the conversational tab
    void sendChat();                     // send the typed message (or Stop if busy)
    void doSendChat();                   // actually issue the request (server ready)
    void newChat();                      // start a fresh conversation
    void copyChatReply();                // copy the latest assistant reply
    void recordChatSession();            // upsert the current conversation into history
    void refreshChatHistoryList();       // rebuild the chat-history side list
    void onChatHistorySelected(int row); // reload a past conversation
    void setChatBusy(bool busy);
    void appendChatSpan(const QString &text, const QColor &color,
                        bool italic, bool bold);   // styled append to the transcript
    void renderChatHistory();            // rebuild the transcript from m_chatMessages
    void onChatReasoning(const QString &text);     // streamed chain-of-thought
    void onChatChunk(const QString &text);         // streamed answer
    void onChatFinished();
    void loadChatHistory();              // read chat.json
    void saveChatHistory();              // write chat.json
    QString chatHistoryPath() const;

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
    QPushButton    *m_intentBtn    = nullptr;   // open the context-preset manager
    QComboBox      *m_intentCombo  = nullptr;   // pick a saved context preset (global)
    QPushButton    *m_expandBtn    = nullptr;   // expand input to fill the window
    QComboBox      *m_themeCombo   = nullptr;
    QFontComboBox  *m_fontCombo    = nullptr;
    QSpinBox       *m_fontSize     = nullptr;
    QLabel         *m_status       = nullptr;
    QLabel         *m_usageLabel   = nullptr;   // hidden in expanded mode
    QLabel         *m_modelInfo    = nullptr;   // detailed panel (now on the Help / Tech info tab)
    QLabel         *m_modelLine    = nullptr;   // persistent one-line status-bar summary
    QLabel         *m_vramLabel    = nullptr;   // live GPU VRAM usage (status bar, right)
    QTimer         *m_vramTimer    = nullptr;
    QProcess       *m_vramProc     = nullptr;
    QListWidget    *m_historyList  = nullptr;
    QWidget        *m_outputsWrap  = nullptr;   // the output cards row (hidden when expanded)
    QWidget        *m_rightCol     = nullptr;   // history + model panel (hidden when expanded)

    // Chat tab widgets
    QTabWidget     *m_tabs         = nullptr;   // Refine / Chat tab container
    QWidget        *m_refineTab    = nullptr;   // tab 1 host (the proofreader)
    QWidget        *m_chatTab      = nullptr;   // tab 2 host (the conversation)
    QTextEdit      *m_chatLog      = nullptr;   // scrolling transcript
    QPlainTextEdit *m_chatInput    = nullptr;   // message composer
    QPushButton    *m_chatSendBtn  = nullptr;   // Send / Stop
    QPushButton    *m_chatNewBtn   = nullptr;   // Clear chat
    QPushButton    *m_chatCopyBtn  = nullptr;   // Copy latest reply
    QCheckBox      *m_chatThinkChk = nullptr;   // show/hide the model's thinking
    QVector<QCheckBox*> m_chatEdits;            // edit toggles (Rewrite/Clarity/Concise/Formal)
    QVector<QCheckBox*> m_chatTones;            // tone toggles (Warm/Friendly/…)
    QListWidget    *m_chatHistoryList  = nullptr;  // past conversations
    QPushButton    *m_chatClearHistBtn = nullptr;
    LlamaClient    *m_chatClient   = nullptr;

    ServerManager         *m_server  = nullptr;
    QNetworkAccessManager *m_infoNet = nullptr;

    QString m_baseUrl;
    double  m_temperature = 0.6;
    int     m_numOutputs  = 3;
    int     m_idleUnload  = 0;      // 0 = keep model loaded (fast); >0 = unload after N s idle
    QString m_defaultMode = "Rewrite";   // first-run default checkbox

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
    QString m_intent;               // active global context, applied to Refine + Chat
    struct IntentPreset { QString name; QString text; };
    QVector<IntentPreset> m_intentPresets;   // reusable, saved (and team-seedable) presets
    bool    m_expanded    = false;  // input-fills-window mode

    QJsonArray m_history;           // persisted conversation history (newest first)
    QJsonArray m_clearedHistory;    // one-step backup so "Clear history" can be undone

    // Chat tab state
    QJsonArray m_chatMessages;      // conversation turns (user/assistant), oldest first
    QString    m_chatAnswer;        // accumulates the current assistant answer
    bool       m_chatBusy      = false;
    bool       m_chatPending   = false;   // a message is waiting for the model to load
    bool       m_chatReasoningShown = false;  // inserted the "thinking" header this turn
    bool       m_chatAnswerShown    = false;  // inserted the answer separator this turn
    QJsonArray m_chatHistory;       // saved past conversations (newest first)
    int        m_chatActiveRow = -1;// row in m_chatHistory the current chat maps to (-1 = new)
};

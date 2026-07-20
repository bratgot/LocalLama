#include "MainWindow.h"
#include "ServerManager.h"
#include "LlamaClient.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QCheckBox>
#include <QPushButton>
#include <QListWidget>
#include <QFontComboBox>
#include <QComboBox>
#include <QSpinBox>
#include <QStatusBar>
#include <QLabel>
#include <QFrame>
#include <QShortcut>
#include <QKeySequence>
#include <QTextCursor>
#include <QClipboard>
#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QScreen>
#include <QCloseEvent>
#include <QDateTime>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QInputDialog>
#include <QDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QTextCharFormat>
#include <QColor>
#include <QUrl>

namespace {
struct Mode { const char *label; const char *prompt; };

// Each becomes a checkbox + its own output column; the prompt is the system message.
const Mode kModes[] = {
    {"Fix grammar & spelling",
     "You are a meticulous proofreader. Correct only the spelling, grammar, and "
     "punctuation errors in the user's text. Preserve the original meaning, "
     "wording, tone, and line breaks as closely as possible. Do not add notes, "
     "explanations, or surrounding quotation marks. Output only the corrected text."},
    {"Improve clarity",
     "You are a skilled editor. Revise the user's text so it reads clearly and "
     "smoothly while preserving its original meaning and intent. Fix all grammar "
     "and spelling errors. Do not add commentary. Output only the revised text."},
    {"Make it formal",
     "You are an editor. Rewrite the user's text in a professional, formal tone "
     "suitable for business or academic writing, preserving the original meaning. "
     "Fix all grammar and spelling errors. Output only the rewritten text."},
    {"Make it concise",
     "You are an editor. Rewrite the user's text to be as clear and concise as "
     "possible without losing meaning. Remove redundancy and wordiness. Fix all "
     "grammar and spelling errors. Output only the rewritten text."},
};
const int kModeCount = int(sizeof(kModes) / sizeof(kModes[0]));

// Regenerate cycles through these to widen the variety of answers: each adds a
// nudge to the system prompt and bumps the sampling temperature. Index 0 is the
// plain first Generate; every ↻ Regenerate advances to the next strategy.
struct Variety { const char *name; const char *note; double tempDelta; };
const Variety kVariety[] = {
    {"standard",     "", 0.0},
    {"reworded",     "Use noticeably different word choices and phrasing than the most obvious version.", 0.15},
    {"restructured", "Restructure the sentences — vary their length and order — while preserving the meaning.", 0.30},
    {"creative",     "Take a more expressive, creative approach while keeping the original meaning and facts intact.", 0.50},
};
const int kVarietyCount = int(sizeof(kVariety) / sizeof(kVariety[0]));

// Quick tone toggles for the Chat tab. Any checked ones are folded into the
// chat system prompt as a tone instruction (multi-select — you can combine them).
struct ChatTone { const char *label; const char *phrase; };
const ChatTone kChatTones[] = {
    {"Warm",         "warm and caring"},
    {"Friendly",     "friendly and approachable"},
    {"Professional", "professional and polished"},
    {"Assertive",    "assertive and confident"},
    {"Urgent",       "direct and urgent, conveying time-sensitivity"},
    {"No worries",   "relaxed and reassuring, keeping things easy-going"},
};
const int kChatToneCount = int(sizeof(kChatTones) / sizeof(kChatTones[0]));

// Dark theme. Light theme is the default Qt look (empty stylesheet).
const char *kDarkQss = R"(
QWidget        { background-color: #1e1f22; color: #e6e6e6; }
QMainWindow    { background-color: #1e1f22; }
QPlainTextEdit,
QTextEdit,
QListWidget    { background-color: #2b2d31; color: #e6e6e6; border: 1px solid #3a3d42; }
QGroupBox      { border: 1px solid #3a3d42; border-radius: 4px; margin-top: 8px; }
QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; color: #b8b8b8; }
QPushButton    { background-color: #3a3d42; color: #e6e6e6; border: 1px solid #4a4d52;
                 padding: 4px 10px; border-radius: 3px; }
QPushButton:hover    { background-color: #45494f; }
QPushButton:disabled { color: #777; border-color: #333; }
QComboBox, QSpinBox  { background-color: #2b2d31; color: #e6e6e6; border: 1px solid #3a3d42;
                       padding: 2px 4px; }
QListWidget::item:selected { background-color: #3d5a80; color: #ffffff; }
QTabWidget::pane { border: 1px solid #3a3d42; background-color: #1e1f22; top: -1px; }
QTabBar          { background-color: #1e1f22; }
QTabBar::tab     { background-color: #2b2d31; color: #b8b8b8; border: 1px solid #3a3d42;
                   padding: 5px 14px; margin-right: 2px;
                   border-top-left-radius: 4px; border-top-right-radius: 4px; }
QTabBar::tab:selected { background-color: #1e1f22; color: #e6e6e6; border-bottom-color: #1e1f22; }
QTabBar::tab:hover    { background-color: #45494f; }
)";

// "Wild" theme — deep purple canvas, hot-pink controls, neon-green accents.
const char *kWildQss = R"(
QWidget        { background-color: #2a0a3a; color: #ffe9ff; }
QMainWindow    { background-color: #2a0a3a; }
QPlainTextEdit,
QTextEdit,
QListWidget    { background-color: #3d1457; color: #eafff0; border: 1px solid #ff5fd2;
                 border-radius: 4px; }
QGroupBox      { border: 2px solid #9b5cff; border-radius: 6px; margin-top: 8px; }
QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px;
                   color: #7CFFB2; font-weight: bold; }
QPushButton    { background-color: #ff3fa4; color: white; border: 1px solid #ffa6e0;
                 padding: 4px 10px; border-radius: 4px; font-weight: bold; }
QPushButton:hover    { background-color: #ff66bb; }
QPushButton:disabled { background-color: #5a3a6a; color: #b39ec2; border-color: #5a3a6a; }
QComboBox, QSpinBox  { background-color: #3d1457; color: #7CFFB2; border: 1px solid #9b5cff;
                       padding: 2px 4px; }
QCheckBox      { color: #7CFFB2; }
QListWidget::item:selected { background-color: #7CFFB2; color: #2a0a3a; }
QTabWidget::pane { border: 2px solid #9b5cff; background-color: #2a0a3a; top: -1px; }
QTabBar          { background-color: #2a0a3a; }
QTabBar::tab     { background-color: #3d1457; color: #7CFFB2; border: 1px solid #9b5cff;
                   padding: 5px 14px; margin-right: 2px;
                   border-top-left-radius: 5px; border-top-right-radius: 5px; }
QTabBar::tab:selected { background-color: #2a0a3a; color: #ff3fa4; font-weight: bold;
                        border-bottom-color: #2a0a3a; }
QTabBar::tab:hover    { background-color: #5a3a6a; }
)";
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_server  = new ServerManager(this);
    m_infoNet = new QNetworkAccessManager(this);

    readConfig();          // sets m_numOutputs, m_baseUrl, m_temperature, m_idleUnload
    setupUi();             // builds header, mode checkboxes + cards, history, footer
    wireSignals();

    // First run for this user: seed prefs from an admin default placed next to the
    // exe (optional), so a shared deployment can ship sensible defaults while each
    // user's later changes stay private in their own profile.
    const QString userIni = settingsPath();
    const QString seedIni = QCoreApplication::applicationDirPath() + "/settings.ini";
    if (!QFileInfo::exists(userIni) && QFileInfo::exists(seedIni)
        && QFileInfo(seedIni).absoluteFilePath() != QFileInfo(userIni).absoluteFilePath())
        QFile::copy(seedIni, userIni);

    loadHistory();         // per-user history.json -> m_history
    refreshHistoryList();
    loadChatHistory();     // per-user chat.json -> m_chatMessages (Chat tab)
    loadPersistedState();  // theme + font + checked modes (per-user settings.ini)
    updateModelInfo();     // initial model info panel (config-derived)

    if (m_idleUnload == 0) {
        // Resident model: preload now so the first refine is instant.
        m_status->setText(QStringLiteral("Loading model…"));
        m_server->ensureRunning();
    } else {
        m_status->setText(QStringLiteral("Ready — the model loads on first refine."));
    }
}

MainWindow::~MainWindow()
{
    if (m_server)
        m_server->stop();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    persistState();
    saveHistory();
    saveChatHistory();
    QMainWindow::closeEvent(e);
}

QString MainWindow::configDir() const
{
    // Per-user, writable location — Windows: %LOCALAPPDATA%\LlamaChat,
    // Linux: ~/.config/LlamaChat. This lets the app + model live on a read-only
    // network share while every user keeps private prefs/history. Not derived
    // from org/app name (avoids a doubled "LlamaChat/LlamaChat" subfolder).
    QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    dir += "/LlamaChat";
    QDir().mkpath(dir);
    return dir;
}

QString MainWindow::settingsPath() const
{
    return configDir() + "/settings.ini";
}

QString MainWindow::historyPath() const
{
    return configDir() + "/history.json";
}

void MainWindow::readConfig()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    auto resolve = [&](const QString &p) -> QString {
        if (p.isEmpty()) return p;
        return QFileInfo(p).isAbsolute() ? p : QDir(appDir).filePath(p);
    };
    // Normalise the server binary name to this platform so one config.json works
    // on both: ".exe" on Windows, no extension on Linux/macOS.
    auto platformBinary = [](QString p) -> QString {
#ifdef Q_OS_WIN
        if (!p.endsWith(".exe", Qt::CaseInsensitive)) p += ".exe";
#else
        if (p.endsWith(".exe", Qt::CaseInsensitive)) p.chop(4);
#endif
        return p;
    };

    ServerConfig cfg;
    cfg.binaryPath = resolve(platformBinary("llama/llama-server"));
    cfg.modelPath  = resolve("models/model.gguf");

    QFile f(appDir + "/config.json");
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        if (o.contains("server_binary")) cfg.binaryPath = resolve(platformBinary(o.value("server_binary").toString()));
        if (o.contains("model_path"))    cfg.modelPath  = resolve(o.value("model_path").toString());
        cfg.host       = o.value("host").toString(cfg.host);
        cfg.port       = o.value("port").toInt(cfg.port);
        cfg.nGpuLayers = o.value("n_gpu_layers").toInt(cfg.nGpuLayers);
        cfg.ctxSize    = o.value("ctx_size").toInt(cfg.ctxSize);
        m_temperature  = o.value("temperature").toDouble(m_temperature);
        m_numOutputs   = qBound(1, o.value("num_outputs").toInt(m_numOutputs), 6);
        m_idleUnload   = o.value("idle_unload_seconds").toInt(m_idleUnload);
        m_defaultMode  = o.value("default_mode").toString(m_defaultMode);
        const QJsonArray extra = o.value("extra_args").toArray();
        for (const auto &v : extra) cfg.extraArgs << v.toString();
    }

    // Every checked mode runs concurrently, plus one more slot for the Chat tab.
    cfg.parallelSlots = kModeCount + 1;
    m_baseUrl = QStringLiteral("http://%1:%2").arg(cfg.host).arg(cfg.port);

    // remember a few facts for the model info panel
    m_modelName   = QFileInfo(cfg.modelPath).fileName();
    m_ctxSize     = cfg.ctxSize;
    m_nGpuLayers  = cfg.nGpuLayers;

    m_server->configure(cfg, m_idleUnload);
}

void MainWindow::setupUi()
{
    const QString version = qApp->applicationVersion();
    setWindowTitle(QStringLiteral("LlamaChat %1 (offline)").arg(version));
    resize(1200, 780);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);

    // status label created first so per-card lambdas can use it safely
    m_status = new QLabel(QString(), this);
    m_status->setStyleSheet("color:#888;");

    // --- header: title / subtitle / usage notes ---------------------------
    auto *titleRow = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("LlamaChat v%1").arg(version), this);
    QFont tf = title->font(); tf.setPointSize(20); tf.setBold(true); title->setFont(tf);
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    // Theme is a global, app-wide preference — keep it in the shared header so
    // it's reachable from both the Refine and Chat tabs.
    titleRow->addWidget(new QLabel(QStringLiteral("Theme:"), this));
    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItems({"Light", "Dark", "Wild"});
    m_themeCombo->setToolTip("Light, Dark, or Wild (purple/pink/green) — saved between sessions");
    titleRow->addWidget(m_themeCombo);

    auto *subtitle = new QLabel(
        QStringLiteral("Offline writing refinement — everything runs locally on your machine."), this);
    subtitle->setStyleSheet("color:#888;");

    m_usageLabel = new QLabel(
        QStringLiteral("Type or paste text below, tick one or more modes, then press "
                       "Generate (Ctrl+Enter). Each ticked mode produces its own suggestion "
                       "side by side — click “Copy this” under the one you like. Set a "
                       "Context (above) to apply a standing instruction, tone, or audience "
                       "to every refinement and chat reply — save presets via Manage…; press "
                       "↻ Regenerate for fresh, progressively more varied answers; ◀ / ▶ "
                       "step through earlier results; ↶ / ↷ undo/redo your edits; and ⛶ "
                       "Expand grows the input to fill the window for long text. "
                       "Your theme (Light / Dark / Wild), font, selected modes, window "
                       "size, and history are saved for your user account. "
                       "Or switch to the Chat tab for a full back-and-forth "
                       "conversation with the model. "
                       "Nothing ever leaves this computer."), this);
    m_usageLabel->setWordWrap(true);
    m_usageLabel->setStyleSheet("color:#888;");

    root->addLayout(titleRow);
    root->addWidget(subtitle);
    root->addWidget(m_usageLabel);

    auto *hr = new QFrame(this);
    hr->setFrameShape(QFrame::HLine);
    hr->setFrameShadow(QFrame::Sunken);
    root->addWidget(hr);

    // Global "Context": a reusable instruction/tone/audience applied to BOTH the
    // Refine modes and Chat replies. Lives above the tabs so it's always visible.
    // Presets are saved per-user (and seedable for a whole team) via settings.ini.
    auto *ctxRow = new QHBoxLayout();
    ctxRow->addWidget(new QLabel(QStringLiteral("Context:"), this));
    m_intentCombo = new QComboBox(this);
    m_intentCombo->setToolTip(QStringLiteral(
        "A reusable instruction/tone/audience applied to every refinement and chat "
        "reply. Pick a saved preset, or Manage… to create your own."));
    m_intentCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_intentCombo->setMinimumWidth(240);
    ctxRow->addWidget(m_intentCombo, 1);
    m_intentBtn = new QPushButton(QStringLiteral("Manage…"), this);
    m_intentBtn->setToolTip(QStringLiteral("Create, edit, or delete saved context presets"));
    ctxRow->addWidget(m_intentBtn);
    root->addLayout(ctxRow);

    // Two tabs: the side-by-side "Refine" proofreader, and a conversational
    // "Chat". Everything below (through the main split) lives in the Refine tab;
    // the shared status line and footer stay outside the tabs.
    m_tabs = new QTabWidget(this);
    m_refineTab = new QWidget(this);
    auto *refineLay = new QVBoxLayout(m_refineTab);
    refineLay->setContentsMargins(0, 0, 0, 0);

    // --- row A: mode checkboxes + dark mode -------------------------------
    auto *modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel("Modes:", this));
    m_modes.resize(kModeCount);
    for (int i = 0; i < kModeCount; ++i) {
        m_modes[i].label  = QString::fromUtf8(kModes[i].label);
        m_modes[i].prompt = QString::fromUtf8(kModes[i].prompt);
        auto *cb = new QCheckBox(m_modes[i].label, this);
        m_modes[i].check = cb;
        modeRow->addWidget(cb);
    }
    modeRow->addStretch(1);
    refineLay->addLayout(modeRow);

    // --- row B: font controls + generate / navigation ---------------------
    auto *ctlRow = new QHBoxLayout();
    ctlRow->addWidget(new QLabel("Font:", this));
    m_fontCombo = new QFontComboBox(this);
    ctlRow->addWidget(m_fontCombo);
    ctlRow->addWidget(new QLabel("Size:", this));
    m_fontSize = new QSpinBox(this);
    m_fontSize->setRange(8, 32);
    m_fontSize->setValue(11);
    ctlRow->addWidget(m_fontSize);
    ctlRow->addStretch(1);

    // input edit history (undo/redo)
    m_undoBtn = new QPushButton(QStringLiteral("↶ Undo"), this);
    m_redoBtn = new QPushButton(QStringLiteral("↷ Redo"), this);
    m_undoBtn->setToolTip("Undo the last change to your text (Ctrl+Z)");
    m_redoBtn->setToolTip("Redo (Ctrl+Y)");
    m_undoBtn->setEnabled(false);
    m_redoBtn->setEnabled(false);
    ctlRow->addWidget(m_undoBtn);
    ctlRow->addWidget(m_redoBtn);

    auto *sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFrameShadow(QFrame::Sunken);
    ctlRow->addWidget(sep1);

    // step through earlier generations
    m_prevBtn = new QPushButton(QStringLiteral("◀ Prev"), this);
    m_nextBtn = new QPushButton(QStringLiteral("Next ▶"), this);
    m_prevBtn->setToolTip("Show an older result from history");
    m_nextBtn->setToolTip("Show a newer result from history");
    ctlRow->addWidget(m_prevBtn);
    ctlRow->addWidget(m_nextBtn);

    auto *sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFrameShadow(QFrame::Sunken);
    ctlRow->addWidget(sep2);

    // secondary: regenerate (only useful after a first generation)
    m_newBtn = new QPushButton(QStringLiteral("↻ Regenerate"), this);
    m_newBtn->setToolTip("Generate fresh answers for the same text");
    m_newBtn->setEnabled(false);
    ctlRow->addWidget(m_newBtn);

    // primary: Generate — the obvious call to action
    m_refineBtn = new QPushButton("Generate", this);
    m_refineBtn->setDefault(true);
    m_refineBtn->setMinimumWidth(120);
    m_refineBtn->setToolTip("Generate suggestions for your text (Ctrl+Enter)");
    // accent styling, set directly so it survives both light and dark themes
    m_refineBtn->setStyleSheet(
        "QPushButton { background-color:#2f7d32; color:white; font-weight:bold;"
        " border:none; padding:6px 16px; border-radius:4px; }"
        "QPushButton:hover { background-color:#37923b; }"
        "QPushButton:disabled { background-color:#6c6c6c; color:#cccccc; }");
    ctlRow->addWidget(m_refineBtn);
    refineLay->addLayout(ctlRow);

    // --- main area: (input + output cards)  |  history panel --------------
    auto *mainSplit = new QSplitter(Qt::Horizontal, this);

    // left: vertical splitter of input over the output cards
    auto *vSplit = new QSplitter(Qt::Vertical, this);

    auto *inBox = new QGroupBox("Input", this);
    auto *inLay = new QVBoxLayout(inBox);
    auto *inHdr = new QHBoxLayout();
    inHdr->setContentsMargins(0, 0, 0, 0);
    inHdr->addStretch(1);
    m_expandBtn = new QPushButton(QStringLiteral("⛶ Expand"), this);
    m_expandBtn->setToolTip("Expand the input box to fill the window (for long text)");
    inHdr->addWidget(m_expandBtn);
    inLay->addLayout(inHdr);
    m_input = new QPlainTextEdit(this);
    m_input->setPlaceholderText("Type or paste text here, then press Generate (Ctrl+Enter).");
    inLay->addWidget(m_input);
    vSplit->addWidget(inBox);

    m_outputsWrap = new QWidget(this);
    auto *outWrap = m_outputsWrap;
    auto *outRow  = new QHBoxLayout(outWrap);
    outRow->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < kModeCount; ++i) {
        auto *card = new QGroupBox(m_modes[i].label, this);
        auto *cl   = new QVBoxLayout(card);
        auto *ed   = new QPlainTextEdit(this);
        ed->setReadOnly(true);
        auto *copyBtn = new QPushButton("Copy this", this);
        connect(copyBtn, &QPushButton::clicked, this, [this, i]() {
            QApplication::clipboard()->setText(m_modes[i].output->toPlainText());
            m_status->setText(QStringLiteral("%1 copied to clipboard.").arg(m_modes[i].label));
        });
        cl->addWidget(ed);
        cl->addWidget(copyBtn);
        m_modes[i].card   = card;
        m_modes[i].output = ed;
        outRow->addWidget(card);
    }
    vSplit->addWidget(outWrap);
    vSplit->setSizes({240, 460});
    mainSplit->addWidget(vSplit);

    // right: history panel above a model-info panel
    m_rightCol = new QWidget(this);
    auto *rightCol = m_rightCol;
    auto *rightLay = new QVBoxLayout(rightCol);
    rightLay->setContentsMargins(0, 0, 0, 0);

    auto *histBox = new QGroupBox("History", this);
    auto *histLay = new QVBoxLayout(histBox);
    m_historyList = new QListWidget(this);
    m_clearHistBtn = new QPushButton("Clear history", this);
    histLay->addWidget(m_historyList, 1);
    histLay->addWidget(m_clearHistBtn);
    rightLay->addWidget(histBox, 1);

    auto *modelBox = new QGroupBox("Model info", this);
    auto *modelLay = new QVBoxLayout(modelBox);
    m_modelInfo = new QLabel(this);
    m_modelInfo->setWordWrap(true);
    m_modelInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    modelLay->addWidget(m_modelInfo);
    rightLay->addWidget(modelBox);

    mainSplit->addWidget(rightCol);
    mainSplit->setStretchFactor(0, 1);
    mainSplit->setSizes({900, 300});

    refineLay->addWidget(mainSplit, 1);

    m_tabs->addTab(m_refineTab, QStringLiteral("Refine"));
    buildChatTab();
    m_tabs->addTab(m_chatTab, QStringLiteral("Chat"));
    root->addWidget(m_tabs, 1);

    root->addWidget(m_status);

    // --- footer: reset preferences (left) + credit (right) ----------------
    auto *footer = new QHBoxLayout();
    m_resetBtn = new QPushButton("Reset preferences", this);
    m_resetBtn->setToolTip("Restore default theme, font, modes and window size/position");
    footer->addWidget(m_resetBtn);
    footer->addStretch(1);
    auto *credit = new QLabel(QStringLiteral("Created by Marten Blumen"), this);
    credit->setStyleSheet("color:#888; font-style:italic;");
    footer->addWidget(credit);
    root->addLayout(footer);

    setCentralWidget(central);

    // persistent one-line model summary in the status bar (always visible)
    m_modelLine = new QLabel(this);
    statusBar()->addWidget(m_modelLine, 1);

    // one client per mode
    for (int i = 0; i < kModeCount; ++i) {
        auto *client = new LlamaClient(this);
        client->setBaseUrl(m_baseUrl);
        client->setTemperature(m_temperature);
        m_modes[i].client = client;
        connect(client, &LlamaClient::chunk,    this, [this, i](const QString &t){ appendOutput(i, t); });
        connect(client, &LlamaClient::finished, this, [this, i](){ onCandidateFinished(i); });
        connect(client, &LlamaClient::errorOccurred, this, [this, i](const QString &e){
            m_modes[i].output->setPlainText(QStringLiteral("[error] ") + e);
            onCandidateFinished(i);
        });
    }

    // dedicated client for the Chat tab (multi-turn conversation)
    m_chatClient = new LlamaClient(this);
    m_chatClient->setBaseUrl(m_baseUrl);
    m_chatClient->setTemperature(m_temperature);
    connect(m_chatClient, &LlamaClient::reasoningChunk, this, &MainWindow::onChatReasoning);
    connect(m_chatClient, &LlamaClient::chunk,          this, &MainWindow::onChatChunk);
    connect(m_chatClient, &LlamaClient::finished,       this, &MainWindow::onChatFinished);
    connect(m_chatClient, &LlamaClient::errorOccurred,  this, [this](const QString &e){
        appendChatSpan(QStringLiteral("[error] %1\n").arg(e), QColor("#e5484d"), false, false);
    });
}

void MainWindow::wireSignals()
{
    connect(m_server, &ServerManager::starting, this, &MainWindow::onServerStarting);
    connect(m_server, &ServerManager::ready,    this, &MainWindow::onServerReady);
    connect(m_server, &ServerManager::unloaded, this, &MainWindow::onServerUnloaded);
    connect(m_server, &ServerManager::failed,   this, &MainWindow::onServerFailed);
    connect(m_server, &ServerManager::logMessage, this, [this](const QString &line) {
        if (m_server->state() != ServerManager::Ready)
            m_status->setText(line.left(120));
    });

    connect(m_refineBtn, &QPushButton::clicked, this, &MainWindow::onRefineClicked);
    connect(m_newBtn,    &QPushButton::clicked, this, [this]() { if (!m_busy) startGeneration(); });
    connect(m_prevBtn,   &QPushButton::clicked, this, [this]() { historyStep(+1); });  // older
    connect(m_nextBtn,   &QPushButton::clicked, this, [this]() { historyStep(-1); });  // newer

    // undo/redo for the input text, mirrored by the toolbar buttons
    connect(m_undoBtn, &QPushButton::clicked, m_input, &QPlainTextEdit::undo);
    connect(m_redoBtn, &QPushButton::clicked, m_input, &QPlainTextEdit::redo);
    connect(m_input, &QPlainTextEdit::undoAvailable, this,
            [this](bool on) { if (!m_busy) m_undoBtn->setEnabled(on); });
    connect(m_input, &QPlainTextEdit::redoAvailable, this,
            [this](bool on) { if (!m_busy) m_redoBtn->setEnabled(on); });
    connect(m_clearHistBtn, &QPushButton::clicked, this, [this]() {
        if (!m_clearedHistory.isEmpty()) {           // currently cleared -> restore
            m_history = m_clearedHistory;
            m_clearedHistory = QJsonArray();
            saveHistory();
            refreshHistoryList();
            m_clearHistBtn->setText("Clear history");
            m_status->setText("History restored.");
        } else if (!m_history.isEmpty()) {           // clear, keeping a one-step undo
            m_clearedHistory = m_history;
            m_history = QJsonArray();
            saveHistory();
            refreshHistoryList();
            m_clearHistBtn->setText(QStringLiteral("↶ Undo clear"));
            m_status->setText("History cleared — press “Undo clear” to restore.");
        }
    });

    auto *sc = new QShortcut(QKeySequence("Ctrl+Return"), m_refineTab);
    sc->setContext(Qt::WidgetWithChildrenShortcut);   // only when the Refine tab has focus
    connect(sc, &QShortcut::activated, this, &MainWindow::onRefineClicked);

    // remember the active tab (Refine / Chat) across sessions
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) { persistState(); });

    // mode checkboxes: live show/hide + persist
    for (int i = 0; i < kModeCount; ++i) {
        connect(m_modes[i].check, &QCheckBox::toggled, this, [this]() {
            updateModeVisibility();
            persistState();
        });
    }

    connect(m_themeCombo, &QComboBox::currentTextChanged, this, [this](const QString &theme) {
        applyTheme(theme);
        persistState();
    });
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, [this]() {
        applyFont();
        persistState();
    });
    connect(m_fontSize, qOverload<int>(&QSpinBox::valueChanged), this, [this]() {
        applyFont();
        persistState();
    });

    connect(m_historyList, &QListWidget::currentRowChanged, this, &MainWindow::onHistorySelected);

    connect(m_resetBtn,  &QPushButton::clicked, this, &MainWindow::resetPreferences);
    connect(m_intentBtn, &QPushButton::clicked, this, &MainWindow::manageIntents);
    connect(m_intentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onIntentComboChanged);

    // remember chat tone toggles across sessions
    for (auto *cb : m_chatTones)
        connect(cb, &QCheckBox::toggled, this, [this]() { persistState(); });
    connect(m_expandBtn, &QPushButton::clicked, this, &MainWindow::toggleExpand);
}

void MainWindow::onRefineClicked()
{
    if (m_busy) {                       // button acts as "Stop"
        m_hasPending = false;
        for (auto &m : m_modes) if (m.client->isBusy()) m.client->cancel();
        setBusy(false);
        m_server->notifyActivity();
        m_status->setText("Stopped.");
        return;
    }
    startGeneration();
}

void MainWindow::startGeneration()
{
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) {
        m_status->setText("Nothing to refine.");
        return;
    }

    m_activeModes.clear();
    for (int i = 0; i < kModeCount; ++i)
        if (m_modes[i].check->isChecked())
            m_activeModes.append(i);

    if (m_activeModes.isEmpty()) {
        m_status->setText("Tick at least one mode to generate a result.");
        return;
    }

    ++m_genCounter;                     // fresh seeds -> different answers each run
    m_pendingText = text;
    m_hasPending  = true;
    for (int idx : m_activeModes)
        m_modes[idx].output->clear();
    setBusy(true);

    m_server->notifyActivity();
    if (m_server->isReady()) {
        flushPending();
    } else {
        m_status->setText(QStringLiteral("Loading model…"));
        m_server->ensureRunning();      // ready() -> flushPending()
    }
}

void MainWindow::refreshIntentCombo()
{
    if (!m_intentCombo) return;
    m_intentCombo->blockSignals(true);
    m_intentCombo->clear();
    m_intentCombo->addItem(QStringLiteral("(no context)"), QString());   // itemData = text
    int sel = 0;
    for (int i = 0; i < m_intentPresets.size(); ++i) {
        m_intentCombo->addItem(m_intentPresets[i].name, m_intentPresets[i].text);
        if (!m_intent.isEmpty() && m_intentPresets[i].text == m_intent) sel = i + 1;
    }
    // an active context that matches no preset is shown as a transient "(custom)"
    if (!m_intent.isEmpty() && sel == 0) {
        m_intentCombo->addItem(QStringLiteral("(custom)"), m_intent);
        sel = m_intentCombo->count() - 1;
    }
    m_intentCombo->setCurrentIndex(sel);
    m_intentCombo->blockSignals(false);
}

void MainWindow::onIntentComboChanged(int idx)
{
    if (idx < 0) return;
    m_intent = m_intentCombo->itemData(idx).toString();
    saveIntentState();
    m_status->setText(m_intent.isEmpty()
        ? QStringLiteral("Context cleared — refinements and chat use no extra instruction.")
        : QStringLiteral("Context: %1 — applied to refinements and chat.").arg(m_intentCombo->currentText()));
}

void MainWindow::manageIntents()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Context presets"));
    dlg.resize(560, 440);
    auto *lay = new QVBoxLayout(&dlg);

    auto *hint = new QLabel(QStringLiteral(
        "Save reusable context (tone, audience, house style, standing instructions) "
        "and apply it to every refinement and chat reply. Pick a saved preset from "
        "the Context box after saving, or “Use as context” to apply the text below now."), &dlg);
    hint->setWordWrap(true);
    lay->addWidget(hint);

    auto *mid = new QHBoxLayout();
    auto *list = new QListWidget(&dlg);
    list->setMaximumWidth(190);
    mid->addWidget(list);

    auto *right = new QVBoxLayout();
    right->addWidget(new QLabel(QStringLiteral("Name:"), &dlg));
    auto *nameEd = new QLineEdit(&dlg);
    right->addWidget(nameEd);
    right->addWidget(new QLabel(QStringLiteral("Context text:"), &dlg));
    auto *textEd = new QPlainTextEdit(&dlg);
    right->addWidget(textEd, 1);
    mid->addLayout(right, 1);
    lay->addLayout(mid, 1);

    auto *btns = new QHBoxLayout();
    auto *newBtn   = new QPushButton(QStringLiteral("New"), &dlg);
    auto *saveBtn  = new QPushButton(QStringLiteral("Save preset"), &dlg);
    auto *delBtn   = new QPushButton(QStringLiteral("Delete"), &dlg);
    auto *useBtn   = new QPushButton(QStringLiteral("Use as context"), &dlg);
    auto *closeBtn = new QPushButton(QStringLiteral("Close"), &dlg);
    btns->addWidget(newBtn); btns->addWidget(saveBtn); btns->addWidget(delBtn);
    btns->addStretch(1); btns->addWidget(useBtn); btns->addWidget(closeBtn);
    lay->addLayout(btns);

    QVector<IntentPreset> work = m_intentPresets;   // edit a copy, commit on close
    auto reloadList = [&](int selRow) {
        list->blockSignals(true);
        list->clear();
        for (const auto &p : work) list->addItem(p.name);
        if (selRow >= 0 && selRow < list->count()) list->setCurrentRow(selRow);
        list->blockSignals(false);
    };
    reloadList(-1);

    connect(list, &QListWidget::currentRowChanged, &dlg, [&](int row) {
        if (row < 0 || row >= work.size()) return;
        nameEd->setText(work[row].name);
        textEd->setPlainText(work[row].text);
    });
    connect(newBtn, &QPushButton::clicked, &dlg, [&]() {
        list->setCurrentRow(-1); nameEd->clear(); textEd->clear(); nameEd->setFocus();
    });
    connect(saveBtn, &QPushButton::clicked, &dlg, [&]() {
        const QString nm = nameEd->text().trimmed();
        const QString tx = textEd->toPlainText().trimmed();
        if (nm.isEmpty()) {
            QMessageBox::information(&dlg, QStringLiteral("Name needed"),
                                     QStringLiteral("Give the preset a name first."));
            return;
        }
        int found = -1;
        for (int i = 0; i < work.size(); ++i)
            if (work[i].name.compare(nm, Qt::CaseInsensitive) == 0) { found = i; break; }
        if (found >= 0) work[found].text = tx;
        else           { work.append({nm, tx}); found = work.size() - 1; }
        reloadList(found);
    });
    connect(delBtn, &QPushButton::clicked, &dlg, [&]() {
        const int row = list->currentRow();
        if (row < 0 || row >= work.size()) return;
        work.remove(row);
        nameEd->clear(); textEd->clear();
        reloadList(qMin(row, work.size() - 1));
    });
    QString chosen; bool useChosen = false;
    connect(useBtn, &QPushButton::clicked, &dlg, [&]() {
        chosen = textEd->toPlainText().trimmed(); useChosen = true; dlg.accept();
    });
    connect(closeBtn, &QPushButton::clicked, &dlg, [&]() { dlg.accept(); });

    dlg.exec();

    m_intentPresets = work;                 // commit preset edits
    if (useChosen) m_intent = chosen;
    saveIntentState();
    refreshIntentCombo();
    if (useChosen)
        m_status->setText(m_intent.isEmpty() ? QStringLiteral("Context cleared.")
                                             : QStringLiteral("Context applied."));
}

void MainWindow::loadIntentState()
{
    QSettings s(settingsPath(), QSettings::IniFormat);
    m_intentPresets.clear();
    const int n = s.beginReadArray(QStringLiteral("intentPresets"));
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        IntentPreset p;
        p.name = s.value(QStringLiteral("name")).toString();
        p.text = s.value(QStringLiteral("text")).toString();
        if (!p.name.isEmpty()) m_intentPresets.append(p);
    }
    s.endArray();
    m_intent = s.value(QStringLiteral("intent/current")).toString();
    refreshIntentCombo();
}

void MainWindow::saveIntentState()
{
    QSettings s(settingsPath(), QSettings::IniFormat);
    s.setValue(QStringLiteral("intent/current"), m_intent);
    s.beginWriteArray(QStringLiteral("intentPresets"));
    for (int i = 0; i < m_intentPresets.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("name"), m_intentPresets[i].name);
        s.setValue(QStringLiteral("text"), m_intentPresets[i].text);
    }
    s.endArray();
}

void MainWindow::toggleExpand()
{
    m_expanded = !m_expanded;
    if (m_outputsWrap) m_outputsWrap->setVisible(!m_expanded);
    if (m_rightCol)    m_rightCol->setVisible(!m_expanded);
    if (m_usageLabel)  m_usageLabel->setVisible(!m_expanded);   // reclaim vertical space
    m_expandBtn->setText(m_expanded ? QStringLiteral("⛶ Restore") : QStringLiteral("⛶ Expand"));
    m_expandBtn->setToolTip(m_expanded
        ? QStringLiteral("Restore the results and history panels")
        : QStringLiteral("Expand the input box to fill the window (for long text)"));
}

void MainWindow::flushPending()
{
    if (!m_hasPending) return;
    m_hasPending = false;
    m_server->holdIdle();               // don't unload mid-generation
    m_running = m_activeModes.size();

    // variety strategy for this run (cycles as you press Regenerate)
    const Variety &v   = kVariety[(m_genCounter - 1 + kVarietyCount) % kVarietyCount];
    const double   temp = qBound(0.1, m_temperature + v.tempDelta, 1.3);

    QString status = QStringLiteral("Generating %1 result(s)…").arg(m_running);
    if (v.tempDelta > 0.0) status += QStringLiteral("  (variation: %1)").arg(QString::fromUtf8(v.name));
    m_status->setText(status);

    for (int idx : m_activeModes) {
        QString sys = m_modes[idx].prompt;
        if (!m_intent.isEmpty())
            sys += QStringLiteral("\n\nContext — what the user is trying to say / the intended "
                                  "tone or audience: %1\nHonour this while refining.").arg(m_intent);
        if (v.note[0] != '\0')
            sys += QStringLiteral("\n\n%1").arg(QString::fromUtf8(v.note));
        // Proofreading wants a fast, direct answer — disable chain-of-thought.
        // ("/no_think" is Qwen3's per-turn switch; harmless on models without it.
        // The Chat tab omits this, so it keeps full thinking.)
        sys += QStringLiteral("\n\n/no_think");

        m_modes[idx].client->setTemperature(temp);
        m_modes[idx].client->setSeed(m_genCounter * 16 + idx);   // distinct + fresh
        m_modes[idx].client->refine(sys, m_pendingText);
    }
}

void MainWindow::appendOutput(int i, const QString &text)
{
    auto *ed = m_modes[i].output;
    ed->moveCursor(QTextCursor::End);
    ed->insertPlainText(text);
    ed->moveCursor(QTextCursor::End);
}

void MainWindow::onCandidateFinished(int)
{
    if (!m_busy) return;                // already stopped
    if (m_running > 0) --m_running;
    if (m_running == 0) {
        setBusy(false);
        m_server->notifyActivity();
        recordHistory();
        m_status->setText(QStringLiteral("Done — pick the best and press Copy."));
    }
}

void MainWindow::recordHistory()
{
    QJsonObject rec;
    rec["time"]  = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    rec["input"] = m_pendingText;
    QJsonArray results;
    for (int idx : m_activeModes) {
        QJsonObject r;
        r["mode"] = m_modes[idx].label;
        r["text"] = m_modes[idx].output->toPlainText();
        results.append(r);
    }
    rec["results"] = results;

    m_history.insert(0, rec);                       // newest first
    while (m_history.size() > 100)                  // cap stored conversations
        m_history.removeAt(m_history.size() - 1);

    if (!m_clearedHistory.isEmpty()) {              // new activity voids the clear-undo
        m_clearedHistory = QJsonArray();
        m_clearHistBtn->setText("Clear history");
    }

    saveHistory();
    refreshHistoryList();
    m_historyList->blockSignals(true);
    m_historyList->setCurrentRow(0);
    m_historyList->blockSignals(false);
}

void MainWindow::refreshHistoryList()
{
    m_historyList->blockSignals(true);
    m_historyList->clear();
    for (const auto &v : m_history) {
        const QJsonObject rec = v.toObject();
        QString snippet = rec.value("input").toString().simplified();
        if (snippet.size() > 48) snippet = snippet.left(48) + QStringLiteral("…");
        m_historyList->addItem(QStringLiteral("%1  —  %2")
                                   .arg(rec.value("time").toString(), snippet));
    }
    m_historyList->blockSignals(false);
    const bool any = m_history.size() > 0;
    m_prevBtn->setEnabled(any);
    m_nextBtn->setEnabled(any);
}

void MainWindow::onHistorySelected(int row)
{
    if (row < 0 || row >= m_history.size()) return;
    const QJsonObject rec = m_history.at(row).toObject();

    m_input->setPlainText(rec.value("input").toString());

    // reset every card, then restore the modes this conversation used
    for (auto &m : m_modes) {
        m.check->blockSignals(true);
        m.check->setChecked(false);
        m.check->blockSignals(false);
        m.output->clear();
    }
    const QJsonArray results = rec.value("results").toArray();
    for (const auto &rv : results) {
        const QJsonObject r = rv.toObject();
        const QString mode = r.value("mode").toString();
        for (auto &m : m_modes) {
            if (m.label == mode) {
                m.check->blockSignals(true);
                m.check->setChecked(true);
                m.check->blockSignals(false);
                m.output->setPlainText(r.value("text").toString());
                break;
            }
        }
    }
    updateModeVisibility();
    persistState();
    m_status->setText(QStringLiteral("Loaded a previous result from %1.")
                          .arg(rec.value("time").toString()));
}

void MainWindow::historyStep(int delta)
{
    if (m_history.isEmpty()) return;
    int row = m_historyList->currentRow();
    if (row < 0) row = (delta > 0) ? -1 : m_history.size();  // first Prev -> row 0
    int next = qBound(0, row + delta, m_history.size() - 1);
    m_historyList->setCurrentRow(next);                       // triggers onHistorySelected
}

void MainWindow::onServerStarting()
{
    m_status->setText(QStringLiteral("Loading model…"));
    updateModelInfo();
}

void MainWindow::onServerReady()
{
    fetchServerProps();                 // confirm the actually-loaded model
    updateModelInfo();
    if (m_hasPending)
        flushPending();
    else if (!m_chatPending)
        m_status->setText(QStringLiteral("Model loaded. Ready."));

    if (m_chatPending) {          // a chat message was waiting for the model
        m_chatPending = false;
        doSendChat();
    }
}

void MainWindow::onServerUnloaded()
{
    m_status->setText(QStringLiteral("Model unloaded (idle) — reloads on next refine."));
    m_liveModel.clear();
    m_liveCtx = 0;
    updateModelInfo();
}

void MainWindow::onServerFailed(const QString &reason)
{
    m_status->setText(QStringLiteral("Server failed — ") + reason);
    m_hasPending = false;
    m_running = 0;
    setBusy(false);
    updateModelInfo();
}

void MainWindow::setBusy(bool busy)
{
    m_busy = busy;
    m_refineBtn->setText(busy ? "Stop" : "Generate");
    m_newBtn->setEnabled(!busy && m_genCounter > 0);   // regenerate needs a prior run
    m_input->setReadOnly(busy);
    m_undoBtn->setEnabled(!busy && m_input->document()->isUndoAvailable());
    m_redoBtn->setEnabled(!busy && m_input->document()->isRedoAvailable());
    for (auto &m : m_modes)
        m.check->setEnabled(!busy);
    m_historyList->setEnabled(!busy);
}

void MainWindow::applyTheme(const QString &theme)
{
    QString qss;
    if (theme == "Dark")      qss = QString::fromUtf8(kDarkQss);
    else if (theme == "Wild") qss = QString::fromUtf8(kWildQss);
    // "Light" (or anything else) -> default Qt look (empty stylesheet)
    qApp->setStyleSheet(qss);
}

void MainWindow::applyFont()
{
    QFont f = m_fontCombo->currentFont();
    f.setPointSize(m_fontSize->value());
    m_input->setFont(f);
    for (auto &m : m_modes)
        m.output->setFont(f);
    if (m_chatLog)   m_chatLog->setFont(f);
    if (m_chatInput) m_chatInput->setFont(f);
}

void MainWindow::updateModeVisibility()
{
    for (auto &m : m_modes)
        m.card->setVisible(m.check->isChecked());
}

void MainWindow::loadPersistedState()
{
    QSettings s(settingsPath(), QSettings::IniFormat);

    // theme (migrate the old "dark" bool from earlier builds)
    QString theme = s.value("theme").toString();
    if (theme.isEmpty())
        theme = s.value("dark", false).toBool() ? "Dark" : "Light";
    if (m_themeCombo->findText(theme) < 0)
        theme = "Light";
    m_themeCombo->blockSignals(true);
    m_themeCombo->setCurrentText(theme);
    m_themeCombo->blockSignals(false);
    applyTheme(theme);

    // font
    const QString family = s.value("font_family").toString();
    const int     size   = s.value("font_size", 11).toInt();
    m_fontCombo->blockSignals(true);
    if (!family.isEmpty()) m_fontCombo->setCurrentFont(QFont(family));
    m_fontCombo->blockSignals(false);
    m_fontSize->blockSignals(true);
    m_fontSize->setValue(qBound(8, size, 32));
    m_fontSize->blockSignals(false);
    applyFont();

    // checked modes (default: only the configured default_mode on first run)
    const QStringList saved = s.value("modes").toStringList();
    bool any = false;
    for (auto &m : m_modes) {
        const bool on = saved.isEmpty() ? (m.label == m_defaultMode) : saved.contains(m.label);
        m.check->blockSignals(true);
        m.check->setChecked(on);
        m.check->blockSignals(false);
        any = any || on;
    }
    if (!any) {                          // never leave the user with no mode
        m_modes[0].check->blockSignals(true);
        m_modes[0].check->setChecked(true);
        m_modes[0].check->blockSignals(false);
    }
    updateModeVisibility();

    // active tab (Refine / Chat) — restore without re-triggering a save
    if (m_tabs) {
        const int tab = s.value("tab", 0).toInt();
        m_tabs->blockSignals(true);
        m_tabs->setCurrentIndex(qBound(0, tab, m_tabs->count() - 1));
        m_tabs->blockSignals(false);
    }

    // chat tone toggles
    const QStringList savedTones = s.value("chatTones").toStringList();
    for (int i = 0; i < m_chatTones.size(); ++i) {
        m_chatTones[i]->blockSignals(true);
        m_chatTones[i]->setChecked(savedTones.contains(QString::fromUtf8(kChatTones[i].label)));
        m_chatTones[i]->blockSignals(false);
    }

    // global context presets + the active context selection
    loadIntentState();

    // window size + position (restore, then make sure it's actually visible)
    const QByteArray geo = s.value("geometry").toByteArray();
    if (!geo.isEmpty())
        restoreGeometry(geo);
    ensureOnScreen();
}

void MainWindow::persistState()
{
    QSettings s(settingsPath(), QSettings::IniFormat);
    s.setValue("theme", m_themeCombo->currentText());
    s.setValue("font_family", m_fontCombo->currentFont().family());
    s.setValue("font_size", m_fontSize->value());
    QStringList checked;
    for (auto &m : m_modes)
        if (m.check->isChecked()) checked << m.label;
    s.setValue("modes", checked);
    if (m_tabs) s.setValue("tab", m_tabs->currentIndex());   // active tab (Refine/Chat)
    QStringList tones;
    for (int i = 0; i < m_chatTones.size(); ++i)
        if (m_chatTones[i]->isChecked()) tones << QString::fromUtf8(kChatTones[i].label);
    s.setValue("chatTones", tones);
    s.setValue("geometry", saveGeometry());        // window size + position
}

void MainWindow::ensureOnScreen()
{
    // If the saved geometry leaves the window on no connected screen (e.g. an
    // unplugged monitor), snap back to a centered default so it stays clickable.
    const QRect frame = frameGeometry();
    bool visible = false;
    for (const QScreen *scr : QGuiApplication::screens()) {
        if (scr->availableGeometry().intersects(frame)) { visible = true; break; }
    }
    if (!visible) {
        resize(1200, 780);
        const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
        move(avail.center() - rect().center());
    }
}

void MainWindow::resetPreferences()
{
    // controls back to defaults (these signals re-apply + persist)
    m_themeCombo->setCurrentText("Light");
    m_fontCombo->setCurrentFont(QFont());
    m_fontSize->setValue(11);

    for (auto &m : m_modes) {
        m.check->blockSignals(true);
        m.check->setChecked(m.label == m_defaultMode);
        m.check->blockSignals(false);
    }
    bool any = false;
    for (auto &m : m_modes) any = any || m.check->isChecked();
    if (!any) {
        m_modes[0].check->blockSignals(true);
        m_modes[0].check->setChecked(true);
        m_modes[0].check->blockSignals(false);
    }
    updateModeVisibility();
    if (m_tabs) m_tabs->setCurrentIndex(0);   // back to the Refine tab
    m_intent.clear();                         // clear active context (keep saved presets)
    refreshIntentCombo();
    saveIntentState();
    for (auto *cb : m_chatTones) {            // clear chat tone toggles
        cb->blockSignals(true);
        cb->setChecked(false);
        cb->blockSignals(false);
    }

    // window back to a centered default
    showNormal();                        // undo maximize/fullscreen first
    resize(1200, 780);
    const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
    move(avail.center() - rect().center());

    persistState();                      // save the defaults (incl. new geometry)
    m_status->setText("Preferences reset to defaults.");
}

void MainWindow::loadHistory()
{
    QFile f(historyPath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isArray())
        m_history = doc.array();
}

void MainWindow::saveHistory()
{
    QFile f(historyPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(m_history).toJson(QJsonDocument::Compact));
}

void MainWindow::updateModelInfo()
{
    if (!m_modelInfo) return;

    QString state;
    switch (m_server->state()) {
    case ServerManager::Starting: state = QStringLiteral("Loading…");   break;
    case ServerManager::Ready:    state = QStringLiteral("Loaded ✓");    break;
    default:                      state = QStringLiteral("Not loaded");  break;
    }

    const bool    gpu  = m_nGpuLayers > 0;
    const QString name = m_liveModel.isEmpty() ? m_modelName : m_liveModel;
    const int     ctx  = m_liveCtx > 0 ? m_liveCtx : m_ctxSize;
    const QString backend = gpu
        ? QStringLiteral("GPU (CUDA) — %1 layers offloaded").arg(m_nGpuLayers)
        : QStringLiteral("CPU");

    const QString shownName = name.isEmpty() ? QStringLiteral("(none)") : name;

    m_modelInfo->setText(
        QStringLiteral(
            "<b>App:</b> LlamaChat v%1<br>"
            "<b>Model:</b> %2<br>"
            "<b>Status:</b> %3<br>"
            "<b>Backend:</b> %4<br>"
            "<b>Context:</b> %5 tokens<br>"
            "<b>Temperature:</b> %6<br>"
            "<b>Server:</b> %7")
            .arg(qApp->applicationVersion(),
                 shownName,
                 state,
                 backend,
                 QString::number(ctx),
                 QString::number(m_temperature, 'g', 2),
                 m_baseUrl));

    // persistent one-line summary in the status bar
    if (m_modelLine)
        m_modelLine->setText(QStringLiteral("Model: %1   ·   %2   ·   %3   ·   ctx %4")
                                 .arg(shownName, state, backend, QString::number(ctx)));
}

void MainWindow::fetchServerProps()
{
    QNetworkReply *reply = m_infoNet->get(
        QNetworkRequest(QUrl(m_baseUrl + QStringLiteral("/props"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;   // keep config-derived info
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();

        QString mp = o.value("model_path").toString();
        if (mp.isEmpty())
            mp = o.value("default_generation_settings").toObject().value("model").toString();
        if (!mp.isEmpty())
            m_liveModel = QFileInfo(mp).fileName();

        int ctx = o.value("default_generation_settings").toObject().value("n_ctx").toInt(0);
        if (ctx <= 0) ctx = o.value("n_ctx").toInt(0);
        if (ctx > 0) m_liveCtx = ctx;

        updateModelInfo();
    });
}

// ============================ Chat tab ===================================

void MainWindow::buildChatTab()
{
    m_chatTab = new QWidget(this);
    auto *lay = new QVBoxLayout(m_chatTab);
    lay->setContentsMargins(0, 0, 0, 0);

    // top row: New chat  +  show-thinking toggle
    auto *top = new QHBoxLayout();
    m_chatNewBtn = new QPushButton(QStringLiteral("Clear chat"), this);
    m_chatNewBtn->setToolTip(QStringLiteral("Clear the conversation and start fresh"));
    top->addWidget(m_chatNewBtn);
    m_chatCopyBtn = new QPushButton(QStringLiteral("Copy reply"), this);
    m_chatCopyBtn->setToolTip(QStringLiteral("Copy the latest assistant reply to the clipboard"));
    top->addWidget(m_chatCopyBtn);
    top->addStretch(1);
    m_chatThinkChk = new QCheckBox(QStringLiteral("Show thinking"), this);
    m_chatThinkChk->setChecked(true);
    m_chatThinkChk->setToolTip(QStringLiteral("Show the model's reasoning above each answer"));
    top->addWidget(m_chatThinkChk);
    lay->addLayout(top);

    // tone toggles — checked ones shape the reply's tone (multi-select)
    auto *toneRow = new QHBoxLayout();
    toneRow->addWidget(new QLabel(QStringLiteral("Tone:"), this));
    m_chatTones.clear();
    for (int i = 0; i < kChatToneCount; ++i) {
        auto *cb = new QCheckBox(QString::fromUtf8(kChatTones[i].label), this);
        cb->setToolTip(QStringLiteral("Ask for a %1 tone").arg(QString::fromUtf8(kChatTones[i].phrase)));
        m_chatTones.append(cb);
        toneRow->addWidget(cb);
    }
    toneRow->addStretch(1);
    lay->addLayout(toneRow);

    // composer: multi-line input + Send (on top, so you type above the replies)
    auto *composer = new QHBoxLayout();
    m_chatInput = new QPlainTextEdit(this);
    m_chatInput->setPlaceholderText(QStringLiteral("Type a message, then press Send (Ctrl+Enter)."));
    m_chatInput->setMaximumHeight(90);
    composer->addWidget(m_chatInput, 1);
    m_chatSendBtn = new QPushButton(QStringLiteral("Send"), this);
    m_chatSendBtn->setMinimumWidth(90);
    m_chatSendBtn->setToolTip(QStringLiteral("Send your message (Ctrl+Enter)"));
    m_chatSendBtn->setStyleSheet(         // same green accent as Generate, survives all themes
        "QPushButton { background-color:#2f7d32; color:white; font-weight:bold;"
        " border:none; padding:6px 16px; border-radius:4px; }"
        "QPushButton:hover { background-color:#37923b; }"
        "QPushButton:disabled { background-color:#6c6c6c; color:#cccccc; }");
    composer->addWidget(m_chatSendBtn);
    lay->addLayout(composer);

    // scrolling transcript below the composer
    m_chatLog = new QTextEdit(this);
    m_chatLog->setReadOnly(true);
    m_chatLog->setPlaceholderText(
        QStringLiteral("Ask anything — the whole conversation stays on this machine."));
    lay->addWidget(m_chatLog, 1);

    connect(m_chatSendBtn, &QPushButton::clicked, this, &MainWindow::sendChat);
    connect(m_chatNewBtn,  &QPushButton::clicked, this, &MainWindow::newChat);
    connect(m_chatCopyBtn, &QPushButton::clicked, this, &MainWindow::copyChatReply);

    // Ctrl+Enter sends — scoped to this tab so it never clashes with Refine's.
    auto *csc = new QShortcut(QKeySequence("Ctrl+Return"), m_chatTab);
    csc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(csc, &QShortcut::activated, this, &MainWindow::sendChat);
}

void MainWindow::appendChatSpan(const QString &text, const QColor &color,
                                bool italic, bool bold)
{
    QTextCursor c = m_chatLog->textCursor();
    c.movePosition(QTextCursor::End);
    QTextCharFormat fmt;
    if (color.isValid()) fmt.setForeground(color);   // else inherit the theme's text colour
    fmt.setFontItalic(italic);
    fmt.setFontWeight(bold ? QFont::Bold : QFont::Normal);
    c.insertText(text, fmt);
    m_chatLog->setTextCursor(c);
    m_chatLog->ensureCursorVisible();
}

void MainWindow::sendChat()
{
    if (m_chatBusy) {                    // while streaming, the button is "Stop"
        m_chatClient->cancel();
        return;
    }
    const QString msg = m_chatInput->toPlainText().trimmed();
    if (msg.isEmpty()) {
        m_status->setText(QStringLiteral("Type a message to send."));
        return;
    }
    m_chatInput->clear();

    // render the user's turn, then open the assistant's
    if (!m_chatLog->document()->isEmpty())
        appendChatSpan(QStringLiteral("\n"), QColor(), false, false);
    appendChatSpan(QStringLiteral("You\n"),       QColor("#3b82f6"), false, true);
    appendChatSpan(msg + QStringLiteral("\n\n"),  QColor(),          false, false);
    appendChatSpan(QStringLiteral("Assistant\n"), QColor("#2f9e44"), false, true);

    m_chatMessages.append(QJsonObject{{"role", "user"}, {"content", msg}});

    m_chatAnswer.clear();
    m_chatReasoningShown = false;
    m_chatAnswerShown    = false;

    setChatBusy(true);
    m_server->notifyActivity();
    if (m_server->isReady()) {
        doSendChat();
    } else {
        m_chatPending = true;
        m_status->setText(QStringLiteral("Loading model…"));
        m_server->ensureRunning();       // ready() -> doSendChat() via onServerReady
    }
}

void MainWindow::doSendChat()
{
    static const QString kSystem = QStringLiteral(
        "You are a helpful, knowledgeable assistant running locally and fully "
        "offline on the user's machine. Answer clearly and concisely, and use "
        "Markdown when it aids readability.");

    QString sys = kSystem;
    if (!m_intent.isEmpty())               // honour the global context in chat too
        sys += QStringLiteral("\n\nUser context — keep this in mind throughout the "
                              "conversation: %1").arg(m_intent);

    QStringList tones;                     // tone toggles (Warm / Professional / …)
    for (int i = 0; i < m_chatTones.size(); ++i)
        if (m_chatTones[i]->isChecked())
            tones << QString::fromUtf8(kChatTones[i].phrase);
    if (!tones.isEmpty())
        sys += QStringLiteral("\n\nAdopt this tone in your reply: %1.")
                   .arg(tones.join(QStringLiteral(", ")));

    QJsonArray msgs;
    msgs.append(QJsonObject{{"role", "system"}, {"content", sys}});
    for (const auto &m : m_chatMessages)
        msgs.append(m);

    m_server->holdIdle();                // don't unload mid-reply
    m_chatClient->setTemperature(m_temperature);
    m_chatClient->setSeed(-1);           // fresh sampling each turn
    m_chatClient->chat(msgs);
}

void MainWindow::onChatReasoning(const QString &text)
{
    if (!m_chatThinkChk->isChecked()) return;      // user chose to hide the thinking
    if (!m_chatReasoningShown) {
        appendChatSpan(QStringLiteral("💭 thinking…\n"), QColor("#8a8a8a"), true, false);
        m_chatReasoningShown = true;
    }
    appendChatSpan(text, QColor("#8a8a8a"), true, false);
}

void MainWindow::onChatChunk(const QString &text)
{
    if (!m_chatAnswerShown) {
        if (m_chatReasoningShown)                  // separate thinking from the answer
            appendChatSpan(QStringLiteral("\n\n"), QColor(), false, false);
        m_chatAnswerShown = true;
    }
    m_chatAnswer += text;
    appendChatSpan(text, QColor(), false, false);
}

void MainWindow::onChatFinished()
{
    const QString answer = m_chatAnswer.trimmed();
    if (!answer.isEmpty())
        m_chatMessages.append(QJsonObject{{"role", "assistant"}, {"content", answer}});
    else if (m_chatReasoningShown || m_chatAnswerShown)
        appendChatSpan(QStringLiteral("[stopped]"), QColor("#8a8a8a"), true, false);

    appendChatSpan(QStringLiteral("\n"), QColor(), false, false);

    while (m_chatMessages.size() > 200)            // cap stored turns
        m_chatMessages.removeAt(0);

    saveChatHistory();
    setChatBusy(false);
    m_server->notifyActivity();
    if (!m_busy)                                   // don't stomp a refine-in-progress status
        m_status->setText(QStringLiteral("Ready."));
}

void MainWindow::setChatBusy(bool busy)
{
    m_chatBusy = busy;
    m_chatSendBtn->setText(busy ? QStringLiteral("Stop") : QStringLiteral("Send"));
    m_chatInput->setReadOnly(busy);
    m_chatNewBtn->setEnabled(!busy);
}

void MainWindow::newChat()
{
    if (m_chatBusy)
        m_chatClient->cancel();
    m_chatMessages = QJsonArray();
    m_chatAnswer.clear();
    m_chatLog->clear();
    saveChatHistory();
    m_status->setText(QStringLiteral("Chat cleared."));
}

void MainWindow::copyChatReply()
{
    QString reply;
    for (int i = m_chatMessages.size() - 1; i >= 0; --i) {   // newest assistant turn
        const QJsonObject m = m_chatMessages.at(i).toObject();
        if (m.value("role").toString() == QLatin1String("assistant")) {
            reply = m.value("content").toString();
            break;
        }
    }
    if (reply.isEmpty() && !m_chatAnswer.trimmed().isEmpty())
        reply = m_chatAnswer.trimmed();      // a reply still streaming / not yet stored
    if (reply.isEmpty()) {
        m_status->setText(QStringLiteral("No reply to copy yet."));
        return;
    }
    QApplication::clipboard()->setText(reply);
    m_status->setText(QStringLiteral("Reply copied to clipboard."));
}

void MainWindow::renderChatHistory()
{
    if (!m_chatLog) return;
    m_chatLog->clear();
    bool first = true;
    for (const auto &v : m_chatMessages) {
        const QJsonObject m = v.toObject();
        const QString role    = m.value("role").toString();
        const QString content = m.value("content").toString();
        if (role != "user" && role != "assistant") continue;
        if (!first) appendChatSpan(QStringLiteral("\n"), QColor(), false, false);
        first = false;
        if (role == "user")
            appendChatSpan(QStringLiteral("You\n"), QColor("#3b82f6"), false, true);
        else
            appendChatSpan(QStringLiteral("Assistant\n"), QColor("#2f9e44"), false, true);
        appendChatSpan(content + QStringLiteral("\n"), QColor(), false, false);
    }
    m_chatLog->moveCursor(QTextCursor::End);
    m_chatLog->ensureCursorVisible();
}

void MainWindow::loadChatHistory()
{
    QFile f(chatHistoryPath());
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isArray())
            m_chatMessages = doc.array();
    }
    renderChatHistory();
}

void MainWindow::saveChatHistory()
{
    QFile f(chatHistoryPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(m_chatMessages).toJson(QJsonDocument::Compact));
}

QString MainWindow::chatHistoryPath() const
{
    return configDir() + "/chat.json";
}

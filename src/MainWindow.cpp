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

// Dark theme. Light theme is the default Qt look (empty stylesheet).
const char *kDarkQss = R"(
QWidget        { background-color: #1e1f22; color: #e6e6e6; }
QMainWindow    { background-color: #1e1f22; }
QPlainTextEdit,
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
)";

// "Wild" theme — deep purple canvas, hot-pink controls, neon-green accents.
const char *kWildQss = R"(
QWidget        { background-color: #2a0a3a; color: #ffe9ff; }
QMainWindow    { background-color: #2a0a3a; }
QPlainTextEdit,
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
)";
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_server  = new ServerManager(this);
    m_infoNet = new QNetworkAccessManager(this);

    readConfig();          // sets m_numOutputs, m_baseUrl, m_temperature, m_idleUnload
    setupUi();             // builds header, mode checkboxes + cards, history, footer
    wireSignals();

    loadHistory();         // history.json -> m_history
    refreshHistoryList();
    loadPersistedState();  // theme + font + checked modes (settings.ini)
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
    QMainWindow::closeEvent(e);
}

QString MainWindow::settingsPath() const
{
    return QCoreApplication::applicationDirPath() + "/settings.ini";
}

QString MainWindow::historyPath() const
{
    return QCoreApplication::applicationDirPath() + "/history.json";
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

    // Every checked mode runs concurrently, so give the server a slot per mode.
    cfg.parallelSlots = kModeCount;
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
    auto *title = new QLabel(QStringLiteral("LlamaChat"), this);
    QFont tf = title->font(); tf.setPointSize(20); tf.setBold(true); title->setFont(tf);
    auto *verLbl = new QLabel(QStringLiteral("v%1").arg(version), this);
    verLbl->setStyleSheet("color:#888;");
    verLbl->setAlignment(Qt::AlignBottom);
    titleRow->addWidget(title);
    titleRow->addWidget(verLbl);
    titleRow->addStretch(1);

    auto *subtitle = new QLabel(
        QStringLiteral("Offline writing refinement — everything runs locally on your machine."), this);
    subtitle->setStyleSheet("color:#888;");

    auto *usage = new QLabel(
        QStringLiteral("Type or paste text below, tick one or more modes, then press "
                       "Generate (Ctrl+Enter). Each ticked mode produces its own suggestion "
                       "side by side — click “Copy this” under the one you like. Press "
                       "↻ Regenerate for fresh answers, ◀ / ▶ to step through earlier "
                       "results, and ↶ / ↷ to undo or redo edits to your text. "
                       "Your theme (Light / Dark / Wild), font, selected modes, window "
                       "size, and history are saved automatically and restored next time "
                       "you open the app — use Reset preferences to return to defaults. "
                       "Nothing ever leaves this computer."), this);
    usage->setWordWrap(true);
    usage->setStyleSheet("color:#888;");

    root->addLayout(titleRow);
    root->addWidget(subtitle);
    root->addWidget(usage);

    auto *hr = new QFrame(this);
    hr->setFrameShape(QFrame::HLine);
    hr->setFrameShadow(QFrame::Sunken);
    root->addWidget(hr);

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
    modeRow->addWidget(new QLabel("Theme:", this));
    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItems({"Light", "Dark", "Wild"});
    m_themeCombo->setToolTip("Light, Dark, or Wild (purple/pink/green) — saved between sessions");
    modeRow->addWidget(m_themeCombo);
    root->addLayout(modeRow);

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
    root->addLayout(ctlRow);

    // --- main area: (input + output cards)  |  history panel --------------
    auto *mainSplit = new QSplitter(Qt::Horizontal, this);

    // left: vertical splitter of input over the output cards
    auto *vSplit = new QSplitter(Qt::Vertical, this);

    auto *inBox = new QGroupBox("Input", this);
    auto *inLay = new QVBoxLayout(inBox);
    m_input = new QPlainTextEdit(this);
    m_input->setPlaceholderText("Type or paste text here, then press Refine (Ctrl+Enter).");
    inLay->addWidget(m_input);
    vSplit->addWidget(inBox);

    auto *outWrap = new QWidget(this);
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
    auto *rightCol = new QWidget(this);
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

    root->addWidget(mainSplit, 1);
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

    auto *sc = new QShortcut(QKeySequence("Ctrl+Return"), this);
    connect(sc, &QShortcut::activated, this, &MainWindow::onRefineClicked);

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

    connect(m_resetBtn, &QPushButton::clicked, this, &MainWindow::resetPreferences);
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

void MainWindow::flushPending()
{
    if (!m_hasPending) return;
    m_hasPending = false;
    m_server->holdIdle();               // don't unload mid-generation
    m_running = m_activeModes.size();
    m_status->setText(QStringLiteral("Generating %1 result(s)…").arg(m_running));
    for (int idx : m_activeModes) {
        m_modes[idx].client->setSeed(m_genCounter * 16 + idx);   // distinct + fresh
        m_modes[idx].client->refine(m_modes[idx].prompt, m_pendingText);
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
    else
        m_status->setText(QStringLiteral("Model loaded. Ready."));
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

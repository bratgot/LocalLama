#include "MainWindow.h"
#include "ServerManager.h"
#include "LlamaClient.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QShortcut>
#include <QKeySequence>
#include <QTextCursor>
#include <QClipboard>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace {
struct Mode { const char *label; const char *prompt; };

// Each becomes a dropdown entry; the prompt is sent as the system message.
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
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_server = new ServerManager(this);

    readConfig();    // sets m_numOutputs, m_baseUrl, m_temperature, m_idleUnload
    setupUi();       // builds the cards + clients (needs m_numOutputs)
    wireSignals();

    if (m_idleUnload == 0) {
        // Resident model: preload now so the first refine is instant.
        m_status->setText(QStringLiteral("Loading model\u2026"));
        m_server->ensureRunning();
    } else {
        m_status->setText(QStringLiteral("Ready \u2014 the model loads on first refine."));
    }
}

MainWindow::~MainWindow()
{
    if (m_server)
        m_server->stop();
}

void MainWindow::readConfig()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    auto resolve = [&](const QString &p) -> QString {
        if (p.isEmpty()) return p;
        return QFileInfo(p).isAbsolute() ? p : QDir(appDir).filePath(p);
    };

    ServerConfig cfg;
    cfg.binaryPath = resolve("llama/llama-server.exe");
    cfg.modelPath  = resolve("models/model.gguf");

    QFile f(appDir + "/config.json");
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        if (o.contains("server_binary")) cfg.binaryPath = resolve(o.value("server_binary").toString());
        if (o.contains("model_path"))    cfg.modelPath  = resolve(o.value("model_path").toString());
        cfg.host       = o.value("host").toString(cfg.host);
        cfg.port       = o.value("port").toInt(cfg.port);
        cfg.nGpuLayers = o.value("n_gpu_layers").toInt(cfg.nGpuLayers);
        cfg.ctxSize    = o.value("ctx_size").toInt(cfg.ctxSize);
        m_temperature  = o.value("temperature").toDouble(m_temperature);
        m_numOutputs   = qBound(1, o.value("num_outputs").toInt(m_numOutputs), 6);
        m_idleUnload   = o.value("idle_unload_seconds").toInt(m_idleUnload);
        const QJsonArray extra = o.value("extra_args").toArray();
        for (const auto &v : extra) cfg.extraArgs << v.toString();
    }

    cfg.parallelSlots = m_numOutputs;
    m_baseUrl = QStringLiteral("http://%1:%2").arg(cfg.host).arg(cfg.port);
    m_server->configure(cfg, m_idleUnload);
}

void MainWindow::setupUi()
{
    setWindowTitle("Grammar Refine (offline)");
    resize(qMax(900, 360 * m_numOutputs), 680);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);

    // status label created first so per-card lambdas can use it safely
    m_status = new QLabel(QString(), this);
    m_status->setStyleSheet("color:#888;");

    // --- top bar -----------------------------------------------------------
    auto *top = new QHBoxLayout();
    m_mode = new QComboBox(this);
    for (const auto &m : kModes)
        m_mode->addItem(QString::fromUtf8(m.label), QString::fromUtf8(m.prompt));
    m_refineBtn = new QPushButton("Refine", this);
    top->addWidget(new QLabel("Mode:", this));
    top->addWidget(m_mode, 1);
    top->addWidget(m_refineBtn);

    // --- input + N output cards -------------------------------------------
    auto *split = new QSplitter(Qt::Vertical, this);

    auto *inBox = new QGroupBox("Input", this);
    auto *inLay = new QVBoxLayout(inBox);
    m_input = new QPlainTextEdit(this);
    m_input->setPlaceholderText("Type or paste text here, then press Refine (Ctrl+Enter).");
    inLay->addWidget(m_input);
    split->addWidget(inBox);

    auto *outWrap = new QWidget(this);
    auto *outRow  = new QHBoxLayout(outWrap);
    outRow->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < m_numOutputs; ++i) {
        auto *card = new QGroupBox(QStringLiteral("Option %1").arg(i + 1), this);
        auto *cl   = new QVBoxLayout(card);
        auto *ed   = new QPlainTextEdit(this);
        ed->setReadOnly(true);
        m_outputs.append(ed);
        auto *copyBtn = new QPushButton("Copy this", this);
        connect(copyBtn, &QPushButton::clicked, this, [this, i]() {
            QApplication::clipboard()->setText(m_outputs[i]->toPlainText());
            m_status->setText(QStringLiteral("Option %1 copied to clipboard.").arg(i + 1));
        });
        cl->addWidget(ed);
        cl->addWidget(copyBtn);
        outRow->addWidget(card);
    }
    split->addWidget(outWrap);
    split->setSizes({240, 440});

    root->addLayout(top);
    root->addWidget(split, 1);
    root->addWidget(m_status);
    setCentralWidget(central);

    connect(m_refineBtn, &QPushButton::clicked, this, &MainWindow::onRefineClicked);
    auto *sc = new QShortcut(QKeySequence("Ctrl+Return"), this);
    connect(sc, &QShortcut::activated, this, &MainWindow::onRefineClicked);

    // --- one client per output card ---------------------------------------
    for (int i = 0; i < m_numOutputs; ++i) {
        auto *client = new LlamaClient(this);
        client->setBaseUrl(m_baseUrl);
        client->setTemperature(m_temperature);
        client->setSeed(i);          // distinct sampling per option, reproducible
        m_clients.append(client);
        connect(client, &LlamaClient::chunk,    this, [this, i](const QString &t){ appendOutput(i, t); });
        connect(client, &LlamaClient::finished, this, [this, i](){ onCandidateFinished(i); });
        connect(client, &LlamaClient::errorOccurred, this, [this, i](const QString &e){
            m_outputs[i]->setPlainText(QStringLiteral("[error] ") + e);
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
}

void MainWindow::onRefineClicked()
{
    if (m_busy) {                       // button acts as "Stop"
        m_hasPending = false;
        for (auto *c : m_clients) if (c->isBusy()) c->cancel();
        setBusy(false);
        m_server->notifyActivity();
        m_status->setText("Stopped.");
        return;
    }

    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) {
        m_status->setText("Nothing to refine.");
        return;
    }

    m_pendingPrompt = m_mode->currentData().toString();
    m_pendingText   = text;
    m_hasPending    = true;
    for (auto *ed : m_outputs) ed->clear();
    setBusy(true);

    m_server->notifyActivity();
    if (m_server->isReady()) {
        flushPending();
    } else {
        m_status->setText(QStringLiteral("Loading model\u2026"));
        m_server->ensureRunning();      // ready() -> flushPending()
    }
}

void MainWindow::flushPending()
{
    if (!m_hasPending) return;
    m_hasPending = false;
    m_server->holdIdle();               // don't unload mid-generation
    m_running = m_clients.size();
    m_status->setText(QStringLiteral("Generating %1 option(s)\u2026").arg(m_running));
    for (auto *c : m_clients)
        c->refine(m_pendingPrompt, m_pendingText);
}

void MainWindow::appendOutput(int i, const QString &text)
{
    auto *ed = m_outputs[i];
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
        m_status->setText(QStringLiteral("Done \u2014 pick the best and press Copy."));
    }
}

void MainWindow::onServerStarting()
{
    m_status->setText(QStringLiteral("Loading model\u2026"));
}

void MainWindow::onServerReady()
{
    if (m_hasPending)
        flushPending();
    else
        m_status->setText(QStringLiteral("Model loaded. Ready."));
}

void MainWindow::onServerUnloaded()
{
    m_status->setText(QStringLiteral("Model unloaded (idle) \u2014 reloads on next refine."));
}

void MainWindow::onServerFailed(const QString &reason)
{
    m_status->setText(QStringLiteral("Server failed \u2014 ") + reason);
    m_hasPending = false;
    m_running = 0;
    setBusy(false);
}

void MainWindow::setBusy(bool busy)
{
    m_busy = busy;
    m_refineBtn->setText(busy ? "Stop" : "Refine");
    m_mode->setEnabled(!busy);
    m_input->setReadOnly(busy);
}

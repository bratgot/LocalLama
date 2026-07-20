#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QJsonArray>

class QNetworkAccessManager;
class QNetworkReply;

// Talks to llama-server's OpenAI-compatible /v1/chat/completions endpoint and
// streams the reply back token-by-token via the chunk() signal.
//
// Reasoning models (e.g. Qwen3 with thinking on) emit their chain-of-thought
// either in a separate `reasoning_content` delta field or inline between
// <think>…</think> tags. Either way that text is routed to reasoningChunk() and
// kept out of chunk(), so callers that only want the answer (the refine columns)
// can ignore reasoning, while the chat view can show it separately.
class LlamaClient : public QObject {
    Q_OBJECT
public:
    explicit LlamaClient(QObject *parent = nullptr);

    void setBaseUrl(const QString &url)   { m_baseUrl = url; }
    void setTemperature(double t)         { m_temperature = t; }
    void setSeed(int s)                   { m_seed = s; }

    // Single-shot refinement: one system message + one user message.
    void refine(const QString &systemPrompt, const QString &userText);
    // Multi-turn conversation: caller supplies the full OpenAI-style message
    // array ({role, content} objects, oldest first).
    void chat(const QJsonArray &messages);
    void cancel();
    bool isBusy() const { return m_reply != nullptr; }

signals:
    void chunk(const QString &text);          // incremental answer text
    void reasoningChunk(const QString &text); // incremental chain-of-thought (thinking)
    void finished();                          // stream complete (or cancelled)
    void errorOccurred(const QString &message);

private:
    void post(const QJsonArray &messages);    // shared request/streaming setup
    void handleReadyRead();
    void feedContent(const QString &piece);   // split answer vs inline <think> reasoning
    void flushContentTail();                  // emit any held-back partial-tag buffer

    QNetworkAccessManager *m_net   = nullptr;
    QNetworkReply         *m_reply = nullptr;
    QString    m_baseUrl;
    QByteArray m_buffer;
    double     m_temperature = 0.2;
    int        m_seed = -1;

    // inline <think> parsing state (reset per request)
    QString    m_contentTail;         // unemitted content that may hold a partial tag
    bool       m_inThink = false;     // currently inside a <think>…</think> block
};

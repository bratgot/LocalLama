#include "LlamaClient.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

LlamaClient::LlamaClient(QObject *parent) : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);
}

void LlamaClient::refine(const QString &systemPrompt, const QString &userText)
{
    post(QJsonArray{
        QJsonObject{{"role", "system"}, {"content", systemPrompt}},
        QJsonObject{{"role", "user"},   {"content", userText}}
    });
}

void LlamaClient::chat(const QJsonArray &messages)
{
    post(messages);
}

void LlamaClient::post(const QJsonArray &messages)
{
    if (m_reply)
        cancel();
    m_buffer.clear();
    m_contentTail.clear();
    m_inThink = false;

    QJsonObject body{
        {"model",        "local"},
        {"messages",     messages},
        {"stream",       true},
        {"temperature",  m_temperature},
        {"cache_prompt", true}
    };
    if (m_seed >= 0) body.insert("seed", m_seed);

    QNetworkRequest req{QUrl(m_baseUrl + "/v1/chat/completions")};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_reply = m_net->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(m_reply, &QNetworkReply::readyRead, this, &LlamaClient::handleReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        if (!m_reply)
            return;
        const auto err = m_reply->error();
        if (err != QNetworkReply::NoError && err != QNetworkReply::OperationCanceledError)
            emit errorOccurred(m_reply->errorString());
        flushContentTail();
        m_reply->deleteLater();
        m_reply = nullptr;
        emit finished();
    });
}

void LlamaClient::cancel()
{
    if (m_reply)
        m_reply->abort();   // the finished handler does the cleanup
}

void LlamaClient::handleReadyRead()
{
    if (!m_reply)
        return;
    m_buffer += m_reply->readAll();

    // Server-Sent Events: frames are separated by a blank line ("\n\n").
    int sep;
    while ((sep = m_buffer.indexOf("\n\n")) != -1) {
        const QByteArray frame = m_buffer.left(sep);
        m_buffer.remove(0, sep + 2);

        const auto lines = frame.split('\n');
        for (const QByteArray &raw : lines) {
            const QByteArray line = raw.trimmed();
            if (!line.startsWith("data:"))
                continue;

            const QByteArray payload = line.mid(5).trimmed();
            if (payload == "[DONE]")
                return;

            QJsonParseError perr;
            const QJsonDocument doc = QJsonDocument::fromJson(payload, &perr);
            if (perr.error != QJsonParseError::NoError)
                continue;

            const QJsonArray choices = doc.object().value("choices").toArray();
            if (choices.isEmpty())
                continue;

            const QJsonObject delta = choices.at(0).toObject().value("delta").toObject();

            // Some builds surface chain-of-thought in a dedicated field.
            const QString reasoning = delta.value("reasoning_content").toString();
            if (!reasoning.isEmpty())
                emit reasoningChunk(reasoning);

            // The answer (which may still contain inline <think>…</think> tags).
            const QString piece = delta.value("content").toString();
            if (!piece.isEmpty())
                feedContent(piece);
        }
    }
}

// Longest suffix of `buf` that is a (proper) prefix of `tag` — i.e. how much of
// a tag might be starting at the very end of the buffer and still be incomplete.
static int partialTagSuffix(const QString &buf, const QString &tag)
{
    const int maxK = qMin(buf.size(), tag.size() - 1);
    for (int k = maxK; k > 0; --k)
        if (buf.endsWith(QStringView(tag).left(k)))
            return k;
    return 0;
}

// Route streamed answer text to chunk(), diverting anything between inline
// <think> and </think> tags to reasoningChunk(). Tags may be split across
// network chunks, so a possible partial tag at the tail is held back.
void LlamaClient::feedContent(const QString &piece)
{
    static const QString kOpen  = QStringLiteral("<think>");
    static const QString kClose = QStringLiteral("</think>");

    m_contentTail += piece;

    for (;;) {
        const QString &tag = m_inThink ? kClose : kOpen;
        const int at = m_contentTail.indexOf(tag);
        if (at != -1) {
            const QString before = m_contentTail.left(at);
            if (!before.isEmpty()) {
                if (m_inThink) emit reasoningChunk(before);
                else           emit chunk(before);
            }
            m_contentTail.remove(0, at + tag.size());
            m_inThink = !m_inThink;
            continue;
        }
        // No full tag: emit everything except a possible partial tag at the end.
        const int hold = partialTagSuffix(m_contentTail, tag);
        const QString emit_ = m_contentTail.left(m_contentTail.size() - hold);
        if (!emit_.isEmpty()) {
            if (m_inThink) emit reasoningChunk(emit_);
            else           emit chunk(emit_);
        }
        m_contentTail = m_contentTail.right(hold);
        break;
    }
}

void LlamaClient::flushContentTail()
{
    if (m_contentTail.isEmpty())
        return;
    if (m_inThink) emit reasoningChunk(m_contentTail);
    else           emit chunk(m_contentTail);
    m_contentTail.clear();
}

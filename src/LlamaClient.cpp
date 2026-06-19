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
    if (m_reply)
        cancel();
    m_buffer.clear();

    QJsonArray messages{
        QJsonObject{{"role", "system"}, {"content", systemPrompt}},
        QJsonObject{{"role", "user"},   {"content", userText}}
    };

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

            const QString piece = choices.at(0).toObject()
                                         .value("delta").toObject()
                                         .value("content").toString();
            if (!piece.isEmpty())
                emit chunk(piece);
        }
    }
}

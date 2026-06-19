#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>

class QNetworkAccessManager;
class QNetworkReply;

// Talks to llama-server's OpenAI-compatible /v1/chat/completions endpoint and
// streams the reply back token-by-token via the chunk() signal.
class LlamaClient : public QObject {
    Q_OBJECT
public:
    explicit LlamaClient(QObject *parent = nullptr);

    void setBaseUrl(const QString &url)   { m_baseUrl = url; }
    void setTemperature(double t)         { m_temperature = t; }
    void setSeed(int s)                   { m_seed = s; }

    void refine(const QString &systemPrompt, const QString &userText);
    void cancel();
    bool isBusy() const { return m_reply != nullptr; }

signals:
    void chunk(const QString &text);      // incremental output
    void finished();                      // stream complete (or cancelled)
    void errorOccurred(const QString &message);

private:
    void handleReadyRead();

    QNetworkAccessManager *m_net   = nullptr;
    QNetworkReply         *m_reply = nullptr;
    QString    m_baseUrl;
    QByteArray m_buffer;
    double     m_temperature = 0.2;
    int        m_seed = -1;
};

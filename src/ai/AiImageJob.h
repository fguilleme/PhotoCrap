#pragma once
#include "AiImageTypes.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>
#include <functional>

class AiImageJob final : public QObject
{
    Q_OBJECT
public:
    explicit AiImageJob(QObject *_parent = nullptr,
                        std::unique_ptr<QNetworkAccessManager> _network = {});
    void start(const QVariantMap &_provider, const QString &_key, const QString &_prompt,
               const PhotoCrap::Ai::Input &_input);
    void cancel();
    QVariantMap resultMetadata() const
    {
        return m_resultMetadata;
    }
signals:
    void status(const QString &_status);
    void finished(const QByteArray &_image, const QString &_error);

private:
    using Handler = std::function<void(const QByteArray &)>;
    using Retry   = std::function<bool(const QNetworkReply &, int, bool)>;
    void       request(const QByteArray &_method, const QUrl &_url, const QByteArray &_body,
                       const QByteArray &_contentType, bool _auth, Handler _handler, int _redirects = 0,
                       bool _download = false, Handler _chunks = {}, Retry _retry = {});
    void json(const QByteArray &_method, const QUrl &_url, const QJsonObject &_body,
              Handler _handler);
    void multipart(const QUrl &_url, const QList<QPair<QByteArray, QByteArray>> &_fields,
                   const QList<QPair<QByteArray, QByteArray>> &_files, Handler _handler);
    void fail(const QString &_message);
    void done(const QByteArray &_bytes);
    void parseImages(const QByteArray &_bytes);
    void download(const QUrl &_url, bool _auth = false);
    void pollFal();
    void startComfy();
    void pollComfy();
    void       startGradio();
    void       submitGradio(const QUrl &_url, const QByteArray &_body, bool _allowRetry);
    void       streamGradio(const QString &_eventId);
    bool       retryGradioStream(const QString &_eventId);
    void       finishGradio(const QByteArray &_data);
    QByteArray authorization() const;
    QUrl endpoint(const QString &_path) const;
    bool sameOrigin(const QUrl &_url) const;
    bool acceptQueueUrl(const QUrl &_url) const;
    std::unique_ptr<QNetworkAccessManager> m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer                  m_poll, m_deadline;
    QVariantMap             m_provider;
    QVariantMap             m_resultMetadata;
    PhotoCrap::Ai::Input    m_input;
    QString                 m_key, m_prompt, m_jobId, m_uploadedImage, m_uploadedMask;
    QUrl                    m_statusUrl, m_responseUrl, m_cancelUrl;
    bool                    m_running    = false;
    quint64                 m_generation = 0;
    int                     m_gradioReconnects = 0;
};

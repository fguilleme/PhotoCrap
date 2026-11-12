#include "AiImageJob.h"
#include "AiSettings.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QUuid>
#include <cstring>

namespace
{
constexpr qint64 MaxReply = 100LL * 1024 * 1024;

// Qt can rewind an ordinary upload and replay it after a socket closes.
// A Gradio submission may already have created a generation on the server.
class GradioUpload final : public QIODevice
{
public:
    explicit GradioUpload(const QByteArray &_data) : m_data(_data)
    {
        open(ReadOnly);
    }
    qint64 size() const override
    {
        return m_data.size();
    }
    bool reset() override
    {
        return false;
    }

protected:
    qint64 readData(char *_target, qint64 _limit) override
    {
        const qint64 count = qMin(_limit, size() - pos());
        if (count <= 0)
            return -1;
        std::memcpy(_target, m_data.constData() + pos(), size_t(count));
        return count;
    }
    qint64 writeData(const char *, qint64) override
    {
        return -1;
    }

private:
    const QByteArray m_data;
};

QJsonObject      object(const QByteArray &_bytes)
{
    return QJsonDocument::fromJson(_bytes).object();
}
QString errorDetail(const QJsonObject &data)
{
    QString detail = data.value("error").isObject()
                         ? data.value("error").toObject().value("message").toString()
                         : data.value("error").toString();
    if (detail.isEmpty())
        detail = data.value("message").toString();
    if (detail.isEmpty())
        detail = data.value("detail").toString();
    if (detail.isEmpty() && data.value("detail").isObject())
        detail = data.value("detail").toObject().value("message").toString();
    if (detail.isEmpty()) {
        QStringList messages;
        for (const auto value : data.value("detail").toArray()) {
            const auto item    = value.toObject();
            const auto message = item.value("msg").toString();
            if (message.isEmpty())
                continue;
            QStringList location;
            for (const auto part : item.value("loc").toArray()) {
                if (part.isString())
                    location.append(part.toString());
                else if (part.isDouble())
                    location.append(QString::number(part.toInt()));
            }
            messages.append(location.isEmpty() ? message : location.join('.') + ": " + message);
            if (messages.size() == 4)
                break;
        }
        detail = messages.join('\n');
    }
    // Validation responses can echo the entire submitted image under "input".
    // Never stringify that object; retain only field names and short messages.
    detail.replace(QRegularExpression(R"(data:[^\s"'<>]+)"), "[image masquée]");
    return detail.left(900);
}
QJsonValue substitute(const QJsonValue &_value, const QJsonObject &_values)
{
    if (_value.isString()) {
        const auto text = _value.toString();
        if (_values.contains(text))
            return _values.value(text);
        return text;
    }
    if (_value.isArray()) {
        QJsonArray a;
        for (const auto &v : _value.toArray())
            a.append(substitute(v, _values));
        return a;
    }
    if (_value.isObject()) {
        auto o = _value.toObject();
        for (auto it = o.begin(); it != o.end(); ++it)
            it.value() = substitute(it.value(), _values);
        return o;
    }
    return _value;
}
} // namespace
AiImageJob::AiImageJob(QObject *_parent, std::unique_ptr<QNetworkAccessManager> _network)
    : QObject(_parent),
      m_network(_network ? std::move(_network) : std::make_unique<QNetworkAccessManager>())
{
    m_poll.setSingleShot(true);
    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        if (m_provider.value("kind") == "fal")
            pollFal();
        else
            pollComfy();
    });
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(15 * 60 * 1000);
    connect(&m_deadline, &QTimer::timeout, this,
            [this] { fail(tr("Le délai de génération IA est dépassé.")); });
}
QUrl AiImageJob::endpoint(const QString &_path) const
{
    auto text = m_provider.value("url").toString();
    while (text.endsWith('/'))
        text.chop(1);
    return QUrl(text + '/' + _path);
}
bool AiImageJob::sameOrigin(const QUrl &_url) const
{
    const auto base = endpoint({});
    return _url.scheme() == base.scheme() && _url.host() == base.host() &&
           _url.port(_url.scheme() == "https" ? 443 : 80) ==
               base.port(base.scheme() == "https" ? 443 : 80);
}
bool AiImageJob::acceptQueueUrl(const QUrl &_url) const
{
    return AiSettings::validEndpoint(QUrl(_url.toString(QUrl::RemoveQuery))) && sameOrigin(_url);
}
void AiImageJob::fail(const QString &_message)
{
    if (!m_running)
        return;
    auto message = _message;
    if (!m_key.isEmpty())
        message.replace(m_key, "[clé masquée]");
    const auto auth  = authorization();
    const auto token = auth.mid(auth.indexOf(' ') + 1);
    if (!token.isEmpty())
        message.replace(QString::fromLatin1(token), "[identifiants masqués]");
    cancel();
    emit finished({}, message.left(1000));
}
void AiImageJob::done(const QByteArray &_bytes)
{
    if (!m_running)
        return;
    m_running = false;
    m_poll.stop();
    m_deadline.stop();
    m_key.clear();
    m_jobId.clear();
    m_cancelUrl   = QUrl{};
    m_statusUrl   = QUrl{};
    m_responseUrl = QUrl{};
    m_input       = {};
    emit finished(_bytes, {});
}
void AiImageJob::request(const QByteArray &_method, const QUrl &_url, const QByteArray &_body,
                         const QByteArray &_contentType, bool _auth, Handler _handler,
                         int _redirects, bool _download, Handler _chunks, Retry _retry)
{
    if (!m_running)
        return;
    QNetworkRequest request(_url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(_chunks ? 0 : 180000);
    if (m_provider.value("kind") == "gradio") {
        // A completed/aborted SSE response must never poison the next upload's
        // pooled socket. Qwen's local Gradio server uses HTTP/1.1.
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        request.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, 0);
        request.setRawHeader("Connection", "close");
        if (_chunks) {
            request.setRawHeader("Cache-Control", "no-cache");
            request.setRawHeader("Accept-Encoding", "identity");
        }
    }
    request.setRawHeader("Accept", _chunks ? "text/event-stream"
                                           : "application/json, image/png, image/jpeg, image/webp");
    if (!_contentType.isEmpty())
        request.setRawHeader("Content-Type", _contentType);
    const auto auth = authorization();
    if (_auth && !m_key.isEmpty() && sameOrigin(_url) && !auth.isEmpty())
        request.setRawHeader("Authorization", auth);
    QNetworkReply *reply;
    if (_method == "POST" && m_provider.value("kind") == "gradio") {
        auto *upload = new GradioUpload(_body);
        request.setHeader(QNetworkRequest::ContentLengthHeader, _body.size());
        reply = m_network->sendCustomRequest(request, _method, upload);
        upload->setParent(reply);
    } else {
        reply = m_network->sendCustomRequest(request, _method, _body);
    }
    m_reply               = reply;
    const auto generation = m_generation;
    const auto requestSent = _retry && _method == "POST" ? std::make_shared<bool>(false) : nullptr;
    if (requestSent)
        connect(reply, &QNetworkReply::requestSent, reply, [requestSent] { *requestSent = true; });
    if (_method == "POST" && _url.path().endsWith("/images/edits"))
        connect(reply, &QNetworkReply::uploadProgress, this,
                [this, generation](qint64 sent, qint64 total) {
                    if (m_running && generation == m_generation && total > 0 && sent >= total)
                        emit status(tr("En attente du résultat chez le fournisseur…"));
                });
    auto bytes = std::make_shared<QByteArray>();
    auto received = std::make_shared<qint64>(0);
    connect(reply, &QNetworkReply::readyRead, this,
            [this, reply, bytes, received, generation, _chunks] {
                if (generation != m_generation || !m_running)
                    return;
                const auto chunk = reply->readAll();
                *received += chunk.size();
                if (*received > MaxReply) {
                    fail(tr("La réponse IA dépasse 100 Mio."));
                    return;
                }
                const auto code =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (_chunks && code >= 200 && code < 300)
                    _chunks(chunk);
                else
                    bytes->append(chunk);
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, bytes, received, generation, _auth, _redirects, _download, _chunks,
             _retry, requestSent, _handler = std::move(_handler)] {
                reply->deleteLater();
                if (generation != m_generation || !m_running)
                    return;
                m_reply.clear();
                const auto tail = reply->readAll();
                *received += tail.size();
                const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (*received > MaxReply) {
                    fail(tr("La réponse IA dépasse 100 Mio."));
                    return;
                }
                if (code >= 300 && code < 400 && _download && _redirects < 4) {
                    const auto target = reply->url().resolved(
                        reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
                    if (target.isValid() && target.userInfo().isEmpty() &&
                        (target.scheme() == "https" ||
                         (target.scheme() == "http" && sameOrigin(target)))) {
                        this->request("GET", target, {}, {}, _auth && sameOrigin(target),
                                      std::move(_handler), _redirects + 1, true);
                        return;
                    }
                }
                if (code >= 300 && code < 400) {
                    fail(tr("Le fournisseur redirige la requête. Vérifiez son adresse dans les "
                            "réglages IA."));
                    return;
                }
                // A terminal event can accompany the last bytes of a closed
                // socket. Consume it before deciding whether the stream failed.
                if (_chunks && code >= 200 && code < 300 && !tail.isEmpty()) {
                    _chunks(tail);
                    if (generation != m_generation || !m_running)
                        return;
                }
                if (reply->error() != QNetworkReply::NoError || code >= 400) {
                    if (_retry && _retry(*reply, code, requestSent && *requestSent))
                        return;
                    bytes->append(tail);
                    const auto data = object(*bytes);
                    const auto detail = errorDetail(data);
                    const auto message = tr("Erreur IA (%1) : %2")
                                             .arg(code)
                                             .arg(detail.isEmpty() ? reply->errorString() : detail);
                    fail(m_provider.value("kind") == "gradio" && code == 0
                             ? (m_jobId.isEmpty()
                                    ? tr("Envoi à Qwen non confirmé : %1. Aucun nouvel envoi "
                                         "automatique.")
                                          .arg(message)
                                    : tr("Lecture du résultat Qwen interrompue (tâche %1) : %2")
                                          .arg(m_jobId, message))
                             : message);
                    return;
                }
                if (!_chunks)
                    bytes->append(tail);
                _handler(*bytes);
            });
}
void AiImageJob::json(const QByteArray &_method, const QUrl &_url, const QJsonObject &_body,
                      Handler _handler)
{
    request(_method, _url,
            _method == "GET" ? QByteArray{} : QJsonDocument(_body).toJson(QJsonDocument::Compact),
            "application/json", true, std::move(_handler));
}
void AiImageJob::multipart(const QUrl &_url, const QList<QPair<QByteArray, QByteArray>> &_fields,
                           const QList<QPair<QByteArray, QByteArray>> &_files, Handler _handler)
{
    const auto boundary = "PhotoCrap" + QUuid::createUuid().toByteArray(QUuid::Id128);
    QByteArray body;
    for (const auto &field : _fields)
        body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + field.first +
                "\"\r\n\r\n" + field.second + "\r\n";
    for (const auto &file : _files)
        body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + file.first +
                "\"; filename=\"photocrap.png\"\r\nContent-Type: image/png\r\n\r\n" + file.second +
                "\r\n";
    body += "--" + boundary + "--\r\n";
    request("POST", _url, body, "multipart/form-data; boundary=" + boundary, true,
            std::move(_handler));
}
void AiImageJob::start(const QVariantMap &_rawProvider, const QString &_key, const QString &_prompt,
                       const PhotoCrap::Ai::Input &_input)
{
    const auto _provider = AiSettings::normalizedProvider(_rawProvider);
    cancel();
    m_jobId.clear();
    m_cancelUrl   = QUrl{};
    m_statusUrl   = QUrl{};
    m_responseUrl = QUrl{};
    m_provider    = _provider;
    m_resultMetadata.clear();
    m_key         = _key;
    m_prompt      = _provider.value("promptMode") == "none" ? QString{} : _prompt;
    m_input       = _input;
    m_running     = true;
    ++m_generation;
    m_deadline.start();
    emit       status(tr("Envoi au fournisseur…"));
    const auto kind    = _provider.value("kind").toString();
    auto       options = object(_provider.value("options", "{}").toString().toUtf8());
    if (kind == "openai" || kind == "compatible") {
        QList<QPair<QByteArray, QByteArray>> fields{
            {"model", _provider.value("model").toString().toUtf8()},
            {"prompt", _prompt.toUtf8()},
            {"n", "1"}};
        for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
            if (QStringList{"model", "prompt", "n", "image", "mask"}.contains(it.key()))
                continue;
            if (!QRegularExpression("^[A-Za-z0-9_]+$").match(it.key()).hasMatch())
                continue;
            fields.append({it.key().toUtf8(), it.value().isString()
                                                  ? it.value().toString().toUtf8()
                                                  : QJsonDocument(QJsonArray{it.value()})
                                                        .toJson(QJsonDocument::Compact)
                                                        .mid(1)
                                                        .chopped(1)});
        }
        QList<QPair<QByteArray, QByteArray>> files{{"image", _input.image}};
        if (_input.selected && _input.maskEnabled)
            files.append({"mask", _input.mask});
        multipart(endpoint("images/edits"), fields, files,
                  [this](const QByteArray &_bytes) { parseImages(_bytes); });
    } else if (kind == "fal") {
        const auto image =
            QStringLiteral("data:image/png;base64,") + QString::fromLatin1(_input.image.toBase64());
        const auto field      = _provider.value("imageField", "image_urls").toString();
        const auto mode       = _provider.value("promptMode", "required").toString();
        if (mode == "none" || (mode == "optional" && _prompt.trimmed().isEmpty()))
            options.remove("prompt");
        else
            options["prompt"] = _prompt;
        if (mode == "required")
            options["num_images"] = 1;
        options[field]        = _provider.value("imageArray", true).toBool()
                                    ? QJsonValue(QJsonArray{image})
                                    : QJsonValue(image);
        json("POST", endpoint(_provider.value("model").toString()), options,
             [this](const QByteArray &_bytes) {
                 const auto data = object(_bytes);
                 if (!data.value("images").toArray().isEmpty() || data.value("image").isObject()) {
                     parseImages(_bytes);
                     return;
                 }
                 m_statusUrl   = QUrl(data.value("status_url").toString());
                 m_responseUrl = QUrl(data.value("response_url").toString());
                 m_cancelUrl   = QUrl(data.value("cancel_url").toString());
                 if (!acceptQueueUrl(m_statusUrl) || !acceptQueueUrl(m_responseUrl) ||
                     (!m_cancelUrl.isEmpty() && !acceptQueueUrl(m_cancelUrl))) {
                     fail(tr("La file IA a renvoyé des adresses de suivi invalides."));
                     return;
                 }
                 pollFal();
             });
    } else if (kind == "gradio")
        startGradio();
    else if (kind == "comfy")
        startComfy();
    else
        fail(tr("Ce type de fournisseur n’est pas pris en charge."));
}
void AiImageJob::pollFal()
{
    json("GET", m_statusUrl, {}, [this](const QByteArray &_bytes) {
        const auto state = object(_bytes).value("status").toString();
        if (state == "COMPLETED")
            json("GET", m_responseUrl, {},
                 [this](const QByteArray &_result) { parseImages(_result); });
        else if (state == "IN_QUEUE" || state == "IN_PROGRESS") {
            emit status(state == "IN_QUEUE" ? tr("En attente chez le fournisseur…")
                                            : tr("Génération de l’image…"));
            m_poll.start();
        } else
            fail(tr("La file IA a renvoyé un état inattendu."));
    });
}
void AiImageJob::parseImages(const QByteArray &_bytes)
{
    const auto data = object(_bytes);
    auto       list = data.value("data").toArray();
    if (list.isEmpty())
        list = data.value("images").toArray();
    if (list.isEmpty() && data.value("image").isObject())
        list.append(data.value("image"));
    if (list.isEmpty()) {
        fail(tr("Le fournisseur n’a renvoyé aucune image."));
        return;
    }
    const auto first = list.first().toObject();
    if (first.contains("b64_json")) {
        const auto decoded = QByteArray::fromBase64(first.value("b64_json").toString().toLatin1(),
                                                    QByteArray::AbortOnBase64DecodingErrors);
        if (decoded.isEmpty())
            fail(tr("L’image IA encodée est invalide."));
        else
            done(decoded);
    } else
        download(QUrl(first.value("url").toString()));
}
void AiImageJob::download(const QUrl &_url, bool _auth)
{
    if (_url.scheme() == "data") {
        const auto text = _url.toString();
        const auto match =
            QRegularExpression("^data:image/(png|jpeg|webp);base64,([A-Za-z0-9+/=\\r\\n]+)$")
                .match(text);
        if (!match.hasMatch()) {
            fail(tr("L’image IA encodée est invalide."));
            return;
        }
        const auto bytes = QByteArray::fromBase64(match.captured(2).toLatin1(),
                                                  QByteArray::AbortOnBase64DecodingErrors);
        if (bytes.isEmpty())
            fail(tr("L’image IA encodée est invalide."));
        else
            done(bytes);
        return;
    }

    if (!_url.isValid() || !_url.userInfo().isEmpty() ||
        !(_url.scheme() == "https" || (_url.scheme() == "http" && sameOrigin(_url)))) {
        fail(tr("L’adresse de l’image renvoyée par l’IA n’est pas valide."));
        return;
    }
    emit status(tr("Récupération du résultat…"));
    request(
        "GET", _url, {}, {}, _auth, [this](const QByteArray &_bytes) { done(_bytes); }, 0, true);
}
void AiImageJob::startComfy()
{
    multipart(endpoint("upload/image"), {{"overwrite", "false"}}, {{"image", m_input.image}},
              [this](const QByteArray &_bytes) {
                  const auto value  = object(_bytes);
                  m_uploadedImage   = value.value("name").toString();
                  const auto folder = value.value("subfolder").toString();
                  if (!folder.isEmpty() && !m_uploadedImage.isEmpty())
                      m_uploadedImage = folder + '/' + m_uploadedImage;
                  if (m_uploadedImage.isEmpty()) {
                      fail(tr("ComfyUI n’a pas accepté l’image source."));
                      return;
                  }
                  multipart(
                      endpoint("upload/image"), {{"overwrite", "false"}}, {{"image", m_input.mask}},
                      [this](const QByteArray &_mask) {
                          const auto value  = object(_mask);
                          m_uploadedMask    = value.value("name").toString();
                          const auto folder = value.value("subfolder").toString();
                          if (!folder.isEmpty() && !m_uploadedMask.isEmpty())
                              m_uploadedMask = folder + '/' + m_uploadedMask;
                          if (m_uploadedMask.isEmpty()) {
                              fail(tr("ComfyUI n’a pas accepté le masque."));
                              return;
                          }
                          const QJsonObject values{
                              {"{{image}}", m_uploadedImage},
                              {"{{mask}}", m_uploadedMask},
                              {"{{prompt}}", m_prompt},
                              {"{{model}}", m_provider.value("model").toString()},
                              {"{{width}}", m_input.sentSize.width()},
                              {"{{height}}", m_input.sentSize.height()},
                              {"{{seed}}", double(QRandomGenerator::global()->generate64() &
                                                  ((1ULL << 53) - 1))}};
                          const auto workflow = substitute(
                              object(m_provider.value("workflow").toString().toUtf8()), values);
                          json("POST", endpoint("prompt"),
                               {{"prompt", workflow},
                                {"client_id", QUuid::createUuid().toString(QUuid::WithoutBraces)}},
                               [this](const QByteArray &_reply) {
                                   const auto value = object(_reply);
                                   m_jobId          = value.value("prompt_id").toString();
                                   if (m_jobId.isEmpty()) {
                                       fail(tr("ComfyUI a refusé le workflow. Vérifiez le "
                                               "checkpoint et les nœuds installés."));
                                       return;
                                   }
                                   pollComfy();
                               });
                      });
              });
}
void AiImageJob::pollComfy()
{
    json("GET", endpoint("history/" + QString::fromLatin1(QUrl::toPercentEncoding(m_jobId))), {},
         [this](const QByteArray &_bytes) {
             const auto entry = object(_bytes).value(m_jobId).toObject();
             const auto state = entry.value("status").toObject();
             if (state.value("status_str").toString() == "error") {
                 fail(tr("Le workflow ComfyUI a échoué."));
                 return;
             }
             const auto outputs = entry.value("outputs").toObject();
             for (auto it = outputs.constBegin(); it != outputs.constEnd(); ++it) {
                 const auto images = it.value().toObject().value("images").toArray();
                 if (images.isEmpty())
                     continue;
                 const auto image = images.first().toObject();
                 QUrl       url   = endpoint("view");
                 QUrlQuery  query;
                 query.addQueryItem("filename", image.value("filename").toString());
                 query.addQueryItem("subfolder", image.value("subfolder").toString());
                 query.addQueryItem("type", image.value("type").toString("output"));
                 url.setQuery(query);
                 download(url, true);
                 return;
             }
             if (state.value("completed").toBool()) {
                 fail(tr("Le workflow ComfyUI ne produit aucune image. Ajoutez un nœud SaveImage "
                         "ou PreviewImage."));
                 return;
             }
             emit status(tr("Génération dans ComfyUI…"));
             m_poll.start();
         });
}
void AiImageJob::cancel()
{
    if (!m_running)
        return;
    const bool fal       = m_provider.value("kind") == "fal";
    const auto cancelUrl = m_cancelUrl;
    const auto jobId     = m_jobId;
    m_running            = false;
    ++m_generation;
    m_poll.stop();
    m_deadline.stop();
    if (m_reply)
        m_reply->abort();
    m_reply.clear();
    // Target only this queued task. Never interrupt another ComfyUI execution.
    if ((fal && acceptQueueUrl(cancelUrl)) ||
        (m_provider.value("kind") == "comfy" && !jobId.isEmpty())) {
        QNetworkRequest request(fal ? cancelUrl : endpoint("queue"));
        request.setTransferTimeout(10000);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);
        if (!m_key.isEmpty())
            request.setRawHeader("Authorization", (fal ? "Key " : "Bearer ") + m_key.toUtf8());
        request.setRawHeader("Content-Type", "application/json");
        auto *reply = m_network->sendCustomRequest(
            request, fal ? "PUT" : "POST",
            fal ? QByteArray{}
                : QJsonDocument(QJsonObject{{"delete", QJsonArray{jobId}}})
                      .toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }
    m_input = {};
    m_prompt.clear();
    m_key.clear();
    m_jobId.clear();
    m_statusUrl   = QUrl{};
    m_responseUrl = QUrl{};
    m_cancelUrl   = QUrl{};
}

#include "AiImageJob.h"
#include "AiSettings.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <cmath>
#include <utility>

namespace
{
struct Events {
    QByteArray pending, data, type;
    qsizetype  search = 0;
};
QString serverError(const QByteArray &bytes)
{
    const auto document = QJsonDocument::fromJson(bytes);
    const auto value    = document.isArray()
                              ? (document.array().isEmpty() ? QJsonValue{} : document.array().first())
                              : QJsonValue(document.object());
    auto text = value.isString() ? value.toString() : value.toObject().value("error").toString();
    text.replace(QRegularExpression(R"(data:[^\s"'<>]+)"), "[image masquée]");
    return text.left(800);
}
} // namespace

QByteArray AiImageJob::authorization() const
{
    if (m_provider.value("kind") == "gradio") {
        if (m_provider.value("authMode", "basic") == "none")
            return {};
        return "Basic " +
               (m_provider.value("username").toString().toUtf8() + ':' + m_key.toUtf8()).toBase64();
    }
    return (m_provider.value("kind") == "fal" ? "Key " : "Bearer ") + m_key.toUtf8();
}

void AiImageJob::startGradio()
{
    m_gradioReconnects = 0;
    m_network.clearConnectionCache();
    auto options =
        QJsonDocument::fromJson(m_provider.value("options", "{}").toString().toUtf8()).object();
    const auto error = AiSettings::gradioOptionsError(options.toVariantMap());
    const auto api   = m_provider.value("model").toString();
    const auto auth  = m_provider.value("authMode", "basic").toString();
    if (!QRegularExpression("^[A-Za-z0-9_-]+$").match(api).hasMatch() ||
        !QStringList{"basic", "none"}.contains(auth) || !error.isEmpty()) {
        fail(error.isEmpty() ? tr("Le nom de l’API Qwen n’est pas valide.") : error);
        return;
    }
    const auto username = m_provider.value("username").toString();
    if (auth == "basic" && (username.isEmpty() || m_key.isEmpty() || username.contains(':') ||
                            username.contains('\r') || username.contains('\n'))) {
        fail(tr("Renseignez l’utilisateur et le mot de passe HTTP Basic dans Réglages → IA."));
        return;
    }
    const QString image = "data:image/png;base64," + QString::fromLatin1(m_input.image.toBase64());
    // The named v2 API expects a JSON *string* containing an array, not the
    // positional {data:[...]} body used by the ordinary Gradio endpoint.
    options["images_b64_json"] =
        QString::fromUtf8(QJsonDocument(QJsonArray{image}).toJson(QJsonDocument::Compact));
    options["prompt"] = m_prompt;
    options.remove("data");
    if (!options.contains("lora_adapter"))
        options["lora_adapter"] = "Photo-to-Anime";
    if (!options.contains("seed"))
        options["seed"] = 42;
    if (!options.contains("randomize_seed"))
        options["randomize_seed"] = true;
    if (!options.contains("guidance_scale"))
        options["guidance_scale"] = 1.;
    if (!options.contains("steps"))
        options["steps"] = 4;
    json("POST", endpoint("gradio_api/call/v2/" + api), options, [this](const QByteArray &bytes) {
        const auto eventId = QJsonDocument::fromJson(bytes).object().value("event_id").toString();
        if (!QRegularExpression("^[A-Za-z0-9_-]{1,256}$").match(eventId).hasMatch()) {
            fail(tr("Qwen n’a pas renvoyé un identifiant de tâche valide."));
            return;
        }
        m_jobId = eventId;
        streamGradio(eventId);
    });
}

void AiImageJob::streamGradio(const QString &eventId)
{
    const auto events     = std::make_shared<Events>();
    const auto generation = m_generation;
    const auto consume    = [this, events, generation](const QByteArray &chunk) {
        if (!m_running || generation != m_generation)
            return;
        events->pending += chunk;
        while (m_running && generation == m_generation) {
            const auto at = events->pending.indexOf('\n', events->search);
            if (at < 0) {
                events->search = events->pending.size();
                break;
            }
            auto line = events->pending.left(at);
            events->pending.remove(0, at + 1);
            events->search = 0;
            if (line.endsWith('\r'))
                line.chop(1);
            if (line.startsWith("\xef\xbb\xbf"))
                line.remove(0, 3);
            if (line.isEmpty()) {
                const auto type = std::exchange(events->type, {});
                const auto data = std::exchange(events->data, {});
                if (type == "complete") {
                    // Stop reading at the terminal event, even when the server
                    // leaves its SSE socket open. Do not cancel any other job.
                    if (m_reply) {
                        const auto reply = m_reply;
                        m_reply.clear();
                        reply->disconnect(this);
                        reply->abort();
                        reply->deleteLater();
                    }
                    finishGradio(data);
                    return;
                }
                if (type == "error") {
                    const auto detail = serverError(data);
                    fail(detail.isEmpty() ? tr("La génération Qwen a échoué.")
                                             : tr("Erreur Qwen : %1").arg(detail));
                    return;
                }
                if (type == "generating")
                    emit status(tr("Génération de l’image avec Qwen…"));
                continue;
            }
            if (line.startsWith(':'))
                continue;
            const auto colon = line.indexOf(':');
            const auto field = colon < 0 ? line : line.left(colon);
            auto       value = colon < 0 ? QByteArray{} : line.mid(colon + 1);
            if (value.startsWith(' '))
                value.remove(0, 1);
            if (field == "event")
                events->type = value;
            else if (field == "data") {
                if (!events->data.isEmpty())
                    events->data += '\n';
                events->data += value;
            }
        }
    };
    emit status(tr("En attente du résultat Qwen…"));
    request(
        "GET", endpoint("gradio_api/call/" + m_provider.value("model").toString() + '/' + eventId),
        {}, {}, true,
        [this, eventId](const QByteArray &) {
            if (!retryGradioStream(eventId))
                fail(tr("Le flux Qwen s’est interrompu avant le résultat final (tâche %1).")
                         .arg(eventId));
        },
        0, false, consume,
        [this, eventId](QNetworkReply::NetworkError error, int code) {
            if (code == 502 || code == 503 || code == 504)
                return retryGradioStream(eventId);
            if (code != 0 && (code < 200 || code >= 300))
                return false;
            switch (error) {
            case QNetworkReply::TimeoutError:
            case QNetworkReply::RemoteHostClosedError:
            case QNetworkReply::TemporaryNetworkFailureError:
            case QNetworkReply::NetworkSessionFailedError:
            case QNetworkReply::ConnectionRefusedError:
            case QNetworkReply::HostNotFoundError:
            case QNetworkReply::UnknownNetworkError:
                return retryGradioStream(eventId);
            default:
                return false;
            }
        });
}

bool AiImageJob::retryGradioStream(const QString &eventId)
{
    if (!m_running || m_jobId != eventId || m_gradioReconnects >= 3)
        return false;
    const int  delay      = 500 * (1 << m_gradioReconnects++);
    const auto generation = m_generation;
    m_network.clearConnectionCache();
    emit status(
        tr("Connexion Qwen interrompue — reprise de la lecture (%1/3)…").arg(m_gradioReconnects));
    QTimer::singleShot(delay, this, [this, eventId, generation] {
        if (m_running && generation == m_generation && m_jobId == eventId)
            streamGradio(eventId);
    });
    return true;
}

void AiImageJob::finishGradio(const QByteArray &data)
{
    const auto document = QJsonDocument::fromJson(data);
    QJsonValue result =
        document.isArray() ? QJsonValue(document.array()) : QJsonValue(document.object());
    for (int depth = 0; depth < 3; ++depth) {
        if (result.isArray()) {
            const auto array = result.toArray();
            if (array.isEmpty())
                break;
            if (array.first().isString() && array.first().toString().startsWith("data:image/")) {
                result = QJsonObject{{"image", array.first()},
                                     {"seed", array.size() > 1 ? array[1] : QJsonValue{}}};
                break;
            }
            result = array.first();
        } else if (result.isString()) {
            const auto nested = QJsonDocument::fromJson(result.toString().toUtf8());
            result = nested.isArray() ? QJsonValue(nested.array()) : QJsonValue(nested.object());
        } else
            break;
    }
    const auto object = result.toObject();
    const auto image  = object.value("image").toString();
    if (!image.startsWith("data:image/")) {
        fail(tr("L’API Qwen n’a renvoyé aucune image encodée valide."));
        return;
    }
    const auto seed = object.value("seed");
    if (seed.isDouble() && std::isfinite(seed.toDouble()) && seed.toDouble() >= 0 &&
        seed.toDouble() <= 9007199254740991. && seed.toDouble() == std::floor(seed.toDouble()))
        m_resultMetadata["seed"] = seed.toVariant();
    download(QUrl(image));
}

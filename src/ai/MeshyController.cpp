#include "MeshyController.h"
#include "AiImageTypes.h"
#include "AiSettings.h"
#include "Document.h"
#include "model3d/Model3D.h"
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <utility>

namespace
{
constexpr qsizetype ResponseLimit = 1024 * 1024;
constexpr qsizetype ImageLimit    = 20000000;

QJsonObject meshOptions(bool _ultra)
{
    return {{"ai_model", "meshy-7.1"},
            {"should_remesh", false},
            {"geometry_resolution", _ultra ? "4k" : "standard"},
            {"target_formats", QJsonArray{"glb"}}};
}
} // namespace

MeshyController::MeshyController(Document *_document, AiSettings *_settings, const QUrl &_endpoint)
    : QObject(_document), m_document(_document), m_settings(_settings), m_endpoint(_endpoint)
{
    m_next.setSingleShot(true);
    m_deadline.setSingleShot(true);
    connect(&m_next, &QTimer::timeout, this, [this] {
        if (!m_model)
            poll();
        else
            insert();
    });
    connect(&m_deadline, &QTimer::timeout, this,
            [this] { finish({}, tr("Le délai d’attente du résultat Meshy est dépassé.")); });
    connect(m_document, &Document::errorOccurred, this, [this](const QString &_error) {
        if (m_importing)
            m_importError = _error;
    });
    connect(m_document, &Document::operationChanged, this, [this] {
        if (!m_importing || m_document->operationBusy())
            return;
        const bool inserted = currentDocument() && m_document->layerCount() == m_layerCount + 1;
        if (inserted)
            finish(tr("Calque 3D Meshy ajouté."));
        else
            finish({}, m_importError.isEmpty()
                           ? tr("L’importation du modèle Meshy a été interrompue.")
                           : m_importError);
    });
}

MeshyController::~MeshyController()
{
    if (m_cancel)
        m_cancel->store(true);
    if (m_reply)
        m_reply->disconnect(this);
}

bool MeshyController::generate(int _source, const QString &_prompt, const QString &_key,
                               bool _remember, bool _ultra)
{
    if (m_busy || m_document->aiInsertionBusy() || m_document->transformActive() || _source < 0 ||
        _source > 2 || (_source == 2 && !m_document->hasSelection()))
        return false;
    const auto prompt = _prompt.trimmed();
    if (_source == 0 && (prompt.isEmpty() || prompt.size() > 800))
        return false;

    m_busy      = true;
    m_stage     = _source == 0 ? Stage::TextPreview : Stage::Image;
    m_session   = m_document->documentSession();
    m_reference = QRectF(0, 0, 1, 1);
    m_ultra     = _ultra;
    m_cancel    = std::make_shared<std::atomic_bool>(false);
    m_name      = _source == 0 ? tr("Meshy — %1").arg(prompt.simplified().left(64))
                               : tr("Meshy — %1").arg(m_document->name().left(64));
    m_status    = tr("Préparation de la génération Meshy…");
    m_error.clear();
    m_progress = 0;
    m_deadline.start(20 * 60 * 1000);
    emit changed();

    const auto start = [this, _source, prompt, cancel = m_cancel](const QString &_secret,
                                                                  const QString &_error) {
        if (cancel->load())
            return;
        if (_secret.isEmpty() || _secret.contains(u'\r') || _secret.contains(u'\n')) {
            finish({}, _error.isEmpty() ? tr("Saisissez une clé API Meshy valide.") : _error);
            return;
        }
        m_key = _secret.toUtf8();
        if (_source != 0) {
            prepare(_source == 2);
            return;
        }
        auto body      = meshOptions(m_ultra);
        body["mode"]   = "preview";
        body["prompt"] = prompt;
        submit(body);
    };
    const auto key = _key.trimmed();
    m_remember     = _remember && !key.isEmpty();
    if (key.isEmpty())
        m_settings->resolveKey("meshy", this, start);
    else
        start(key, {});
    return true;
}

void MeshyController::prepare(bool _selection)
{
    if (!currentDocument() || m_document->aiInsertionBusy() || m_document->transformActive()) {
        finish({}, tr("Le document n’est plus disponible pour préparer l’image."));
        return;
    }
    auto *watcher = new QFutureWatcher<PhotoCrap::Ai::Input>(this);
    connect(watcher, &QFutureWatcher<PhotoCrap::Ai::Input>::finished, this,
            [this, watcher, cancel = m_cancel] {
                watcher->deleteLater();
                if (cancel->load())
                    return;
                PhotoCrap::Ai::Input input;
                try {
                    input = watcher->result();
                } catch (...) {
                    finish({}, tr("Impossible de préparer l’image pour Meshy."));
                    return;
                }
                if (!input.error.isEmpty() || input.image.isEmpty() ||
                    input.image.size() > ImageLimit || input.sentSize.width() < 32 ||
                    input.sentSize.height() < 32) {
                    finish(
                        {},
                        input.error.isEmpty()
                            ? tr("Meshy exige une image d’au moins 32 × 32 px et de 20 Mo maximum.")
                            : input.error);
                    return;
                }
                // Store placement in document coordinates relative to the canvas,
                // independent of upload downsampling and the view's zoom.
                const double width = input.canvas.width(), height = input.canvas.height();
                m_reference = {input.bounds.x() / width, input.bounds.y() / height,
                               input.bounds.width() / width, input.bounds.height() / height};
                auto body   = meshOptions(m_ultra);
                body["image_url"] =
                    QString::fromLatin1("data:image/png;base64," + input.image.toBase64());
                body["should_texture"]     = true;
                body["enable_pbr"]         = false;
                body["texture_resolution"] = "4k";
                submit(body);
            });
    watcher->setFuture(m_document->prepareAiInput(_selection, 2200, m_cancel));
}

QUrl MeshyController::taskUrl() const
{
    auto    url  = m_endpoint;
    QString path = m_stage == Stage::Image ? QStringLiteral("/openapi/v1/image-to-3d")
                                           : QStringLiteral("/openapi/v2/text-to-3d");
    if (!m_task.isEmpty())
        path += u'/' + m_task;
    url.setPath(path);
    return url;
}

void MeshyController::submit(const QJsonObject &_body)
{
    m_task.clear();
    switch (m_stage) {
    case Stage::TextPreview:
        m_status = tr("Meshy génère la géométrie…");
        break;
    case Stage::TextRefine:
        m_status = tr("Meshy génère la texture…");
        break;
    case Stage::Image:
        m_status = tr("Meshy génère le modèle à partir de l’image…");
        break;
    }
    emit changed();
    request(taskUrl(), QJsonDocument(_body).toJson(QJsonDocument::Compact), true, ResponseLimit,
            [this](const QByteArray &_data) {
                m_task = QJsonDocument::fromJson(_data).object().value("result").toString();
                static const QRegularExpression idPattern(QStringLiteral("^[A-Za-z0-9_-]{1,128}$"));
                if (!idPattern.match(m_task).hasMatch()) {
                    finish({}, tr("Meshy n’a pas renvoyé d’identifiant de tâche valide."));
                    return;
                }
                if (std::exchange(m_remember, false))
                    m_settings->saveKey("meshy", QString::fromUtf8(m_key));
                m_next.start(2000);
            });
}

void MeshyController::poll()
{
    request(taskUrl(), {}, true, ResponseLimit, [this](const QByteArray &_data) {
        const auto task   = QJsonDocument::fromJson(_data).object();
        const auto status = task.value("status").toString();
        if (status == "PENDING" || status == "IN_PROGRESS") {
            const int progress = qBound(0, task.value("progress").toInt(), 100);
            const int overall  = m_stage == Stage::TextPreview  ? progress * 45 / 100
                                 : m_stage == Stage::TextRefine ? 45 + progress * 45 / 100
                                                                : progress * 90 / 100;
            m_progress         = qMax(m_progress, overall);
            emit changed();
            m_next.start(2000);
            return;
        }
        if (status != "SUCCEEDED") {
            finish({}, status == "FAILED" || status == "CANCELED"
                           ? tr("Meshy a interrompu la génération.")
                           : tr("Meshy a renvoyé un état de tâche inconnu."));
            return;
        }
        if (m_stage == Stage::TextPreview) {
            const auto preview = m_task;
            m_stage            = Stage::TextRefine;
            m_progress         = 45;
            submit({{"mode", "refine"},
                    {"preview_task_id", preview},
                    {"ai_model", "meshy-7.1"},
                    {"enable_pbr", false},
                    {"texture_resolution", "4k"},
                    {"target_formats", QJsonArray{"glb"}}});
            return;
        }
        download(QUrl(task.value("model_urls").toObject().value("glb").toString()));
    });
}

void MeshyController::download(const QUrl &_url)
{
    const bool trusted =
        _url.scheme() == "https" && _url.host().endsWith(".meshy.ai") && _url.port(443) == 443;
    const bool localTest = m_endpoint.scheme() == "http" && _url.scheme() == "http" &&
                           _url.host() == m_endpoint.host() && _url.port() == m_endpoint.port();
    if (!_url.isValid() || !_url.userInfo().isEmpty() || (!trusted && !localTest)) {
        finish({}, tr("Meshy n’a pas fourni de lien GLB valide."));
        return;
    }
    auto file = std::make_shared<QTemporaryFile>();
    if (!file->open()) {
        finish({}, tr("Impossible de créer le fichier temporaire du modèle 3D."));
        return;
    }
    m_key.clear();
    m_progress = 90;
    m_status   = tr("Téléchargement du modèle GLB…");
    emit changed();
    request(
        _url, {}, false, PhotoCrap::Model3D::MaxFileBytes,
        [this, file](QByteArray) {
            if (file->size() == 0) {
                finish({}, tr("Le modèle GLB téléchargé est vide."));
                return;
            }
            if (!file->flush()) {
                finish({}, tr("Impossible d’écrire le modèle 3D sur le disque."));
                return;
            }
            m_model    = file;
            m_progress = 95;
            insert();
        },
        file);
}

void MeshyController::insert()
{
    if (!currentDocument()) {
        finish({}, tr("Le document source a changé."));
        return;
    }
    if (m_document->aiInsertionBusy() || m_document->transformActive()) {
        if (m_status != tr("En attente de la fin de l’édition…")) {
            m_status = tr("En attente de la fin de l’édition…");
            emit changed();
        }
        m_next.start(250);
        return;
    }
    m_status = tr("Importation du calque 3D…");
    emit changed();
    if (!m_busy)
        return;
    m_layerCount = m_document->layerCount();
    m_importError.clear();
    m_importing       = true;
    const auto scale  = QTransform::fromScale(m_document->width(), m_document->height());
    const auto bounds = scale.mapRect(m_reference);
    const bool started =
        m_document->importDownloadedModel3D(std::exchange(m_model, {}), m_name, bounds);
    if (!started || !m_document->operationBusy()) {
        if (started)
            finish(tr("Calque 3D Meshy ajouté."));
        else
            finish({}, m_importError.isEmpty() ? tr("Impossible d’importer le modèle Meshy.")
                                               : m_importError);
    }
}

void MeshyController::request(const QUrl &_url, const QByteArray &_body, bool _authenticated,
                              qsizetype _limit, Response _response,
                              std::shared_ptr<QTemporaryFile> _file)
{
    if (!m_busy)
        return;
    if (!currentDocument()) {
        finish({}, tr("Le document source a changé."));
        return;
    }
    QNetworkRequest request(_url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    request.setTransferTimeout(60000);
    if (_authenticated)
        request.setRawHeader("Authorization", "Bearer " + m_key);
    if (!_body.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto *reply = _body.isEmpty() ? m_network.get(request) : m_network.post(request, _body);
    m_reply     = reply;
    constexpr qint64 chunkSize = 256 * 1024;
    reply->setReadBufferSize(chunkSize);
    const auto data = std::make_shared<QByteArray>();
    const auto sizeError =
        _file ? tr("Le GLB téléchargé dépasse %L1 Mio.").arg(_limit / (1024 * 1024))
              : tr("La réponse Meshy dépasse la taille maximale autorisée.");
    const auto read = [reply, data, _file, _limit, sizeError, chunkSize]() -> QString {
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code >= 200 && code < 300 &&
            reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() > _limit)
            return sizeError;
        while (reply->bytesAvailable() > 0) {
            const auto stored = _file ? _file->pos() : data->size();
            const auto chunk  = reply->read(qMin(chunkSize, _limit - stored + 1));
            if (chunk.isEmpty())
                break;
            if (chunk.size() > _limit - stored)
                return sizeError;
            if (_file) {
                if (_file->write(chunk) != chunk.size())
                    return tr("Impossible d’écrire le modèle 3D sur le disque.");
            } else {
                data->append(chunk);
            }
        }
        return {};
    };
    const auto consume = [this, reply, read] {
        if (m_reply != reply)
            return;
        const auto error = read();
        if (!error.isEmpty())
            finish({}, error);
    };
    connect(reply, &QNetworkReply::readyRead, this, consume);
    connect(reply, &QNetworkReply::metaDataChanged, this, consume);
    if (_file) {
        connect(reply, &QNetworkReply::downloadProgress, this,
                [this, reply](qint64 received, qint64 total) {
                    if (m_reply != reply || total <= 0)
                        return;
                    const int progress = 90 + int(4. * qBound(0., double(received) / total, 1.));
                    if (progress != m_progress) {
                        m_progress = progress;
                        emit changed();
                    }
                });
    }
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, data, read, response = std::move(_response)] {
                reply->deleteLater();
                if (m_reply != reply)
                    return;
                m_reply.clear();
                const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (reply->error() != QNetworkReply::NoError || code < 200 || code >= 300) {
                    QString message;
                    if (code == 401 || code == 403)
                        message = tr("Meshy a refusé l’accès. Vérifiez la clé API.");
                    else if (code == 402)
                        message = tr("Le solde de crédits Meshy est insuffisant.");
                    else if (code == 429)
                        message = tr("La limite de requêtes Meshy est atteinte.");
                    else if (code)
                        message = tr("La requête Meshy a échoué (HTTP %1).").arg(code);
                    else
                        message = tr("La connexion à Meshy a échoué.");
                    finish({}, message);
                    return;
                }
                const auto readError = read();
                if (!readError.isEmpty()) {
                    finish({}, readError);
                    return;
                }
                if (!currentDocument()) {
                    finish({}, tr("Le document source a changé."));
                    return;
                }
                response(std::move(*data));
            });
}

bool MeshyController::currentDocument() const
{
    return m_document->documentSession() == m_session;
}

void MeshyController::cancel()
{
    if (m_busy)
        finish(tr("Suivi Meshy arrêté."));
}

void MeshyController::dismiss()
{
    if (m_busy)
        return;
    m_status.clear();
    m_error.clear();
    emit changed();
}

void MeshyController::finish(const QString &_status, const QString &_error)
{
    m_cancel->store(true);
    m_deadline.stop();
    m_next.stop();
    if (auto *reply = m_reply.data()) {
        m_reply.clear();
        reply->abort();
    }
    if (std::exchange(m_importing, false) && m_document->operationBusy())
        m_document->cancelOperation();
    m_model.reset();
    m_key.clear();
    m_task.clear();
    m_reference = {};
    m_busy      = false;
    m_status    = _status;
    m_error     = _error;
    emit changed();
}

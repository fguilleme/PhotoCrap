#include "MeshyController.h"
#include "AiImageTypes.h"
#include "AiSettings.h"
#include "Document.h"
#include "model3d/Model3D.h"
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <utility>

namespace
{
constexpr qsizetype ResponseLimit = 1024 * 1024;
constexpr qsizetype ImageLimit    = 20000000;
constexpr int       TaskTimeoutMs = 60 * 60 * 1000;

bool validTaskId(const QString &_id)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_-]{1,128}$"));
    return pattern.match(_id).hasMatch();
}

int pollDelay(const QNetworkReply &reply)
{
    bool      ok      = false;
    const int seconds = reply.rawHeader("Retry-After").trimmed().toInt(&ok);
    return (ok ? qBound(1, seconds, 3600) : 5) * 1000;
}

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
    m_deadline.setTimerType(Qt::PreciseTimer);
    connect(&m_next, &QTimer::timeout, this, [this] {
        if (!m_model)
            poll();
        else
            insert();
    });
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        finish({}, tr("Meshy n’a pas terminé après une heure. Vous pouvez reprendre le suivi."),
               true);
    });
    connect(m_document, &Document::errorOccurred, this, [this](const QString &_error) {
        if (m_importing)
            m_importError = _error;
    });
    connect(m_document, &Document::operationChanged, this, [this] {
        if (!m_importing || m_document->operationBusy())
            return;
        const bool inserted = currentDocument() && m_document->layerCount() == m_layerCount + 1;
        if (inserted)
            completeImport();
        else
            finish({},
                   m_importError.isEmpty() ? tr("L’importation du modèle Meshy a été interrompue.")
                                           : m_importError,
                   true);
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
                               bool _remember, bool _ultra, int _targetTriangles)
{
    if (m_busy || m_document->aiInsertionBusy() || m_document->transformActive() || _source < 0 ||
        _source > 2 || (_source == 2 && !m_document->hasSelection()) ||
        !PhotoCrap::Model3D::validTriangleTarget(_targetTriangles))
        return false;
    const auto prompt = _prompt.trimmed();
    if (_source == 0 && (prompt.isEmpty() || prompt.size() > 800))
        return false;

    start(_source == 0 ? Stage::TextPreview : Stage::Image,
          tr("Meshy — %1")
              .arg(_source == 0 ? prompt.simplified().left(64) : m_document->name().left(64)),
          tr("Préparation de la génération Meshy…"), _targetTriangles);
    withKey(_key, _remember, [this, _source, prompt, body = meshOptions(_ultra)]() mutable {
        if (_source != 0) {
            prepare(_source == 2, std::move(body));
            return;
        }
        body["mode"]   = "preview";
        body["prompt"] = prompt;
        submit(body);
    });
    return true;
}

bool MeshyController::recover(const QString &_task, bool _text, const QString &_key, bool _remember,
                              int _targetTriangles)
{
    const auto task = _task.trimmed();
    if (m_busy || !validTaskId(task) || !PhotoCrap::Model3D::validTriangleTarget(_targetTriangles))
        return false;
    // Fetch the selected text task as-is, even if it is only a geometry preview.
    start(_text ? Stage::TextRefine : Stage::Image, tr("Meshy — %1").arg(task),
          tr("Recherche de la tâche Meshy…"), _targetTriangles);
    withKey(_key, _remember, [this, task] {
        m_task = task;
        followTask();
    });
    return true;
}

void MeshyController::start(Stage _stage, const QString &_name, const QString &_status,
                            int _triangleTarget)
{
    m_busy           = true;
    m_stage          = _stage;
    m_session        = m_document->documentSession();
    m_reference      = QRectF(0, 0, 1, 1);
    m_cancel         = std::make_shared<std::atomic_bool>(false);
    m_name           = _name;
    m_status         = _status;
    m_triangleTarget = _triangleTarget;
    m_resultInfo.clear();
    m_error.clear();
    m_task.clear();
    m_key.clear();
    m_progress = 0;
    emit changed();
}

void MeshyController::withKey(const QString &_key, bool _remember, std::function<void()> _ready)
{
    if (!m_busy)
        return;
    const auto ready = [this, run = m_cancel, ready = std::move(_ready)](const QString &secret,
                                                                         const QString &error) {
        if (run->load())
            return;
        if (secret.isEmpty() || secret.contains(u'\r') || secret.contains(u'\n')) {
            finish({}, error.isEmpty() ? tr("Saisissez une clé API Meshy valide.") : error);
            return;
        }
        m_key = secret.toUtf8();
        ready();
    };
    const auto key = _key.trimmed();
    m_remember     = _remember && !key.isEmpty();
    if (key.isEmpty())
        m_settings->resolveKey("meshy", this, ready);
    else
        ready(key, {});
}

void MeshyController::followTask()
{
    if (!m_busy || m_task.isEmpty())
        return;
    m_deadline.start(TaskTimeoutMs);
    m_next.start(0);
}

void MeshyController::rememberKey()
{
    if (std::exchange(m_remember, false))
        m_settings->saveKey("meshy", QString::fromUtf8(m_key));
}

void MeshyController::prepare(bool _selection, QJsonObject _options)
{
    if (!currentDocument() || m_document->aiInsertionBusy() || m_document->transformActive()) {
        finish({}, tr("Le document n’est plus disponible pour préparer l’image."));
        return;
    }
    auto *watcher = new QFutureWatcher<PhotoCrap::Ai::Input>(this);
    connect(watcher, &QFutureWatcher<PhotoCrap::Ai::Input>::finished, this,
            [this, watcher, cancel = m_cancel, body = std::move(_options)]() mutable {
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
    m_deadline.stop();
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
    request(taskUrl(), RequestKind::Submit, QJsonDocument(_body).toJson(QJsonDocument::Compact),
            ResponseLimit, [this](const QByteArray &_data, int) {
                const auto id = QJsonDocument::fromJson(_data).object().value("result").toString();
                if (!validTaskId(id)) {
                    finish({}, tr("Meshy n’a pas renvoyé d’identifiant de tâche valide."));
                    return;
                }
                m_task = id;
                rememberKey();
                followTask();
            });
}

void MeshyController::poll()
{
    request(taskUrl(), RequestKind::Poll, {}, ResponseLimit,
            [this](const QByteArray &_data, int delay) {
                const auto task   = QJsonDocument::fromJson(_data).object();
                const auto status = task.value("status").toString();
                if (status == "PENDING" || status == "IN_PROGRESS") {
                    rememberKey();
                    const int progress = qBound(0, task.value("progress").toInt(), 100);
                    const int overall  = m_stage == Stage::TextPreview  ? progress * 45 / 100
                                         : m_stage == Stage::TextRefine ? 45 + progress * 45 / 100
                                                                        : progress * 90 / 100;
                    m_progress         = qMax(m_progress, overall);
                    m_status = status == "PENDING" ? tr("Tâche Meshy en file d’attente…")
                                                   : tr("Génération Meshy en cours…");
                    emit changed();
                    if (m_busy)
                        m_next.start(delay);
                    return;
                }
                rememberKey();
                m_deadline.stop();
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
    m_progress = 90;
    m_status   = tr("Téléchargement du modèle GLB…");
    emit changed();
    request(
        _url, RequestKind::Download, {}, PhotoCrap::Model3D::MaxFileBytes,
        [this, file](QByteArray, int) {
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
    m_importing        = true;
    const auto scale   = QTransform::fromScale(m_document->width(), m_document->height());
    const auto bounds  = scale.mapRect(m_reference);
    const bool started = m_document->importDownloadedModel3D(std::exchange(m_model, {}), m_name,
                                                             bounds, m_triangleTarget);
    if (!started || !m_document->operationBusy()) {
        if (started)
            completeImport();
        else
            finish({},
                   m_importError.isEmpty() ? tr("Impossible d’importer le modèle Meshy.")
                                           : m_importError,
                   true);
    }
}

void MeshyController::completeImport()
{
    m_resultInfo = m_document->model3DStatistics(m_document->activeLayer());
    if (!m_resultInfo.isEmpty()) {
        const QLocale locale;
        m_resultInfo["fileSize"] =
            locale.formattedDataSize(m_resultInfo.value("fileBytes").toLongLong(), 1);
        m_resultInfo["decodedSize"] =
            locale.formattedDataSize(m_resultInfo.value("decodedBytes").toLongLong(), 1);
        auto textures = m_resultInfo.value("textures").toList();
        for (auto &entry : textures) {
            auto texture    = entry.toMap();
            texture["size"] = locale.formattedDataSize(texture.value("bytes").toLongLong(), 1);
            entry           = std::move(texture);
        }
        m_resultInfo["textures"] = std::move(textures);
    }
    finish(tr("Calque 3D Meshy ajouté."));
}

void MeshyController::request(const QUrl &_url, RequestKind _kind, const QByteArray &_body,
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
    request.setTransferTimeout(_kind == RequestKind::Poll ? 30000 : 60000);
    if (_kind != RequestKind::Download)
        request.setRawHeader("Authorization", "Bearer " + m_key);
    if (!_body.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto *reply =
        _kind == RequestKind::Submit ? m_network.post(request, _body) : m_network.get(request);
    m_reply                    = reply;
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
            [this, reply, data, read, _kind, response = std::move(_response)] {
                reply->deleteLater();
                if (m_reply != reply)
                    return;
                m_reply.clear();
                const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const auto networkError = reply->error();
                const auto delay        = pollDelay(*reply);
                if (networkError != QNetworkReply::NoError || code < 200 || code >= 300) {
                    const bool interrupted =
                        !code && (networkError == QNetworkReply::TimeoutError ||
                                  networkError == QNetworkReply::OperationCanceledError ||
                                  networkError == QNetworkReply::RemoteHostClosedError ||
                                  networkError == QNetworkReply::TemporaryNetworkFailureError ||
                                  networkError == QNetworkReply::HostNotFoundError ||
                                  networkError == QNetworkReply::ConnectionRefusedError);
                    if (_kind == RequestKind::Poll && (interrupted || code == 429 || code >= 500)) {
                        m_status = tr("Connexion Meshy interrompue. Nouvelle vérification prévue…");
                        m_next.start(delay);
                        emit changed();
                        return;
                    }
                    QString message;
                    if (_kind == RequestKind::Download)
                        message =
                            tr("Téléchargement du GLB interrompu. Vous pouvez reprendre le suivi.");
                    else if (_kind == RequestKind::Poll && code == 404)
                        message = tr("Tâche Meshy introuvable ou expirée.");
                    else if (code == 401 || code == 403)
                        message = tr("Meshy a refusé l’accès. Vérifiez la clé API.");
                    else if (code == 402)
                        message = tr("Le solde de crédits Meshy est insuffisant.");
                    else if (code == 429)
                        message = tr("La limite de requêtes Meshy est atteinte.");
                    else if (code)
                        message = tr("La requête Meshy a échoué (HTTP %1).").arg(code);
                    else
                        message = tr("La connexion à Meshy a échoué.");
                    finish({}, message, _kind == RequestKind::Download);
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
                response(std::move(*data), delay);
            });
}

bool MeshyController::currentDocument() const
{
    return m_document->documentSession() == m_session;
}

void MeshyController::cancel()
{
    if (m_busy)
        finish(tr("Suivi Meshy arrêté."), {}, true);
}

bool MeshyController::canResume() const
{
    return !m_busy && !m_task.isEmpty() && currentDocument();
}

void MeshyController::resume()
{
    if (!canResume())
        return;
    m_busy   = true;
    m_cancel = std::make_shared<std::atomic_bool>(false);
    m_status = tr("Recherche de la tâche Meshy…");
    m_error.clear();
    followTask();
    emit changed();
}

void MeshyController::dismiss()
{
    if (m_busy)
        return;
    m_status.clear();
    m_error.clear();
    m_resultInfo.clear();
    m_task.clear();
    m_key.clear();
    emit changed();
}

void MeshyController::finish(const QString &_status, const QString &_error, bool _resumable)
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
    if (!_resumable || m_task.isEmpty() || !currentDocument()) {
        m_key.clear();
        m_task.clear();
        m_reference = {};
    }
    m_busy   = false;
    m_status = _status;
    m_error  = _error;
    emit changed();
}

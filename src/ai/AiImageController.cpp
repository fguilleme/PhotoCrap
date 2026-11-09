#include "AiImageController.h"
#include "Document.h"
#include <QDateTime>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtConcurrentRun>
#include <QtMath>
#include <cmath>

AiImageController::AiImageController(Document *_document, AiSettings *_settings, QObject *_parent)
    : QObject(_parent), m_document(_document), m_settings(_settings), m_job(this)
{
    m_clock.setInterval(1000);
    m_clock.setTimerType(Qt::PreciseTimer);
    connect(&m_clock, &QTimer::timeout, this, [this] {
        if (!m_busy || !m_elapsed.isValid())
            return;
        const int elapsed = int(m_elapsed.elapsed() / 1000);
        if (elapsed != m_elapsedSeconds) {
            m_elapsedSeconds = elapsed;
            emit changed();
        }
    });
    m_insertionTimer.setSingleShot(true);
    m_insertionTimer.setInterval(100);
    connect(&m_insertionTimer, &QTimer::timeout, this, &AiImageController::tryInsertion);
    connect(&m_job, &AiImageJob::status, this, [this](const QString &_value) {
        m_status = variantStatus(_value);
        emit changed();
    });
    connect(&m_job, &AiImageJob::finished, this, &AiImageController::received);
    connect(_document, &Document::canvasSizeChanged, this, [this] {
        if (m_busy || pendingResult()) {
            cancel();
            m_error = tr("La taille du document a changé. La demande IA a été annulée.");
            emit changed();
        }
    });
    connect(_document, &Document::documentChanged, this, [this] {
        if ((m_busy || pendingResult()) && m_input.session && m_document &&
            m_input.session != m_document->documentSession())
            cancel();
    });
}
AiImageController::~AiImageController()
{
    if (m_cancel)
        m_cancel->store(true);
    m_job.cancel();
}
void AiImageController::stop(const QString &_error)
{
    m_awaitingResponse = false;
    if (m_cancel)
        m_cancel->store(true);
    m_job.cancel();
    m_insertionTimer.stop();
    m_clock.stop();
    if (m_elapsed.isValid())
        m_elapsedSeconds = int(m_elapsed.elapsed() / 1000);
    m_resultBytes.clear();
    m_readyResult.reset();
    m_busy     = false;
    m_error    = _error.isEmpty() || !m_completed
                     ? _error
                     : tr("%1 variante(s) créée(s). %2").arg(m_completed).arg(_error);
    m_input    = {};
    m_provider = {};
    m_prompt.clear();
    m_key.clear();
    m_status = _error.isEmpty() ? tr("Prêt") : tr("La génération n’a pas abouti");
    emit changed();
}
void AiImageController::cancel()
{
    ++m_generation;
    stop();
    m_status = m_completed ? tr("Demande annulée — %1 variante(s) conservée(s)").arg(m_completed)
                           : tr("Demande IA annulée");
    emit changed();
}
bool AiImageController::generate(const QString &_providerId, const QString &_prompt,
                                 bool _selection, int _variants, bool _original,
                                 const QVariantMap &_options)
{
    if (m_busy || pendingResult() || !m_document)
        return false;
    auto       provider = m_settings->provider(_providerId);
    const bool topaz    = provider.value("kind") == "fal" &&
                       provider.value("model") == "topaz/upscale/image/precision";
    const bool esrgan =
        provider.value("kind") == "fal" && provider.value("model") == "fal-ai/esrgan";
    if ((topaz || esrgan) && !_options.isEmpty()) {
        auto options =
            QJsonDocument::fromJson(provider.value("options", "{}").toString().toUtf8()).object();
        for (const auto &key : {QStringLiteral("model"),
                                topaz ? QStringLiteral("upscale_factor") : QStringLiteral("scale")})
            if (_options.contains(key))
                options[key] = QJsonValue::fromVariant(_options.value(key));
        provider["options"] =
            QString::fromUtf8(QJsonDocument(options).toJson(QJsonDocument::Compact));
    }
    const auto mode   = provider.value("promptMode", "required").toString();
    const auto prompt = mode == "none" ? QString{} : _prompt.trimmed();
    if (provider.isEmpty() || !AiSettings::validEndpoint(QUrl(provider.value("url").toString())) ||
        (mode == "required" && prompt.isEmpty()) || prompt.size() > 32000 || _variants < 1 ||
        _variants > 8) {
        m_error = tr("Choisissez un fournisseur et saisissez un prompt valide.");
        emit changed();
        return false;
    }
    if ((topaz || esrgan) && _variants != 1) {
        m_error = tr("L’upscale traite une seule image à la fois.");
        emit changed();
        return false;
    }
    if (provider.value("model").toString().trimmed().isEmpty()) {
        m_error = tr("Indiquez un modèle dans les réglages du fournisseur IA.");
        emit changed();
        return false;
    }
    m_runProvider    = provider.value("name").toString();
    m_runModel       = provider.value("model").toString();
    m_elapsedSeconds = 0;
    m_elapsed.restart();
    m_clock.start();
    m_variantCount = _variants;
    m_completed    = 0;
    m_batchId      = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_key.clear();
    m_error.clear();
    m_busy                = true;
    m_status              = tr("Accès à la clé du fournisseur…");
    m_provider            = provider;
    m_prompt              = prompt;
    m_cancel              = std::make_shared<std::atomic_bool>(false);
    const auto generation = ++m_generation;
    m_input               = {};
    m_input.session       = m_document->documentSession();
    emit changed();
    if (!m_busy || generation != m_generation)
        return true;
    m_settings->resolveKey(
        _providerId, this,
        [this, generation, selection = _selection, original = _original](const QString &key,
                                                                         const QString &error) {
            if (!m_busy || generation != m_generation)
                return;
            if (!error.isEmpty()) {
                stop(tr("Impossible d’accéder au trousseau : %1").arg(error));
                return;
            }
            if ((m_provider.value("kind") == "openai" || m_provider.value("kind") == "fal") &&
                key.isEmpty()) {
                stop(tr("Ajoutez une clé API dans Réglages → IA, ou définissez sa variable "
                        "d’environnement."));
                return;
            }
            if (!m_document || m_input.session != m_document->documentSession()) {
                stop(tr("Le document a changé pendant la préparation IA."));
                return;
            }
            m_key = key;
            prepareInput(selection, generation, original);
        });
    return true;
}
void AiImageController::prepareInput(bool _selection, quint64 generation, bool _original)
{
    m_status = tr("Préparation de l’image…");
    emit changed();
    if (!m_busy || generation != m_generation)
        return;
    auto *watcher = new QFutureWatcher<PhotoCrap::Ai::Input>(this);
    connect(watcher, &QFutureWatcher<PhotoCrap::Ai::Input>::finished, this,
            [this, watcher, generation] {
                watcher->deleteLater();
                if (!m_busy || generation != m_generation)
                    return;
                try {
                    m_input = watcher->result();
                } catch (...) {
                    stop(tr("Mémoire insuffisante pour préparer l’image IA."));
                    return;
                }
                if (!m_input.error.isEmpty()) {
                    stop(m_input.error);
                    return;
                }
                if (!m_document || m_input.session != m_document->documentSession() ||
                    m_input.canvas != QSize(m_document->width(), m_document->height())) {
                    stop(tr("Le document a changé pendant la préparation IA."));
                    return;
                }
                startVariant();
            });
    watcher->setFuture(m_document->prepareAiInput(
        _selection,
        m_provider.value("nativeInput", false).toBool() ? 16384
                                                        : m_provider.value("maxEdge", 1536).toInt(),
        m_cancel, _original, m_provider.value("nativeInput", false).toBool()));
}
QString AiImageController::variantStatus(const QString &_status) const
{
    return m_variantCount > 1
               ? tr("Variante %1/%2 — %3").arg(m_completed + 1).arg(m_variantCount).arg(_status)
               : _status;
}
void AiImageController::startVariant()
{
    if (!m_busy || !m_document)
        return;
    if (m_input.session != m_document->documentSession() ||
        m_input.canvas != QSize(m_document->width(), m_document->height())) {
        stop(tr("Le document a changé. Le résultat IA n’a pas été inséré."));
        return;
    }
    m_awaitingResponse = true;
    const bool topaz   = m_provider.value("model") == "topaz/upscale/image/precision";
    const bool esrgan  = m_provider.value("model") == "fal-ai/esrgan";
    if (m_provider.value("kind") == "fal" && (topaz || esrgan)) {
        const auto options =
            QJsonDocument::fromJson(m_provider.value("options", "{}").toString().toUtf8()).object();
        const auto field  = topaz ? "upscale_factor" : "scale";
        const auto factor = options.value(field).toDouble(2.);
        const auto model =
            options.value("model").toString(topaz ? "Standard V2" : "RealESRGAN_x2plus");
        const QStringList models =
            topaz ? QStringList{"Standard V2",
                                "High Fidelity V3",
                                "High Fidelity V2",
                                "Low Resolution V2",
                                "CGI",
                                "Text Refine",
                                "Faces"}
                  : QStringList{"RealESRGAN_x2plus",          "RealESRGAN_x4plus",
                                "RealESRGAN_x4_v3",           "RealESRGAN_x4_wdn_v3",
                                "RealESRGAN_x4plus_anime_6B", "RealESRGAN_x4_anime_v3"};
        if ((!options.value(field).isUndefined() && !options.value(field).isDouble()) ||
            (!options.value("model").isUndefined() && !options.value("model").isString()) ||
            !std::isfinite(factor) || factor < 1. || factor > 4. || !models.contains(model) ||
            !PhotoCrap::Ai::validSize(QSize(qCeil(m_input.sentSize.width() * factor),
                                            qCeil(m_input.sentSize.height() * factor)))) {
            stop(tr(
                "Réglages d’upscale invalides ou sortie au-delà de 32 Mpx / 16 384 px. Réduisez le "
                "facteur ou utilisez l’image IA d’origine."));
            return;
        }
    }
    m_job.start(m_provider, m_key, m_prompt, m_input);
}
void AiImageController::received(const QByteArray &_bytes, const QString &_error)
{
    if (!m_busy || !m_document || !m_awaitingResponse)
        return;
    // Consume each request once, before decoding asynchronously. A repeated
    // completion must not start another worker or insert the same variant twice.
    m_awaitingResponse = false;
    if (!_error.isEmpty()) {
        stop(_error);
        return;
    }
    m_resultBytes = _bytes;
    prepareResult();
}
void AiImageController::prepareResult()
{
    const int  bits       = m_document->channelBits();
    const auto generation = m_generation;
    const auto input      = m_input;
    const auto cancel     = m_cancel;
    m_status              = variantStatus(tr("Préparation du nouveau calque…"));
    emit  changed();
    auto *watcher = new QFutureWatcher<PhotoCrap::Ai::Result>(this);
    connect(watcher, &QFutureWatcher<PhotoCrap::Ai::Result>::finished, this,
            [this, watcher, generation] {
                watcher->deleteLater();
                if (!m_busy || generation != m_generation)
                    return;
                PhotoCrap::Ai::Result result;
                try {
                    result = watcher->result();
                } catch (...) {
                    stop(tr("Mémoire insuffisante pour préparer le calque IA."));
                    return;
                }
                if (!result.error.isEmpty()) {
                    stop(result.error);
                    return;
                }
                m_readyResult = std::make_shared<PhotoCrap::Ai::Result>(std::move(result));
                tryInsertion();
            });
    watcher->setFuture(QtConcurrent::run([input, bytes = m_resultBytes, bits, cancel] {
        return PhotoCrap::Ai::prepareResult(input, bytes, bits, cancel);
    }));
}
void AiImageController::tryInsertion()
{
    if (!m_readyResult || !m_document || !m_busy)
        return;
    if (m_input.session != m_document->documentSession() ||
        m_input.canvas != QSize(m_document->width(), m_document->height())) {
        stop(tr("Le document a changé. Le résultat IA n’a pas été inséré."));
        return;
    }
    if (m_document->aiInsertionBusy()) {
        const auto text = variantStatus(tr("Résultat prêt — terminez l’édition en cours…"));
        if (m_status != text) {
            m_status = text;
            emit changed();
        }
        m_insertionTimer.start();
        return;
    }
    if (m_readyResult->bits != m_document->channelBits()) {
        m_readyResult.reset();
        prepareResult();
        return;
    }
    QString     error;
    QVariantMap metadata{
        {"provider", m_provider.value("name")},
        {"model", m_provider.value("model")},
        {"prompt", m_prompt},
        {"inputSize", QVariantList{m_input.sentSize.width(), m_input.sentSize.height()}},
        {"originalSource", m_input.originalSource},
        {"selection", m_input.selected},
        {"batch", m_batchId},
        {"variant", m_completed + 1},
        {"variantCount", m_variantCount},
        {"date", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    const bool topaz = m_provider.value("model") == "topaz/upscale/image/precision";
    if (m_provider.value("kind") == "fal" &&
        (topaz || m_provider.value("model") == "fal-ai/esrgan")) {
        const auto options =
            QJsonDocument::fromJson(m_provider.value("options", "{}").toString().toUtf8()).object();
        metadata["upscaleFactor"] = options.value(topaz ? "upscale_factor" : "scale").toDouble(2.);
        metadata["upscaleModel"] =
            options.value("model").toString(topaz ? "Standard V2" : "RealESRGAN_x2plus");
    }
    if (!m_document->insertAiResult(m_input, *m_readyResult, metadata, error)) {
        m_busy = false;
        m_clock.stop();
        m_elapsedSeconds = int(m_elapsed.elapsed() / 1000);
        m_error          = error;
        m_status = tr("Résultat conservé — libérez de la place puis ajoutez le calque");
        emit changed();
        return;
    }
    ++m_completed;
    m_resultBytes.clear();
    m_readyResult.reset();
    if (m_completed == m_variantCount) {
        stop();
        m_status = m_completed > 1
                       ? tr("%1 variantes IA ajoutées sur des calques séparés").arg(m_completed)
                       : tr("Résultat IA ajouté sur un nouveau calque");
        emit changed();
    } else {
        m_status = variantStatus(tr("Envoi au fournisseur…"));
        emit       changed();
        const auto generation = m_generation;
        QTimer::singleShot(0, this, [this, generation] {
            if (m_busy && generation == m_generation)
                startVariant();
        });
    }
    emit layerAdded();
}
void AiImageController::retryInsertion()
{
    if (m_busy || !m_readyResult || !pendingResult())
        return;
    m_busy = true;
    m_clock.start();
    m_error.clear();
    emit changed();
    tryInsertion();
}

#include "AiImageController.h"
#include "Document.h"
#include "DocumentManager.h"
#include <QDateTime>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtConcurrentRun>
#include <QtMath>
#include <cmath>
#include <utility>

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
    connect(&m_localModel, &PhotoCrap::Ai::LocalSurfaceModel::status, this,
            [this](const QString &status) {
                m_status = status;
                emit changed();
            });
    connect(&m_localModel, &PhotoCrap::Ai::LocalSurfaceModel::ready, this,
            [this](const QString &path, const QString &error) {
                if (!m_busy || !m_surfaceAnalysis)
                    return;
                if (!error.isEmpty())
                    stop(error);
                else
                    prepareLocalSurface(path);
            });
    connect(_document, &Document::canvasSizeChanged, this, [this] {
        if (m_busy || pendingResult() || surfacePicking()) {
            cancel();
            m_error = tr("La taille du document a changé. La demande IA a été annulée.");
            emit changed();
        }
    });
    connect(_document, &Document::documentChanged, this, [this] {
        if ((m_busy || pendingResult() || surfacePicking()) && m_input.session && m_document &&
            m_input.session != m_document->documentSession())
            cancel();
    });
    connect(_document, &Document::historyChanged, this, [this] {
        if (surfacePicking() && !m_document->matchesAiSource(m_input))
            stop(tr("L’image a changé. Relancez l’analyse de la surface."));
    });
}
AiImageController::~AiImageController()
{
    if (m_cancel)
        m_cancel->store(true);
    m_job.cancel();
    m_localModel.cancel();
}
void AiImageController::setDocumentManager(DocumentManager *manager)
{
    m_documents = manager;
}
void AiImageController::stop(const QString &_error)
{
    m_awaitingResponse = false;
    if (m_cancel)
        m_cancel->store(true);
    m_job.cancel();
    m_localModel.cancel();
    m_insertionTimer.stop();
    m_clock.stop();
    if (m_elapsed.isValid())
        m_elapsedSeconds = int(m_elapsed.elapsed() / 1000);
    m_resultBytes.clear();
    m_readyResult.reset();
    m_surfaceGeometry = {};
    m_surfaceSelection  = {};
    m_surfaceProtection = {};
    m_surfaceEmbedding.reset();
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
    m_status = m_surfaceAnalysis ? tr("Analyse de surface annulée")
               : m_completed ? tr("Demande annulée — %1 variante(s) conservée(s)").arg(m_completed)
                             : tr("Demande IA annulée");
    emit changed();
}
bool AiImageController::generate(const QString &_providerId, const QString &_prompt,
                                 bool _selection, int _variants, bool _original,
                                 const QVariantMap &_options, bool newDocument)
{
    if (m_busy || pendingResult() || !m_document)
        return false;
    if (newDocument && !m_documents) {
        m_error = tr("La création d’un nouveau document n’est pas disponible.");
        emit changed();
        return false;
    }
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
    if (provider.value("kind") == "gradio") {
        auto options =
            QJsonDocument::fromJson(provider.value("options", "{}").toString().toUtf8()).object();
        for (const auto &key :
             {"lora_adapter", "seed", "randomize_seed", "guidance_scale", "steps"})
            if (_options.contains(key))
                options[key] = QJsonValue::fromVariant(_options.value(key));
        const auto error = AiSettings::gradioOptionsError(options.toVariantMap());
        if (!error.isEmpty() || (provider.value("authMode", "basic") == "basic" &&
                                 provider.value("username").toString().isEmpty())) {
            m_error = error.isEmpty()
                          ? tr("Renseignez l’utilisateur HTTP Basic dans Réglages → IA.")
                          : error;
            emit changed();
            return false;
        }
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
    begin(_providerId, provider, prompt, _selection, _variants, _original, newDocument, false);
    return true;
}

bool AiImageController::analyzeSurfaceLocally(bool relight)
{
    if (!localSurfaceAvailable() || m_busy || pendingResult() || !m_document ||
        m_document->aiInsertionBusy() || m_document->transformActive())
        return false;
    begin({},
          {{"name", tr("Sur cet ordinateur")},
           {"model", "MoGe-2 ViT-S"}, {"relight", relight},
           {"maxEdge", PhotoCrap::Ai::MaxSurfaceNormalEdge}},
          {}, false, 1, false, false, true);
    return true;
}

void AiImageController::begin(const QString &providerId, const QVariantMap &provider,
                              const QString &prompt, bool selection, int variants, bool original,
                              bool newDocument, bool surface)
{
    m_newDocument     = newDocument;
    m_surfaceAnalysis = surface;
    m_surfaceGeometry = {};
    m_surfaceSelection  = {};
    m_surfaceProtection = {};
    m_surfaceEmbedding.reset();
    m_runProvider    = provider.value("name").toString();
    m_runModel       = provider.value("model").toString();
    m_elapsedSeconds = 0;
    m_elapsed.restart();
    m_clock.start();
    m_variantCount = variants;
    m_completed    = 0;
    m_batchId      = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_key.clear();
    m_error.clear();
    m_busy                = true;
    m_status =
        m_surfaceAnalysis ? tr("Préparation de l’image…") : tr("Accès à la clé du fournisseur…");
    m_provider            = provider;
    m_prompt              = prompt;
    m_cancel              = std::make_shared<std::atomic_bool>(false);
    const auto generation = ++m_generation;
    m_input               = {};
    m_input.session       = m_document->documentSession();
    emit changed();
    if (!m_busy || generation != m_generation)
        return;
    if (m_surfaceAnalysis) {
        prepareInput(false, generation, false);
        return;
    }
    m_settings->resolveKey(
        providerId, this,
        [this, generation, selection, original](const QString &key, const QString &error) {
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
            if (m_provider.value("kind") == "gradio" &&
                m_provider.value("authMode", "basic") == "basic" && key.isEmpty()) {
                stop(tr("Ajoutez le mot de passe HTTP Basic dans Réglages → IA."));
                return;
            }
            if (!m_document || m_input.session != m_document->documentSession()) {
                stop(tr("Le document a changé pendant la préparation IA."));
                return;
            }
            m_key = key;
            prepareInput(selection, generation, original);
        });
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
                if (m_surfaceAnalysis)
                    m_localModel.prepare();
                else
                    startVariant();
            });
    watcher->setFuture(m_document->prepareAiInput(
        _selection,
        m_provider.value("nativeInput", false).toBool() ? 16384
                                                        : m_provider.value("maxEdge", 1536).toInt(),
        m_cancel, _original, m_provider.value("nativeInput", false).toBool(), m_surfaceAnalysis));
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
void AiImageController::prepareLocalSurface(const QString &path)
{
    if (!m_document || !m_document->matchesAiSource(m_input)) {
        stop(tr("L’image a changé. Relancez l’analyse de la surface."));
        return;
    }
    const auto generation = m_generation;
    const auto cancel     = m_cancel;
    const auto image      = m_input.localImage;
    m_status              = tr("Analyse de la surface sur cet ordinateur…");
    emit changed();
    if (!m_busy || generation != m_generation)
        return;
    auto *watcher = new QFutureWatcher<PhotoCrap::Ai::LocalSurfaceResult>(this);
    connect(watcher, &QFutureWatcher<PhotoCrap::Ai::LocalSurfaceResult>::finished, this,
            [this, watcher, generation] {
                watcher->deleteLater();
                if (!m_busy || generation != m_generation || !m_document)
                    return;
                try {
                    auto result = watcher->result();
                    if (result.error.isEmpty())
                        surfaceReady(std::move(result.geometry));
                    else
                        stop(result.error);
                } catch (...) {
                    stop(tr("Mémoire insuffisante pour lire les cartes de surface."));
                }
            });
    watcher->setFuture(QtConcurrent::run([image, path, cancel] {
        return PhotoCrap::Ai::LocalSurfaceModel::analyze(image, path, cancel);
    }));
}

void AiImageController::surfaceReady(PhotoCrap::Ai::SurfaceGeometry geometry)
{
    if (!m_document || !m_document->matchesAiSource(m_input)) {
        stop(tr("L’image a changé. Relancez l’analyse de la surface."));
        return;
    }
    if (!geometry.isValid()) {
        stop(tr("MoGe‑2 n’a détecté aucune surface exploitable."));
        return;
    }
    if (m_provider.value("relight").toBool()) {
        if (!m_document->addRelightLayer(m_input, geometry)) {
            stop(tr("Impossible de créer le calque de rééclairage."));
            return;
        }
        m_completed = 1;
        stop();
        emit layerAdded();
        return;
    }
    m_surfaceGeometry = std::move(geometry);
    m_input.mask.clear();
    m_key.clear();
    m_busy = false;
    m_clock.stop();
    m_elapsedSeconds = int(m_elapsed.elapsed() / 1000);
    showSurfacePickHint();
    emit changed();
}

void AiImageController::showSurfacePickHint()
{
    m_surfacePickingHint = true;
    m_status             = tr("Cliquez pour ajouter une surface. Clic droit pour la retirer.");
}

bool AiImageController::canEditSurfaceSelection()
{
    if (!surfacePicking() || m_busy || !m_document || m_document->aiInsertionBusy() ||
        m_document->transformActive())
        return false;
    if (!m_document->matchesAiSource(m_input)) {
        stop(tr("L’image a changé. Relancez l’analyse de la surface."));
        return false;
    }
    return true;
}

void AiImageController::pickSurface(int x, int y, bool followRelief, bool subtract)
{
    updateSurfaceSelection({x, y}, subtract, followRelief, false);
}

void AiImageController::protectSurfaceObject(int x, int y, bool subtract)
{
    updateSurfaceSelection({x, y}, subtract, false, true);
}

void AiImageController::updateSurfaceSelection(const QPoint &seed, bool subtract, bool followRelief,
                                               bool protect)
{
    const bool segment = surfaceProtectionAvailable();
    if ((protect && !segment) || !canEditSurfaceSelection() ||
        (subtract && (protect ? m_surfaceProtection.isNull() : !hasSurfaceSelection())) ||
        !QRect(QPoint{}, m_input.canvas).contains(seed))
        return;
    using namespace PhotoCrap::Ai;
    struct Result {
        std::shared_ptr<const SurfaceObjectEmbedding> embedding;
        QImage                                        protection;
        SurfaceSelection                              selection;
        bool                                          found = false;
    };
    const auto    generation = m_generation;
    const auto    cancel     = m_cancel;
    const auto    geometry   = m_surfaceGeometry;
    const auto    canvas     = m_input.canvas;
    const auto    image    = m_input.localImage;
    const auto    selected = m_surfaceSelection.mask;
    const auto    previous = m_surfaceProtection;
    m_busy                 = true;
    m_surfacePickingHint   = false;
    m_status                 = (protect || followRelief) && segment && !m_surfaceEmbedding
                                   ? tr("Préparation de MobileSAM et détourage…")
                               : protect ? tr("Détourage de l’objet…")
                                         : tr("Sélection de la surface…");
    emit changed();
    if (!m_busy || generation != m_generation)
        return;
    auto *watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation, protect] {
        watcher->deleteLater();
        if (!m_busy || generation != m_generation || !m_document)
            return;
        if (!m_document->matchesAiSource(m_input)) {
            stop(tr("L’image a changé. Relancez l’analyse de la surface."));
            return;
        }
        m_busy = false;
        try {
            auto result = watcher->result();
            if (result.embedding) {
                m_surfaceEmbedding = std::move(result.embedding);
                m_input.localImage = {};
            }
            if (result.found) {
                m_surfaceProtection = std::move(result.protection);
                m_surfaceSelection  = std::move(result.selection);
                ++m_surfacePreviewRevision;
                showSurfacePickHint();
            } else {
                m_status =
                    protect ? tr("Aucun objet détecté ici. Cliquez au centre de l’objet.")
                            : tr("Aucune surface exploitable ici. Cliquez ailleurs sur l’image.");
            }
        } catch (const std::exception &error) {
            m_status = tr("Segmentation impossible. %1").arg(QString::fromUtf8(error.what()));
        } catch (...) {
            m_status = tr("Segmentation impossible.");
        }
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([embedding = m_surfaceEmbedding,
                                          seeds = m_surfaceSelection.planarSeeds, image,
                                          previous, selected, geometry, canvas, seed, subtract,
                                          protect, followRelief, segment, cancel]() mutable {
        QImage region;
        if (!protect && !followRelief) {
            region = geometry.regionAt(seed, canvas, false, cancel);
            if (region.isNull())
                return Result{};
        }
        QImage boundary;
        if (segment && !cancel->load()) {
            try {
                if (!embedding)
                    embedding = SurfaceObjectModel::encode(image, cancel, protect || followRelief);
                const QPointF point((seed.x() + .5) / canvas.width(),
                                    (seed.y() + .5) / canvas.height());
                if (embedding)
                    boundary = SurfaceObjectModel::mask(
                        *embedding, point, geometry.disparity.size(), cancel, followRelief);
            } catch (...) {
                if (protect || followRelief)
                    throw;
                // Planar selection is still available if model loading fails.
            }
        }
        if (protect || (followRelief && segment))
            region = std::move(boundary);
        else if (followRelief)
            region = geometry.regionAt(seed, canvas, true, cancel);
        else
            region = restrictSurfaceRegion(std::move(region), boundary, cancel);
        if (region.isNull() || cancel->load())
            return Result{std::move(embedding), {}, {}, false};
        auto mask       = combineSurfaceRegions(std::move(region), protect ? previous : selected,
                                                subtract, cancel);
        auto protection = protect ? mask : previous;
        auto selection  = surfaceSelection(protect ? selected : mask, protection, cancel);
        if (!protect) {
            seeds.removeIf([&](const QPoint &point) {
                if (mask.isNull())
                    return true;
                const int x = int((2LL * point.x() + 1) * mask.width() / (2LL * canvas.width()));
                const int y = int((2LL * point.y() + 1) * mask.height() / (2LL * canvas.height()));
                return !mask.constScanLine(y)[x];
            });
            if (!followRelief && !subtract && !seeds.contains(seed)) {
                if (seeds.size() == 16)
                    seeds.removeFirst();
                seeds.append(seed);
            }
        }
        selection.planarSeeds = std::move(seeds);
        return Result{std::move(embedding), std::move(protection), std::move(selection), true};
    }));
}

void AiImageController::applySurfaceSelection()
{
    applySurfaceSelection(std::nullopt);
}

void AiImageController::applySurfaceText(int x, int y, bool followRelief)
{
    applySurfaceSelection(QPoint(x, y), followRelief);
}

void AiImageController::applySurfaceSelection(std::optional<QPoint> textPosition, bool followRelief)
{
    if (!hasSurfaceSelection() || !canEditSurfaceSelection())
        return;
    const auto generation = m_generation;
    const auto selected   = m_surfaceSelection.mask;
    const auto protection = m_surfaceProtection;
    const auto canvas     = m_input.canvas;
    if (textPosition) {
        if (!QRect(QPoint{}, canvas).contains(*textPosition))
            return;
        const QPoint p(
            int((2LL * textPosition->x() + 1) * selected.width() / (2LL * canvas.width())),
            int((2LL * textPosition->y() + 1) * selected.height() / (2LL * canvas.height())));
        if (!selected.constScanLine(p.y())[p.x()] ||
            (!protection.isNull() && protection.constScanLine(p.y())[p.x()])) {
            m_surfacePickingHint = false;
            m_status             = tr("Placez le texte à l’intérieur de la surface sélectionnée.");
            emit changed();
            return;
        }
    }
    const auto cancel     = m_cancel;
    m_busy                = true;
    m_status              = tr("Création du masque de surface…");
    emit changed();
    if (!m_busy || generation != m_generation)
        return;
    struct PaintLayer {
        PixelPlane   mask;
        QImage       normals;
        QVariantList crease;
    };
    const auto geometry   = m_surfaceGeometry;
    const auto focalRatio = m_surfaceGeometry.focalRatio;
    auto      *watcher = new QFutureWatcher<PaintLayer>(this);
    connect(
        watcher, &QFutureWatcherBase::finished, this,
        [this, watcher, generation, textPosition, followRelief, focalRatio] {
            watcher->deleteLater();
            if (!m_busy || generation != m_generation || !m_document)
                return;
            if (!m_document->matchesAiSource(m_input)) {
                stop(tr("L’image a changé. Relancez l’analyse de la surface."));
                return;
            }
            PaintLayer layer;
            try {
                layer = watcher->result();
            } catch (...) {
                stop(tr("Mémoire insuffisante pour créer le masque de surface."));
                return;
            }
            if (layer.mask.isNull() || layer.normals.isNull()) {
                stop(tr("Mémoire insuffisante pour créer le masque de surface."));
                return;
            }
            m_surfaceGeometry = {};
            if (!m_document->addSurfaceLayer(m_input, std::move(layer.mask),
                                             std::move(layer.normals), textPosition, followRelief,
                                             focalRatio, layer.crease)) {
                stop(textPosition ? tr("Impossible d’ajouter le texte sur la surface.")
                                  : tr("Impossible d’ajouter le calque de peinture sur surface."));
                return;
            }
            m_completed = 1;
            stop();
            m_status =
                textPosition
                    ? tr("Saisissez le texte sur la surface.")
                    : tr("Calque de peinture prêt. Le masque protège les zones non sélectionnées.");
            emit changed();
            emit layerAdded();
        });
    watcher->setFuture(QtConcurrent::run([coverage = selected,
                                          seeds    = m_surfaceSelection.planarSeeds, protection,
                                          geometry, canvas, cancel, textPosition,
                                          followRelief]() mutable {
        constexpr int edge = PhotoCrap::Ai::MaxSurfaceNormalEdge;
        const auto   &normals = geometry.normals;
        const auto    reduced =
            qMax(normals.width(), normals.height()) <= edge
                ? normals
                : normals.scaled(QSize(edge, edge), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QImage                                      face;
        std::optional<PhotoCrap::Ai::SurfaceCrease> crease;
        if (textPosition && !followRelief) {
            crease = geometry.textCrease(coverage, seeds, canvas, *textPosition, cancel);
            if (crease)
                geometry.closeTextCrease(coverage, canvas, *crease, cancel);
            else
                face = geometry.regionAt(*textPosition, canvas, false, cancel);
        }
        return PaintLayer{
            PhotoCrap::Ai::SurfaceGeometry::paintMask(coverage, canvas, cancel, protection, face),
            reduced.convertToFormat(QImage::Format_RGB888),
            crease ? crease->parameters() : QVariantList{}};
    }));
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
    watcher->setFuture(
        QtConcurrent::run([input, bytes = m_resultBytes, bits, cancel, nativeSize = m_newDocument] {
            return PhotoCrap::Ai::prepareResult(input, bytes, bits, cancel, nativeSize);
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
    if (!m_newDocument && m_document->aiInsertionBusy()) {
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
        {"newDocument", m_newDocument},
        {"sourceName", m_document->name()},
        {"outputSize", QVariantList{m_readyResult->image.width(), m_readyResult->image.height()}},
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
    if (m_provider.value("kind") == "gradio") {
        const auto options =
            QJsonDocument::fromJson(m_provider.value("options").toString().toUtf8()).object();
        QVariantMap parameters;
        for (const auto &key :
             {"lora_adapter", "seed", "randomize_seed", "guidance_scale", "steps"})
            parameters[key] = options.value(key).toVariant();
        metadata["qwenParameters"] = parameters;
        if (m_job.resultMetadata().contains("seed"))
            metadata["seed"] = m_job.resultMetadata().value("seed");
    }
    const bool inserted =
        m_newDocument ? m_documents && m_documents->addAiDocument(*m_readyResult, metadata, m_input,
                                                                  error, m_document)
                      : m_document->insertAiResult(m_input, *m_readyResult, metadata, error);
    if (!inserted) {
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
                   : m_newDocument ? tr("Résultat IA ouvert dans un nouveau document")
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

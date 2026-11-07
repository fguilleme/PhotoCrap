#include "Document.h"
#include "tools/GradientRaster.h"
#include "DocumentStorage.h"
#include "ImageUtils.h"
#include "shapes/ShapeLayer.h"
#include "storage/ProjectPayloadDevice.h"
#include "storage/ProjectPng.h"
#include "storage/ProjectReadIndex.h"
#include "storage/ProjectWriteDevice.h"
#include "storage/StreamingImageWriter.h"
#include "storage/WebpExport.h"
#include "text/TextLayer.h"

#include <QBuffer>
#include <QCache>
#include <QColorSpace>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSaveFile>
#include <QSet>
#include <QSignalBlocker>
#include <QtConcurrentRun>
#include <algorithm>
#include <limits>
#include <new>

namespace
{
constexpr qint64 MaxProjectBytes = 256LL * 1024 * 1024;

QString localPath(const QUrl &_url)
{
    return _url.isLocalFile() ? _url.toLocalFile() : QString{};
}
} // namespace

bool Document::newDocument(int _w, int _h, bool _transparent, int _bits)
{
    if ((_bits != 8 && _bits != 16) || !validSize(_w, _h))
        return false;
    QImage image = ImageUtils::blank({1, 1}, _bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                                         : QImage::Format_ARGB32_Premultiplied);
    if (!_transparent)
        image.fill(Qt::white);
    Layer layer{tr("Arrière-plan"), QImage{}};
    layer.image = PixelPlane::uniform({_w, _h}, image);
    if (layer.image.isNull())
        return fail(tr("Allocation des pixels impossible."));
    m_channelBits = _bits;
    m_size        = {_w, _h};
    m_layers      = {layer};
    m_active      = 0;
    m_name        = tr("Sans titre");
    m_projectUrl  = QUrl{};
    resetHistory(true);
    return true;
}
QImage Document::readImage(const QString &_path)
{
    if (RawImageReader::isRawFile(_path)) {
        const auto result = RawImageReader::read(_path);
        if (result.image.isNull())
            fail(result.error);
        return result.image;
    }
    if (!operationStep(0, 4))
        return {};
    QImageReader reader(_path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (!size.isValid() || !validSize(size.width(), size.height())) {
        if (!size.isValid())
            fail(tr("Format non reconnu ou image illisible."));
        return {};
    }
    if (!operationStep(1, 4))
        return {};
    QImage image = reader.read();
    if (image.isNull()) {
        fail(reader.errorString());
        return {};
    }
    if (!operationStep(2, 4))
        return {};
    if (image.colorSpace().isValid())
        image.convertToColorSpace(QColorSpace::SRgb);
    else
        image.setColorSpace(QColorSpace::SRgb);
    if (!operationStep(3, 4))
        return {};
    image.convertTo(ImageUtils::workingFormat(image));
    return operationStep(4, 4) ? image : QImage{};
}
bool Document::openFile(const QUrl &_url)
{
    const QString path = localPath(_url);
    if (path.isEmpty())
        return fail(tr("Choisissez un fichier local."));
    if (QFileInfo(path).suffix().compare("pcr", Qt::CaseInsensitive) == 0)
        return loadProject(path);
    auto image = readImage(path);
    if (image.isNull())
        return false;
    return adoptDecodedImage(image, _url, false);
}
void Document::requestOpenFile(const QUrl &_url)
{
    const QUrl target = _url;
    if (isRawFile(target)) {
        openRawFile(target);
        return;
    }
    const QString path = localPath(target);
    if (!path.isEmpty()) {
        const bool project = QFileInfo(path).suffix().compare("pcr", Qt::CaseInsensitive) == 0;
        struct LoadedFile {
            Snapshot                           state{};
            std::shared_ptr<ScratchImageCache> cache;
        };
        auto loaded = std::make_shared<LoadedFile>();
        // Even a small compressed file can expand to a large native plane.
        // Keep the explicit synchronous-test override, otherwise use a worker.
        if (deferOperation(
                project ? tr("Ouvrir le projet") : tr("Ouvrir l’image"),
                std::numeric_limits<qint64>::max() - 1,
                [target, loaded](Document &_renderer) {
                    loaded->cache = _renderer.m_scratchCache;
                    if (_renderer.openFile(target))
                        loaded->state = _renderer.snapshot({});
                },
                false,
                [this, path, loaded, target, project] {
                    if (loaded->state.layers.isEmpty())
                        return;
                    const bool reuseCache = m_scratchCache == loaded->cache;
                    adoptFileSnapshot(loaded->state, path, project);
                    // Keep the store behind prepared mappings, unless the user
                    // changed cache settings while decoding was in flight.
                    if (reuseCache)
                        m_scratchCache = loaded->cache;
                    emit fileOpened(target);
                },
                false))
            return;
    }
    if (openFile(target))
        emit fileOpened(target);
}
bool Document::isRawFile(const QUrl &_url) const
{
    return _url.isLocalFile() && RawImageReader::isRawFile(_url.toLocalFile());
}
QString Document::rawFilePatterns() const
{
    return RawImageReader::filePatterns();
}
bool Document::openRawFile(const QUrl &_url, bool _asLayer)
{
    if (!isRawFile(_url))
        return fail(tr("Choisissez un fichier RAW local."));
    if (m_rawLoading)
        return fail(tr("Un RAW est déjà en cours de lecture."));
    finishTextEdit();
    endStroke();
    finishLayerPreview();
    if (_asLayer && (!validSize(width(), height(), int(m_layers.size()) + 1) ||
                     !validLayerStorage(width(), height(), 1)))
        return false;
    // Replacing a document does not need to retain its old planes on the worker.
    m_beforeRawLoad        = _asLayer ? snapshot(tr("Importer un calque")) : Snapshot{};
    m_rawUrl               = _url;
    m_rawAsLayer           = _asLayer;
    m_rawSession           = m_documentSession;
    m_rawState             = m_stateId;
    m_rawCancel            = std::make_shared<std::atomic_bool>(false);
    const auto before      = m_beforeRawLoad;
    const auto cache       = m_scratchCache;
    const int  planeRamMiB = m_planeRamMiB;
    const auto tool        = m_tool;
    const auto color       = m_color;
    const auto secondary   = m_secondary;
    const auto cancel      = m_rawCancel;
    const QUrl target      = _url;
    m_rawLoading           = true;
    emit rawLoadingChanged();
    m_rawWatcher.setFuture(QtConcurrent::run(
        [target, cancel, before, cache, planeRamMiB, tool, color, secondary, _asLayer] {
            PreparedRawResult output;
            output.cache = cache;
            try {
                auto decoded     = RawImageReader::read(target.toLocalFile(), cancel);
                output.error     = decoded.error;
                output.cancelled = decoded.cancelled || cancel->load();
                if (!output.cancelled && !decoded.image.isNull()) {
                    Document renderer;
                    renderer.m_backgroundWorker = true;
                    renderer.m_backgroundCancel = cancel;
                    renderer.m_tool             = tool;
                    renderer.m_color            = color;
                    renderer.m_secondary        = secondary;
                    if (_asLayer)
                        renderer.restore(before);
                    renderer.m_historyBrushSource = {};
                    renderer.m_undo.clear();
                    renderer.m_redo.clear();
                    renderer.m_scratchCache = cache;
                    renderer.m_planeRamMiB  = planeRamMiB;
                    QObject::connect(&renderer, &Document::errorOccurred, &renderer,
                                     [&output](const QString &message) { output.error = message; });
                    if (renderer.adoptDecodedImage(decoded.image, target, _asLayer) &&
                        !cancel->load()) {
                        decoded.image = {};
                        output.state  = renderer.snapshot({});
                    }
                }
            } catch (const std::bad_alloc &) {
                output.error = tr("Mémoire insuffisante pour terminer l’opération.");
            } catch (...) {
                output.error = tr("Le calcul de l’opération a échoué.");
            }
            output.cancelled = output.cancelled || cancel->load();
            return output;
        }));
    return true;
}
void Document::cancelRawLoad()
{
    if (m_rawCancel)
        m_rawCancel->store(true);
    m_beforeRawLoad = {};
}
void Document::finishRawLoad()
{
    PreparedRawResult result;
    try {
        result = m_rawWatcher.future().takeResult();
    } catch (...) {
        result.error = tr("Mémoire insuffisante pour terminer l’opération.");
    }
    const bool cancelled = result.cancelled || !m_rawCancel || m_rawCancel->load() ||
                           m_rawSession != m_documentSession || m_rawState != m_stateId;
    const QUrl url       = m_rawUrl;
    const bool asLayer   = m_rawAsLayer;
    const auto before    = std::move(m_beforeRawLoad);
    m_beforeRawLoad      = {};
    bool opened          = false;
    if (!cancelled) {
        if (!result.error.isEmpty())
            fail(result.error);
        else if (!result.state.layers.isEmpty()) {
            const bool reuseCache = m_scratchCache == result.cache;
            if (asLayer) {
                {
                    QSignalBlocker blocker(this);
                    restore(result.state);
                }
                commit(before, tr("Importer un calque"));
                emit documentChanged();
                emit canvasSizeChanged();
                emit selectionChanged();
                emit slicesChanged();
                emit penPathsChanged();
            } else
                adoptFileSnapshot(result.state, url.toLocalFile(), false);
            if (reuseCache)
                m_scratchCache = result.cache;
            opened = true;
        }
    }
    m_rawLoading = false;
    m_rawCancel.reset();
    emit rawLoadingChanged();
    emit settingsChanged();
    if (opened && !asLayer)
        emit rawFileOpened(url);
}
bool Document::adoptDecodedImage(const QImage &_image, const QUrl &_url, bool _asLayer)
{
    if (!operationStep(0, 1) || _image.isNull() || !validSize(_image.width(), _image.height()))
        return false;
    if (_asLayer) {
        if (!validSize(width(), height(), int(m_layers.size()) + 1) ||
            !validLayerStorage(width(), height(), 1))
            return false;
        const int bits = qMax(m_channelBits, ImageUtils::is16Bit(_image) ? 16 : 8);
        if (!validLayerStorage(width(), height(), 1, 0, bits))
            return false;
        const auto format =
            bits == 16 ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied;
        PixelPlane target;
        if (_image.size() == m_size && _image.devicePixelRatio() == 1.)
            target = _image.convertToFormat(format);
        else {
            target = ImageUtils::blank({1, 1}, format);
            const QPoint origin((width() - _image.width()) / 2, (height() - _image.height()) / 2);
            const QSizeF logicalSize = QSizeF(_image.size()) / _image.devicePixelRatio();
            const QRect  bounds      = QRectF(QPointF(origin), logicalSize)
                                           .toAlignedRect()
                                           .adjusted(-1, -1, 1, 1)
                                           .intersected(QRect(QPoint{}, m_size));
            if (!bounds.isEmpty()) {
                QImage patch = ImageUtils::blank(bounds.size(), format);
                if (patch.isNull())
                    return fail(tr("Allocation des pixels impossible."));
                QPainter painter(&patch);
                painter.drawImage(origin - bounds.topLeft(), _image);
                painter.end();
                if (!target.writeRegion(bounds.topLeft(), patch, m_size))
                    return fail(tr("Allocation des pixels impossible."));
            }
        }
        if (target.isNull() || !operationStep(0, 1))
            return false;
        auto                layers = m_layers;
        PreparedPlaneBudget budget;
        for (auto &layer : layers)
            if (!prepareNativePlane(layer.image, format, budget) ||
                !prepareNativePlane(layer.source, format, budget))
                return false;
        if (!prepareNativePlane(target, format, budget))
            return false;
        edit(tr("Importer un calque"), [&] {
            m_channelBits = bits;
            m_layers      = std::move(layers);
            Layer layer;
            layer.name  = QFileInfo(_url.toLocalFile()).completeBaseName();
            layer.image = std::move(target);
            m_layers.insert(++m_active, std::move(layer));
            selectOnly(m_active);
        });
        return true;
    }
    const QString       path = _url.toLocalFile();
    PixelPlane          pixels(_image);
    PreparedPlaneBudget budget;
    if (!prepareNativePlane(pixels, ImageUtils::workingFormat(_image), budget))
        return false;
    Layer layer;
    layer.name    = QFileInfo(path).completeBaseName();
    layer.image   = std::move(pixels);
    m_channelBits = ImageUtils::is16Bit(_image) ? 16 : 8;
    m_size        = _image.size();
    m_layers      = {std::move(layer)};
    m_active      = 0;
    m_name        = QFileInfo(path).completeBaseName();
    m_projectUrl  = QUrl{};
    resetHistory(false);
    return true;
}
void Document::requestImportLayer(const QUrl &_url)
{
    const QUrl target = _url;
    if (isRawFile(target)) {
        openRawFile(target, true);
        return;
    }
    if (localPath(target).isEmpty()) {
        fail(tr("Choisissez un fichier local."));
        return;
    }
    if (!validSize(width(), height(), int(m_layers.size()) + 1) ||
        !validLayerStorage(width(), height(), 1))
        return;
    if (!deferEdit(tr("Importer un calque"), std::numeric_limits<qint64>::max() - 1,
                   [target](Document &_renderer) { _renderer.importLayer(target); }))
        importLayer(target);
}
bool Document::importLayer(const QUrl &_url)
{
    if (!validSize(width(), height(), int(m_layers.size()) + 1) ||
        !validLayerStorage(width(), height(), 1))
        return false;
    auto image = readImage(localPath(_url));
    if (image.isNull())
        return false;
    return adoptDecodedImage(image, _url, true);
}
void Document::acceptProjectSave(const QString &_path)
{
    m_savedId    = m_contentId;
    m_projectUrl = QUrl::fromLocalFile(_path);
    m_name       = QFileInfo(_path).completeBaseName();
    emit documentChanged();
    emit historyChanged();
    emit statusMessage(tr("Projet enregistré"));
}
void Document::requestSaveProject(const QUrl &_url)
{
    QString path = localPath(_url);
    if (path.isEmpty()) {
        fail(tr("Chemin de sauvegarde invalide."));
        return;
    }
    if (!path.endsWith(".pcr", Qt::CaseInsensitive))
        path += ".pcr";
    const QUrl target = QUrl::fromLocalFile(path);
    if (!deferOperation(
            tr("Enregistrer le projet"), editCost(true),
            [target](Document &_renderer) { _renderer.saveProject(target); }, false,
            [this, path] {
                acceptProjectSave(path);
                const QUrl saved = m_projectUrl;
                emit       projectSaved(saved);
            })) {
        if (saveProject(target)) {
            const QUrl saved = m_projectUrl;
            emit       projectSaved(saved);
        }
    }
}
bool Document::saveProject(const QUrl &_url)
{
    finishTextEdit();
    endStroke();
    if (pixelEditBusy())
        return fail(
            tr("Attendez la fin du traitement ou annulez-le avant de sauvegarder ou exporter."));
    QString path = localPath(_url);
    if (path.isEmpty())
        return fail(tr("Chemin de sauvegarde invalide."));
    if (!path.endsWith(".pcr", Qt::CaseInsensitive))
        path += ".pcr";
    int                planes = 0;
    QHash<qint64, int> references;
    for (const auto &layer : m_layers)
        for (const PixelPlane *plane : {&layer.image, &layer.mask, &layer.source})
            if (!plane->isNull()) {
                ++planes;
                ++references[plane->cacheKey()];
            }
    // Only repeated immutable versions are worth retaining. Large payloads
    // keep streaming without caching; neither cache nor recording can grow
    // beyond this byte budget, independently of the document's native size.
    constexpr int              payloadBudget = 8 * 1024 * 1024;
    QCache<qint64, QByteArray> payloads(payloadBudget);
    QSet<qint64>               uncachedPayloads;
    const int                  total     = qMax(2, planes * 2 + 1);
    int                        completed = 0;
    if (!operationStep(0, total))
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());
    LimitedWriteDevice output(file, MaxProjectBytes);
    const auto         saveError = [&](bool _encoding = false) {
        if (output.limitExceeded())
            return fail(tr("Le projet dépasse 256 Mio. Réduisez ses dimensions ou ses calques."));
        if (file.error() != QFileDevice::NoError)
            return fail(file.errorString());
        return fail(_encoding ? tr("Impossible d’encoder les données du calque.")
                              : output.errorString());
    };
    const auto write = [&](const QByteArray &_bytes) {
        return output.write(_bytes) == _bytes.size();
    };
    const auto prefix = [&](const QJsonObject &_object) {
        auto bytes = QJsonDocument(_object).toJson(QJsonDocument::Compact);
        bytes.chop(1); // Append streamed fields before the object's closing brace.
        return write(bytes);
    };
    const auto image = [&](const PixelPlane &_plane) {
        if (_plane.isNull())
            return true;
        if (!operationStep(completed++, total))
            return false;
        const auto key = _plane.cacheKey();
        if (const auto payload = payloads.object(key)) {
            if (!operationStep(completed++, total))
                return false;
            for (qsizetype offset = 0; offset < payload->size();) {
                if (!operationStep(completed, total))
                    return false;
                const auto count = qMin(qsizetype(16 * 1024), payload->size() - offset);
                if (output.write(payload->constData() + offset, count) != count)
                    return false;
                offset += count;
            }
            return operationStep(completed, total);
        }
        if (!operationStep(completed++, total))
            return false;
        const auto encode = [&](Base64WriteDevice &_encoder) {
            const auto space = _plane.colorSpace();
            const auto format = _plane.format();
            const bool native = format == QImage::Format_Grayscale8 ||
                                format == QImage::Format_ARGB32_Premultiplied ||
                                format == QImage::Format_RGBA64_Premultiplied;
            const bool largeNative =
                native && qint64(_plane.width()) * _plane.height() >= 4LL * 1024 * 1024 &&
                (!space.isValid() || space == QColorSpace(QColorSpace::SRgb));
            const bool written =
                largeNative ? PhotoCrap::Storage::writeProjectPng(
                                  _encoder, _plane, [&] { return operationStep(completed, total); })
                            : QImage(_plane).save(&_encoder, "PNG");
            return written && _encoder.finish();
        };
        if (references.value(key) > 1 && !uncachedPayloads.contains(key)) {
            RecordingWriteDevice recording(output, payloadBudget);
            Base64WriteDevice    encoder(recording);
            if (!encode(encoder))
                return false;
            if (recording.recordingComplete()) {
                const auto bytes = recording.recording();
                payloads.insert(key, new QByteArray(bytes), int(bytes.size()));
            } else
                uncachedPayloads.insert(key);
            return operationStep(completed, total);
        }
        Base64WriteDevice encoder(output);
        return encode(encoder) && operationStep(completed, total);
    };
    // Empty named paths need the new reader; keep ordinary documents readable
    // by v9 applications, whose path decoder requires at least one anchor.
    const bool  emptyPaths = std::any_of(m_penPaths.cbegin(), m_penPaths.cend(),
                                         [](const PenPath &path) { return path.points.isEmpty(); });
    const bool  shapes     = std::any_of(m_layers.cbegin(), m_layers.cend(),
                                         [](const Layer &layer) { return layer.kind == "shape"; });
    const bool gradients = std::any_of(m_layers.cbegin(), m_layers.cend(),
                                      [](const Layer &layer) { return layer.adjustmentType == "gradientFill"; });
    QJsonObject root{{"format", "PhotoCrap"},
                     {"version", gradients ? 12
                                 : shapes       ? 11
                                 : emptyPaths ? 10
                                              : 9},
                     {"width", width()},
                     {"height", height()},
                     {"activeLayer", m_active}};
    root["channelBits"] = m_channelBits;
    QVector<PenPath> savedPaths;
    int              savedActivePath = -1;
    for (int i = 0; i < m_penPaths.size(); ++i) {
        if (!m_penPaths[i].shapeLayerId.isEmpty())
            continue;
        if (i == m_activePenPath)
            savedActivePath = int(savedPaths.size());
        savedPaths.append(m_penPaths[i]);
    }
    root["penPaths"]      = PenGeometry::encode(savedPaths);
    root["activePenPath"] = savedActivePath;
    QJsonArray slices;
    for (const auto &rect : m_slices)
        slices.append(QJsonObject{
            {"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()}});
    root["slices"] = slices;
    if (!prefix(root) || !write(",\"layers\":["))
        return saveError();
    bool first = true;
    for (const auto &layer : m_layers) {
        if (!operationStep(completed, total))
            return false;
        if (!first && !write(","))
            return saveError();
        first = false;
        const QJsonObject metadata{{"name", layer.name},
                                   {"visible", layer.visible},
                                   {"locked", layer.locked},
                                   {"opacity", layer.opacity},
                                   {"blend", layer.blend},
                                   {"kind", layer.kind},
                                   {"adjustmentType", layer.adjustmentType},
                                   {"parameters", QJsonObject::fromVariantMap(layer.parameters)},
                                   {"compactPixels", ImageUtils::compact(layer.image)},
                                   {"compactMask", ImageUtils::compact(layer.mask)},
                                   {"maskEnabled", layer.maskEnabled},
                                   {"clippingMask", layer.clippingMask},
                                   {"groupId", layer.groupId},
                                   {"groupCollapsed", layer.groupCollapsed}};
        if (!prefix(metadata) || !write(",\"png\":\""))
            return saveError();
        if (!image(layer.image))
            return saveError(true);
        if (!write("\",\"mask\":\""))
            return saveError();
        if (!image(layer.mask))
            return saveError(true);
        if (!write("\",\"source\":\""))
            return saveError();
        if (!image(layer.source))
            return saveError(true);
        if (!write("\"}"))
            return saveError();
    }
    if (!write("]}"))
        return saveError();
    if (!operationStep(completed, total) || !beginOperationPublication())
        return false;
    operationStep(total, total);
    if (!file.commit())
        return fail(file.errorString());
    acceptProjectSave(path);
    return true;
}
bool Document::loadProject(const QString &_path)
{
    if (!operationStep(0, 2))
        return false;
    QFile file(_path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(file.errorString());
    if (file.size() > MaxProjectBytes)
        return fail(tr("Projet trop volumineux (256 Mio maximum)."));
    ProjectReadIndex index;
    if (!index.read(file, [this] { return operationStep(0, 1); }))
        return fail(tr("Projet PhotoCrap invalide ou version non prise en charge."));
    if (!operationStep(0, 2))
        return false;
    const auto root    = index.metadata;
    const int  version = root["version"].toInt();
    if (root["format"] != "PhotoCrap" || version < 1 || version > 12)
        return fail(tr("Projet PhotoCrap invalide ou version non prise en charge."));
    const int   w = root["width"].toInt(), h = root["height"].toInt();
    const auto &array = index.layers;
    const int   bits  = version >= 6 ? root["channelBits"].toInt() : 8;
    if (bits != 8 && bits != 16)
        return fail(tr("Profondeur de projet invalide."));
    const auto format =
        bits == 16 ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied;
    if (!validSize(w, h, int(array.size())))
        return false;
    int colors = 0, masks = 0;
    QVector<ProjectReadIndex::LayerEntry> entries;
    entries.reserve(array.size());
    // Reserve the complete logical storage before decoding any pixel. The
    // preflight parses one envelope at a time and immediately releases it.
    for (int layerIndex = 0; layerIndex < array.size(); ++layerIndex) {
        bool valid = false;
        auto entry = index.layer(layerIndex, valid);
        if (!valid)
            return fail(tr("Projet PhotoCrap invalide ou version non prise en charge."));
        colors += (entry.payload("png").size > 0 &&
                   !(version >= 7 && entry.metadata["compactPixels"].toBool())) +
                  (entry.payload("source").size > 0);
        masks += entry.payload("mask").size > 0 &&
                 !(version >= 7 && entry.metadata["compactMask"].toBool());
        entries.append(std::move(entry));
    }
    if (!validStorageBytes(DocumentStorage::estimateBytes(w, h, colors, masks, bits)))
        return false;
    QVector<Layer>      layers;
    PreparedPlaneBudget budget;
    // Duplicated layers and frequency bands can serialize identical PNGs.
    // Include the native target format: the same PNG used as colour and as a
    // grayscale mask must not share their differently converted pixels.
    QHash<QByteArray, PixelPlane> planes;
    int                           completed = 1;
    const int                     total     = 3 * int(array.size()) + 2;
    for (auto &stored : entries) {
        if (!operationStep(completed, total))
            return false;
        const auto  entry  = std::move(stored);
        const auto &object = entry.metadata;
        QString decodeError;
        auto        decode = [&](ProjectReadIndex::Payload _payload, QImage::Format _format,
                                 bool _compact = false) -> PixelPlane {
            if (!operationStep(++completed, total) || !_payload.size || !decodeError.isEmpty())
                return {};
            ProjectPayloadDevice buffer(file, _payload,
                                        [&] { return operationStep(completed, total); });
            QCryptographicHash   hash(QCryptographicHash::Sha256);
            if (!buffer.isOpen() || !hash.addData(&buffer) || buffer.failed())
                return {};
            const QSize      size = _compact ? QSize(1, 1) : QSize(w, h);
            const QByteArray key =
                QByteArray::number(int(_format)) + (_compact ? 'c' : 'd') + ':' + hash.result();
            if (auto found = planes.constFind(key); found != planes.cend())
                return operationStep(completed, total) ? found.value() : PixelPlane{};
            if (!buffer.rewind())
                return {};
            using PhotoCrap::Storage::ProjectPngRead;
            PixelPlane plane;
            const auto streamed = qint64(size.width()) * size.height() >= 4LL * 1024 * 1024
                                      ? PhotoCrap::Storage::readProjectPng(
                                            buffer, size, _format, plane,
                                            [&] { return operationStep(completed, total); })
                                      : ProjectPngRead::Unsupported;
            if (streamed == ProjectPngRead::Failed)
                return {};
            if (streamed == ProjectPngRead::Unsupported) {
                if (!buffer.rewind())
                    return {};
                QImageReader reader(&buffer, "PNG");
                if (reader.size() != size)
                    return {};
                QImage decoded = reader.read().convertToFormat(_format);
                if (decoded.isNull())
                    return {};
                if (_format != QImage::Format_Grayscale8)
                    decoded.setColorSpace(QColorSpace::SRgb);
                plane = PixelPlane(decoded);
            }
            if (!operationStep(completed, total))
                return {};
            // Seal each plane before decoding the next. Do not retain an
            // entire stack of PNG buffers beside the imported tile versions.
            if (!plane.freeze() || !prepareNativePlane(plane, _format, budget)) {
                decodeError = tr("Allocation des pixels impossible.");
                return {};
            }
            planes.insert(key, plane);
            return plane;
        };
        const QString kind =
            version == 1 ? QStringLiteral("pixel") : object["kind"].toString("pixel");
        if (kind != "pixel" && kind != "adjustment" && kind != "group" && kind != "frequency" &&
            kind != "frequencyLow" && kind != "frequencyHigh" &&
            !(version >= 9 && kind == "text") && !(version >= 11 && kind == "shape"))
            return fail(tr("Type de calque inconnu."));
        auto image =
            decode(entry.payload("png"), format, version >= 7 && object["compactPixels"].toBool());
        auto mask   = decode(entry.payload("mask"), QImage::Format_Grayscale8,
                             version >= 7 && object["compactMask"].toBool());
        auto source = decode(entry.payload("source"), format);
        if (!decodeError.isEmpty())
            return fail(decodeError);
        if (((kind == "pixel" || kind == "text" || kind == "shape") && image.isNull()) ||
            (kind.startsWith("frequency") && source.isNull()) ||
            (entry.payload("mask").size > 0 && mask.isNull()))
            return fail(tr("Calque corrompu ou dimensions incohérentes."));
        if (!image.isNull())
            image.setColorSpace(QColorSpace::SRgb);
        if (!source.isNull())
            source.setColorSpace(QColorSpace::SRgb);
        const double     opacity    = object["opacity"].toDouble(-1);
        int              blend      = object["blend"].toInt(-1);
        static const int legacyV1[] = {0, 3, 8, 12, 2, 7, 19};
        static const int legacyV2[] = {0, 3, 8, 12, 2, 7, 19, 13, 14, 9, 4, 20, 10};
        if (version == 1 && blend >= 0 && blend < 7)
            blend = legacyV1[blend];
        else if (version == 2 && blend >= 0 && blend < 13)
            blend = legacyV2[blend];
        if (opacity < 0 || opacity > 1 || blend < 0 || blend > 26)
            return fail(tr("Propriétés de calque invalides."));
        Layer layer{object["name"].toString().left(256), QImage{}, object["visible"].toBool(true),
                    object["locked"].toBool(),           opacity,  blend};
        layer.image          = image;
        layer.kind           = kind;
        layer.adjustmentType = object["adjustmentType"].toString();
        layer.parameters     = object["parameters"].toObject().toVariantMap();
        if (kind == "text" && !PhotoCrap::Text::normalize(layer.parameters))
            return fail(tr("Calque de texte invalide."));
        if (kind == "shape" && !PhotoCrap::Shape::normalize(layer.parameters))
            return fail(tr("Calque de forme invalide."));
        if (layer.adjustmentType == "gradientFill") {
            PhotoCrap::Tools::GradientRaster gradient;
            if (version < 12 || kind != "adjustment" ||
                !PhotoCrap::Tools::GradientRaster::decode(
                    layer.parameters.value("gradient").toMap(), QSize(w, h), gradient))
                return fail(tr("Calque de dégradé invalide."));
        }
        layer.mask           = mask;
        layer.maskEnabled    = object["maskEnabled"].toBool(true);
        layer.clippingMask   = object["clippingMask"].toBool(false);
        layer.groupId        = object["groupId"].toString().left(64);
        layer.groupCollapsed = object["groupCollapsed"].toBool(false);
        layer.source         = source;
        layers.append(layer);
        if (version == 2 && kind == "frequency") {
            layers.last().kind       = "pixel";
            layers.last().image      = source;
            layers.last().source     = {};
            layers.last().parameters = {};
        }
    }
    QVector<QRect> slices;
    const auto     sliceArray = root["slices"].toArray();
    if (sliceArray.size() > 256)
        return fail(tr("Le projet contient trop de tranches."));
    for (const auto &value : sliceArray) {
        const auto object = value.toObject();
        const int  x = object["x"].toInt(-1), y = object["y"].toInt(-1);
        const int  sw = object["width"].toInt(), sh = object["height"].toInt();
        if (x < 0 || y < 0 || sw < 1 || sh < 1 || x > w || y > h || sw > w - x || sh > h - y)
            return fail(tr("Tranche de projet invalide."));
        slices.append(QRect(x, y, sw, sh));
    }
    QVector<PenPath> penPaths;
    if (version >= 8 && (!root["penPaths"].isArray() ||
                         !PenGeometry::decode(root["penPaths"].toArray(), penPaths, version >= 10)))
        return fail(tr("Tracés du projet invalides."));
    const int activePenPath = version >= 8 ? root["activePenPath"].toInt(-1) : -1;
    if (activePenPath < -1 || activePenPath >= penPaths.size())
        return fail(tr("Tracé actif invalide."));
    if (!operationStep(total, total))
        return false;
    Snapshot loaded{};
    loaded.channelBits   = bits;
    loaded.layers        = std::move(layers);
    loaded.size          = {w, h};
    loaded.active        = qBound(0, root["activeLayer"].toInt(), int(loaded.layers.size()) - 1);
    loaded.penPaths      = std::move(penPaths);
    loaded.activePenPath = activePenPath;
    loaded.slices        = std::move(slices);
    adoptFileSnapshot(loaded, _path, true);
    return true;
}
void Document::adoptFileSnapshot(const Snapshot &_loaded, const QString &_path, bool _project)
{
    // Only replace the current document after validating every layer and path.
    m_channelBits = _loaded.channelBits;
    m_layers      = _loaded.layers;
    m_size        = _loaded.size;
    m_active      = _loaded.active;
    m_projectUrl  = _project ? QUrl::fromLocalFile(_path) : QUrl{};
    m_name        = QFileInfo(_path).completeBaseName();
    resetHistory(_project);
    m_penPaths      = _loaded.penPaths;
    m_activePenPath = _loaded.activePenPath;
    syncPenShapePath(_loaded.activePenPath < 0);
    emit penPathsChanged();
    m_slices      = _loaded.slices;
    m_activeSlice = m_slices.isEmpty() ? -1 : 0;
    emit slicesChanged();
}
void Document::requestExportImage(const QUrl &_url)
{
    if (!deferOperation(
            tr("Exporter l’image"), qMax(editCost(false), editCost(true)),
            [_url](Document &_renderer) { _renderer.exportImage(_url); }, false,
            [this] { emit statusMessage(tr("Image exportée")); }))
        exportImage(_url);
}
bool Document::exportImage(const QUrl &_url)
{
    finishTextEdit();
    endStroke();
    if (pixelEditBusy())
        return fail(
            tr("Attendez la fin du traitement ou annulez-le avant de sauvegarder ou exporter."));
    QString path = localPath(_url);
    if (path.isEmpty())
        return fail(tr("Chemin d’export invalide."));
    QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix.isEmpty()) {
        path += ".png";
        suffix = "png";
    }
    if (suffix != "png" && suffix != "jpg" && suffix != "jpeg" && suffix != "bmp" &&
        suffix != "webp")
        return fail(
            tr("Export disponible en PNG, JPEG, BMP ou WebP (si le module Qt est installé)."));
    if (!operationStep(0, 4))
        return false;
    const bool jpeg = suffix == "jpg" || suffix == "jpeg";
    if ((suffix == "png" || suffix == "bmp" || jpeg || suffix == "webp") &&
        qint64(width()) * height() >= 4LL * 1024 * 1024) {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return fail(file.errorString());
        QString    error;
        const auto read     = [this](const QRect &_region) { return compositeRegion(_region); };
        const auto progress = [this](int _done, int _total) {
            return operationStep(100 + 650 * _done / _total, 1000);
        };
        const bool written =
            jpeg ? PhotoCrap::Storage::writeJpegBands(file, m_size, read, progress, error)
            : suffix == "webp"
                ? PhotoCrap::Storage::writeWebpBands(file, m_size, read, progress, error)
                : PhotoCrap::Storage::writeImageBands(file, m_size, m_channelBits, suffix == "png",
                                                      read, progress, error);
        if (!written) {
            if (!operationStep(0, 1))
                return false;
            return fail(error);
        }
        if (!operationStep(3, 4) || !beginOperationPublication())
            return false;
        operationStep(4, 4);
        if (!file.commit())
            return fail(file.errorString());
        emit statusMessage(tr("Image exportée"));
        return true;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());
    QImage result;
    if (m_backgroundWorker) {
        composite();
        result       = std::move(m_composite);
        m_cacheDirty = m_fullCompositeDirty = true;
    } else
        result = composite();
    if (!operationStep(1, 4))
        return false;
    if (suffix == "jpg" || suffix == "jpeg" || suffix == "bmp") {
        if (result.format() == QImage::Format_ARGB32_Premultiplied && result.isDetached()) {
            // The worker owns its accumulator. Compose white behind its pixels
            // in place instead of allocating a second full RGB image.
            QPainter painter(&result);
            painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
            painter.fillRect(result.rect(), Qt::white);
            painter.end();
            if (!result.reinterpretAsFormat(QImage::Format_RGB32))
                result = result.convertToFormat(QImage::Format_RGB32);
        } else {
            QImage white(result.size(), QImage::Format_RGB32);
            white.fill(Qt::white);
            QPainter painter(&white);
            painter.drawImage(0, 0, result);
            painter.end();
            result = std::move(white);
        }
    }
    result.setColorSpace(QColorSpace::SRgb);
    if (!operationStep(2, 4))
        return false;
    QImageWriter writer(&file, suffix.toLatin1());
    writer.setQuality(95);
    if (!writer.write(result))
        return fail(writer.errorString());
    if (!operationStep(3, 4) || !beginOperationPublication())
        return false;
    operationStep(4, 4);
    if (!file.commit())
        return fail(file.errorString());
    emit statusMessage(tr("Image exportée"));
    return true;
}

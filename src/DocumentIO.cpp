#include "Document.h"
#include "DocumentStorage.h"
#include "ImageUtils.h"
#include "storage/ProjectWriteDevice.h"
#include "text/TextLayer.h"

#include <QBuffer>
#include <QColorSpace>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSaveFile>
#include <QtConcurrentRun>
#include <limits>

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
    m_channelBits = _bits;
    m_size        = {_w, _h};
    QImage image  = ImageUtils::blank(m_size, workingFormat());
    if (!_transparent)
        image.fill(Qt::white);
    m_layers     = {{tr("Arrière-plan"), image}};
    m_active     = 0;
    m_name       = tr("Sans titre");
    m_projectUrl = QUrl{};
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
        auto       loaded  = std::make_shared<Snapshot>();
        // Even a small compressed file can expand to a large native plane.
        // Keep the explicit synchronous-test override, otherwise use a worker.
        if (deferOperation(
                project ? tr("Ouvrir le projet") : tr("Ouvrir l’image"),
                std::numeric_limits<qint64>::max() - 1,
                [target, loaded](Document &_renderer) {
                    if (_renderer.openFile(target))
                        *loaded = _renderer.snapshot({});
                },
                false,
                [this, path, loaded, target, project] {
                    if (loaded->layers.isEmpty())
                        return;
                    adoptFileSnapshot(*loaded, path, project);
                    emit fileOpened(target);
                }))
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
    endStroke();
    if (_asLayer && (!validSize(width(), height(), int(m_layers.size()) + 1) ||
                     !validLayerStorage(width(), height(), 1)))
        return false;
    m_rawUrl     = _url;
    m_rawAsLayer = _asLayer;
    m_rawSession = m_documentSession;
    m_rawState   = m_stateId;
    m_rawCancel  = std::make_shared<std::atomic_bool>(false);
    m_rawLoading = true;
    emit          rawLoadingChanged();
    const QString path   = _url.toLocalFile();
    const auto    cancel = m_rawCancel;
    m_rawWatcher.setFuture(
        QtConcurrent::run([path, cancel] { return RawImageReader::read(path, cancel); }));
    return true;
}
void Document::cancelRawLoad()
{
    if (m_rawCancel)
        m_rawCancel->store(true);
}
void Document::finishRawLoad()
{
    const auto result    = m_rawWatcher.future().takeResult();
    const bool cancelled = result.cancelled || m_rawCancel->load() ||
                           m_rawSession != m_documentSession || m_rawState != m_stateId;
    const QUrl url       = m_rawUrl;
    const bool asLayer   = m_rawAsLayer;
    bool       opened    = false;
    if (!cancelled) {
        if (result.image.isNull())
            fail(result.error);
        else
            opened = adoptDecodedImage(result.image, url, asLayer);
    }
    m_rawLoading = false;
    m_rawCancel.reset();
    emit rawLoadingChanged();
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
        edit(tr("Importer un calque"), [&] {
            m_channelBits = bits;
            for (auto &layer : m_layers) {
                if (!operationStep(0, 1))
                    return;
                if (!layer.image.isNull())
                    layer.image.convertTo(workingFormat());
                if (!layer.source.isNull())
                    layer.source.convertTo(workingFormat());
            }
            if (!operationStep(0, 1))
                return;
            QImage target;
            if (_image.size() == m_size && _image.devicePixelRatio() == 1.)
                target = _image.convertToFormat(workingFormat());
            else {
                target = ImageUtils::blank(m_size, workingFormat());
                QPainter painter(&target);
                painter.drawImage((width() - _image.width()) / 2, (height() - _image.height()) / 2,
                                  _image);
                painter.end();
            }
            if (!operationStep(0, 1))
                return;
            m_layers.insert(++m_active, {QFileInfo(_url.toLocalFile()).completeBaseName(), target});
            selectOnly(m_active);
        });
        return true;
    }
    const QString path = _url.toLocalFile();
    m_channelBits      = ImageUtils::is16Bit(_image) ? 16 : 8;
    m_size             = _image.size();
    m_layers           = {{QFileInfo(path).completeBaseName(), _image}};
    m_active           = 0;
    m_name             = QFileInfo(path).completeBaseName();
    m_projectUrl       = QUrl{};
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
    int planes = 0;
    for (const auto &layer : m_layers)
        planes += !layer.image.isNull() + !layer.mask.isNull() + !layer.source.isNull();
    const int total     = qMax(2, planes * 2 + 1);
    int       completed = 0;
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
        const QImage pixels = _plane;
        if (pixels.isNull() || !operationStep(completed++, total))
            return false;
        Base64WriteDevice encoder(output);
        return pixels.save(&encoder, "PNG") && encoder.finish() && operationStep(completed, total);
    };
    QJsonObject root{{"format", "PhotoCrap"},
                     {"version", 9},
                     {"width", width()},
                     {"height", height()},
                     {"activeLayer", m_active}};
    root["channelBits"]   = m_channelBits;
    root["penPaths"]      = PenGeometry::encode(m_penPaths);
    root["activePenPath"] = m_activePenPath;
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
    QJsonParseError error;
    const auto      json = QJsonDocument::fromJson(file.readAll(), &error);
    if (!operationStep(1, 2))
        return false;
    const auto root    = json.object();
    const int  version = root["version"].toInt();
    if (error.error != QJsonParseError::NoError || root["format"] != "PhotoCrap" ||
        (version < 1 || version > 9))
        return fail(tr("Projet PhotoCrap invalide ou version non prise en charge."));
    const int  w = root["width"].toInt(), h = root["height"].toInt();
    const auto array = root["layers"].toArray();
    const int  bits  = version >= 6 ? root["channelBits"].toInt() : 8;
    if (bits != 8 && bits != 16)
        return fail(tr("Profondeur de projet invalide."));
    const auto format =
        bits == 16 ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied;
    if (!validSize(w, h, int(array.size())))
        return false;
    int colors = 0, masks = 0;
    for (const auto &value : array) {
        const auto object = value.toObject();
        colors += (!object["png"].toString().isEmpty() &&
                   !(version >= 7 && object["compactPixels"].toBool())) +
                  !object["source"].toString().isEmpty();
        masks += !object["mask"].toString().isEmpty() &&
                 !(version >= 7 && object["compactMask"].toBool());
    }
    if (!validStorageBytes(DocumentStorage::estimateBytes(w, h, colors, masks, bits)))
        return false;
    QVector<Layer> layers;
    int            completed = 1;
    const int      total     = 3 * int(array.size()) + 2;
    for (const auto &value : array) {
        if (!operationStep(completed, total))
            return false;
        auto    object = value.toObject();
        QString decodeError;
        auto    decode = [&](const QString &_encoded, QImage::Format _format,
                             bool _compact = false) -> PixelPlane {
            if (!operationStep(++completed, total) || _encoded.isEmpty() || !decodeError.isEmpty())
                return {};
            QByteArray bytes = QByteArray::fromBase64(_encoded.toLatin1(),
                                                      QByteArray::AbortOnBase64DecodingErrors);
            QBuffer    buffer(&bytes);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer, "PNG");
            if (reader.size() != (_compact ? QSize(1, 1) : QSize(w, h)))
                return {};
            QImage decoded = reader.read().convertToFormat(_format);
            if (_format != QImage::Format_Grayscale8)
                decoded.setColorSpace(QColorSpace::SRgb);
            if (!operationStep(completed, total))
                return {};
            PixelPlane plane(decoded);
            decoded = {};
            // Seal each plane before decoding the next. Do not retain an
            // entire stack of PNG buffers beside the imported tile versions.
            if (!plane.freeze()) {
                decodeError = tr("Allocation des pixels impossible.");
                return {};
            }
            return plane;
        };
        const QString kind =
            version == 1 ? QStringLiteral("pixel") : object["kind"].toString("pixel");
        if (kind != "pixel" && kind != "adjustment" && kind != "group" && kind != "frequency" &&
            kind != "frequencyLow" && kind != "frequencyHigh" && !(version >= 9 && kind == "text"))
            return fail(tr("Type de calque inconnu."));
        auto image  = decode(object["png"].toString(), format,
                             version >= 7 && object["compactPixels"].toBool());
        auto mask   = decode(object["mask"].toString(), QImage::Format_Grayscale8,
                             version >= 7 && object["compactMask"].toBool());
        auto source = decode(object["source"].toString(), format);
        if (!decodeError.isEmpty())
            return fail(decodeError);
        if (((kind == "pixel" || kind == "text") && image.isNull()) ||
            (kind.startsWith("frequency") && source.isNull()) ||
            (!object["mask"].toString().isEmpty() && mask.isNull()))
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
    if (version >= 8 &&
        (!root["penPaths"].isArray() || !PenGeometry::decode(root["penPaths"].toArray(), penPaths)))
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
    QImage result = composite();
    if (!operationStep(1, 4))
        return false;
    if (suffix == "jpg" || suffix == "jpeg" || suffix == "bmp") {
        QImage white(result.size(), QImage::Format_RGB32);
        white.fill(Qt::white);
        QPainter p(&white);
        p.drawImage(0, 0, result);
        p.end();
        result = white;
    }
    result.setColorSpace(QColorSpace::SRgb);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());
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

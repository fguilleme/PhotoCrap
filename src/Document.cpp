#include "Document.h"
#include "ImageUtils.h"
#include <QBuffer>
#include <QClipboard>
#include <QColorSpace>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QThreadPool>
#include <QTimer>
#include <QUuid>
#include <QtConcurrent>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <utility>
#ifdef PHOTOCRAP_USE_ACCELERATE
#include <Accelerate/Accelerate.h>
#endif

namespace
{
constexpr qint64 MaxPixels        = 32LL * 1024 * 1024;
constexpr qint64 MaxDocumentBytes = 512LL * 1024 * 1024;
constexpr qint64 MaxProjectBytes  = 256LL * 1024 * 1024;
constexpr int    MaxLayers        = 64;
QString          localPath(const QUrl &_url)
{
    return _url.isLocalFile() ? _url.toLocalFile() : QString{};
}
} // namespace

Document::Document(QObject *_parent) : QObject(_parent)
{
    connect(&m_previewRenderWatcher, &QFutureWatcher<QImage>::finished, this, [this] {
        const QImage result  = m_previewRenderWatcher.result();
        const bool   current = m_runningPreviewGeneration == m_previewRequestGeneration;
        if (current && !result.isNull()) {
            QImage screenResult = result;
            if (!m_runningPreviewDraft && m_runningPreviewTargetSize.isValid()) {
                const QSize target = m_runningPreviewTargetSize.boundedTo(result.size());
                if (target != result.size())
                    screenResult =
                        result.scaled(target, Qt::IgnoreAspectRatio, Qt::FastTransformation);
                screenResult = screenResult.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            }
            {
                QMutexLocker locker(&m_previewImageMutex);
                if (m_runningPreviewDraft)
                    m_previewComposite = result;
                else {
                    m_composite        = result;
                    m_previewComposite = screenResult;
                }
            }
            if (!m_runningPreviewDraft) {
                m_cacheDirty         = false;
                m_fullCompositeDirty = false;
                m_dirtyRegion        = {};
            }
            ++m_revision;
            emit imageChanged();
        }
        if (m_previewRenderPending)
            QTimer::singleShot(0, this, &Document::startPendingPreviewRender);
        else if (current && m_runningPreviewDraft && m_runningPreviewRefine)
            requestPreviewRender(false);
        else
            m_previewRenderActive.store(false);
    });
    newDocument(1280, 800);
}
bool Document::fail(const QString &_message)
{
    emit errorOccurred(_message);
    return false;
}
bool Document::validSize(int _w, int _h, int _count)
{
    if (_w < 1 || _h < 1 || _w > 16384 || _h > 16384 || qint64(_w) * _h > MaxPixels)
        return fail(tr("Dimensions invalides : 1 à 16 384 px, 32 mégapixels maximum."));
    if (_count < 1 || _count > MaxLayers || qint64(_w) * _h * 4 * _count > MaxDocumentBytes)
        return fail(tr("Limite atteinte : 64 calques et 512 Mio de pixels par document."));
    return true;
}
bool Document::editable()
{
    if (m_active < 0 || m_active >= m_layers.size())
        return false;
    if (m_layers[m_active].locked)
        return fail(tr("Ce calque est verrouillé."));
    if (!m_layers[m_active].groupId.isEmpty())
        for (const auto &layer : std::as_const(m_layers))
            if (layer.kind == QStringLiteral("group") &&
                layer.groupId == m_layers[m_active].groupId && (layer.locked || !layer.visible))
                return fail(layer.locked ? tr("Ce groupe est verrouillé.")
                                         : tr("Ce groupe est masqué."));
    if (m_layers[m_active].kind != QStringLiteral("pixel") && !m_editingMask)
        return fail(tr("Sélectionnez le masque de ce calque pour peindre dessus."));
    return true;
}
Document::Snapshot Document::snapshot(const QString &_label) const
{
    return {m_layers,      m_size,           m_active,
            m_selection,   m_selectedLayers, m_layerSelectionAnchor,
            m_editingMask, m_stateId,        _label};
}
void Document::restore(const Snapshot &_state)
{
    const bool sizeChanged = m_size != _state.size;
    m_layers               = _state.layers;
    m_size                 = _state.size;
    m_active               = _state.active;
    m_selection            = _state.selection;
    m_selectedLayers       = _state.selectedLayers;
    m_layerSelectionAnchor = _state.layerSelectionAnchor;
    m_editingMask          = _state.editingMask;
    m_visibilityBeforeIsolation.clear();
    m_isolatedLayer = -1;
    m_stateId       = _state.id;
    changed();
    if (sizeChanged)
        emit canvasSizeChanged();
    emit documentChanged();
    emit selectionChanged();
    emit historyChanged();
}
void Document::trimHistory()
{
    // QImage snapshots share their storage until an edit detaches a layer.
    // Count unique storage, not theoretical copies, to bound retained history.
    while (m_undo.size() > 1) {
        QSet<qint64> seen;
        qint64       bytes = 0;
        for (const auto &state : m_undo)
            for (const auto &layer : state.layers) {
                for (const QImage *image : {&layer.image, &layer.mask, &layer.source})
                    if (!image->isNull() && !seen.contains(image->cacheKey())) {
                        seen.insert(image->cacheKey());
                        bytes += image->sizeInBytes();
                    }
            }
        if (m_undo.size() <= 40 && bytes <= 256LL * 1024 * 1024)
            break;
        m_undo.removeFirst();
    }
}
void Document::commit(const Snapshot &_before, const QString &_label, bool _invalidateFull,
                      bool _notifyImage)
{
    Snapshot state = _before;
    state.label    = _label;
    m_undo.append(state);
    m_redo.clear();
    m_stateId = m_nextId++;
    trimHistory();
    changed(_invalidateFull, _notifyImage);
    emit historyChanged();
}
void Document::edit(const QString &_label, const std::function<void()> &_operation)
{
    endStroke();
    auto before = snapshot(_label);
    _operation();
    commit(before, _label);
    if (before.size != m_size)
        emit canvasSizeChanged();
}
void Document::changed(bool _metadata, bool _notifyImage)
{
    if (m_maskOnlyView &&
        (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].mask.isNull()))
        m_maskOnlyView = false;
    ++m_previewRequestGeneration;
    m_previewRenderPending = false;
    m_previewRenderActive.store(false);
    {
        QMutexLocker locker(&m_previewImageMutex);
        m_previewComposite = {};
    }
    m_cacheDirty = true;
    if (_metadata)
        m_fullCompositeDirty = true;
    ++m_revision;
    if (_metadata) {
        ++m_thumbnailRevision;
        emit thumbnailsChanged();
        emit layersChanged();
    }
    if (_notifyImage)
        emit imageChanged();
}

void Document::setPreviewSize(int _width, int _height)
{
    if (_width < 1 || _height < 1)
        return;
    QSize target(_width, _height);
    if (target.width() > 1280 || target.height() > 1280)
        target.scale(1280, 1280, Qt::KeepAspectRatio);
    m_previewTargetSize = target;
}
void Document::resetHistory(bool _clean)
{
    m_undo.clear();
    m_redo.clear();
    m_stateId   = m_nextId++;
    m_savedId   = _clean ? m_stateId : 0;
    m_selection = {};
    selectOnly(m_active);
    m_drawing               = false;
    m_editingMask           = false;
    m_previewingLayer       = false;
    m_frequencyPreviewDraft = false;
    m_beforeStroke          = {};
    m_beforeLayerPreview    = {};
    m_strokeBase            = {};
    m_strokeCoverage        = {};
    m_strokeBounds          = {};
    m_frequencyCache.clear();
    m_fullCompositeDirty = true;
    m_dirtyRegion        = {};
    changed();
    emit canvasSizeChanged();
    emit documentChanged();
    emit selectionChanged();
    emit historyChanged();
}
QVariantList Document::layers() const
{
    QVariantList  result;
    QSet<QString> collapsedGroups;
    for (const auto &layer : m_layers)
        if (layer.kind == QStringLiteral("group") && layer.groupCollapsed)
            collapsedGroups.insert(layer.groupId);
    // UI order is top to bottom, storage order is bottom to top.
    for (int i = int(m_layers.size()) - 1; i >= 0; --i) {
        const auto &l = m_layers[i];
        if (l.kind != QStringLiteral("group") && collapsedGroups.contains(l.groupId))
            continue;
        result.append(QVariantMap{{"layerIndex", i},
                                  {"name", l.name},
                                  {"visible", l.visible},
                                  {"locked", l.locked},
                                  {"opacity", l.opacity},
                                  {"blend", l.blend},
                                  {"kind", l.kind},
                                  {"adjustmentType", l.adjustmentType},
                                  {"parameters", l.parameters},
                                  {"hasMask", !l.mask.isNull()},
                                  {"maskEnabled", l.maskEnabled},
                                  {"clippingMask", l.clippingMask},
                                  {"groupId", l.groupId},
                                  {"groupCollapsed", l.groupCollapsed},
                                  {"selected", m_selectedLayers.contains(i)},
                                  {"editingMask", m_editingMask && i == m_active}});
    }
    return result;
}
QVariantList Document::histogram() const
{
    const QImage     image = composite().convertToFormat(QImage::Format_ARGB32);
    QVector<quint64> bins(256, 0);
    const qint64     pixels  = qint64(image.width()) * image.height();
    const int        stride  = qMax(1, int(std::sqrt(double(pixels) / 262144.)));
    quint64          maximum = 0;
    for (int y = 0; y < image.height(); y += stride) {
        const auto row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); x += stride) {
            const QRgb pixel     = row[x];
            const int  luminance = qBound(
                0, qRound(.2126 * qRed(pixel) + .7152 * qGreen(pixel) + .0722 * qBlue(pixel)), 255);
            bins[luminance] += quint64(qAlpha(pixel));
            maximum = qMax(maximum, bins[luminance]);
        }
    }
    QVariantList result;
    result.reserve(256);
    for (const quint64 count : std::as_const(bins))
        result.append(maximum == 0 ? 0. : double(count) / double(maximum));
    return result;
}
QVariantMap Document::colorHistogram() const
{
    const QImage image = displayComposite().convertToFormat(QImage::Format_ARGB32);
    std::array<QVector<quint64>, 4> bins;
    for (auto &channel : bins)
        channel.fill(0, 256);
    const qint64 pixels  = qint64(image.width()) * image.height();
    const int    stride  = qMax(1, int(std::sqrt(double(pixels) / 65536.)));
    quint64      maximum = 0;
    for (int y = 0; y < image.height(); y += stride) {
        const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); x += stride) {
            const QRgb pixel     = row[x];
            const auto alpha     = quint64(qAlpha(pixel));
            const int  luminance = qBound(
                0, qRound(.2126 * qRed(pixel) + .7152 * qGreen(pixel) + .0722 * qBlue(pixel)), 255);
            bins[0][qRed(pixel)] += alpha;
            bins[1][qGreen(pixel)] += alpha;
            bins[2][qBlue(pixel)] += alpha;
            bins[3][luminance] += alpha;
            maximum = qMax(maximum, qMax(bins[3][luminance],
                                         qMax(bins[0][qRed(pixel)], qMax(bins[1][qGreen(pixel)],
                                                                         bins[2][qBlue(pixel)]))));
        }
    }
    const auto normalized = [&](const QVector<quint64> &_bins) {
        QVariantList values;
        values.reserve(256);
        for (const quint64 count : _bins)
            values.append(maximum == 0 ? 0. : double(count) / double(maximum));
        return values;
    };
    return {{QStringLiteral("red"), normalized(bins[0])},
            {QStringLiteral("green"), normalized(bins[1])},
            {QStringLiteral("blue"), normalized(bins[2])},
            {QStringLiteral("luminance"), normalized(bins[3])}};
}
QStringList Document::history() const
{
    QStringList result{tr("État initial")};
    for (const auto &state : m_undo)
        result.append(state.label);
    for (auto it = m_redo.crbegin(); it != m_redo.crend(); ++it)
        result.append(it->label);
    return result;
}
QImage Document::layerImage(int _index) const
{
    if (_index < 0 || _index >= m_layers.size())
        return {};
    const auto &layer = m_layers[_index];
    if (layer.kind == QStringLiteral("group")) {
        QImage icon(96, 72, QImage::Format_ARGB32_Premultiplied);
        icon.fill(QColor("#232b32"));
        QPainter painter(&icon);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#c7a95a"));
        painter.drawRoundedRect(QRect(14, 24, 68, 38), 5, 5);
        painter.drawRoundedRect(QRect(18, 15, 30, 18), 4, 4);
        return icon;
    }
    if (layer.kind.startsWith(QStringLiteral("frequency")))
        return frequencyImage(layer);
    if (layer.kind == QStringLiteral("adjustment")) {
        QImage icon(96, 72, QImage::Format_ARGB32_Premultiplied);
        icon.fill(QColor("#232b32"));
        QPainter        p(&icon);
        QLinearGradient g(0, 72, 96, 0);
        g.setColorAt(0, QColor("#273845"));
        g.setColorAt(1, QColor("#c5f2cf"));
        p.fillRect(icon.rect().adjusted(8, 8, -8, -8), g);
        p.setPen(QColor("#ffffff"));
        p.drawText(icon.rect(), Qt::AlignCenter, QStringLiteral("A"));
        return icon;
    }
    return layer.image;
}
QImage Document::maskImage(int _index) const
{
    return _index >= 0 && _index < m_layers.size() ? m_layers[_index].mask : QImage{};
}
void Document::setActiveLayer(int _index)
{
    if (_index < 0 || _index >= m_layers.size())
        return;
    endStroke();
    cancelLayerPreview();
    if (m_active != _index)
        m_maskOnlyView = false;
    m_active      = _index;
    m_editingMask = false;
    selectOnly(_index);
    emit layersChanged();
}
void Document::setDisplayChannel(int _channel)
{
    _channel = qBound(0, _channel, 3);
    if (m_displayChannel == _channel && !m_maskOnlyView)
        return;
    m_displayChannel          = _channel;
    m_maskOnlyView            = false;
    m_channelDisplayCache     = {};
    m_channelDisplaySourceKey = 0;
    emit displayChannelChanged();
    emit imageChanged();
}
void Document::toggleMaskOnlyView(int _index)
{
    if (_index < 0 || _index >= m_layers.size() || m_layers[_index].mask.isNull())
        return;
    const bool disable = m_maskOnlyView && m_active == _index;
    if (m_active != _index)
        setActiveLayer(_index);
    m_maskOnlyView = !disable;
    emit displayChannelChanged();
    emit imageChanged();
}
void Document::showCompositeView()
{
    if (!m_maskOnlyView)
        return;
    m_maskOnlyView = false;
    emit displayChannelChanged();
    emit imageChanged();
}
void Document::selectOnly(int _index)
{
    m_selectedLayers       = {_index};
    m_layerSelectionAnchor = _index;
}
int Document::selectedLayerCount() const
{
    return m_selectedLayers.size();
}
bool Document::canGroupSelected() const
{
    if (m_selectedLayers.size() < 2)
        return false;
    return std::all_of(m_selectedLayers.cbegin(), m_selectedLayers.cend(), [&](int _index) {
        return _index >= 0 && _index < m_layers.size() &&
               m_layers[_index].kind != QStringLiteral("group") &&
               m_layers[_index].groupId.isEmpty();
    });
}
bool Document::canUngroupActive() const
{
    return m_active >= 0 && m_active < m_layers.size() &&
           (m_layers[m_active].kind == QStringLiteral("group") ||
            !m_layers[m_active].groupId.isEmpty());
}
void Document::selectLayer(int _index, bool _toggle, bool _range)
{
    if (_index < 0 || _index >= m_layers.size())
        return;
    endStroke();
    cancelLayerPreview();
    if (_range && m_layerSelectionAnchor >= 0 && m_layerSelectionAnchor < m_layers.size()) {
        m_selectedLayers.clear();
        const int first = qMin(_index, m_layerSelectionAnchor);
        const int last  = qMax(_index, m_layerSelectionAnchor);
        for (int i = first; i <= last; ++i)
            m_selectedLayers.insert(i);
    } else if (_toggle) {
        if (m_selectedLayers.contains(_index) && m_selectedLayers.size() > 1)
            m_selectedLayers.remove(_index);
        else
            m_selectedLayers.insert(_index);
        m_layerSelectionAnchor = _index;
    } else
        selectOnly(_index);
    m_active      = _index;
    m_editingMask = false;
    emit layersChanged();
}
bool Document::newDocument(int _w, int _h, bool _transparent)
{
    if (!validSize(_w, _h))
        return false;
    m_size       = {_w, _h};
    QImage image = ImageUtils::blank(m_size);
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
    QImageReader reader(_path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (!size.isValid() || !validSize(size.width(), size.height())) {
        if (!size.isValid())
            fail(tr("Format non reconnu ou image illisible."));
        return {};
    }
    QImage image = reader.read();
    if (image.isNull()) {
        fail(reader.errorString());
        return {};
    }
    if (image.colorSpace().isValid())
        image.convertToColorSpace(QColorSpace::SRgb);
    else
        image.setColorSpace(QColorSpace::SRgb);
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
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
    m_size       = image.size();
    m_layers     = {{QFileInfo(path).completeBaseName(), image}};
    m_active     = 0;
    m_name       = QFileInfo(path).completeBaseName();
    m_projectUrl = QUrl{};
    resetHistory(false);
    return true;
}
bool Document::importLayer(const QUrl &_url)
{
    if (!validSize(width(), height(), int(m_layers.size()) + 1))
        return false;
    auto image = readImage(localPath(_url));
    if (image.isNull())
        return false;
    edit(tr("Importer un calque"), [&] {
        QImage   target = ImageUtils::blank(m_size);
        QPainter painter(&target);
        painter.drawImage((width() - image.width()) / 2, (height() - image.height()) / 2, image);
        painter.end();
        m_layers.insert(++m_active, {QFileInfo(_url.toLocalFile()).completeBaseName(), target});
        selectOnly(m_active);
    });
    return true;
}
bool Document::saveProject(const QUrl &_url)
{
    endStroke();
    QString path = localPath(_url);
    if (path.isEmpty())
        return fail(tr("Chemin de sauvegarde invalide."));
    if (!path.endsWith(".pcr", Qt::CaseInsensitive))
        path += ".pcr";
    QJsonArray layers;
    for (const auto &layer : m_layers) {
        auto encode = [&](const QImage &_image) -> QString {
            if (_image.isNull())
                return {};
            QByteArray bytes;
            QBuffer    buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            return _image.save(&buffer, "PNG") ? QString::fromLatin1(bytes.toBase64()) : QString{};
        };
        const QString png = encode(layer.image), mask = encode(layer.mask),
                      source = encode(layer.source);
        if ((!layer.image.isNull() && png.isEmpty()) || (!layer.mask.isNull() && mask.isEmpty()) ||
            (!layer.source.isNull() && source.isEmpty()))
            return fail(tr("Impossible d’encoder les données du calque."));
        layers.append(QJsonObject{{"name", layer.name},
                                  {"visible", layer.visible},
                                  {"locked", layer.locked},
                                  {"opacity", layer.opacity},
                                  {"blend", layer.blend},
                                  {"kind", layer.kind},
                                  {"adjustmentType", layer.adjustmentType},
                                  {"parameters", QJsonObject::fromVariantMap(layer.parameters)},
                                  {"png", png},
                                  {"mask", mask},
                                  {"maskEnabled", layer.maskEnabled},
                                  {"clippingMask", layer.clippingMask},
                                  {"groupId", layer.groupId},
                                  {"groupCollapsed", layer.groupCollapsed},
                                  {"source", source}});
    }
    QJsonObject      root{{"format", "PhotoCrap"},   {"version", 5},
                          {"width", width()},        {"height", height()},
                          {"activeLayer", m_active}, {"layers", layers}};
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (data.size() > MaxProjectBytes)
        return fail(tr("Le projet dépasse 256 Mio. Réduisez ses dimensions ou ses calques."));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        return fail(file.errorString());
    m_savedId    = m_stateId;
    m_projectUrl = QUrl::fromLocalFile(path);
    m_name       = QFileInfo(path).completeBaseName();
    emit documentChanged();
    emit historyChanged();
    emit statusMessage(tr("Projet enregistré"));
    return true;
}
bool Document::loadProject(const QString &_path)
{
    QFile file(_path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(file.errorString());
    if (file.size() > MaxProjectBytes)
        return fail(tr("Projet trop volumineux (256 Mio maximum)."));
    QJsonParseError error;
    const auto      json    = QJsonDocument::fromJson(file.readAll(), &error);
    const auto      root    = json.object();
    const int       version = root["version"].toInt();
    if (error.error != QJsonParseError::NoError || root["format"] != "PhotoCrap" ||
        (version < 1 || version > 5))
        return fail(tr("Projet PhotoCrap invalide ou version non prise en charge."));
    const int  w = root["width"].toInt(), h = root["height"].toInt();
    const auto array = root["layers"].toArray();
    if (!validSize(w, h, int(array.size())))
        return false;
    QVector<Layer> layers;
    for (const auto &value : array) {
        auto object = value.toObject();
        auto decode = [&](const QString &_encoded, QImage::Format _format) -> QImage {
            if (_encoded.isEmpty())
                return {};
            QByteArray bytes = QByteArray::fromBase64(_encoded.toLatin1(),
                                                      QByteArray::AbortOnBase64DecodingErrors);
            QBuffer    buffer(&bytes);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer, "PNG");
            if (reader.size() != QSize(w, h))
                return {};
            return reader.read().convertToFormat(_format);
        };
        const QString kind =
            version == 1 ? QStringLiteral("pixel") : object["kind"].toString("pixel");
        if (kind != "pixel" && kind != "adjustment" && kind != "group" && kind != "frequency" &&
            kind != "frequencyLow" && kind != "frequencyHigh")
            return fail(tr("Type de calque inconnu."));
        auto image  = decode(object["png"].toString(), QImage::Format_ARGB32_Premultiplied);
        auto mask   = decode(object["mask"].toString(), QImage::Format_Grayscale8);
        auto source = decode(object["source"].toString(), QImage::Format_ARGB32_Premultiplied);
        if ((kind == "pixel" && image.isNull()) ||
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
        Layer layer{object["name"].toString().left(256), image,   object["visible"].toBool(true),
                    object["locked"].toBool(),           opacity, blend};
        layer.kind           = kind;
        layer.adjustmentType = object["adjustmentType"].toString();
        layer.parameters     = object["parameters"].toObject().toVariantMap();
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
    // Only replace the current document after validating every layer.
    m_layers     = layers;
    m_size       = {w, h};
    m_active     = qBound(0, root["activeLayer"].toInt(), int(layers.size()) - 1);
    m_projectUrl = QUrl::fromLocalFile(_path);
    m_name       = QFileInfo(_path).completeBaseName();
    resetHistory(true);
    return true;
}
bool Document::exportImage(const QUrl &_url)
{
    endStroke();
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
    QImage result = composite();
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
    QImageWriter writer(&file, suffix.toLatin1());
    writer.setQuality(95);
    if (!writer.write(result))
        return fail(writer.errorString());
    if (!file.commit())
        return fail(file.errorString());
    emit statusMessage(tr("Image exportée"));
    return true;
}

void Document::addLayer()
{
    if (!validSize(width(), height(), int(m_layers.size()) + 1))
        return;
    edit(tr("Nouveau calque"), [&] {
        m_layers.insert(++m_active,
                        {tr("Calque %1").arg(m_layers.size() + 1), ImageUtils::blank(m_size)});
        selectOnly(m_active);
    });
}
void Document::duplicateLayer()
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind == QStringLiteral("group"))
        return;
    if (!validSize(width(), height(), int(m_layers.size()) + 1))
        return;
    edit(tr("Dupliquer le calque"), [&] {
        auto layer = m_layers[m_active];
        layer.name += tr(" — copie");
        m_layers.insert(++m_active, layer);
        selectOnly(m_active);
    });
}
void Document::removeLayer()
{
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].locked ||
        m_layers.size() == 1)
        return;
    edit(tr("Supprimer le calque"), [&] {
        if (m_layers[m_active].kind == QStringLiteral("group")) {
            const QString groupId = m_layers[m_active].groupId;
            for (int i = m_layers.size() - 1; i >= 0; --i)
                if (m_layers[i].groupId == groupId)
                    m_layers.removeAt(i);
        } else
            m_layers.removeAt(m_active);
        if (m_layers.isEmpty())
            m_layers.append({tr("Calque 1"), ImageUtils::blank(m_size)});
        m_active      = qMin(m_active, int(m_layers.size()) - 1);
        m_editingMask = false;
        selectOnly(m_active);
    });
}
void Document::moveLayer(int _direction)
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind == QStringLiteral("group"))
        return;
    const int to = m_active + (_direction > 0 ? 1 : -1);
    if (to < 0 || to >= m_layers.size())
        return;
    if (!m_layers[m_active].groupId.isEmpty() && m_layers[to].groupId != m_layers[m_active].groupId)
        return;
    edit(tr("Réordonner le calque"), [&] {
        m_layers.swapItemsAt(m_active, to);
        m_active = to;
        selectOnly(m_active);
    });
}
void Document::mergeDown()
{
    if (m_active < 1 || m_active >= m_layers.size())
        return;
    const auto &lower = m_layers[m_active - 1];
    const auto &upper = m_layers[m_active];
    if (lower.locked || upper.locked || lower.kind != "pixel" || upper.kind != "pixel" ||
        (lower.maskEnabled && !lower.mask.isNull()) ||
        (upper.maskEnabled && !upper.mask.isNull()) || !lower.visible || !upper.visible ||
        lower.blend != 0 || upper.blend != 0 || lower.groupId != upper.groupId) {
        fail(tr("La fusion nécessite deux calques visibles en mode Normal, non verrouillés."));
        return;
    }
    edit(tr("Fusionner vers le bas"), [&] {
        auto       &bottom = m_layers[m_active - 1];
        const auto &top    = m_layers[m_active];
        QImage      image  = ImageUtils::blank(m_size);
        QPainter    painter(&image);
        painter.setOpacity(bottom.opacity);
        painter.drawImage(0, 0, bottom.image);
        painter.setOpacity(top.opacity);
        painter.drawImage(0, 0, top.image);
        painter.end();
        bottom.image   = image;
        bottom.opacity = 1;
        m_layers.removeAt(m_active--);
        selectOnly(m_active);
    });
}
void Document::groupSelectedLayers()
{
    if (!canGroupSelected() || !validSize(width(), height(), int(m_layers.size()) + 1))
        return;
    edit(tr("Grouper les calques"), [&] {
        QList<int> indices = m_selectedLayers.values();
        std::sort(indices.begin(), indices.end());
        QVector<Layer> selected;
        selected.reserve(indices.size());
        for (const int index : std::as_const(indices))
            selected.append(m_layers[index]);
        for (auto it = indices.crbegin(); it != indices.crend(); ++it)
            m_layers.removeAt(*it);

        const QString groupId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const int insertion = qBound(0, indices.last() - indices.size() + 1, int(m_layers.size()));
        for (int i = 0; i < selected.size(); ++i) {
            selected[i].groupId = groupId;
            m_layers.insert(insertion + i, selected[i]);
        }
        const int groupCount =
            int(std::count_if(m_layers.cbegin(), m_layers.cend(), [](const Layer &_layer) {
                return _layer.kind == QStringLiteral("group");
            }));
        Layer group;
        group.name    = tr("Groupe %1").arg(groupCount + 1);
        group.kind    = QStringLiteral("group");
        group.groupId = groupId;
        m_active      = insertion + selected.size();
        m_layers.insert(m_active, group);
        m_editingMask = false;
        selectOnly(m_active);
    });
}
void Document::ungroupActiveLayers()
{
    if (!canUngroupActive())
        return;
    const QString groupId = m_layers[m_active].groupId;
    edit(tr("Sortir du groupe"), [&] {
        int header = -1;
        for (int i = 0; i < m_layers.size(); ++i) {
            if (m_layers[i].groupId != groupId)
                continue;
            if (m_layers[i].kind == QStringLiteral("group"))
                header = i;
            else
                m_layers[i].groupId.clear();
        }
        if (header >= 0)
            m_layers.removeAt(header);
        m_active      = qBound(0, header - 1, int(m_layers.size()) - 1);
        m_editingMask = false;
        selectOnly(m_active);
    });
}
void Document::toggleGroupCollapsed(int _index)
{
    if (_index < 0 || _index >= m_layers.size() || m_layers[_index].kind != QStringLiteral("group"))
        return;
    m_layers[_index].groupCollapsed = !m_layers[_index].groupCollapsed;
    selectOnly(_index);
    m_active      = _index;
    m_editingMask = false;
    emit layersChanged();
}
void Document::renameLayer(int _index, const QString &_name)
{
    if (_index < 0 || _index >= m_layers.size() || _name.trimmed().isEmpty() ||
        m_layers[_index].name == _name.trimmed())
        return;
    edit(tr("Renommer le calque"), [&] { m_layers[_index].name = _name.trimmed().left(256); });
}
void Document::setLayerVisible(int _i, bool _visible)
{
    if (_i < 0 || _i >= m_layers.size() || m_layers[_i].visible == _visible)
        return;
    m_visibilityBeforeIsolation.clear();
    m_isolatedLayer = -1;
    edit(tr("Visibilité du calque"), [&] { m_layers[_i].visible = _visible; });
}
void Document::toggleLayerIsolation(int _i)
{
    if (_i < 0 || _i >= m_layers.size())
        return;
    if (m_isolatedLayer == _i && m_visibilityBeforeIsolation.size() == m_layers.size()) {
        const QVector<bool> previousVisibility = m_visibilityBeforeIsolation;
        edit(tr("Restaurer la visibilité des calques"), [&] {
            for (int index = 0; index < m_layers.size(); ++index)
                m_layers[index].visible = previousVisibility[index];
        });
        m_visibilityBeforeIsolation.clear();
        m_isolatedLayer = -1;
        return;
    }

    QVector<bool> previousVisibility;
    previousVisibility.reserve(m_layers.size());
    for (const Layer &layer : std::as_const(m_layers))
        previousVisibility.append(layer.visible);
    const QString targetGroup   = m_layers[_i].groupId;
    const bool    targetIsGroup = m_layers[_i].kind == QStringLiteral("group");
    const QString targetGroupId = targetIsGroup ? m_layers[_i].groupId : QString{};
    edit(tr("Isoler le calque"), [&] {
        for (int index = 0; index < m_layers.size(); ++index) {
            Layer &layer = m_layers[index];
            if (targetIsGroup)
                layer.visible = index == _i || (layer.groupId == targetGroupId &&
                                                layer.kind != QStringLiteral("group") &&
                                                previousVisibility[index]);
            else
                layer.visible =
                    index == _i || (layer.kind == QStringLiteral("group") &&
                                    !targetGroup.isEmpty() && layer.groupId == targetGroup);
        }
    });
    m_visibilityBeforeIsolation = previousVisibility;
    m_isolatedLayer             = _i;
}
void Document::setLayerLocked(int _i, bool _locked)
{
    if (_i < 0 || _i >= m_layers.size() || m_layers[_i].locked == _locked)
        return;
    edit(tr("Verrouillage du calque"), [&] { m_layers[_i].locked = _locked; });
}
void Document::setLayerOpacity(int _i, qreal _opacity)
{
    if (_i < 0 || _i >= m_layers.size() || m_layers[_i].kind == QStringLiteral("group") ||
        !std::isfinite(_opacity))
        return;
    _opacity = qBound(0., _opacity, 1.);
    if (qFuzzyCompare(m_layers[_i].opacity, _opacity))
        return;
    edit(tr("Opacité du calque"), [&] { m_layers[_i].opacity = _opacity; });
}
void Document::setLayerBlend(int _i, int _blend)
{
    if (_i < 0 || _i >= m_layers.size() || m_layers[_i].kind == QStringLiteral("group") ||
        _blend < 0 || _blend > 26 || m_layers[_i].blend == _blend)
        return;
    edit(tr("Mode de fusion"), [&] { m_layers[_i].blend = _blend; });
}
void Document::previewLayerBlend(int _i, int _blend)
{
    if (_i < 0 || _i >= m_layers.size() || m_layers[_i].kind == QStringLiteral("group") ||
        _blend < 0 || _blend > 26)
        return;
    if (!m_previewingLayer)
        beginLayerPreview();
    if (m_layers[_i].blend == _blend)
        return;
    m_layers[_i].blend    = _blend;
    m_layerPreviewChanged = true;
    m_fullCompositeDirty  = true;
    m_cacheDirty          = true;
    requestPreviewRender(true);
}
void Document::setClippingMask(bool _enabled)
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind == QStringLiteral("group") ||
        m_layers[m_active].clippingMask == _enabled || (_enabled && m_active == 0))
        return;
    edit(_enabled ? tr("Créer un masque d’écrêtage") : tr("Libérer le masque d’écrêtage"),
         [&] { m_layers[m_active].clippingMask = _enabled; });
}
void Document::addLayerMask(bool _revealAll)
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind == QStringLiteral("group"))
        return;
    if (!m_layers[m_active].mask.isNull()) {
        m_editingMask = true;
        emit layersChanged();
        return;
    }
    edit(tr("Ajouter un masque de fusion"), [&] {
        QImage mask(m_size, QImage::Format_Grayscale8);
        mask.fill(hasSelection() ? (_revealAll ? 0 : 255) : (_revealAll ? 255 : 0));
        if (hasSelection()) {
            QPainter painter(&mask);
            painter.fillRect(m_selection, _revealAll ? Qt::white : Qt::black);
        }
        m_layers[m_active].mask        = mask;
        m_layers[m_active].maskEnabled = true;
        m_layers[m_active].parameters.insert("_maskDensity", 100.);
        m_layers[m_active].parameters.insert("_maskFeather", 0.);
        m_editingMask = true;
    });
}
void Document::copyLayerMask(int _sourceIndex, int _targetIndex)
{
    if (_sourceIndex < 0 || _sourceIndex >= m_layers.size() || _targetIndex < 0 ||
        _targetIndex >= m_layers.size() || _sourceIndex == _targetIndex ||
        m_layers[_sourceIndex].mask.isNull() ||
        m_layers[_targetIndex].kind == QStringLiteral("group") || m_layers[_targetIndex].locked)
        return;
    edit(tr("Copier le masque de fusion"), [&] {
        const auto &source                = m_layers[_sourceIndex];
        auto       &target                = m_layers[_targetIndex];
        target.mask                       = source.mask;
        target.maskEnabled                = source.maskEnabled;
        target.parameters["_maskDensity"] = source.parameters.value("_maskDensity", 100.);
        target.parameters["_maskFeather"] = source.parameters.value("_maskFeather", 0.);
        m_active                          = _targetIndex;
        m_editingMask                     = true;
        m_maskOnlyView                    = false;
        selectOnly(m_active);
    });
}
void Document::removeLayerMask()
{
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].mask.isNull())
        return;
    edit(tr("Supprimer le masque de fusion"), [&] {
        m_layers[m_active].mask = {};
        m_editingMask           = false;
    });
}
void Document::invertLayerMask()
{
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].mask.isNull())
        return;
    edit(tr("Inverser le masque de fusion"), [&] { m_layers[m_active].mask.invertPixels(); });
}
void Document::setMaskEnabled(bool _enabled)
{
    if (m_active < 0 || m_active >= m_layers.size() || m_layers[m_active].mask.isNull() ||
        m_layers[m_active].maskEnabled == _enabled)
        return;
    edit(tr("Activer le masque de fusion"), [&] { m_layers[m_active].maskEnabled = _enabled; });
}
void Document::setEditingMask(bool _enabled)
{
    if (m_active < 0 || m_active >= m_layers.size())
        return;
    _enabled = _enabled && !m_layers[m_active].mask.isNull();
    if (m_editingMask == _enabled)
        return;
    endStroke();
    m_editingMask = _enabled;
    emit layersChanged();
}
void Document::createDemo()
{
    newDocument(1440, 1000);
    auto &background = m_layers[0];
    background.name  = tr("Fond · bleu nuit");
    QPainter        bg(&background.image);
    QLinearGradient gradient(0, 0, 1440, 1000);
    gradient.setColorAt(0, QColor("#0e2632"));
    gradient.setColorAt(1, QColor("#30424c"));
    bg.fillRect(background.image.rect(), gradient);
    bg.end();
    QImage   orb = ImageUtils::blank(m_size);
    QPainter p(&orb);
    p.setRenderHint(QPainter::Antialiasing);
    QRadialGradient sphere(QPointF(1010, 405), 350, QPointF(910, 270));
    sphere.setColorAt(0, QColor("#edffe0"));
    sphere.setColorAt(.36, QColor("#b3ecaf"));
    sphere.setColorAt(.7, QColor("#70bda5"));
    sphere.setColorAt(1, QColor("#28595c"));
    p.setBrush(sphere);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(1010, 405), 330, 330);
    p.setPen(QPen(QColor(213, 255, 224, 75), 1.5));
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 8; ++i)
        p.drawEllipse(QPointF(1010, 405), 350 + i * 17, 350 + i * 17);
    p.end();
    m_layers.append({tr("Sphère · lumière"), orb});
    QImage   typography = ImageUtils::blank(m_size);
    QPainter t(&typography);
    t.setRenderHint(QPainter::TextAntialiasing);
    QFont font("sans-serif");
    font.setPixelSize(19);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 5);
    t.setFont(font);
    t.setPen(QColor("#bee8ce"));
    t.drawText(QPointF(88, 106), QStringLiteral("PHOTOGRAPHIE  /  DESIGN  /  EXPLORATION"));
    font.setPixelSize(146);
    font.setLetterSpacing(QFont::AbsoluteSpacing, -6);
    font.setWeight(QFont::Bold);
    t.setFont(font);
    t.setPen(QColor("#f2f2e7"));
    t.drawText(QPointF(78, 390), QStringLiteral("Make room"));
    t.drawText(QPointF(78, 552), QStringLiteral("for wonder."));
    font.setPixelSize(25);
    font.setWeight(QFont::Normal);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0);
    t.setFont(font);
    t.setPen(QColor("#afc4c8"));
    t.drawText(QPointF(88, 646), tr("Une idée. Quelques calques. Des possibilités infinies."));
    t.setPen(QPen(QColor("#91b8a7"), 2));
    t.drawLine(QPointF(88, 856), QPointF(1352, 856));
    font.setPixelSize(19);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    t.setFont(font);
    t.drawText(QPointF(88, 912), QStringLiteral("STUDIO EXPÉRIMENTAL"));
    t.drawText(QPointF(1162, 912), QStringLiteral("VOL. 001"));
    t.end();
    m_layers.append({tr("Composition · typographie"), typography});
    m_layers.append({tr("Vos retouches"), ImageUtils::blank(m_size)});
    m_active = 3;
    selectOnly(m_active);
    m_name = tr("Exploration créative");
    resetHistory(false);
}

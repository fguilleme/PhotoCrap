#include "CanvasItem.h"
#include "render/ToneMappingNode.h"

#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QSet>
#include <QtMath>
#include <cmath>

namespace
{
constexpr int tileSize = 1024;
using PhotoCrap::Filters::ToneMappingSettings;

QImage displaySource(Document *_document)
{
    return !_document                          ? QImage{}
           : _document->gpuTonePreviewActive() ? _document->gpuTonePreviewBase()
                                               : _document->displayComposite();
}

class PhotoTileNode : public QSGTransformNode
{
public:
    QQuickWindow    *owner;
    QSGTexture      *texture;
    QSGImageNode    *image   = nullptr;
    ToneMappingNode *mapping = nullptr;
    QRectF           sourceRect;
    PhotoTileNode(QQuickWindow *_window, const QImage &_image, QSGTexture *_table,
                  const ToneMappingSettings &_settings)
        : owner(_window), texture(_window->createTextureFromImage(
                              _image.convertToFormat(QImage::Format_ARGB32_Premultiplied)))
    {
        if (texture)
            setMapping(_table, _settings);
    }
    ~PhotoTileNode() override
    {
        while (auto *child = firstChild()) {
            removeChildNode(child);
            delete child;
        }
        delete texture;
    }
    void setMapping(QSGTexture *_table, const ToneMappingSettings &_settings)
    {
        if (_table) {
            if (image) {
                removeChildNode(image);
                delete image;
                image = nullptr;
            }
            if (!mapping) {
                mapping = new ToneMappingNode(texture, _table, _settings);
                appendChildNode(mapping);
            } else
                mapping->setTable(_table, _settings);
        } else {
            if (mapping) {
                removeChildNode(mapping);
                delete mapping;
                mapping = nullptr;
            }
            if (!image) {
                image = owner->createImageNode();
                if (image) {
                    image->setTexture(texture);
                    appendChildNode(image);
                }
            }
        }
    }
    void setRect(const QRectF &_rect)
    {
        if (mapping)
            mapping->setImageRect(_rect, sourceRect);
        else if (image) {
            image->setSourceRect(sourceRect);
            image->setRect(_rect);
        }
    }
    void setFiltering(QSGTexture::Filtering _filtering)
    {
        if (mapping)
            mapping->setFiltering(_filtering);
        else if (image)
            image->setFiltering(_filtering);
    }
};

// All nodes and textures live and die on the scene graph thread. No Document
// methods are called here: updatePolish prepares immutable GUI-thread snapshots.
struct CanvasNode : QSGTransformNode {
    QSGTransformNode                background, photo, effects;
    QHash<quint64, PhotoTileNode *> tiles;
    QQuickWindow                   *owner    = nullptr;
    qint64                          imageKey = 0, overlayKey = 0;
    QSize                           checkerSize;
    QPoint                          checkerPhase;
    QSGImageNode                   *checker = nullptr, *overlay = nullptr;
    QSGSimpleTextureNode           *repeatedChecker = nullptr;
    QSGTexture                     *toneTable       = nullptr;
    qint64                          toneTableKey    = 0;
    ToneMappingSettings             toneSettings;

    ~CanvasNode() override
    {
        photo.removeAllChildNodes();
        qDeleteAll(tiles);
        delete toneTable;
    }

    explicit CanvasNode(QQuickWindow *_window) : owner(_window)
    {
        // Explicit transforms preserve the inherited item matrix when texture
        // children are replaced, including with the software scene graph.
        // Member nodes are not owned by their parent's child list.
        background.setFlag(OwnedByParent, false);
        photo.setFlag(OwnedByParent, false);
        effects.setFlag(OwnedByParent, false);
        appendChildNode(&background);
        appendChildNode(&photo);
        appendChildNode(&effects);
    }
};

QSGImageNode *imageNode(QQuickWindow *_window, const QImage &_image)
{
    // Match the previous 8-bit display surface without requiring high-depth
    // texture support. Document pixels and exports retain 16-bit precision.
    auto *texture = _window->createTextureFromImage(
        _image.convertToFormat(QImage::Format_ARGB32_Premultiplied));
    if (!texture)
        return nullptr;
    auto *node = _window->createImageNode();
    if (!node) {
        delete texture;
        return nullptr;
    }
    node->setOwnsTexture(true);
    node->setTexture(texture);
    node->setSourceRect(QRectF(_image.rect()));
    return node;
}

void removeImage(QSGNode &_parent, QSGImageNode *&_node)
{
    if (_node) {
        _parent.removeChildNode(_node);
        delete _node;
        _node = nullptr;
    }
}

QRectF visibleBounds(const QRectF &_requested, const QRectF &_bounds)
{
    const auto rect = _requested.width() < 0 ? _bounds : _requested.intersected(_bounds);
    return rect.isEmpty() ? QRectF{} : QRectF(rect.toAlignedRect()).intersected(_bounds);
}
} // namespace

void CanvasItem::queueFrame(bool _image, bool _overlay)
{
    m_imageDirty |= _image;
    m_overlayDirty |= _overlay;
    polish();
}

void CanvasItem::setViewportRect(const QRectF &_rect)
{
    if (m_viewportRect == _rect)
        return;
    m_viewportRect = _rect;
    queueFrame(false, true);
    emit viewportRectChanged();
}

void CanvasItem::geometryChange(const QRectF &_newGeometry, const QRectF &_oldGeometry)
{
    QQuickItem::geometryChange(_newGeometry, _oldGeometry);
    if (_newGeometry.size() != _oldGeometry.size())
        queueFrame(false, true);
}

void CanvasItem::itemChange(ItemChange _change, const ItemChangeData &_value)
{
    QQuickItem::itemChange(_change, _value);
    if (_change == ItemDevicePixelRatioHasChanged)
        queueFrame(false, true);
}

void CanvasItem::updatePolish()
{
    QImage source;
    bool   changed = false;
    if (m_imageDirty) {
        source         = displaySource(m_document);
        m_toneTable    = m_document && m_document->gpuTonePreviewActive()
                             ? m_document->gpuTonePreviewTable()
                             : QImage{};
        m_toneSettings = m_document ? m_document->gpuTonePreviewSettings() : ToneMappingSettings{};
        changed        = m_textureSourceKey != source.cacheKey();
        m_textureSourceKey = source.cacheKey();
        m_sourceSize       = source.size();
        m_documentSize = m_document ? QSize(m_document->width(), m_document->height()) : QSize{};
        m_imageDirty   = false;
    }
    // Retaining the full composite would force Document's next regional edit
    // to detach/copy the whole image. Keep only independent visible tile copies
    // and, at reduced zoom, one detached display-resolution pyramid level.
    if (m_sourceSize.isEmpty() || width() <= 0 || height() <= 0) {
        m_textureImage = {};
        m_tileImages.clear();
        m_textureSize      = {};
        m_textureLevel     = -1;
        m_selectionOutline = {};
        m_selectionPath    = {};
    } else {
        const qreal ratio = window() ? window()->effectiveDevicePixelRatio() : 1.;
        const qreal scale =
            qMin(width() * ratio / m_sourceSize.width(), height() * ratio / m_sourceSize.height());
        const int level = qBound(0, int(std::floor(std::log2(1. / scale))), 30);
        if (changed || m_textureLevel != level) {
            m_textureSize  = QSize(qMax(1, m_sourceSize.width() >> level),
                                   qMax(1, m_sourceSize.height() >> level));
            m_textureImage = {};
            if (level > 0) {
                if (source.isNull() && m_document)
                    source = displaySource(m_document);
                m_textureImage =
                    source.scaled(m_textureSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
            m_tileImages.clear();
            m_textureLevel = level;
            ++m_textureRevision;
        }
        const QRectF visible = visibleBounds(m_viewportRect, boundingRect());
        const qreal  sx = width() / m_textureSize.width(), sy = height() / m_textureSize.height();
        const QRect  imageRect(QPoint{}, m_textureSize);
        const QRect  region =
            QRectF(visible.x() / sx, visible.y() / sy, visible.width() / sx, visible.height() / sy)
                .toAlignedRect()
                .intersected(imageRect);
        QSet<quint64> used;
        if (!region.isEmpty()) {
            for (int y = region.top() / tileSize; y <= region.bottom() / tileSize; ++y)
                for (int x = region.left() / tileSize; x <= region.right() / tileSize; ++x) {
                    const quint64 key = (quint64(quint32(y)) << 32) | quint32(x);
                    used.insert(key);
                    if (m_tileImages.contains(key))
                        continue;
                    const QRect tile   = QRect(x * tileSize, y * tileSize, tileSize, tileSize)
                                             .intersected(imageRect);
                    const QRect padded = tile.adjusted(-1, -1, 1, 1).intersected(imageRect);
                    if (level == 0) {
                        if (source.isNull() && m_document)
                            source = displaySource(m_document);
                        m_tileImages.insert(key, source.copy(padded));
                    } else
                        m_tileImages.insert(key, m_textureImage.copy(padded));
                }
        }
        for (auto it = m_tileImages.begin(); it != m_tileImages.end();) {
            if (!used.contains(it.key()))
                it = m_tileImages.erase(it);
            else
                ++it;
        }
    }
    if (m_overlayDirty) {
        m_overlayDirty = false;
        m_overlayImage = {};
        m_overlayRect  = visibleBounds(m_viewportRect, boundingRect());
        if (m_document && !m_overlayRect.isEmpty() && m_document->width() > 0 &&
            m_document->height() > 0) {
            const bool patch =
                m_document->patchDragging() && !m_document->patchPreviewImage().isNull();
            const bool remove    = m_document->removeHasPending();
            const bool selection = m_document->selectionShape() >= 6 &&
                                   m_document->hasSelection() &&
                                   m_document->selectionEdgesVisible();
            if (patch || remove || selection) {
                const qreal ratio = window() ? window()->effectiveDevicePixelRatio() : 1.;
                m_overlayImage    = QImage(QSize(qCeil(m_overlayRect.width() * ratio),
                                                 qCeil(m_overlayRect.height() * ratio)),
                                           QImage::Format_ARGB32_Premultiplied);
                if (!m_overlayImage.isNull()) {
                    m_overlayImage.setDevicePixelRatio(ratio);
                    m_overlayImage.fill(Qt::transparent);
                    QPainter painter(&m_overlayImage);
                    painter.translate(-m_overlayRect.topLeft());
                    painter.scale(width() / m_document->width(), height() / m_document->height());
                    if (patch) {
                        painter.save();
                        painter.setClipRegion(m_document->patchTargetRegion());
                        painter.drawImage(m_document->patchTargetRect(),
                                          m_document->patchPreviewImage());
                        painter.restore();
                    }
                    if (remove) {
                        painter.save();
                        painter.setClipRegion(m_document->removeRegion());
                        painter.fillRect(m_document->removeRegion().boundingRect(),
                                         QColor(235, 70, 180, 110));
                        painter.restore();
                    }
                    if (selection) {
                        const QRegion outline = m_document->selectionOutline();
                        if (m_selectionOutline != outline) {
                            m_selectionOutline = outline;
                            m_selectionPath    = {};
                            m_selectionPath.setFillRule(Qt::WindingFill);
                            m_selectionPath.addRegion(outline);
                            m_selectionPath = m_selectionPath.simplified();
                        }
                        QPen pen(Qt::black, 1.5, Qt::DashLine, Qt::SquareCap, Qt::MiterJoin);
                        pen.setCosmetic(true);
                        pen.setDashPattern({5, 5});
                        pen.setDashOffset(m_selectionPhase);
                        painter.setPen(pen);
                        painter.drawPath(m_selectionPath);
                        pen.setColor(Qt::white);
                        pen.setDashOffset(m_selectionPhase + 5);
                        painter.setPen(pen);
                        painter.drawPath(m_selectionPath);
                    }
                }
            }
        }
    }
    update();
}

QSGNode *CanvasItem::updatePaintNode(QSGNode *_oldNode, UpdatePaintNodeData *)
{
    auto        *node    = static_cast<CanvasNode *>(_oldNode);
    const QRectF visible = visibleBounds(m_viewportRect, boundingRect());
    if (!window() || m_textureSize.isEmpty() || visible.isEmpty()) {
        delete node;
        return nullptr;
    }
    if (node && node->owner != window()) {
        delete node;
        node = nullptr;
    }
    if (!node)
        node = new CanvasNode(window());

    // Hardware repeats one small texture. The software fallback only rasterizes
    // the visible background, independently of the photo and selection overlay.
    const bool repeatChecker =
        QSGRendererInterface::isApiRhiBased(window()->rendererInterface()->graphicsApi());
    const QSize checkerSize =
        repeatChecker ? QSize(24, 24) : QSize(qCeil(visible.width()), qCeil(visible.height()));
    const QPoint checkerPhase =
        repeatChecker ? QPoint{} : QPoint(int(visible.x()) % 24, int(visible.y()) % 24);
    if (repeatChecker && !node->repeatedChecker) {
        QImage checker(24, 24, QImage::Format_RGB32);
        checker.fill(QColor("#d6d6d6"));
        QPainter painter(&checker);
        painter.fillRect(0, 0, 12, 12, QColor("#eeeeee"));
        painter.fillRect(12, 12, 12, 12, QColor("#eeeeee"));
        painter.end();
        if (auto *texture = window()->createTextureFromImage(checker)) {
            texture->setHorizontalWrapMode(QSGTexture::Repeat);
            texture->setVerticalWrapMode(QSGTexture::Repeat);
            node->repeatedChecker = new QSGSimpleTextureNode;
            node->repeatedChecker->setOwnsTexture(true);
            node->repeatedChecker->setTexture(texture);
            // The material owns sampler state; setting the texture alone is
            // overwritten when Qt binds the material on RHI backends.
            for (auto *material :
                 {node->repeatedChecker->material(), node->repeatedChecker->opaqueMaterial()}) {
                if (material) {
                    auto *sampler = static_cast<QSGOpaqueTextureMaterial *>(material);
                    sampler->setHorizontalWrapMode(QSGTexture::Repeat);
                    sampler->setVerticalWrapMode(QSGTexture::Repeat);
                }
            }
            node->background.appendChildNode(node->repeatedChecker);
        }
    }
    if (node->repeatedChecker) {
        node->repeatedChecker->setRect(visible);
        node->repeatedChecker->setSourceRect(visible);
    }
    if (!repeatChecker && (!node->checker || node->checkerSize != checkerSize ||
                           node->checkerPhase != checkerPhase)) {
        removeImage(node->background, node->checker);
        QImage checker(checkerSize, QImage::Format_RGB32);
        checker.fill(QColor("#d6d6d6"));
        QPainter painter(&checker);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#eeeeee"));
        for (int y = -checkerPhase.y(); y < checker.height(); y += 24)
            for (int x = -checkerPhase.x(); x < checker.width(); x += 24) {
                painter.drawRect(x, y, 12, 12);
                painter.drawRect(x + 12, y + 12, 12, 12);
            }
        painter.end();
        node->checker = imageNode(window(), checker);
        if (node->checker) {
            node->background.appendChildNode(node->checker);
        }
        node->checkerSize  = checkerSize;
        node->checkerPhase = checkerPhase;
    }
    if (node->checker) {
        node->checker->setRect(visible);
        node->checker->setSourceRect(QRectF(QPointF{}, checkerSize));
    }

    if (node->toneTableKey != m_toneTable.cacheKey() || node->toneSettings != m_toneSettings) {
        const bool  newTable = node->toneTableKey != m_toneTable.cacheKey();
        QSGTexture *table = !newTable              ? node->toneTable
                            : m_toneTable.isNull() ? nullptr
                                                   : window()->createTextureFromImage(m_toneTable);
        if (!m_toneTable.isNull() && !table) {
            QMetaObject::invokeMethod(
                this, [this] { setGpuTonePreviewEnabled(false); }, Qt::QueuedConnection);
            return node;
        }
        for (auto *tile : std::as_const(node->tiles))
            tile->setMapping(table, m_toneSettings);
        if (newTable)
            delete node->toneTable;
        node->toneTable    = table;
        node->toneTableKey = m_toneTable.cacheKey();
        node->toneSettings = m_toneSettings;
#ifdef PHOTOCRAP_TESTING
        if (table && newTable)
            ++m_toneTableUploads;
#endif
    }
    if (node->imageKey != m_textureRevision) {
        node->photo.removeAllChildNodes();
        qDeleteAll(node->tiles);
        node->tiles.clear();
        node->imageKey = m_textureRevision;
    }
    const qreal sx = width() / m_textureSize.width(), sy = height() / m_textureSize.height();
    const QRect sourceVisible =
        QRectF(visible.x() / sx, visible.y() / sy, visible.width() / sx, visible.height() / sy)
            .toAlignedRect()
            .intersected(QRect(QPoint{}, m_textureSize));
    QSet<quint64> used;
    for (int y = sourceVisible.top() / tileSize; y <= sourceVisible.bottom() / tileSize; ++y) {
        for (int x = sourceVisible.left() / tileSize; x <= sourceVisible.right() / tileSize; ++x) {
            const quint64 key    = (quint64(quint32(y)) << 32) | quint32(x);
            const QRect   source = QRect(x * tileSize, y * tileSize, tileSize, tileSize)
                                       .intersected(QRect(QPoint{}, m_textureSize));
            // One neighbour pixel prevents filtering seams between adjacent tiles.
            const QRect padded =
                source.adjusted(-1, -1, 1, 1).intersected(QRect(QPoint{}, m_textureSize));
            auto *tile = node->tiles.value(key, nullptr);
            if (!tile) {
                tile = new PhotoTileNode(window(), m_tileImages.value(key), node->toneTable,
                                         node->toneSettings);
                if (!tile->texture) {
                    delete tile;
                    continue;
                }
                tile->sourceRect = QRectF(source.translated(-padded.topLeft()));
                node->photo.appendChildNode(tile);
                node->tiles.insert(key, tile);
#ifdef PHOTOCRAP_TESTING
                ++m_imageTextureUploads;
#endif
            }
            used.insert(key);
            tile->setRect(QRectF(source.x() * sx, source.y() * sy, source.width() * sx,
                                 source.height() * sy));
            tile->setFiltering(width() < m_documentSize.width() ? QSGTexture::Linear
                                                                : QSGTexture::Nearest);
        }
    }
    for (auto it = node->tiles.begin(); it != node->tiles.end();) {
        if (!used.contains(it.key())) {
            node->photo.removeChildNode(it.value());
            delete it.value();
            it = node->tiles.erase(it);
        } else
            ++it;
    }
    const qint64 overlayKey = m_overlayImage.cacheKey();
    if (node->overlayKey != overlayKey) {
        removeImage(node->effects, node->overlay);
        if (!m_overlayImage.isNull()) {
            node->overlay = imageNode(window(), m_overlayImage);
            if (node->overlay)
                node->effects.appendChildNode(node->overlay);
        }
        node->overlayKey = overlayKey;
    }
    if (node->overlay) {
        node->overlay->setRect(m_overlayRect);
        node->overlay->setFiltering(QSGTexture::Nearest);
    }
    return node;
}

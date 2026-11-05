#pragma once
#include "Document.h"
#include "render/GraphicsBudget.h"
#include "render/PatchMesh.h"
#include "render/RegionMesh.h"
#include "render/SelectionMesh.h"
#include <QPainterPath>
#include <QPointer>
#include <QQuickItem>
#ifdef PHOTOCRAP_TESTING
#include <atomic>
#endif

class CanvasItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(Document *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(qreal selectionPhase READ selectionPhase WRITE setSelectionPhase NOTIFY
                   selectionPhaseChanged)
    Q_PROPERTY(
        QRectF viewportRect READ viewportRect WRITE setViewportRect NOTIFY viewportRectChanged)
    Q_PROPERTY(bool gpuTonePreviewEnabled READ gpuTonePreviewEnabled WRITE setGpuTonePreviewEnabled)
    Q_PROPERTY(bool gpuStackEnabled READ gpuStackEnabled WRITE setGpuStackEnabled)
    Q_PROPERTY(int textureCacheMiB READ textureCacheMiB WRITE setTextureCacheMiB NOTIFY
                   textureCacheChanged)
public:
    explicit CanvasItem(QQuickItem *_parent = nullptr);
    ~CanvasItem() override;
    Document *document() const
    {
        return m_document;
    }
    void  setDocument(Document *_document);
    qreal selectionPhase() const
    {
        return m_selectionPhase;
    }
    void             setSelectionPhase(qreal _phase);
    Q_INVOKABLE void setSystemCursor(int _shape, bool _active, const QString &_tool,
                                     int _selectionMode, bool _sourcePicking = false,
                                     bool _precise = false);
    QRectF           viewportRect() const
    {
        return m_viewportRect;
    }
    void setViewportRect(const QRectF &_rect);
    bool gpuTonePreviewEnabled() const
    {
        return m_gpuTonePreviewEnabled;
    }
    void setGpuTonePreviewEnabled(bool _enabled);
    bool gpuStackEnabled() const
    {
        return m_gpuStackEnabled;
    }
    void                    setGpuStackEnabled(bool enabled);
    Q_INVOKABLE QVariantMap graphicsMemoryUsage() const;
    int                     textureCacheMiB() const
    {
        return m_textureCacheMiB;
    }
    void setTextureCacheMiB(int _value);
#ifdef PHOTOCRAP_TESTING
    quint64 gpuReductionPasses() const
    {
        return m_stackStatistics->reductions.load();
    }
    quint64 stackTextureUploads() const
    {
        return m_stackStatistics->uploads.load();
    }
    quint64 stackPasses() const
    {
        return m_stackStatistics->passes.load();
    }
    quint64 stackTextureBytes() const
    {
        return m_stackStatistics->bytes.load();
    }
    quint64 stackSnapshotPreparations() const
    {
        return m_stackSnapshotPreparations;
    }
    bool gpuStackActive() const
    {
        return m_gpuStackActive.load();
    }
    quint64 framePreparations() const
    {
        return m_framePreparations;
    }
    quint64 phaseOnlyPreparations() const
    {
        return m_phaseOnlyPreparations;
    }
    quint64 imageTextureUploads() const
    {
        return m_imageTextureUploads.load();
    }
    quint64 toneTableUploads() const
    {
        return m_toneTableUploads.load();
    }
    quint64 maskTextureUploads() const
    {
        return m_maskTextureUploads.load();
    }
    QSize overlayPixelSize() const
    {
        return m_overlayImage.size();
    }
    quint64 displayLevelSourcePixels() const
    {
        return m_displayLevelSourcePixels;
    }
    quint64 residentTextureBytes() const
    {
        return m_gpuStackActive.load() ? m_stackStatistics->bytes.load()
                                       : m_residentTextureBytes.load();
    }
    quint64 selectionGeometryUploads() const
    {
        return m_selectionGeometryUploads.load();
    }
    quint64 overlayTextureUploads() const
    {
        return m_overlayTextureUploads.load();
    }
    bool selectionShaderActive() const
    {
        return m_gpuSelection;
    }
    qsizetype selectionVertexCount() const
    {
        return m_selectionVertices.size();
    }
    bool removeOverlayGpuActive() const
    {
        return m_gpuRemove;
    }
    quint64 regionGeometryUploads() const
    {
        return m_regionGeometryUploads.load();
    }
    bool patchOverlayGpuActive() const
    {
        return m_gpuPatch;
    }
    quint64 patchTextureUploads() const
    {
        return m_patchTextureUploads.load();
    }
    quint64 patchGeometryUploads() const
    {
        return m_patchGeometryUploads.load();
    }
#endif
protected:
    QSGNode *updatePaintNode(QSGNode *_oldNode, UpdatePaintNodeData *) override;
    void     updatePolish() override;
    void     geometryChange(const QRectF &_newGeometry, const QRectF &_oldGeometry) override;
    void     itemChange(ItemChange _change, const ItemChangeData &_value) override;
signals:
    void documentChanged();
    void selectionPhaseChanged();
    void viewportRectChanged();
    void textureCacheChanged();

private:
    void                                        queueFrame(bool _image, bool _overlay);
    QPointer<Document>                          m_document;
    QRegion                                     m_selectionOutline;
    QRect                                       m_selectionBounds;
    int                                         m_selectionShape = -1;
    QPainterPath                                m_selectionPath;
    QPolygonF                                   m_lassoPoints;
    QVector<PhotoCrap::Render::SelectionVertex> m_selectionVertices;
    QRectF                                      m_selectionMeshViewport;
    QSizeF                                      m_selectionMeshScale;
    qreal                                       m_selectionMeshRatio        = 0;
    quint64                                     m_selectionGeometryRevision = 0;
    bool    m_gpuSelection = false, m_selectionMeshCached = false, m_selectionMeshFallback = false;
    QRegion m_removeOverlayRegion;
    QVector<PhotoCrap::Render::RegionVertex> m_removeVertices;
    QSizeF                                   m_removeMeshScale;
    QRectF                                   m_removeMeshViewport;
    quint64                                  m_removeGeometryRevision = 0;
    bool    m_gpuRemove = false, m_removeMeshCached = false, m_removeMeshFallback = false;
    QRegion m_patchOverlayRegion;
    QRect   m_patchTargetRect;
    QVector<PhotoCrap::Render::PatchVertex> m_patchVertices;
    QSizeF                                  m_patchMeshScale;
    QRectF                                  m_patchMeshViewport;
    QImage                                  m_patchImage;
    quint64                                 m_patchGeometryRevision = 0;
    bool   m_gpuPatch = false, m_patchMeshCached = false, m_patchMeshFallback = false,
           m_patchGpuRejected = false;
    QImage m_textureImage, m_overlayImage, m_toneTable, m_toneMask, m_textureMaskImage;
    bool   m_gpuTonePreviewEnabled = false;
    bool   m_gpuStackEnabled = false, m_gpuStackRejected = false;
    bool   m_stackSnapshotPrepared = false;
    PhotoCrap::Render::LayerStack                       m_stackSnapshot;
    PhotoCrap::Render::LayerStack                       m_stack;
    std::shared_ptr<PhotoCrap::Render::StackStatistics> m_stackStatistics =
        std::make_shared<PhotoCrap::Render::StackStatistics>();
#ifdef PHOTOCRAP_TESTING
    std::atomic<bool> m_gpuStackActive{false};
    quint64           m_stackSnapshotPreparations = 0;
#endif
    PhotoCrap::Filters::ToneMappingSettings                    m_toneSettings;
    QHash<quint64, QImage>                                     m_tileImages, m_maskTileImages;
    QHash<quint64, quint64>                                    m_tileLastUse;
    QHash<quint64, qint64>                                     m_maskTileSourceKeys;
    quint64                                                    m_tileClock = 0;
    std::shared_ptr<PhotoCrap::Render::GraphicsBudget::Client> m_graphicsClient;
    int                                                        m_textureCacheMiB  = 64;
    qint64                                                     m_textureSourceKey = 0;
    qint64                                                     m_textureRevision  = 0;
    int                                                        m_textureLevel     = -1;
    QSize  m_documentSize, m_sourceSize, m_textureSize;
    QRectF m_viewportRect{-1, -1, -1, -1}, m_overlayRect;
    bool   m_imageDirty = true, m_overlayDirty = true, m_frameQueued = true;
#ifdef PHOTOCRAP_TESTING
    quint64              m_framePreparations = 0, m_phaseOnlyPreparations = 0;
    std::atomic<quint64> m_imageTextureUploads{0};
    std::atomic<quint64> m_toneTableUploads{0};
    std::atomic<quint64> m_maskTextureUploads{0};
    quint64              m_displayLevelSourcePixels = 0;
    std::atomic<quint64> m_residentTextureBytes{0};
    std::atomic<quint64> m_selectionGeometryUploads{0}, m_overlayTextureUploads{0};
    std::atomic<quint64> m_regionGeometryUploads{0};
    std::atomic<quint64> m_patchTextureUploads{0}, m_patchGeometryUploads{0};
#endif
    qreal m_selectionPhase     = 0;
    bool  m_systemCursorActive = false;
#ifdef Q_OS_MACOS
    bool m_nativeCursorRefreshPending = false;
#endif
};

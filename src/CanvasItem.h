#pragma once
#include "Document.h"
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
    int  textureCacheMiB() const
    {
        return m_textureCacheMiB;
    }
    void setTextureCacheMiB(int _value);
#ifdef PHOTOCRAP_TESTING
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
        return m_residentTextureBytes.load();
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
    QPainterPath                                m_selectionPath;
    QVector<PhotoCrap::Render::SelectionVertex> m_selectionVertices;
    QRectF                                      m_selectionMeshViewport;
    QSizeF                                      m_selectionMeshScale;
    qreal                                       m_selectionMeshRatio        = 0;
    quint64                                     m_selectionGeometryRevision = 0;
    bool   m_gpuSelection = false, m_selectionMeshCached = false, m_selectionMeshFallback = false;
    QImage m_textureImage, m_overlayImage, m_toneTable, m_toneMask, m_textureMaskImage;
    bool   m_gpuTonePreviewEnabled = false;
    PhotoCrap::Filters::ToneMappingSettings m_toneSettings;
    QHash<quint64, QImage>                  m_tileImages, m_maskTileImages;
    QHash<quint64, quint64>                 m_tileLastUse;
    QHash<quint64, qint64>                  m_maskTileSourceKeys;
    quint64                                 m_tileClock        = 0;
    int                                     m_textureCacheMiB  = 64;
    qint64                                  m_textureSourceKey = 0;
    qint64                                  m_textureRevision  = 0;
    int                                     m_textureLevel     = -1;
    QSize                                   m_documentSize, m_sourceSize, m_textureSize;
    QRectF                                  m_viewportRect{-1, -1, -1, -1}, m_overlayRect;
    bool                                    m_imageDirty = true, m_overlayDirty = true;
#ifdef PHOTOCRAP_TESTING
    std::atomic<quint64> m_imageTextureUploads{0};
    std::atomic<quint64> m_toneTableUploads{0};
    std::atomic<quint64> m_maskTextureUploads{0};
    quint64              m_displayLevelSourcePixels = 0;
    std::atomic<quint64> m_residentTextureBytes{0};
    std::atomic<quint64> m_selectionGeometryUploads{0}, m_overlayTextureUploads{0};
#endif
    qreal m_selectionPhase     = 0;
    bool  m_systemCursorActive = false;
#ifdef Q_OS_MACOS
    bool m_nativeCursorRefreshPending = false;
#endif
};

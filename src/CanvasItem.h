#pragma once
#include "Document.h"
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
#ifdef PHOTOCRAP_TESTING
    quint64 imageTextureUploads() const
    {
        return m_imageTextureUploads.load();
    }
    quint64 toneTableUploads() const
    {
        return m_toneTableUploads.load();
    }
    QSize overlayPixelSize() const
    {
        return m_overlayImage.size();
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

private:
    void                   queueFrame(bool _image, bool _overlay);
    QPointer<Document>     m_document;
    QRegion                m_selectionOutline;
    QPainterPath           m_selectionPath;
    QImage                 m_textureImage, m_overlayImage, m_toneTable;
    bool                   m_gpuTonePreviewEnabled = false;
    QHash<quint64, QImage> m_tileImages;
    qint64                 m_textureSourceKey = 0;
    qint64                 m_textureRevision  = 0;
    int                    m_textureLevel     = -1;
    QSize                  m_documentSize, m_sourceSize, m_textureSize;
    QRectF                 m_viewportRect{-1, -1, -1, -1}, m_overlayRect;
    bool                   m_imageDirty = true, m_overlayDirty = true;
#ifdef PHOTOCRAP_TESTING
    std::atomic<quint64> m_imageTextureUploads{0};
    std::atomic<quint64> m_toneTableUploads{0};
#endif
    qreal m_selectionPhase     = 0;
    bool  m_systemCursorActive = false;
#ifdef Q_OS_MACOS
    bool m_nativeCursorRefreshPending = false;
#endif
};

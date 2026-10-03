#include "CanvasItem.h"

CanvasItem::CanvasItem(QQuickItem *_parent) : QQuickPaintedItem(_parent)
{
    setAntialiasing(true);
}
void CanvasItem::setDocument(Document *_document)
{
    if (m_document == _document)
        return;
    if (m_document)
        disconnect(m_document, nullptr, this, nullptr);
    m_document = _document;
    if (_document) {
        connect(_document, &Document::imageChanged, this, [this] { update(); });
        connect(_document, &Document::documentChanged, this, [this] { update(); });
    }
    update();
    emit documentChanged();
}
void CanvasItem::paint(QPainter *_painter)
{
    if (!m_document)
        return;
    // A small tiled texture keeps transparent canvases inexpensive at any zoom.
    static const QImage checker = [] {
        QImage tile(24, 24, QImage::Format_RGB32);
        tile.fill(QColor("#d6d6d6"));
        QPainter p(&tile);
        p.fillRect(0, 0, 12, 12, QColor("#eeeeee"));
        p.fillRect(12, 12, 12, 12, QColor("#eeeeee"));
        return tile;
    }();
    _painter->fillRect(boundingRect(), QBrush(checker));
    _painter->setRenderHint(QPainter::SmoothPixmapTransform, width() < m_document->width());
    _painter->drawImage(boundingRect(), m_document->displayComposite());
}

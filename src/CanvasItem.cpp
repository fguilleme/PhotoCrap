#include "CanvasItem.h"

CanvasItem::CanvasItem(QQuickItem *parent):QQuickPaintedItem(parent) { setAntialiasing(true); }
void CanvasItem::setDocument(Document *document) {
    if(m_document==document) return;
    if(m_document) disconnect(m_document,nullptr,this,nullptr);
    m_document=document;
    if(document) {
        connect(document,&Document::imageChanged,this,[this]{update();});
        connect(document,&Document::documentChanged,this,[this]{update();});
    }
    update(); emit documentChanged();
}
void CanvasItem::paint(QPainter *painter) {
    if(!m_document) return;
    // A small tiled texture keeps transparent canvases inexpensive at any zoom.
    static const QImage checker=[] {
        QImage tile(24,24,QImage::Format_RGB32); tile.fill(QColor("#d6d6d6"));
        QPainter p(&tile); p.fillRect(0,0,12,12,QColor("#eeeeee"));
        p.fillRect(12,12,12,12,QColor("#eeeeee")); return tile;
    }();
    painter->fillRect(boundingRect(),QBrush(checker));
    painter->setRenderHint(QPainter::SmoothPixmapTransform,width()<m_document->width());
    painter->drawImage(boundingRect(),m_document->composite());
}

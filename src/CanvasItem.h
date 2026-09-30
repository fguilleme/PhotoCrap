#pragma once
#include <QQuickPaintedItem>
#include <QPointer>
#include "Document.h"

class CanvasItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(Document* document READ document WRITE setDocument NOTIFY documentChanged)
public:
    explicit CanvasItem(QQuickItem *parent=nullptr);
    Document *document() const {return m_document;}
    void setDocument(Document *document);
    void paint(QPainter *painter) override;
signals:
    void documentChanged();
private:
    QPointer<Document> m_document;
};

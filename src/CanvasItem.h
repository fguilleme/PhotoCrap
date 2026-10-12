#pragma once
#include "Document.h"
#include <QPointer>
#include <QQuickPaintedItem>

class CanvasItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(Document *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(qreal selectionPhase READ selectionPhase WRITE setSelectionPhase NOTIFY
                   selectionPhaseChanged)
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
    void             paint(QPainter *_painter) override;
signals:
    void documentChanged();
    void selectionPhaseChanged();

private:
    QPointer<Document> m_document;
    qreal              m_selectionPhase     = 0;
    bool               m_systemCursorActive = false;
};

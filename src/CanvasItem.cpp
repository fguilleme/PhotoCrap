#include "CanvasItem.h"

#include <QCursor>
#include <QGuiApplication>
#include <QPainterPath>
#include <QPixmap>
#include <QQuickWindow>
#include <QScreen>
#include <QtMath>

namespace
{
QCursor magicWandCursor(int _selectionMode)
{
    constexpr int logicalSize = 36;
    const qreal   ratio       = QGuiApplication::primaryScreen()
                                    ? QGuiApplication::primaryScreen()->devicePixelRatio()
                                    : 1.0;
    QPixmap       pixmap(qRound(logicalSize * ratio), qRound(logicalSize * ratio));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(0, 0, 0, 230), 5, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(5, 30), QPointF(21, 14));
    painter.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(5, 30), QPointF(21, 14));

    const auto drawSparkle = [&painter](const QPointF &_center, qreal _radius) {
        QPainterPath path;
        path.moveTo(_center.x(), _center.y() - _radius);
        path.lineTo(_center.x(), _center.y() + _radius);
        path.moveTo(_center.x() - _radius, _center.y());
        path.lineTo(_center.x() + _radius, _center.y());
        painter.setPen(QPen(QColor(0, 0, 0, 230), 3, Qt::SolidLine, Qt::RoundCap));
        painter.drawPath(path);
        painter.setPen(QPen(Qt::white, 1, Qt::SolidLine, Qt::RoundCap));
        painter.drawPath(path);
    };
    drawSparkle(QPointF(25, 8), 5);
    drawSparkle(QPointF(32, 17), 2.5);

    const QString sign = _selectionMode == 2   ? QStringLiteral("−")
                         : _selectionMode == 1 ? QStringLiteral("+")
                                               : QString();
    if (!sign.isEmpty()) {
        QFont font;
        font.setBold(true);
        font.setPixelSize(13);
        painter.setFont(font);
        QPainterPath text;
        text.addText(QPointF(23, 34), font, sign);
        painter.setPen(QPen(QColor(0, 0, 0, 230), 3, Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
        painter.drawPath(text);
        painter.fillPath(text, Qt::white);
    }
    return QCursor(pixmap, 5, 30);
}
} // namespace

CanvasItem::CanvasItem(QQuickItem *_parent) : QQuickPaintedItem(_parent)
{
    setAntialiasing(true);
}
CanvasItem::~CanvasItem()
{
    if (m_systemCursorActive)
        QGuiApplication::restoreOverrideCursor();
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
        connect(_document, &Document::selectionChanged, this, [this] { update(); });
    }
    update();
    emit documentChanged();
}
void CanvasItem::setSelectionPhase(qreal _phase)
{
    if (qFuzzyCompare(m_selectionPhase + 1, _phase + 1))
        return;
    m_selectionPhase = _phase;
    if (m_document && m_document->selectionShape() >= 6)
        update();
    emit selectionPhaseChanged();
}
void CanvasItem::setSystemCursor(int _shape, bool _active, const QString &_tool, int _selectionMode,
                                 bool _sourcePicking)
{
    if (!_active) {
        if (m_systemCursorActive) {
            QGuiApplication::restoreOverrideCursor();
            m_systemCursorActive = false;
        }
        return;
    }

    QCursor cursor = _tool == QStringLiteral("magicWand")
                         ? magicWandCursor(_selectionMode)
                         : QCursor(static_cast<Qt::CursorShape>(_shape));
    if ((_tool == "cloneStamp" || _tool == "patternStamp" || _tool == "smudge") &&
        _shape == Qt::BlankCursor && !(_sourcePicking && _tool == "cloneStamp") && m_document) {
        const qreal diameter =
            qBound(2., m_document->brushSize() * width() / qMax(1, m_document->width()), 500.);
        const int   size  = qCeil(diameter) + 8;
        const qreal ratio = window() ? window()->devicePixelRatio() : 1.;
        QPixmap     pixmap(qRound(size * ratio), qRound(size * ratio));
        pixmap.setDevicePixelRatio(ratio);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(size / 2., size / 2.);
        painter.rotate(m_document->brushAngle());
        painter.scale(1., m_document->brushRoundness());
        painter.setPen(QPen(Qt::black, 3));
        painter.drawEllipse(QPointF{}, diameter / 2., diameter / 2.);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawEllipse(QPointF{}, diameter / 2., diameter / 2.);
        painter.end();
        cursor = QCursor(pixmap, size / 2, size / 2);
    }
    if (m_systemCursorActive)
        QGuiApplication::changeOverrideCursor(cursor);
    else {
        QGuiApplication::setOverrideCursor(cursor);
        m_systemCursorActive = true;
    }
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
    if (m_document->selectionShape() < 6 || !m_document->hasSelection())
        return;

    QPainterPath outline;
    outline.setFillRule(Qt::WindingFill);
    outline.addRegion(m_document->selectionRegion());
    outline = outline.simplified();
    _painter->save();
    _painter->setRenderHint(QPainter::Antialiasing, false);
    _painter->scale(width() / m_document->width(), height() / m_document->height());
    QPen pen(Qt::black, 1.5, Qt::DashLine, Qt::SquareCap, Qt::MiterJoin);
    pen.setCosmetic(true);
    pen.setDashPattern({5, 5});
    pen.setDashOffset(m_selectionPhase);
    _painter->setPen(pen);
    _painter->drawPath(outline);
    pen.setColor(Qt::white);
    pen.setDashOffset(m_selectionPhase + 5);
    _painter->setPen(pen);
    _painter->drawPath(outline);
    _painter->restore();
}

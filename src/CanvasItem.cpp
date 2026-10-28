#include "CanvasItem.h"
#ifdef Q_OS_MACOS
#include "MacToolCursor.h"
#endif

#include <QCursor>
#include <QGuiApplication>
#include <QPainterPath>
#include <QPixmap>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QtMath>

namespace
{
QCursor penCursor(const QString &_tool)
{
    static QHash<QString, QCursor> cache;
    const qreal   ratio = QGuiApplication::primaryScreen()
                              ? QGuiApplication::primaryScreen()->devicePixelRatio()
                              : 1.;
    const QString key   = _tool + QString::number(ratio, 'f', 2);
    if (cache.contains(key))
        return cache.value(key);
    QPixmap pixmap(qRound(36 * ratio), qRound(36 * ratio));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath nib;
    if (_tool == "penMove") {
        // Align the visible arrow tip with its hotspot, rather than retaining
        // the nib's hotspot twenty pixels below the arrow tip.
        nib.moveTo(3, 3);
        nib.lineTo(3, 23);
        nib.lineTo(9, 18);
        nib.lineTo(13, 27);
        nib.lineTo(18, 25);
        nib.lineTo(13, 16);
        nib.lineTo(21, 16);
        nib.closeSubpath();
    } else {
        nib.moveTo(3, 29);
        nib.lineTo(6, 15);
        nib.lineTo(21, 3);
        nib.lineTo(29, 11);
        nib.lineTo(17, 26);
        nib.closeSubpath();
    }
    p.setPen(QPen(Qt::black, 3));
    p.setBrush(Qt::white);
    p.drawPath(nib);
    p.setPen(QPen(Qt::white, 1));
    p.drawPath(nib);
    p.setPen(QPen(Qt::black, 1.2));
    if (_tool != "penMove") {
        p.drawLine(QPointF(3, 29), QPointF(16, 16));
        p.drawEllipse(QPointF(17, 15), 2, 2);
    }
    for (const QPen pen : {QPen(Qt::black, 3.5), QPen(Qt::white, 1.5)}) {
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        if (_tool == "penAdd" || _tool == "penStart") {
            p.drawLine(25, 28, 33, 28);
            p.drawLine(29, 24, 29, 32);
        }
        if (_tool == "penDelete")
            p.drawLine(25, 28, 33, 28);
        if (_tool == "penClose")
            p.drawEllipse(QPointF(29, 28), 3, 3);
        if (_tool == "penResume")
            p.drawLine(25, 32, 32, 25);
        if (_tool == "penConvert") {
            p.drawLine(25, 25, 29, 31);
            p.drawLine(29, 31, 33, 25);
        }
    }
    const QCursor cursor(pixmap, 3, _tool == "penMove" ? 3 : 29);
    cache.insert(key, cursor);
    return cursor;
}

QCursor eyedropperCursor(const QString &_tool)
{
    constexpr int                  size = 36;
    static QHash<QString, QCursor> cache;
    const qreal   ratio    = QGuiApplication::primaryScreen()
                                 ? QGuiApplication::primaryScreen()->devicePixelRatio()
                                 : 1.;
    const QString cacheKey = _tool + QString::number(ratio, 'f', 2);
    if (cache.contains(cacheKey))
        return cache.value(cacheKey);
    QPixmap pixmap(qRound(size * ratio), qRound(size * ratio));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath tube;
    tube.moveTo(3, 29);
    tube.lineTo(9, 23);
    tube.lineTo(8, 22);
    tube.lineTo(20, 10);
    tube.lineTo(24, 14);
    tube.lineTo(12, 26);
    tube.lineTo(10, 25);
    tube.lineTo(3, 29);
    painter.setPen(QPen(Qt::black, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(tube);
    painter.setPen(QPen(Qt::white, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(tube);
    QPainterPath bulb;
    bulb.moveTo(18, 9);
    bulb.lineTo(24, 3);
    bulb.quadTo(26, 1, 28, 3);
    bulb.lineTo(31, 6);
    bulb.quadTo(33, 8, 31, 10);
    bulb.lineTo(25, 16);
    bulb.closeSubpath();
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(Qt::white);
    painter.drawPath(bulb);
    if (_tool.startsWith("colorSampler")) {
        painter.setBrush(Qt::NoBrush);
        if (_tool == "colorSamplerDelete") {
            for (const QPen pen : {QPen(Qt::black, 3), QPen(Qt::white, 1)}) {
                painter.setPen(pen);
                painter.drawEllipse(QPointF(25, 26), 2, 2);
                painter.drawEllipse(QPointF(25, 32), 2, 2);
                painter.drawLine(QPointF(27, 27), QPointF(34, 33));
                painter.drawLine(QPointF(27, 31), QPointF(34, 25));
            }
        } else {
            for (const QPen pen : {QPen(Qt::black, 3), QPen(Qt::white, 1)}) {
                painter.setPen(pen);
                painter.drawEllipse(QPointF(3, 29), 4, 4);
                painter.drawLine(QPointF(0, 29), QPointF(8, 29));
                painter.drawLine(QPointF(3, 24), QPointF(3, 34));
            }
        }
    } else if (_tool.startsWith("curvePicker_")) {
        const QColor badge = _tool.endsWith("black") ? QColor(Qt::black)
                             : (_tool.endsWith("gray") || _tool.endsWith("whiteBalance"))
                                 ? QColor(128, 128, 128)
                             : _tool.endsWith("white") ? QColor(Qt::white)
                                                       : QColor(95, 202, 138);
        painter.setBrush(badge);
        painter.setPen(QPen(Qt::black, 3));
        painter.drawEllipse(QPointF(29, 28), 4, 4);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawEllipse(QPointF(29, 28), 4, 4);
    }
    const QCursor cursor(pixmap, 3, 29);
    cache.insert(cacheKey, cursor);
    return cursor;
}

QCursor rulerCursor()
{
    constexpr int size  = 32;
    const qreal   ratio = QGuiApplication::primaryScreen()
                              ? QGuiApplication::primaryScreen()->devicePixelRatio()
                              : 1.;
    QPixmap       pixmap(qRound(size * ratio), qRound(size * ratio));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    for (const QPen pen : {QPen(Qt::black, 3), QPen(Qt::white, 1)}) {
        painter.setPen(pen);
        painter.drawLine(QPointF(1, 8), QPointF(15, 8));
        painter.drawLine(QPointF(8, 1), QPointF(8, 15));
        painter.save();
        painter.translate(18, 20);
        painter.rotate(-45);
        painter.drawRect(QRectF(-10, -3, 20, 6));
        for (int x = -6; x < 10; x += 4)
            painter.drawLine(QPointF(x, -3), QPointF(x, 0));
        painter.restore();
    }
    return QCursor(pixmap, 8, 8);
}

QCursor magicEraserCursor()
{
    static QHash<qreal, QCursor> cache;
    const qreal                  ratio = QGuiApplication::primaryScreen()
                                             ? QGuiApplication::primaryScreen()->devicePixelRatio()
                                             : 1.;
    if (cache.contains(ratio))
        return cache.value(ratio);
    QPixmap pixmap(qRound(32 * ratio), qRound(32 * ratio));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath eraser;
    eraser.moveTo(3, 25);
    eraser.lineTo(14, 12);
    eraser.lineTo(24, 21);
    eraser.lineTo(17, 29);
    eraser.lineTo(8, 29);
    eraser.closeSubpath();
    eraser.moveTo(8, 19);
    eraser.lineTo(19, 27);
    eraser.moveTo(25, 2);
    eraser.lineTo(25, 12);
    eraser.moveTo(20, 7);
    eraser.lineTo(30, 7);
    for (const QPen pen : {QPen(Qt::black, 3.5), QPen(Qt::white, 1.5)}) {
        painter.setPen(pen);
        painter.drawPath(eraser);
    }
    const QCursor cursor(pixmap, 3, 25);
    cache.insert(ratio, cursor);
    return cursor;
}

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

    const QString sign = _selectionMode == 3   ? QStringLiteral("×")
                         : _selectionMode == 2 ? QStringLiteral("−")
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
        connect(_document, &Document::removeRegionChanged, this, [this] { update(); });
        connect(_document, &Document::patchPreviewChanged, this, [this] { update(); });
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
                                 bool _sourcePicking, bool _precise)
{
    if (!_active) {
        if (m_systemCursorActive) {
            QGuiApplication::restoreOverrideCursor();
            m_systemCursorActive = false;
        }
        unsetCursor();
        return;
    }

    QCursor cursor = _tool.startsWith("pen") && !_precise   ? penCursor(_tool)
                     : _tool == QStringLiteral("magicWand") ? magicWandCursor(_selectionMode)
                     : _tool == QStringLiteral("magicEraser") && !_precise
                         ? magicEraserCursor()
                         : QCursor(static_cast<Qt::CursorShape>(_shape));
    if (_shape == Qt::CrossCursor || _shape == Qt::BlankCursor) {
        if (_tool == "picker" || _tool.startsWith("colorSampler") ||
            _tool.startsWith("curvePicker_") ||
            (_sourcePicking && (_tool == "colorReplacement" || _tool == "mixerBrush")))
            cursor = eyedropperCursor(_tool);
        else if (_tool == "ruler")
            cursor = rulerCursor();
    }
    if ((_tool == "cloneStamp" || _tool == "patternStamp" || _tool == "smudge" ||
         _tool == "colorReplacement" || _tool == "mixerBrush" || _tool == "healing" ||
         _tool == "spotHealing" || _tool == "remove" ||
         (_tool == "historyBrush" || _tool == "artHistoryBrush")) &&
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
        painter.rotate(_tool == "remove" ? 0 : m_document->brushAngle());
        painter.scale(1., _tool == "remove" ? 1. : m_document->brushRoundness());
        painter.setPen(QPen(Qt::black, 3));
        painter.drawEllipse(QPointF{}, diameter / 2., diameter / 2.);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawEllipse(QPointF{}, diameter / 2., diameter / 2.);
        if (_tool == "colorReplacement") {
            painter.resetTransform();
            painter.translate(size / 2., size / 2.);
            for (const QPen pen : {QPen(Qt::black, 3), QPen(Qt::white, 1)}) {
                painter.setPen(pen);
                painter.drawLine(QPointF(-4, 0), QPointF(4, 0));
                painter.drawLine(QPointF(0, -4), QPointF(0, 4));
            }
        }
        painter.end();
        cursor = QCursor(pixmap, size / 2, size / 2);
    }
    if (_precise)
        cursor = QCursor(Qt::CrossCursor);
    setCursor(cursor);
    if (m_systemCursorActive)
        QGuiApplication::changeOverrideCursor(cursor);
    else {
        QGuiApplication::setOverrideCursor(cursor);
        m_systemCursorActive = true;
    }
#ifdef Q_OS_MACOS
    reapplyMacToolCursor(cursor);
    // Cocoa can reset its tracking cursor after Qt's hover/focus handler returns,
    // including when a floating Info panel opens. Reapply the latest override
    // once that event has finished; coalesce pointer updates and discard stale ones.
    if (!m_nativeCursorRefreshPending) {
        m_nativeCursorRefreshPending = true;
        QTimer::singleShot(0, this, [this] {
            m_nativeCursorRefreshPending = false;
            if (m_systemCursorActive && window() && window()->isActive())
                if (const auto current = QGuiApplication::overrideCursor())
                    reapplyMacToolCursor(*current);
        });
    }
#endif
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
    if (m_document->patchDragging() && !m_document->patchPreviewImage().isNull()) {
        // This is a donor-texture preview; colour adaptation is applied on release.
        _painter->save();
        _painter->scale(width() / m_document->width(), height() / m_document->height());
        _painter->setClipRegion(m_document->patchTargetRegion());
        _painter->drawImage(m_document->patchTargetRect(), m_document->patchPreviewImage());
        _painter->restore();
    }
    if (m_document->removeHasPending()) {
        _painter->save();
        _painter->scale(width() / m_document->width(), height() / m_document->height());
        _painter->setClipRegion(m_document->removeRegion());
        _painter->fillRect(m_document->removeRegion().boundingRect(), QColor(235, 70, 180, 110));
        _painter->restore();
    }
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

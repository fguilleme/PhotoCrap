#include "Document.h"
#include "selection/MarqueeCoverage.h"
#include <QtConcurrent>
#include <cmath>

void Document::setSelectionFeather(int _value)
{
    const int value = qBound(0, _value, 1000);
    if (m_selectionFeather == value)
        return;
    m_selectionFeather = value;
    emit settingsChanged();
}
void Document::setSelectionAntialias(bool _value)
{
    if (m_selectionAntialias == _value)
        return;
    m_selectionAntialias = _value;
    emit settingsChanged();
}
void Document::setMarqueeStyle(int _value)
{
    if (_value < 0 || _value > 2 || m_marqueeStyle == _value)
        return;
    m_marqueeStyle = _value;
    emit settingsChanged();
}
void Document::setMarqueeWidth(qreal _value)
{
    if (!std::isfinite(_value) || _value < .01 || _value > 16384 || m_marqueeWidth == _value)
        return;
    m_marqueeWidth = _value;
    emit settingsChanged();
}
void Document::setMarqueeHeight(qreal _value)
{
    if (!std::isfinite(_value) || _value < .01 || _value > 16384 || m_marqueeHeight == _value)
        return;
    m_marqueeHeight = _value;
    emit settingsChanged();
}
void Document::swapMarqueeDimensions()
{
    std::swap(m_marqueeWidth, m_marqueeHeight);
    emit settingsChanged();
}
QRect Document::marqueeRectangle(const QPointF &_point) const
{
    const bool center = m_toolAlternate && !m_selectionInitialAlternate;
    if (m_marqueeStyle == 0) {
        const auto rect = toolRectangle(m_anchor, _point, center);
        return {qRound(rect.left()), qRound(rect.top()), qRound(rect.width()) + 1,
                qRound(rect.height()) + 1};
    }
    if (m_marqueeStyle == 2) {
        const QSize size(qMax(1, qRound(m_marqueeWidth)), qMax(1, qRound(m_marqueeHeight)));
        return QRect(
            (center ? _point - QPointF((size.width() - 1) / 2., (size.height() - 1) / 2.) : _point)
                .toPoint(),
            size);
    }
    const auto  delta  = _point - m_anchor;
    const qreal factor = center ? 2 : 1, ratio = m_marqueeWidth / m_marqueeHeight;
    qreal       width = qAbs(delta.x()) * factor + 1, height = qAbs(delta.y()) * factor + 1;
    if (width / height > ratio)
        height = width / ratio;
    else
        width = height * ratio;
    const QSize size(qRound(qBound(1., width, 32768.)), qRound(qBound(1., height, 32768.)));
    const auto  origin = center
                             ? m_anchor - QPointF((size.width() - 1) / 2., (size.height() - 1) / 2.)
                             : m_anchor - QPointF(delta.x() < 0 ? size.width() - 1 : 0,
                                                  delta.y() < 0 ? size.height() - 1 : 0);
    return {origin.toPoint(), size};
}
void Document::updateMarquee(const QPointF &_point)
{
    m_marqueeRect       = marqueeRectangle(_point);
    m_selection         = m_marqueeRect.intersected(QRect(QPoint{}, m_size));
    m_selectionRegion   = m_tool == "ellipticalSelect" ? QRegion(m_marqueeRect, QRegion::Ellipse) &
                                                             QRegion(QRect(QPoint{}, m_size))
                                                       : QRegion(m_selection);
    m_selectionCombined = false;
    combineGestureSelection();
    emit selectionChanged();
}
void Document::finishGeometricSelection(const QRegion &_region, const QPainterPath &_path,
                                        bool _antialias)
{
    if (!m_selectionGestureRecorded)
        return;
    const auto before = m_beforeSelectionGesture;
    const int  mode = m_selectionMode, feather = m_selectionFeather;
    const bool aa = _antialias && m_selectionAntialias;
    if ((m_tool == "patch" || m_tool == "contentAwareMove") || (!aa && !feather)) {
        m_selectionMask     = {};
        m_selectionRegion   = _region;
        m_selection         = _region.boundingRect();
        m_selectionCombined = false;
        combineGestureSelection();
        emit selectionChanged();
        finishSelectionGesture();
        return;
    }
    m_beforeSelectionGesture   = {};
    m_selectionGestureRecorded = false;
    const auto calculate = [=, size = m_size](const std::shared_ptr<std::atomic_bool> &cancel) {
        return MarqueeCoverage::create(_region, _path, size, aa, feather, before.selectionRegion,
                                       before.selectionMask, mode, cancel);
    };
    const auto shape  = m_selectionShape;
    const auto points = m_selectionPath;
    const auto apply  = [this, before, mode, feather, shape,
                         points](const SelectionCoverage::Result &result) {
        if (!result.valid) {
            restore(before);
            if (!result.error.isEmpty())
                fail(tr("Impossible de calculer cette sélection."));
            return;
        }
        applySelectionCoverage(result);
        if (!mode && !feather) {
            m_selectionShape = shape;
            m_selectionPath  = points;
        }
        commitSelectionEdit(before, tr("Modifier la sélection"));
        emit selectionChanged();
        if (!result.support.isEmpty() && result.outline.isEmpty())
            emit statusMessage(
                tr("Aucun pixel n’est sélectionné à plus de 50 %. La sélection reste active."));
    };
    auto cancel = std::make_shared<std::atomic_bool>(false);
    // Antialiasing rasterizes only the shape's bounding box. Feathering and
    // larger masks run off the GUI thread with one undo step for the gesture.
    if (!feather && qint64(m_size.width()) * m_size.height() <= 1024 * 1024) {
        apply(calculate(cancel));
        return;
    }
    const auto session       = m_documentSession;
    m_selectionCancel        = cancel;
    m_beforeMarqueeSelection = before;
    m_marqueePending         = true;
    m_marqueeSession         = session;
    m_selectionTaskStateId   = m_stateId;
    emit selectionOperationChanged();
    auto watcher = new QFutureWatcher<SelectionCoverage::Result>(this);
    connect(watcher, &QFutureWatcher<SelectionCoverage::Result>::finished, this,
            [this, watcher, cancel, before, session, apply] {
                const auto result = watcher->result();
                watcher->deleteLater();
                if (m_selectionCancel != cancel)
                    return;
                cancelSelectionOperation();
                if (m_stateId != before.id || m_documentSession != session)
                    return;
                apply(result);
            });
    watcher->setFuture(QtConcurrent::run([calculate, cancel] { return calculate(cancel); }));
}

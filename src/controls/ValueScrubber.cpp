#include "ValueScrubber.h"
#include <QCursor>
#include <QKeyEvent>
#include <QMetaProperty>
#include <QMouseEvent>
#include <cmath>

ValueScrubber::ValueScrubber(QQuickItem *_parent) : QQuickItem(_parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setCursor(Qt::OpenHandCursor);
    connect(this, &QQuickItem::enabledChanged, this, [this] {
        if (!isEnabled())
            finish(true);
    });
    connect(this, &QQuickItem::visibleChanged, this, [this] {
        if (!isVisible())
            finish(true);
    });
}
void ValueScrubber::setTarget(QObject *_target)
{
    if (m_target == _target)
        return;
    finish(true);
    disconnect(m_destroyed);
    disconnect(m_enabled);
    m_target = _target;
    if (_target) {
        m_destroyed = connect(_target, &QObject::destroyed, this, [this] {
            m_target.clear();
            finish(true);
            emit targetChanged();
        });
        if (auto *item = qobject_cast<QQuickItem *>(_target))
            m_enabled = connect(item, &QQuickItem::enabledChanged, this, [this] {
                if (!available())
                    finish(true);
            });
    }
    emit targetChanged();
}
bool ValueScrubber::available() const
{
    return isEnabled() && isVisible() && m_target &&
           (!m_target->property("enabled").isValid() || m_target->property("enabled").toBool());
}
void ValueScrubber::write(qreal _value)
{
    if (!m_target || !std::isfinite(_value))
        return;
    const auto property =
        m_target->metaObject()->property(m_target->metaObject()->indexOfProperty("value"));
    if (!property.isValid() || !property.isWritable())
        return;
    QVariant value =
        property.metaType().id() == QMetaType::Int ? QVariant(qRound(_value)) : QVariant(_value);
    if (m_target->property("value") == value)
        return;
    // A native property write preserves the owner's QML binding. A JS assignment
    // would detach the field from subsequent model changes after the drag.
    if (!property.write(m_target, value))
        return;
    if (!m_target)
        return;
    if (m_target->metaObject()->indexOfMethod("valueModified()") >= 0)
        QMetaObject::invokeMethod(m_target, "valueModified", Qt::DirectConnection);
    else if (m_target->metaObject()->indexOfMethod("moved()") >= 0)
        QMetaObject::invokeMethod(m_target, "moved", Qt::DirectConnection);
    if (m_pressed && !m_finishing)
        forceActiveFocus(Qt::MouseFocusReason);
}
void ValueScrubber::mousePressEvent(QMouseEvent *_event)
{
    if (_event->button() != Qt::LeftButton || !available()) {
        _event->ignore();
        return;
    }
    forceActiveFocus(Qt::MouseFocusReason); // Commit any numeric text being edited first.
    if (!available()) {
        _event->ignore();
        return;
    }
    m_original = m_accumulated = m_target->property("value").toDouble();
    m_position                 = _event->scenePosition().x();
    m_pressed                  = true;
    setKeepMouseGrab(true);
    setCursor(Qt::ClosedHandCursor);
    if (m_target->metaObject()->indexOfProperty("scrubbing") >= 0)
        m_target->setProperty("scrubbing", true);
    emit pressedChanged();
    emit started();
    if (m_pressed)
        forceActiveFocus(Qt::MouseFocusReason);
    _event->accept();
}
void ValueScrubber::mouseMoveEvent(QMouseEvent *_event)
{
    if (!m_pressed || !available()) {
        finish(true);
        _event->ignore();
        return;
    }
    const auto number = [this](const char *_name, qreal _fallback) {
        bool       ok    = false;
        const auto value = m_target->property(_name).toDouble(&ok);
        return ok && std::isfinite(value) ? value : _fallback;
    };
    const qreal from = number("from", 0), to = number("to", 100);
    const qreal rawStep = number("stepSize", 1);
    const qreal step    = rawStep > 0 ? rawStep : 1.;
    const qreal speed   = (_event->modifiers().testFlag(Qt::ShiftModifier) ? 10. : 1.) *
                        (_event->modifiers().testFlag(Qt::AltModifier) ? .1 : 1.);
    m_accumulated = qBound(qMin(from, to),
                           m_accumulated + (_event->scenePosition().x() - m_position) * step *
                                               speed / qMax(qreal(.1), m_pixelsPerStep),
                           qMax(from, to));
    m_position    = _event->scenePosition().x();
    write(qBound(qMin(from, to),
                 m_original + std::round((m_accumulated - m_original) / step) * step,
                 qMax(from, to)));
    _event->accept();
}
void ValueScrubber::mouseReleaseEvent(QMouseEvent *_event)
{
    if (!m_pressed) {
        _event->ignore();
        return;
    }
    mouseMoveEvent(_event);
    finish(false);
    _event->accept();
}
void ValueScrubber::finish(bool _cancelled)
{
    if (!m_pressed || m_finishing)
        return;
    m_finishing = true;
    if (_cancelled)
        write(m_original);
    m_pressed = false;
    setKeepMouseGrab(false);
    setCursor(Qt::OpenHandCursor);
    if (m_target && m_target->metaObject()->indexOfProperty("scrubbing") >= 0)
        m_target->setProperty("scrubbing", false);
    emit pressedChanged();
    if (m_target)
        emit finished(_cancelled);
    ungrabMouse();
    m_finishing = false;
}
void ValueScrubber::mouseUngrabEvent()
{
    finish(true);
}
bool ValueScrubber::event(QEvent *_event)
{
    if (m_pressed && _event->type() == QEvent::ShortcutOverride) {
        const int key = static_cast<QKeyEvent *>(_event)->key();
        if (key == Qt::Key_Escape || key == Qt::Key_Return || key == Qt::Key_Enter) {
            _event->accept();
            return true;
        }
    }
    return QQuickItem::event(_event);
}
void ValueScrubber::keyPressEvent(QKeyEvent *_event)
{
    if (m_pressed && (_event->key() == Qt::Key_Escape || _event->key() == Qt::Key_Return ||
                      _event->key() == Qt::Key_Enter)) {
        finish(_event->key() == Qt::Key_Escape);
        _event->accept();
        return;
    }
    QQuickItem::keyPressEvent(_event);
}

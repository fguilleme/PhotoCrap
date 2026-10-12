#include "ToolInput.h"
#include <QCoreApplication>
#include <QKeyEvent>
#include <QKeySequence>

ToolInput::ToolInput(QObject *_parent) : QObject(_parent)
{
    QCoreApplication::instance()->installEventFilter(this);
}
ToolInput::~ToolInput()
{
    QCoreApplication::instance()->removeEventFilter(this);
}
bool ToolInput::handlesSequence(const QString &_sequence) const
{
    const auto sequence = QKeySequence::fromString(_sequence, QKeySequence::PortableText);
    return !sequence.isEmpty() && sequence.count() == 1;
}
void ToolInput::updateModifiers(int _modifiers)
{
    if (m_modifiers == _modifiers)
        return;
    m_modifiers = _modifiers;
    emit modifiersChanged();
}
void ToolInput::setEnabled(bool _enabled)
{
    if (m_enabled == _enabled)
        return;
    m_enabled = _enabled;
    if (!_enabled) {
        updateModifiers(0);
        if (m_pressedKey) {
            m_pressedKey = 0;
            emit shortcutReleased(m_pressTimer.elapsed() >= 200);
        }
    }
    emit enabledChanged();
}
bool ToolInput::eventFilter(QObject *_watched, QEvent *_event)
{
    if (_watched != m_targetWindow || !m_enabled)
        return false;
    const auto type = _event->type();
    if (type != QEvent::KeyPress && type != QEvent::KeyRelease && type != QEvent::ShortcutOverride)
        return false;
    auto *key = static_cast<QKeyEvent *>(_event);
    if (type != QEvent::ShortcutOverride) {
        int       modifiers = int(key->modifiers());
        const int flag      = key->key() == Qt::Key_Shift     ? Qt::ShiftModifier
                              : key->key() == Qt::Key_Alt     ? Qt::AltModifier
                              : key->key() == Qt::Key_Control ? Qt::ControlModifier
                              : key->key() == Qt::Key_Meta    ? Qt::MetaModifier
                                                              : 0;
        if (type == QEvent::KeyPress)
            modifiers |= flag;
        else
            modifiers &= ~flag;
        updateModifiers(modifiers);
    }
    if (type == QEvent::KeyRelease && key->key() == m_pressedKey && !key->isAutoRepeat()) {
        m_pressedKey = 0;
        emit shortcutReleased(m_pressTimer.elapsed() >= 200);
        return true;
    }
    if (type == QEvent::KeyRelease)
        return false;
    if (key->key() == Qt::Key_CapsLock && type == QEvent::KeyPress && !key->isAutoRepeat()) {
        m_precise = !m_precise;
        emit preciseChanged();
        return true;
    }
    const QKeySequence sequence(key->keyCombination());
    // Exact custom bindings take precedence over a derived Shift-cycle binding.
    for (const bool cycle : {false, true}) {
        for (const QVariant &value : m_bindings) {
            const QVariantMap binding = value.toMap();
            const auto configured = QKeySequence::fromString(binding.value("sequence").toString(),
                                                             QKeySequence::PortableText);
            if (configured.isEmpty() || configured.count() != 1)
                continue;
            const auto         combination = configured[0];
            const QKeySequence candidate(
                cycle ? QKeyCombination(combination.keyboardModifiers() | Qt::ShiftModifier,
                                        combination.key())
                      : combination);
            if (candidate != sequence ||
                (cycle && combination.keyboardModifiers().testFlag(Qt::ShiftModifier)))
                continue;
            if (type == QEvent::ShortcutOverride) {
                key->accept();
                return true;
            }
            if (!key->isAutoRepeat()) {
                if (m_pressedKey)
                    emit shortcutReleased(false);
                m_pressedKey = key->key();
                m_pressTimer.start();
                emit shortcutPressed(binding.value("group").toString(),
                                     binding.value("tool").toString(), cycle);
            }
            return true;
        }
    }
    return false;
}

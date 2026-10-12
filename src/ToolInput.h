#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QVariantList>

// Observes real key releases even when a shortcut consumes its key press.
class ToolInput : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *targetWindow MEMBER m_targetWindow)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QVariantList bindings MEMBER m_bindings)
    Q_PROPERTY(int modifiers READ modifiers NOTIFY modifiersChanged)
    Q_PROPERTY(bool precise READ precise NOTIFY preciseChanged)
public:
    explicit ToolInput(QObject *_parent = nullptr);
    ~ToolInput() override;
    Q_INVOKABLE bool handlesSequence(const QString &_sequence) const;
    bool             enabled() const
    {
        return m_enabled;
    }
    void setEnabled(bool _enabled);
    int  modifiers() const
    {
        return m_modifiers;
    }
    bool precise() const
    {
        return m_precise;
    }
signals:
    void enabledChanged();
    void modifiersChanged();
    void preciseChanged();
    void shortcutPressed(QString _group, QString _tool, bool _cycle);
    void shortcutReleased(bool _springLoaded);

protected:
    bool eventFilter(QObject *_watched, QEvent *_event) override;

private:
    QObject      *m_targetWindow = nullptr;
    QVariantList  m_bindings;
    bool          m_enabled = false, m_precise = false;
    int           m_modifiers = 0, m_pressedKey = 0;
    QElapsedTimer m_pressTimer;
    void          updateModifiers(int _modifiers);
};

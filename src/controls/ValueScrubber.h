#pragma once

#include <QPointer>
#include <QQuickItem>

class ValueScrubber : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QObject *target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(bool pressed READ pressed NOTIFY pressedChanged)
    Q_PROPERTY(qreal pixelsPerStep MEMBER m_pixelsPerStep)
public:
    explicit ValueScrubber(QQuickItem *_parent = nullptr);
    QObject *target() const
    {
        return m_target;
    }
    void setTarget(QObject *_target);
    bool pressed() const
    {
        return m_pressed;
    }
signals:
    void targetChanged();
    void pressedChanged();
    void started();
    void finished(bool cancelled);

protected:
    bool event(QEvent *_event) override;
    void mousePressEvent(QMouseEvent *_event) override;
    void mouseMoveEvent(QMouseEvent *_event) override;
    void mouseReleaseEvent(QMouseEvent *_event) override;
    void mouseUngrabEvent() override;
    void keyPressEvent(QKeyEvent *_event) override;

private:
    bool                    available() const;
    void                    write(qreal _value);
    void                    finish(bool _cancelled);
    QPointer<QObject>       m_target;
    bool                    m_pressed = false, m_finishing = false;
    qreal                   m_pixelsPerStep = 2, m_position = 0, m_original = 0, m_accumulated = 0;
    QMetaObject::Connection m_destroyed, m_enabled;
};

#pragma once
#include <QObject>
#include <QPointF>
#include <QString>

class PenBandGeometry : public QObject
{
    Q_OBJECT
public:
    explicit PenBandGeometry(QObject *_parent = nullptr) : QObject(_parent)
    {
    }
    Q_INVOKABLE QString outline(QPointF _start, QPointF _control, QPointF _end, QPointF _origin,
                                qreal _pixelScale) const;
};

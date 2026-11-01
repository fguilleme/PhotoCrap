#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>

class CurvesGeometry : public QObject
{
    Q_OBJECT
public:
    explicit CurvesGeometry(QObject *_parent = nullptr) : QObject(_parent)
    {
    }
    Q_INVOKABLE QString stroke(const QVariantList &_samples, qreal _width, qreal _height) const;
};

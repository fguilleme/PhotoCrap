#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>

// Prepare the legacy painter's stroke outline; pixels are still drawn on GPU.
class HistogramGeometry : public QObject
{
    Q_OBJECT
public:
    explicit HistogramGeometry(QObject *_parent = nullptr) : QObject(_parent)
    {
    }
    Q_INVOKABLE QString luminanceStroke(const QVariantList &_values, qreal _width,
                                        qreal _height) const;
};

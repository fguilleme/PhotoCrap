#pragma once
#include <QImage>
struct Layer;
namespace PhotoCrap::Filters
{
QImage relight(const QImage &source, const Layer &layer, const QPoint &origin);
}

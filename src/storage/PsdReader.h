#pragma once

#include "Layer.h"
#include <functional>

namespace PhotoCrap::Psd
{
struct Result {
    QSize        size;
    int          bits = 0;
    QList<Layer> layers;
    QStringList  notes;
    QString      error;
};

bool isFile(const QString &path);

// compositeOnly is used by commands that consume one image rather than a document.
Result read(const QString &path, bool compositeOnly, const std::function<bool(int, int)> &progress);
} // namespace PhotoCrap::Psd

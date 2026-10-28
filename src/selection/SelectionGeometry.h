#pragma once
#include <QRegion>
#include <QSize>
#include <QString>
#include <atomic>
#include <memory>

namespace SelectionGeometry
{
struct Result {
    QRegion region;
    bool    valid = false;
    QString error;
};
Result modify(const QRegion &_selection, const QSize &_size, const QString &_operation, int _amount,
              const std::shared_ptr<std::atomic_bool> &_cancel);
} // namespace SelectionGeometry

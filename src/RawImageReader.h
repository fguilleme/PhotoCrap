#pragma once
#include <QImage>
#include <QString>
#include <atomic>
#include <memory>

struct RawImageResult {
    QImage  image;
    QString error;
    bool    cancelled = false;
};
namespace RawImageReader
{
bool           isRawFile(const QString &_path);
QString        filePatterns();
RawImageResult read(const QString &_path, const std::shared_ptr<std::atomic_bool> &_cancel = {});
} // namespace RawImageReader

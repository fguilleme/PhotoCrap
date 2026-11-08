#pragma once
#include <QByteArray>
#include <QImage>
#include <QJsonObject>
#include <QString>

namespace PhotoCrap::Preview
{
QJsonObject encode(QImage image, const QSize &documentSize, int bits);
QImage      read(const QString &path, const QSize &bounds);
} // namespace PhotoCrap::Preview

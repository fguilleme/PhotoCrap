#pragma once

#include <QImage>
#include <QString>
#include <atomic>
#include <memory>

namespace PhotoCrap::Denoise
{
// Bayer input is half-resolution RGGB packed in four unsigned 16-bit channels.
// Linear input is Rec.2020. DisplayRgb accepts the editor's straight or
// premultiplied sRGB images and preserves their depth and coverage.
enum class Domain { Bayer, Linear, DisplayRgb };
using Cancel = std::shared_ptr<std::atomic_bool>;
QImage run(const QImage &source, Domain domain, const Cancel &cancel, QString &error);
QImage adjust(const QImage &source, double amount);
bool   available();
} // namespace PhotoCrap::Denoise

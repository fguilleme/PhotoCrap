#pragma once

#include <QImage>
#include <QList>
#include <QStringList>
#include <QVariantMap>
#include <functional>

namespace PhotoCrap::Look
{
struct Adjustment {
    QString     type;
    QVariantMap parameters;
    QString     name;
};

struct Recipe {
    QList<Adjustment> layers;
    QStringList       skipped;
    QString           error;
};

Recipe readAdobeXmp(const QString &path);
using RenderAdjustment =
    std::function<QImage(const QImage &, const QString &, const QVariantMap &)>;
Recipe matchReference(const QImage &source, const QImage &reference,
                      const RenderAdjustment &render);
} // namespace PhotoCrap::Look

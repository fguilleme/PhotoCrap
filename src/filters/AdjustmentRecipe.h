#pragma once

#include <QImage>
#include <QList>
#include <QStringList>
#include <QVariantMap>

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
Recipe matchReference(const QImage &source, const QImage &reference);
} // namespace PhotoCrap::Look

#include "AdjustmentRecipe.h"

#include <QCoreApplication>
#include <QFile>
#include <QSet>
#include <QXmlStreamReader>
#include <algorithm>
#include <array>
#include <cmath>

namespace PhotoCrap::Look
{
namespace
{
constexpr auto crs = u"http://ns.adobe.com/camera-raw-settings/1.0/";
constexpr auto rdf = u"http://www.w3.org/1999/02/22-rdf-syntax-ns#";

struct CurveField {
    const char *legacy;
    const char *current;
    const char *parameter;
};
constexpr std::array<CurveField, 4> curveFields{
    {{"ToneCurve", "ToneCurvePV2012", "points"},
     {"ToneCurveRed", "ToneCurvePV2012Red", "redPoints"},
     {"ToneCurveGreen", "ToneCurvePV2012Green", "greenPoints"},
     {"ToneCurveBlue", "ToneCurvePV2012Blue", "bluePoints"}}};

bool curveName(const QString &key)
{
    return std::any_of(curveFields.begin(), curveFields.end(), [&](const CurveField &field) {
        return key == field.legacy || key == field.current;
    });
}

QVariantList readCurve(QXmlStreamReader &xml)
{
    QVariantList points;
    int          lastX = -1;
    while (xml.readNextStartElement()) {
        if (xml.namespaceUri() != rdf || xml.name() != u"Seq") {
            xml.skipCurrentElement();
            continue;
        }
        while (xml.readNextStartElement()) {
            if (xml.namespaceUri() != rdf || xml.name() != u"li") {
                xml.skipCurrentElement();
                continue;
            }
            const auto pair = xml.readElementText().split(',');
            bool       xOk = false, yOk = false;
            const int  x = pair.value(0).toInt(&xOk), y = pair.value(1).toInt(&yOk);
            if (pair.size() != 2 || !xOk || !yOk || x <= lastX || x > 255 || y < 0 || y > 255) {
                xml.raiseError(QCoreApplication::translate("PhotoLook", "Courbe XMP invalide."));
                return {};
            }
            points.append(QVariantMap{{"x", x}, {"y", y}});
            lastX = x;
        }
    }
    if (points.size() < 2 && !xml.hasError())
        xml.raiseError(QCoreApplication::translate("PhotoLook", "Courbe XMP invalide."));
    return points;
}

void readDescription(QXmlStreamReader &xml, QMap<QString, QString> &values,
                     QMap<QString, QVariantList> &curves)
{
    for (const auto &attr : xml.attributes())
        if (attr.namespaceUri() == crs)
            values.insert(attr.name().toString(), attr.value().toString());
    while (xml.readNextStartElement()) {
        if (xml.namespaceUri() != crs) {
            xml.skipCurrentElement();
            continue;
        }
        const QString key = xml.name().toString();
        if (curveName(key)) {
            curves.insert(key, readCurve(xml));
        } else {
            // Nested masks, profiles and preset metadata have their own scopes.
            // Only direct scalar settings describe the global development.
            values.insert(key, xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed());
        }
    }
}
} // namespace

Recipe readAdobeXmp(const QString &path)
{
    Recipe           result;
    QFile            file(path);
    constexpr qint64 maxBytes = 8 * 1024 * 1024;
    if (!file.open(QIODevice::ReadOnly) || file.size() > maxBytes) {
        result.error =
            QCoreApplication::translate("PhotoLook", "Fichier XMP illisible ou supérieur à 8 Mio.");
        return result;
    }
    QXmlStreamReader            xml(file.read(maxBytes + 1));
    QMap<QString, QString>      values;
    QMap<QString, QVariantList> curves;
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::DTD)
            xml.raiseError(QCoreApplication::translate(
                "PhotoLook", "La déclaration DTD est interdite dans un XMP."));
        if (!xml.isStartElement() || xml.namespaceUri() != rdf || xml.name() != u"RDF")
            continue;
        while (xml.readNextStartElement()) {
            if (xml.namespaceUri() == rdf && xml.name() == u"Description")
                readDescription(xml, values, curves);
            else
                xml.skipCurrentElement();
        }
    }
    if (xml.hasError()) {
        result.error = QCoreApplication::translate("PhotoLook", "XMP invalide à la ligne %1 (%2).")
                           .arg(xml.lineNumber())
                           .arg(xml.errorString());
        return result;
    }
    if (values.value("HasSettings").compare("false", Qt::CaseInsensitive) == 0 ||
        values.value("HasSettings") == "0")
        return result;
    QSet<QString> handled;
    const auto    scalar = [&](QVariantMap &params, const char *dest, QStringList keys, double min,
                            double max) {
        for (const auto &key : keys)
            handled.insert(key);
        for (const auto &key : keys) {
            if (!values.contains(key))
                continue;
            bool         ok    = false;
            const double value = values[key].toDouble(&ok);
            if (!ok || !std::isfinite(value) || value < min || value > max)
                result.skipped.append(key);
            else if (value != 0)
                params.insert(QString::fromLatin1(dest), value);
            break;
        }
    };
    QVariantMap light, color, tone;
    scalar(light, "exposure", {"Exposure2012", "Exposure"}, -5, 5);
    scalar(light, "contrast", {"Contrast2012", "Contrast"}, -100, 100);
    scalar(light, "highlights", {"Highlights2012"}, -100, 100);
    scalar(light, "shadows", {"Shadows2012"}, -100, 100);
    scalar(light, "whites", {"Whites2012"}, -100, 100);
    scalar(light, "blacks", {"Blacks2012"}, -100, 100);
    scalar(color, "vibrance", {"Vibrance"}, -100, 100);
    scalar(color, "saturation", {"Saturation"}, -100, 100);
    scalar(color, "temperature", {"IncrementalTemperature"}, -100, 100);
    scalar(color, "tint", {"IncrementalTint"}, -100, 100);
    if (!light.isEmpty())
        result.layers.append({"light", light, {}});
    if (!color.isEmpty())
        result.layers.append({"color", color, {}});
    const auto gray = values.value("ConvertToGrayscale").toLower();
    handled.insert("ConvertToGrayscale");
    if (gray == "true" || gray == "1")
        result.layers.append({"blackWhite", {}, {}});
    for (const auto &field : curveFields) {
        auto curve = curves.constFind(field.current);
        if (curve == curves.cend())
            curve = curves.constFind(field.legacy);
        if (curve == curves.cend())
            continue;
        const auto &points   = curve.value();
        bool        identity = true;
        for (const auto &p : points) {
            const auto pt = p.toMap();
            identity &= pt["x"] == pt["y"];
        }
        if (identity && points.first().toMap()["x"].toInt() == 0 &&
            points.last().toMap()["x"].toInt() == 255)
            continue;
        tone.insert(field.parameter, points);
    }
    if (!tone.isEmpty())
        result.layers.append({"curves", tone, {}});
    QVariantMap texture, grading;
    scalar(texture, "amount", {"Texture"}, -100, 100);
    if (!texture.isEmpty())
        result.layers.append({"texture", texture, {}});
    scalar(grading, "shadowHue", {"SplitToningShadowHue"}, 0, 360);
    scalar(grading, "highlightHue", {"SplitToningHighlightHue"}, 0, 360);
    scalar(grading, "shadowSaturation", {"SplitToningShadowSaturation"}, 0, 100);
    scalar(grading, "highlightSaturation", {"SplitToningHighlightSaturation"}, 0, 100);
    scalar(grading, "balance", {"SplitToningBalance"}, -100, 100);
    if (result.skipped.contains("SplitToningShadowHue"))
        grading.remove("shadowSaturation");
    if (result.skipped.contains("SplitToningHighlightHue"))
        grading.remove("highlightSaturation");
    if (grading.contains("shadowSaturation") || grading.contains("highlightSaturation"))
        result.layers.append({"splitToning", grading, {}});
    QVariantMap sharp;
    scalar(sharp, "amount", {"Sharpness"}, 0, 150);
    if (!sharp.isEmpty()) {
        sharp.insert("radius", 1.);
        scalar(sharp, "radius", {"SharpenRadius"}, .5, 3.);
        result.layers.append({"unsharpMask", sharp, {}});
    }
    static const QSet<QString> metadata{"Version",
                                        "ProcessVersion",
                                        "PresetType",
                                        "Cluster",
                                        "UUID",
                                        "Name",
                                        "ShortName",
                                        "SortName",
                                        "Group",
                                        "Description",
                                        "Copyright",
                                        "ContactInfo",
                                        "RawFileName",
                                        "HasSettings",
                                        "HasCrop",
                                        "AlreadyApplied",
                                        "CameraModelRestriction"};
    for (const auto &[key, value] : values.asKeyValueRange()) {
        bool         number = false;
        const double n      = value.toDouble(&number);
        if ((key == "ToneCurveName" || key == "ToneCurveName2012") &&
            (value.isEmpty() || value == "Linear" || !curves.isEmpty()))
            continue;
        if (handled.contains(key) || metadata.contains(key) || key.startsWith("Supports") ||
            (number && n == 0.) || value.compare("false", Qt::CaseInsensitive) == 0 ||
            (key == "WhiteBalance" && value == "As Shot"))
            continue;
        result.skipped.append(key);
    }
    result.skipped.removeDuplicates();
    return result;
}
} // namespace PhotoCrap::Look

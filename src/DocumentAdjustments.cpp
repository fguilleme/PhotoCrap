#include "Document.h"
#include "ImagePixels.h"
#include "ImageUtils.h"
#include "filters/SelectiveColorFilter.h"
#include "render/LayerVisibility.h"

#include <QPainter>
#include <QSet>
#include <QtMath>
#include <cmath>

void Document::addAdjustmentLayer(const QString &_type)
{
    const QMap<QString, QString> names{{"light", tr("Lumière")},
                                       {"color", tr("Couleur")},
                                       {"solidColor", tr("Couleur unie")},
                                       {"brightnessContrast", tr("Luminosité / Contraste")},
                                       {"levels", tr("Niveaux")},
                                       {"curves", tr("Courbes")},
                                       {"exposure", tr("Exposition")},
                                       {"vibrance", tr("Vibrance")},
                                       {"hueSaturation", tr("Teinte / Saturation")},
                                       {"colorBalance", tr("Balance des couleurs")},
                                       {"blackWhite", tr("Noir et blanc")},
                                       {"photoFilter", tr("Filtre photo")},
                                       {"channelMixer", tr("Mélangeur de couches")},
                                       {"colorLookup", tr("Style colorimétrique (LUT)")},
                                       {"invert", tr("Négatif")},
                                       {"posterize", tr("Isohélie")},
                                       {"threshold", tr("Seuil")},
                                       {"gradientMap", tr("Courbe de transfert de dégradé")},
                                       {"selectiveColor", tr("Correction sélective")},
                                       {"gaussianBlur", tr("Flou gaussien")},
                                       {"motionBlur", tr("Flou directionnel")},
                                       {"boxBlur", tr("Flou de boîte")},
                                       {"unsharpMask", tr("Netteté optimisée")},
                                       {"highPass", tr("Passe-haut")},
                                       {"mosaic", tr("Mosaïque")},
                                       {"noise", tr("Ajout de bruit")}};
    if (!names.contains(_type) || !validSize(width(), height(), int(m_layers.size()) + 1) ||
        !validLayerStorage(width(), height(), 0, hasSelection() ? 1 : 0))
        return;
    QVariantMap params;
    if (_type == "solidColor")
        params = {{"color", QStringLiteral("#000000")}};
    else if (_type == "color")
        params = {{"temperature", 0}, {"tint", 0}, {"vibrance", 0}, {"saturation", 0}};
    else if (_type == "light")
        params = {{"exposure", 0.}, {"contrast", 0}, {"highlights", 0},
                  {"shadows", 0},   {"whites", 0},   {"blacks", 0}};
    else if (_type == "brightnessContrast")
        params = {{"brightness", 0}, {"contrast", 0}};
    else if (_type == "levels")
        params = {{"inputBlack", 0},
                  {"inputWhite", 255},
                  {"gamma", 1.},
                  {"outputBlack", 0},
                  {"outputWhite", 255}};
    else if (_type == "curves")
        params = {{"points", QVariantList{QVariantMap{{"x", 0}, {"y", 0}},
                                          QVariantMap{{"x", 255}, {"y", 255}}}}};
    else if (_type == "exposure")
        params = {{"exposure", 0.}, {"offset", 0.}, {"gamma", 1.}};
    else if (_type == "vibrance")
        params = {{"vibrance", 0}, {"saturation", 0}};
    else if (_type == "hueSaturation")
        params = {{"hue", 0}, {"saturation", 0}, {"lightness", 0}};
    else if (_type == "colorBalance")
        params = {{"cyanRed", 0}, {"magentaGreen", 0}, {"yellowBlue", 0}, {"black", 0}};
    else if (_type == "selectiveColor")
        params = PhotoCrap::Filters::defaultSelectiveColorParameters();
    else if (_type == "blackWhite")
        params = {{"reds", 40},           {"yellows", 60},
                  {"greens", 40},         {"cyans", 60},
                  {"blues", 20},          {"magentas", 80},
                  {"tintEnabled", false}, {"tintColor", QStringLiteral("#d6aa78")}};
    else if (_type == "photoFilter")
        params = {{"preset", 0},
                  {"color", QStringLiteral("#ec8a00")},
                  {"useCustomColor", false},
                  {"density", 25},
                  {"preserveLuminosity", true}};
    else if (_type == "channelMixer")
        params = {{"redRed", 100},       {"redGreen", 0},      {"redBlue", 0},
                  {"greenRed", 0},       {"greenGreen", 100},  {"greenBlue", 0},
                  {"blueRed", 0},        {"blueGreen", 0},     {"blueBlue", 100},
                  {"redConstant", 0},    {"greenConstant", 0}, {"blueConstant", 0},
                  {"monochrome", false}, {"monoRed", 40},      {"monoGreen", 40},
                  {"monoBlue", 20},      {"monoConstant", 0}};
    else if (_type == "colorLookup")
        params = {{"preset", 0}, {"strength", 100}, {"preserveLuminosity", false}};
    else if (_type == "posterize")
        params = {{"levels", 4}};
    else if (_type == "threshold")
        params = {{"level", 128}};
    else if (_type == "gradientMap")
        params = {{"shadowColor", "#101828"}, {"highlightColor", "#f5d9a8"}};
    else if (_type == "gaussianBlur" || _type == "boxBlur")
        params = {{"radius", 8.}};
    else if (_type == "motionBlur")
        params = {{"distance", 20.}, {"angle", 0.}};
    else if (_type == "unsharpMask")
        params = {{"amount", 100.}, {"radius", 2.}, {"threshold", 0}};
    else if (_type == "highPass")
        params = {{"radius", 4.}};
    else if (_type == "mosaic")
        params = {{"cellSize", 12}};
    else if (_type == "noise")
        params = {{"amount", 8.}};
    endStroke();
    const auto before = snapshot(tr("Nouveau calque de réglage"));
    {
        Layer layer;
        layer.name           = names.value(_type);
        layer.kind           = "adjustment";
        layer.adjustmentType = _type;
        layer.parameters     = params;
        layer.mask           = ImageUtils::uniformMask(hasSelection() ? 0 : 255);
        layer.parameters.insert("_maskDensity", 100.);
        layer.parameters.insert("_maskFeather", 0.);
        if (hasSelection())
            layer.mask = selectionMask();
        m_layers.insert(++m_active, layer);
        m_editingMask = false;
        selectOnly(m_active);
    }
    // Publish the new layer metadata without prompting the canvas to compose
    // the full document synchronously on the UI thread.
    commit(before, tr("Nouveau calque de réglage"), true, false);
    // Present a screen-sized result first, then refine the document pixels in
    // the background. Adding a costly filter must never block the QML thread.
    requestPreviewRender(true, true);
    emit imageChanged();
}
void Document::addFrequencySeparation(qreal _radius)
{
    if (!std::isfinite(_radius) || !validSize(width(), height(), int(m_layers.size()) + 2) ||
        !validLayerStorage(width(), height(), 2))
        return;
    PixelPlane  source;
    const auto &active = m_layers[m_active];
    const bool neutralMask = !active.maskEnabled || active.mask.isNull() ||
                             active.parameters.value("_maskDensity", 100.).toDouble() <= 0 ||
                             PhotoCrap::Render::LayerVisibility::uniformMaskValue(active.mask, 255);
    // A neutral mask leaves every native pixel unchanged, including with a
    // feather radius. Recognize it before materializing the source image.
    if (active.kind == "pixel" && neutralMask) {
        if (active.image.size() == m_size)
            source = active.image; // Immutable tile versions are already a frozen source.
        else if (ImageUtils::compact(active.image)) {
            const QImage compact = active.image;
            QImage       pixel(1, 1, compact.format());
            // Match expandedPlane's QColor rounding and colour metadata, but
            // normalize only one pixel instead of allocating the whole canvas.
            pixel.fill(compact.pixelColor(0, 0));
            pixel.setColorSpace(compact.colorSpace());
            source = PixelPlane::uniform(m_size, pixel);
        }
    }
    const bool needsPreparation = source.isNull();
    // A completed native composite is immutable while this request is pending.
    // Its seal still belongs on the worker, but the stack need not be rendered again.
    if (needsPreparation && active.kind != "pixel" && !m_cacheDirty && m_composite.size() == m_size)
        source = PixelPlane(m_composite);
    QSet<QString> usedPairs;
    for (const auto &layer : m_layers)
        for (const auto &key : {QStringLiteral("pairId"), QStringLiteral("_frequencyPairId")}) {
            const auto id = layer.parameters.value(key).toString();
            if (!id.isEmpty())
                usedPairs.insert(id);
        }
    const QString base   = QString::number(m_nextId);
    QString       pairId = base;
    for (int suffix = 1; usedPairs.contains(pairId); ++suffix)
        pairId = base + '-' + QString::number(suffix);
    // Capture the GUI's pair identity: a renderer has its own history counter.
    // Only native versions and values cross to the worker, never this Document.
    auto create = [source, _radius, pairId](Document &_document) mutable {
        if (!_document.operationStep(0, 3))
            return;
        if (source.isNull()) {
            const auto &active = _document.m_layers[_document.m_active];
            QImage      pixels = active.kind == "pixel" ? _document.layerImage(_document.m_active)
                                                        : _document.composite();
            if (active.kind == "pixel" && active.maskEnabled && !active.mask.isNull())
                pixels = applyMask(pixels, active.mask,
                                   active.parameters.value("_maskDensity", 100.).toDouble() / 100.,
                                   active.parameters.value("_maskFeather", 0.).toDouble());
            source = pixels;
        }
        if (source.isNull()) {
            _document.fail(tr("Allocation des pixels impossible."));
            return;
        }
        if (!_document.operationStep(1, 3))
            return;
        if (_document.m_backgroundWorker) {
            // Release the assembled QImage before publication. The new source
            // follows the same preparation budget and disk fallback as imports.
            if (!source.freeze()) {
                _document.fail(tr("Allocation des pixels impossible."));
                return;
            }
            PreparedPlaneBudget budget;
            if (!_document.prepareNativePlane(source, _document.workingFormat(), budget))
                return;
        }
        if (!_document.operationStep(2, 3))
            return;
        _document.edit(tr("Séparation de fréquences"), [&] {
            const QVariantMap parameters{{"radius", qBound(1., _radius, 100.)}, {"pairId", pairId}};
            Layer             low;
            low.name       = tr("Basses fréquences");
            low.kind       = "frequencyLow";
            low.source     = source;
            low.parameters = parameters;
            Layer high;
            high.name       = tr("Hautes fréquences");
            high.kind       = "frequencyHigh";
            high.source     = source;
            high.parameters = parameters;
            high.blend      = 16;
            _document.m_layers.insert(++_document.m_active, low);
            _document.m_layers.insert(++_document.m_active, high);
            _document.m_editingMask = false;
            _document.selectOnly(_document.m_active);
        });
        _document.operationStep(3, 3);
    };
    if (needsPreparation &&
        deferEdit(tr("Séparation de fréquences"), editCost(active.kind != "pixel"), create))
        return;
    create(*this);
}
void Document::resetAdjustmentLayer()
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind != QStringLiteral("adjustment"))
        return;

    const QString type = m_layers[m_active].adjustmentType;
    QVariantMap   parameters;
    if (type == QStringLiteral("solidColor"))
        parameters = {{"color", QStringLiteral("#000000")}};
    else if (type == QStringLiteral("color"))
        parameters = {{"temperature", 0}, {"tint", 0}, {"vibrance", 0}, {"saturation", 0}};
    else if (type == QStringLiteral("light"))
        parameters = {{"exposure", 0.}, {"contrast", 0}, {"highlights", 0},
                      {"shadows", 0},   {"whites", 0},   {"blacks", 0}};
    else if (type == QStringLiteral("brightnessContrast"))
        parameters = {{"brightness", 0}, {"contrast", 0}};
    else if (type == QStringLiteral("levels"))
        parameters = {{"inputBlack", 0},
                      {"inputWhite", 255},
                      {"gamma", 1.},
                      {"outputBlack", 0},
                      {"outputWhite", 255}};
    else if (type == QStringLiteral("curves"))
        parameters = {{"points", QVariantList{QVariantMap{{"x", 0}, {"y", 0}},
                                              QVariantMap{{"x", 255}, {"y", 255}}}},
                      {"blackSample", QVariantList{}},
                      {"graySample", QVariantList{}},
                      {"whiteSample", QVariantList{}}};
    else if (type == QStringLiteral("exposure"))
        parameters = {{"exposure", 0.}, {"offset", 0.}, {"gamma", 1.}};
    else if (type == QStringLiteral("vibrance"))
        parameters = {{"vibrance", 0}, {"saturation", 0}};
    else if (type == QStringLiteral("hueSaturation"))
        parameters = {{"hue", 0}, {"saturation", 0}, {"lightness", 0}};
    else if (type == QStringLiteral("colorBalance"))
        parameters = {{"cyanRed", 0}, {"magentaGreen", 0}, {"yellowBlue", 0}, {"black", 0}};
    else if (type == QStringLiteral("selectiveColor"))
        parameters = PhotoCrap::Filters::defaultSelectiveColorParameters();
    else if (type == QStringLiteral("blackWhite"))
        parameters = {{"reds", 40},           {"yellows", 60},
                      {"greens", 40},         {"cyans", 60},
                      {"blues", 20},          {"magentas", 80},
                      {"tintEnabled", false}, {"tintColor", QStringLiteral("#d6aa78")}};
    else if (type == QStringLiteral("photoFilter"))
        parameters = {{"preset", 0},
                      {"color", QStringLiteral("#ec8a00")},
                      {"useCustomColor", false},
                      {"density", 25},
                      {"preserveLuminosity", true}};
    else if (type == QStringLiteral("channelMixer"))
        parameters = {{"redRed", 100},       {"redGreen", 0},      {"redBlue", 0},
                      {"greenRed", 0},       {"greenGreen", 100},  {"greenBlue", 0},
                      {"blueRed", 0},        {"blueGreen", 0},     {"blueBlue", 100},
                      {"redConstant", 0},    {"greenConstant", 0}, {"blueConstant", 0},
                      {"monochrome", false}, {"monoRed", 40},      {"monoGreen", 40},
                      {"monoBlue", 20},      {"monoConstant", 0}};
    else if (type == QStringLiteral("colorLookup"))
        parameters = {{"preset", 0}, {"strength", 100}, {"preserveLuminosity", false}};
    else if (type == QStringLiteral("posterize"))
        parameters = {{"levels", 4}};
    else if (type == QStringLiteral("threshold"))
        parameters = {{"level", 128}};
    else if (type == QStringLiteral("gradientMap"))
        parameters = {{"shadowColor", "#101828"}, {"highlightColor", "#f5d9a8"}};
    else if (type == QStringLiteral("gaussianBlur") || type == QStringLiteral("boxBlur"))
        parameters = {{"radius", 8.}};
    else if (type == QStringLiteral("motionBlur"))
        parameters = {{"distance", 20.}, {"angle", 0.}};
    else if (type == QStringLiteral("unsharpMask"))
        parameters = {{"amount", 100.}, {"radius", 2.}, {"threshold", 0}};
    else if (type == QStringLiteral("highPass"))
        parameters = {{"radius", 4.}};
    else if (type == QStringLiteral("mosaic"))
        parameters = {{"cellSize", 12}};
    else if (type == QStringLiteral("noise"))
        parameters = {{"amount", 8.}};

    parameters.insert(QStringLiteral("_maskDensity"),
                      m_layers[m_active].parameters.value(QStringLiteral("_maskDensity"), 100.));
    parameters.insert(QStringLiteral("_maskFeather"),
                      m_layers[m_active].parameters.value(QStringLiteral("_maskFeather"), 0.));
    beginLayerPreview();
    m_layers[m_active].parameters = parameters;
    m_layerPreviewChanged         = true;
    m_fullCompositeDirty          = true;
    m_cacheDirty                  = true;
    requestPreviewRender(true, true);
    emit layersChanged();
    finishLayerPreview(tr("Réinitialiser le réglage"));
}

void Document::resetCurveAdjustment()
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].adjustmentType != QStringLiteral("curves"))
        return;
    beginLayerPreview();
    previewLayerParameter(
        QStringLiteral("points"),
        QVariantList{QVariantMap{{"x", 0}, {"y", 0}}, QVariantMap{{"x", 255}, {"y", 255}}});
    for (const QString &key : {QStringLiteral("blackSample"), QStringLiteral("graySample"),
                               QStringLiteral("whiteSample")})
        previewLayerParameter(key, QVariantList{});
    finishLayerPreview(tr("Réinitialiser les courbes"));
}

void Document::applyFilter(const QString &_filter, qreal _amount)
{
    static const QStringList supported{"grayscale", "invert",     "sepia", "brightness",
                                       "contrast",  "saturation", "blur",  "sharpen"};
    if (!supported.contains(_filter) || !std::isfinite(_amount) || !editable())
        return;
    if (m_editingMask || m_layers[m_active].kind != "pixel") {
        fail(tr("Ce filtre destructif nécessite un calque de pixels."));
        return;
    }
    _amount = qBound(-100., _amount, 100.);
    if (deferEdit(tr("Filtre : %1").arg(_filter), editCost(false),
                  [_filter, _amount](Document &d) { d.applyFilter(_filter, _amount); }))
        return;
    edit(tr("Filtre : %1").arg(_filter), [&] {
        if (!operationStep(0, 1000))
            return;
        const auto render = [&]<typename Pixel>() {
            using P               = ImageUtils::PixelAccess<Pixel>;
            const QImage original = m_layers[m_active].image;
            QImage       result   = original.convertToFormat(P::format);
            const QRect  area     = hasSelection() ? m_selection : result.rect();
            if (_filter == "blur" || _filter == "sharpen") {
                const int    radius  = _filter == "sharpen" ? 1 : qBound(1, int(_amount), 30);
                const QImage blurred = blurImage(original, radius);
                if (_filter == "blur") {
                    result = original;
                    QPainter p(&result);
                    p.setCompositionMode(QPainter::CompositionMode_Source);
                    if (hasSelection())
                        p.setClipRegion(m_selectionRegion);
                    else
                        p.setClipRect(area);
                    p.drawImage(0, 0, blurred);
                    p.end();
                    blendSelectionEdit(result, original);
                    m_layers[m_active].image = result;
                    return;
                }
                const QImage smooth = blurred.convertToFormat(P::format);
                for (int y = area.top(); y <= area.bottom(); ++y)
                    for (int x = area.left(); x <= area.right(); ++x) {
                        if (hasSelection() && !m_selectionRegion.contains(QPoint(x, y)))
                            continue;
                        const QColor c = result.pixelColor(x, y), b = smooth.pixelColor(x, y);
                        const auto   value = [](double _a, double _b) {
                            return qBound(0., 2 * _a - _b, 1.);
                        };
                        result.setPixelColor(x, y,
                                             QColor::fromRgbF(value(c.redF(), b.redF()),
                                                              value(c.greenF(), b.greenF()),
                                                              value(c.blueF(), b.blueF()),
                                                              c.alphaF()));
                    }
            } else
                for (int y = area.top(); y <= area.bottom(); ++y) {
                    if ((y % 16) == 0 && !operationStep(y - area.top(), area.height()))
                        return;
                    auto row = reinterpret_cast<Pixel *>(result.scanLine(y));
                    for (int x = area.left(); x <= area.right(); ++x) {
                        if (hasSelection() && !m_selectionRegion.contains(QPoint(x, y)))
                            continue;
                        Pixel        pixel = row[x];
                        double       r = P::red(pixel), g = P::green(pixel), b = P::blue(pixel);
                        const double gray = .2126 * r + .7152 * g + .0722 * b;
                        if (_filter == "invert") {
                            r = 255 - r;
                            g = 255 - g;
                            b = 255 - b;
                        } else if (_filter == "grayscale")
                            r = g = b = gray;
                        else if (_filter == "sepia") {
                            double nr = .393 * r + .769 * g + .189 * b,
                                   ng = .349 * r + .686 * g + .168 * b;
                            b         = .272 * r + .534 * g + .131 * b;
                            r         = nr;
                            g         = ng;
                        } else if (_filter == "brightness") {
                            r += _amount * 2.55;
                            g += _amount * 2.55;
                            b += _amount * 2.55;
                        } else if (_filter == "contrast") {
                            const double f = std::pow((100 + _amount) / 100, 2);
                            r              = (r - 128) * f + 128;
                            g              = (g - 128) * f + 128;
                            b              = (b - 128) * f + 128;
                        } else if (_filter == "saturation") {
                            const double f = 1 + _amount / 100;
                            r              = gray + (r - gray) * f;
                            g              = gray + (g - gray) * f;
                            b              = gray + (b - gray) * f;
                        }
                        row[x] =
                            P::rgba(qBound(0., P::round(r), 255.), qBound(0., P::round(g), 255.),
                                    qBound(0., P::round(b), 255.), P::alpha(pixel));
                    }
                }
            result = result.convertToFormat(P::premultiplied);
            blendSelectionEdit(result, original);
            m_layers[m_active].image = result;
        };
        if (m_channelBits == 16)
            render.template operator()<QRgba64>();
        else
            render.template operator()<QRgb>();
    });
}

#include "Document.h"
#include "ImageUtils.h"
#include "filters/AdjustmentRecipe.h"

#include <QFileInfo>
#include <limits>

namespace
{
QSize sampleSize(QSize size)
{
    if (qMax(size.width(), size.height()) > 384)
        size = size.scaled(384, 384, Qt::KeepAspectRatio).expandedTo(QSize(1, 1));
    return size;
}
} // namespace

bool Document::appendLookLayers(const PhotoCrap::Look::Recipe &recipe, const QString &source,
                                const QString &label)
{
    if (recipe.layers.isEmpty() ||
        !validSize(width(), height(), int(m_layers.size() + recipe.layers.size())) ||
        !validLayerStorage(width(), height()))
        return false;
    finishTextEdit();
    endStroke();
    finishLayerPreview();
    const auto before = snapshot(label);
    for (const auto &adjustment : recipe.layers) {
        Layer layer;
        layer.kind           = "adjustment";
        layer.adjustmentType = adjustment.type;
        layer.name =
            (adjustment.name.isEmpty() ? adjustmentName(adjustment.type) : adjustment.name) +
            QStringLiteral(" — ") + source;
        layer.parameters = adjustmentDefaults(adjustment.type);
        layer.parameters.insert(adjustment.parameters);
        layer.parameters.insert("_maskDensity", 100.);
        layer.parameters.insert("_maskFeather", 0.);
        layer.mask = ImageUtils::uniformMask(255);
        m_layers.append(std::move(layer));
    }
    m_active      = int(m_layers.size()) - 1;
    m_editingMask = false;
    selectOnly(m_active);
    commit(before, label, true, false);
    requestPreviewRender(true, true);
    emit imageChanged();
    return true;
}

void Document::importAdobeXmp(const QUrl &url)
{
    if (pixelEditBusy() || rawLoading() || transformActive() || m_layers.isEmpty())
        return;
    const QString path   = url.toLocalFile();
    const auto    recipe = PhotoCrap::Look::readAdobeXmp(path);
    if (!recipe.error.isEmpty()) {
        fail(recipe.error);
        return;
    }
    if (!recipe.layers.isEmpty() && !appendLookLayers(recipe, QFileInfo(path).completeBaseName(),
                                                      tr("Importer des réglages XMP")))
        return;
    if (!recipe.skipped.isEmpty())
        emit xmpSettingsSkipped(int(recipe.layers.size()), recipe.skipped);
    else if (recipe.layers.isEmpty())
        emit statusMessage(tr("Ce XMP ne contient aucun réglage global à appliquer."));
    else
        emit statusMessage(tr("Import XMP terminé (%1 calques).").arg(recipe.layers.size()));
}

void Document::matchReferenceImage(const QUrl &url)
{
    if (pixelEditBusy() || rawLoading() || transformActive() || m_layers.isEmpty())
        return;
    const auto path = url.toLocalFile();
    if (path.isEmpty()) {
        fail(tr("Choisissez un fichier local."));
        return;
    }
    const auto label  = tr("Reproduire le rendu d’une image");
    auto       recipe = std::make_shared<PhotoCrap::Look::Recipe>();
    deferOperation(
        label, std::numeric_limits<qint64>::max(),
        [path, recipe](Document &worker) {
            if (!worker.operationStep(0, 1))
                return;
            const auto source =
                renderPreviewSnapshot(worker.m_layers, worker.m_size, sampleSize(worker.m_size),
                                      true, worker.m_scratchCache);
            // Decode with the same orientation, RAW development and sRGB conversion as opening a
            // document. Sample both images with the canvas proxy's nearest-pixel filter so texture
            // in the reference does not look like a change in contrast.
            auto reference = worker.readImage(path);
            if (reference.isNull())
                return;
            reference = reference.scaled(sampleSize(reference.size()), Qt::IgnoreAspectRatio,
                                         Qt::FastTransformation);
            if (!worker.operationStep(4, 4))
                return;
            *recipe = PhotoCrap::Look::matchReference(source, reference);
            if (!recipe->error.isEmpty())
                worker.fail(recipe->error);
        },
        false,
        [this, recipe, path, label] {
            if (recipe->layers.isEmpty())
                emit statusMessage(tr("Le rendu de la photo est déjà proche de la référence."));
            else
                appendLookLayers(*recipe, QFileInfo(path).completeBaseName(), label);
        });
}

#include "Document.h"
#include "ImageUtils.h"

#include <QtConcurrentRun>
#include <cmath>

bool Document::prepareFrequencyRetouch()
{
    if (m_frequencyRetouchBusy || m_active < 0 || m_active >= m_layers.size())
        return false;
    endStroke();
    finishLayerPreview();
    const auto &active = m_layers[m_active];
    const bool  isLow  = active.kind == QStringLiteral("frequencyLow");
    if (!isLow && active.kind != QStringLiteral("frequencyHigh"))
        return false;
    const QString pairId = active.parameters.value("pairId").toString();
    const QString otherKind =
        isLow ? QStringLiteral("frequencyHigh") : QStringLiteral("frequencyLow");
    int counterpart = -1;
    for (int i = 0; i < m_layers.size(); ++i) {
        if (m_layers[i].kind == otherKind && !pairId.isEmpty() &&
            m_layers[i].parameters.value("pairId").toString() == pairId) {
            if (counterpart >= 0)
                return fail(tr("Cette paire est ambiguë. Conservez un seul calque de chaque "
                               "fréquence avant de préparer la retouche."));
            counterpart = i;
        }
    }
    if (counterpart < 0)
        return fail(
            tr("La préparation nécessite les deux calques liés de la séparation de fréquences."));
    const int   lowIndex  = isLow ? m_active : counterpart;
    const int   highIndex = isLow ? counterpart : m_active;
    const auto &low       = m_layers[lowIndex];
    const auto &high      = m_layers[highIndex];
    for (const Layer *layer : {&low, &high}) {
        if (layer->locked)
            return fail(tr("Ce calque est verrouillé."));
        for (const auto &group : std::as_const(m_layers))
            if (group.kind == QStringLiteral("group") && group.locked &&
                !layer->groupId.isEmpty() && group.groupId == layer->groupId)
                return fail(tr("Ce groupe est verrouillé."));
    }
    if (low.source.isNull() || high.source.isNull())
        return fail(tr("Source de séparation de fréquences manquante."));
    const double radius = low.parameters.value("radius", 8).toDouble();
    if (!std::isfinite(radius) || radius != high.parameters.value("radius", 8).toDouble())
        return fail(tr("Les deux fréquences doivent utiliser le même rayon."));
    // Retain both sources so returning to a dynamic pair remains possible
    // after edits and project reload, independently of the undo history.
    if (!validLayerStorage(width(), height(), 2))
        return false;
    const QString  label     = tr("Préparer la retouche des fréquences");
    const Snapshot before    = snapshot(label);
    const quint64  session   = m_documentSession;
    const QImage   lowSource = low.source, highSource = high.source;
    const auto     cancel    = std::make_shared<std::atomic_bool>(false);
    m_frequencyRetouchCancel = cancel;
    m_frequencyRetouchBusy   = true;
    emit  frequencyRetouchBusyChanged();
    auto *watcher = new QFutureWatcher<QVector<QImage>>(this);
    connect(
        watcher, &QFutureWatcher<QVector<QImage>>::finished, this,
        [this, watcher, cancel, before, session, lowIndex, highIndex, label] {
            const auto images = watcher->result();
            watcher->deleteLater();
            const bool current =
                !cancel->load() && session == m_documentSession && before.id == m_stateId &&
                !m_previewingLayer && !m_drawing && lowIndex < m_layers.size() &&
                highIndex < m_layers.size() &&
                m_layers[lowIndex].historyId == before.layers[lowIndex].historyId &&
                m_layers[highIndex].historyId == before.layers[highIndex].historyId &&
                m_layers[lowIndex].parameters == before.layers[lowIndex].parameters &&
                m_layers[highIndex].parameters == before.layers[highIndex].parameters;
            if (current && images.size() == 2 && !images[0].isNull() && !images[1].isNull()) {
                for (const int index : {lowIndex, highIndex}) {
                    auto &layer = m_layers[index];
                    layer.kind  = QStringLiteral("pixel");
                    layer.image = images[index == lowIndex ? 0 : 1];
                    layer.parameters.insert("_frequencyPairId", layer.parameters.value("pairId"));
                    layer.parameters.insert("_frequencyBand", index == lowIndex ? "low" : "high");
                    layer.parameters.insert("_frequencyRadius",
                                            layer.parameters.value("radius", 8));
                    layer.parameters.remove("pairId");
                    layer.parameters.remove("radius");
                }
                if (m_active == lowIndex || m_active == highIndex)
                    m_editingMask = false;
                m_frequencyCache.clear();
                commit(before, label, true, false);
                requestPreviewRender(true, true);
                emit imageChanged();
                emit statusMessage(tr("Fréquences prêtes : retouchez les couleurs sur Basses "
                                      "fréquences et la texture sur Hautes fréquences."));
            } else if (current)
                fail(tr("Impossible de préparer les pixels de la séparation de fréquences."));
            m_frequencyRetouchBusy = false;
            m_frequencyRetouchCancel.reset();
            emit frequencyRetouchBusyChanged();
        });
    // Only native image data and a numeric radius cross the thread boundary.
    watcher->setFuture(QtConcurrent::run([lowSource, highSource, radius, cancel] {
        QVector<QImage> images;
        if (cancel->load())
            return images;
        Document renderer;
        renderer.m_historyBrushSource = {};
        Layer layer;
        layer.source     = lowSource;
        layer.kind       = QStringLiteral("frequencyLow");
        layer.parameters = {{"radius", radius}};
        images.append(renderer.frequencyImage(layer).convertToFormat(
            ImageUtils::workingFormat(layer.source)));
        if (cancel->load())
            return QVector<QImage>{};
        layer.source = highSource;
        layer.kind   = QStringLiteral("frequencyHigh");
        images.append(renderer.frequencyImage(layer).convertToFormat(
            ImageUtils::workingFormat(layer.source)));
        return images;
    }));
    return true;
}

void Document::cancelFrequencyRetouch()
{
    if (m_frequencyRetouchCancel)
        m_frequencyRetouchCancel->store(true);
}

bool Document::restoreDynamicFrequency()
{
    if (m_frequencyRetouchBusy || m_active < 0 || m_active >= m_layers.size())
        return false;
    endStroke();
    finishLayerPreview();
    const auto   &active = m_layers[m_active];
    const QString band   = active.parameters.value("_frequencyBand").toString();
    const QString pairId = active.parameters.value("_frequencyPairId").toString();
    if (active.kind != QStringLiteral("pixel") ||
        (band != QStringLiteral("low") && band != QStringLiteral("high")))
        return false;
    const QString otherBand =
        band == QStringLiteral("low") ? QStringLiteral("high") : QStringLiteral("low");
    int counterpart   = -1;
    int sameBandCount = 0;
    for (int i = 0; i < m_layers.size(); ++i) {
        const auto &layer = m_layers[i];
        if (layer.kind == QStringLiteral("pixel") && !pairId.isEmpty() &&
            layer.parameters.value("_frequencyPairId").toString() == pairId &&
            layer.parameters.value("_frequencyBand").toString() == band)
            ++sameBandCount;
        if (layer.kind == QStringLiteral("pixel") && !pairId.isEmpty() &&
            layer.parameters.value("_frequencyPairId").toString() == pairId &&
            layer.parameters.value("_frequencyBand").toString() == otherBand) {
            if (counterpart >= 0)
                return fail(tr("Cette paire est ambiguë. Conservez un seul calque de chaque "
                               "fréquence avant de revenir au rayon."));
            counterpart = i;
        }
    }
    if (sameBandCount > 1)
        return fail(tr("Cette paire est ambiguë. Conservez un seul calque de chaque "
                       "fréquence avant de revenir au rayon."));
    if (counterpart < 0)
        return fail(tr("Le retour au rayon nécessite les deux calques préparés et leurs sources "
                       "d’origine."));
    const double radius = active.parameters.value("_frequencyRadius").toDouble();
    for (const int index : {m_active, counterpart}) {
        const auto &layer = m_layers[index];
        if (layer.locked)
            return fail(tr("Ce calque est verrouillé."));
        for (const auto &group : std::as_const(m_layers))
            if (group.kind == QStringLiteral("group") && group.locked && !layer.groupId.isEmpty() &&
                group.groupId == layer.groupId)
                return fail(tr("Ce groupe est verrouillé."));
        if (layer.source.isNull())
            return fail(tr("Source de séparation de fréquences manquante."));
        if (!std::isfinite(radius) || radius < 1 ||
            radius != layer.parameters.value("_frequencyRadius").toDouble())
            return fail(tr("Les deux fréquences doivent utiliser le même rayon."));
    }
    const QString label  = tr("Revenir au réglage du rayon");
    const auto    before = snapshot(label);
    for (const int index : {m_active, counterpart}) {
        auto &layer = m_layers[index];
        layer.kind  = layer.parameters.value("_frequencyBand").toString() == QStringLiteral("low")
                          ? QStringLiteral("frequencyLow")
                          : QStringLiteral("frequencyHigh");
        layer.image = {};
        layer.parameters.insert("pairId", pairId);
        layer.parameters.insert("radius", radius);
        layer.parameters.remove("_frequencyBand");
        layer.parameters.remove("_frequencyPairId");
        layer.parameters.remove("_frequencyRadius");
    }
    m_frequencyCache.clear();
    m_editingMask = false;
    commit(before, label, true, false);
    requestPreviewRender(true, true);
    emit imageChanged();
    return true;
}

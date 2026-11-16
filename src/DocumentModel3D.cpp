#include "Document.h"
#include "masks/VectorMask.h"
#include "model3d/Model3D.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <limits>
#include <utility>

using namespace PhotoCrap;

namespace
{
QVariantMap modelParameters(const Layer &_layer, const QVariantMap &_pose,
                            const QVariantMap &_light)
{
    auto parameters = _layer.parameters;
    auto pose       = parameters.value("pose3d").toMap();
    // Preserve the document-owned basis from whole-image transforms.
    for (const auto &key : {"x", "y", "scale", "pitch", "yaw", "roll"})
        pose[key] = _pose.value(key);
    parameters["pose3d"]  = pose;
    parameters["light3d"] = _light;
    return parameters;
}
} // namespace

bool Document::importModel3D(const QUrl &_url)
{
    if (!_url.isLocalFile() || !QFileInfo(_url.toLocalFile()).isFile())
        return fail(tr("Choisissez un fichier GLB local."));
    const QFileInfo info(_url.toLocalFile());
    return importModel3DWithLoader(
        [path = info.absoluteFilePath()](const auto &keepGoing, QString *error) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly) || file.size() > Model3D::MaxFileBytes) {
                *error = tr("Impossible de lire le GLB (%L1 Mio maximum).")
                             .arg(Model3D::MaxFileBytes / (1024 * 1024));
                return std::shared_ptr<const Model3D::Asset>{};
            }
            return Model3D::load(file.read(Model3D::MaxFileBytes + 1), keepGoing, error);
        },
        info.completeBaseName(), {});
}

bool Document::importModel3DData(const QByteArray &_bytes, const QString &_name,
                                 const QRectF &_reference)
{
    return importModel3DWithLoader(
        [_bytes](const auto &keepGoing, QString *error) {
            return Model3D::load(_bytes, keepGoing, error);
        },
        _name, _reference);
}

bool Document::importDownloadedModel3D(std::shared_ptr<QTemporaryFile> _file, const QString &_name,
                                       const QRectF &_reference, int _triangleTarget)
{
    return importModel3DWithLoader(
        [file = std::move(_file), _triangleTarget](const auto &keepGoing, QString *error) {
            return Model3D::loadTemporary(file, keepGoing, error, _triangleTarget);
        },
        _name, _reference);
}

QVariantMap Document::model3DStatistics(int _index) const
{
    if (_index < 0 || _index >= m_layers.size() || !m_layers.at(_index).model3D)
        return {};
    return Model3D::statistics(*m_layers.at(_index).model3D);
}

bool Document::importModel3DWithLoader(ModelLoader _load, const QString &_name,
                                       const QRectF &_reference)
{
    if (!canImportModel3D())
        return false;
    const auto label = tr("Importer un modèle 3D");
    if (deferEdit(label, std::numeric_limits<qint64>::max(),
                  [_load, _name, _reference](Document &worker) {
                      worker.insertModel3D(_load, _name, _reference);
                  }))
        return true;
    return insertModel3D(_load, _name, _reference);
}

bool Document::canImportModel3D()
{
    if (rawLoading() || pixelEditBusy() || transformActive())
        return fail(tr("Terminez ou annulez l’opération en cours."));
    finishTextEdit();
    endStroke();
    finishLayerPreview();
    if (!validSize(width(), height(), int(m_layers.size() + 1)) ||
        !validLayerStorage(width(), height(), 1))
        return false;
    return true;
}

bool Document::insertModel3D(const ModelLoader &_load, const QString &_name,
                             const QRectF &_reference)
{
    const auto label     = tr("Importer un modèle 3D");
    const auto keepGoing = [this] { return operationStep(0, 1); };
    QString    error;
    auto       asset = _load(keepGoing, &error);
    if (!asset)
        return keepGoing() ? fail(error.isEmpty() ? tr("Le GLB est invalide ou contient des "
                                                       "données non prises en charge.")
                                                  : error)
                           : false;

    auto                         lighting = Model3D::light();
    qsizetype                    bytes    = Model3D::storageBytes(*asset);
    QSet<const Model3D::Asset *> seen;
    for (const auto &layer : std::as_const(m_layers)) {
        if (!layer.model3D || seen.contains(layer.model3D.get()))
            continue;
        seen.insert(layer.model3D.get());
        bytes += Model3D::storageBytes(*layer.model3D);
        lighting = layer.parameters.value("light3d").toMap();
    }
    if (bytes > Model3D::MaxSceneBytes)
        return fail(tr("La limite de mémoire des modèles 3D est atteinte."));

    Layer layer;
    layer.name       = _name.left(256);
    layer.kind       = "model3d";
    layer.model3D    = std::move(asset);
    auto pose        = _reference.isEmpty() ? QVariantMap{}
                                            : Model3D::fittedPose(*layer.model3D, _reference, keepGoing);
    layer.parameters = {{"pose3d", pose.isEmpty() ? Model3D::pose(m_size) : std::move(pose)},
                        {"light3d", lighting}};
    if (const int target = Model3D::triangleTarget(*layer.model3D); target > 0)
        layer.parameters["triangleTarget"] = target;
    layer.image =
        Model3D::render(*layer.model3D, layer.parameters, m_size, workingFormat(), keepGoing);
    if (layer.image.isNull())
        return keepGoing() ? fail(tr("Impossible de calculer le rendu du modèle 3D.")) : false;
    const auto before    = snapshot(label);
    const int  insertion = qBound(0, m_active + 1, int(m_layers.size()));
    if (insertion > 0 && insertion < m_layers.size() &&
        m_layers[insertion - 1].groupId == m_layers[insertion].groupId)
        layer.groupId = m_layers[insertion].groupId;
    m_layers.insert(insertion, std::move(layer));
    m_active       = insertion;
    m_editingMask  = false;
    m_maskOnlyView = false;
    selectOnly(m_active);
    if (!m_backgroundWorker)
        commit(before, label);
    return true;
}

void Document::previewModel3D(int _index, const QVariantMap &_pose, const QVariantMap &_light)
{
    if (_index != m_active || _index < 0 || _index >= m_layers.size() ||
        !m_layers[_index].model3D || m_editingMask || rawLoading() || pixelEditBusy() ||
        transformActive() || m_previewingLayer || m_drawing)
        return;
    const auto &layer      = m_layers[_index];
    auto        parameters = modelParameters(layer, _pose, _light);
    if (!Model3D::normalize(parameters) ||
        (parameters.value("pose3d") != layer.parameters.value("pose3d") &&
         !penShapeEditable(_index)))
        return;
    queueModel3DPreview(_index, std::move(parameters));
}

bool Document::model3DMoveTarget() const
{
    return m_tool == "move" && !m_editingMask && m_active >= 0 && m_active < m_layers.size() &&
           m_layers[m_active].model3D;
}

bool Document::model3DPaintTarget() const
{
    return (m_tool == "brush" || m_tool == "pencil") && !m_editingMask && !editingVectorMask() &&
           m_active >= 0 && m_active < m_layers.size() && m_layers.at(m_active).model3D;
}

void Document::beginModel3DPaint(const QPointF &point)
{
    const auto &layer = m_layers.at(m_active);
    if (!layer.visible || rawLoading() || pixelEditBusy() || transformActive() ||
        !penShapeEditable(m_active) || m_brushOpacity <= 0. || m_brushFlow <= 0. ||
        m_color.alpha() == 0)
        return;
    if (layer.modelPaint.size() >= Model3D::MaxPaintDabs) {
        fail(tr("Ce calque 3D a atteint la limite de peinture."));
        return;
    }
    if (m_modelPaintAsset.lock() != layer.model3D) {
        m_modelPaintPicking = Model3D::paintPicking(*layer.model3D);
        m_modelPaintAsset   = layer.model3D;
    }
    cancelModel3DPreview();
    m_modelPaintDraft   = layer.modelPaint;
    m_drawing           = true;
    m_distanceToNextDab = qMax(.5, m_brushSize * m_brushSpacing);
    addModel3DPaintDab(point);
    if (m_modelPaintDraft.size() != layer.modelPaint.size())
        queueModel3DPreview(m_active, layer.parameters, false, m_modelPaintDraft);
}

void Document::addModel3DPaintDab(const QPointF &point)
{
    if (!QRectF(QPointF{}, m_size).contains(point) ||
        m_modelPaintDraft.size() >= Model3D::MaxPaintDabs)
        return;
    const qreal coverage =
        hasSelection() ? selectionCoverage(int(point.x()), int(point.y())) / 255. : 1.;
    const qreal       opacity = coverage * m_brushOpacity * m_brushFlow * m_color.alphaF();
    Model3D::PaintDab dab;
    if (opacity > 0. &&
        Model3D::paintDab(*m_modelPaintPicking, m_layers.at(m_active).parameters, point,
                          m_brushSize / 2., m_tool == "pencil" ? 1. : m_brushHardness, m_color,
                          opacity, dab))
        m_modelPaintDraft.append(dab);
}

void Document::continueModel3DPaint(const QPointF &point)
{
    const QLineF line(m_last, point);
    const auto   length = line.length();
    const auto   before = m_modelPaintDraft.size();
    if (length <= 0.)
        return;
    for (; m_distanceToNextDab <= length && m_modelPaintDraft.size() < Model3D::MaxPaintDabs;
         m_distanceToNextDab += qMax(.5, m_brushSize * m_brushSpacing))
        addModel3DPaintDab(line.pointAt(m_distanceToNextDab / length));
    m_distanceToNextDab -= length;
    m_last = point;
    if (m_modelPaintDraft.size() != before)
        queueModel3DPreview(m_active, m_layers.at(m_active).parameters, false, m_modelPaintDraft);
}

void Document::finishModel3DPaint(bool cancel)
{
    m_drawing         = false;
    const auto  paint = std::exchange(m_modelPaintDraft, {});
    const auto &layer = m_layers.at(m_active);
    if (!cancel && paint.size() != layer.modelPaint.size() &&
        applyModel3DParameters(m_active, layer.parameters, tr("Peinture 3D"), false, paint) &&
        operationBusy())
        return;
    cancelModel3DPreview();
}

void Document::previewModel3DMove(const QPointF &_position)
{
    const auto delta = _position - m_anchor;
    if (delta.isNull() && !m_model3DPreview)
        return;
    Layer      layer       = m_layers[m_active];
    const auto translation = QTransform::fromTranslate(delta.x(), delta.y());
    if (!Model3D::transformPlacement(layer.parameters, translation) ||
        !Masks::transformVector(layer, translation, m_size))
        return;
    queueModel3DPreview(m_active, std::move(layer.parameters), m_moveCopy);
}

void Document::finishModel3DMove()
{
    const auto preview = m_model3DPreview;
    if (!preview || preview->layer != m_active)
        return;
    if (!applyModel3DParameters(preview->layer, preview->parameters, tr("Déplacement"),
                                preview->copy) ||
        !operationBusy())
        cancelModel3DPreview();
}

void Document::queueModel3DPreview(int _index, QVariantMap _parameters, bool _copy,
                                   const Model3D::Paint &paint)
{
    if (m_model3DPreview && m_model3DPreview->layer == _index && m_model3DPreview->copy == _copy &&
        m_model3DPreview->parameters == _parameters && paint.isEmpty() &&
        m_model3DPreview->paint.isEmpty())
        return;
    const quint64 firstGeneration = m_model3DPreview && m_model3DPreview->layer == _index
                                        ? m_model3DPreview->firstGeneration
                                        : m_previewRequestGeneration + 1;
    m_model3DPreview              = std::make_shared<Model3DPreview>(
        Model3DPreview{_index, std::move(_parameters), firstGeneration, _copy, paint});
    requestPreviewRender(true, false, false);
}

void Document::cancelModel3DPreview()
{
    if (!m_model3DPreview)
        return;
    m_model3DPreview.reset();
    requestPreviewRender(true, true, false);
}

bool Document::editModel3D(int _index, const QVariantMap &_pose, const QVariantMap &_light)
{
    if (_index < 0 || _index >= m_layers.size() || !m_layers[_index].model3D || rawLoading() ||
        pixelEditBusy() || transformActive() || m_drawing)
        return false;
    const auto &selected = m_layers[_index];
    auto        edited   = modelParameters(selected, _pose, _light);
    if (!Model3D::normalize(edited))
        return fail(tr("Paramètres du calque 3D invalides."));
    const bool poseChanged  = edited.value("pose3d") != selected.parameters.value("pose3d");
    const auto light        = edited.value("light3d");
    const bool lightChanged = light != selected.parameters.value("light3d");
    if (!poseChanged && !lightChanged)
        return true;
    if (poseChanged && !penShapeEditable(_index))
        return false;
    const auto label = lightChanged ? tr("Modifier l’éclairage 3D") : tr("Modifier le calque 3D");
    return applyModel3DParameters(_index, edited, label);
}

bool Document::applyModel3DParameters(int _index, const QVariantMap &_parameters,
                                      const QString &_label, bool _copy,
                                      const Model3D::Paint &paint)
{
    const auto &current = m_layers[_index].parameters;
    if (!_copy && current == _parameters && paint.isEmpty())
        return true;
    if (deferEdit(_label, std::numeric_limits<qint64>::max(),
                  [_index, _parameters, _label, _copy, paint](Document &worker) {
                      worker.applyModel3DParameters(_index, _parameters, _label, _copy, paint);
                  }))
        return true;

    const auto light        = _parameters.value("light3d");
    const bool lightChanged = light != current.value("light3d");
    auto       layers       = m_layers;
    if (_copy) {
        auto duplicate = layers[_index];
        duplicate.name += tr(" — copie");
        duplicate.historyId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        layers.insert(++_index, std::move(duplicate));
    }
    const int total =
        lightChanged ? int(std::count_if(layers.cbegin(), layers.cend(),
                                         [](const Layer &layer) { return bool(layer.model3D); }))
                     : 1;
    int completed = 0;
    for (int i = 0; i < layers.size(); ++i) {
        auto &layer = layers[i];
        if (!layer.model3D || (i != _index && !lightChanged))
            continue;
        if (i == _index) {
            layer.parameters = _parameters;
            if (!paint.isEmpty())
                layer.modelPaint = paint;
        }
        layer.parameters["light3d"] = light;
        const auto keepGoing = [this, completed, total] { return operationStep(completed, total); };
        layer.image = Model3D::render(*layer.model3D, layer.parameters, m_size, workingFormat(),
                                      keepGoing, layer.modelPaint);
        if (layer.image.isNull())
            return keepGoing() ? fail(tr("Impossible de calculer le rendu du modèle 3D.")) : false;
        ++completed;
    }
    if (!operationStep(total, total))
        return false;
    const auto before = snapshot(_label);
    m_layers          = std::move(layers);
    if (_copy) {
        m_active = _index;
        selectOnly(m_active);
    }
    if (!m_backgroundWorker)
        commit(before, _label);
    return true;
}

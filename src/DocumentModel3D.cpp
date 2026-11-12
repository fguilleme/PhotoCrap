#include "Document.h"
#include "masks/VectorMask.h"
#include "model3d/Model3D.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <limits>

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

void Document::queueModel3DPreview(int _index, QVariantMap _parameters, bool _copy)
{
    if (m_model3DPreview && m_model3DPreview->layer == _index && m_model3DPreview->copy == _copy &&
        m_model3DPreview->parameters == _parameters)
        return;
    const quint64 firstGeneration = m_model3DPreview && m_model3DPreview->layer == _index
                                        ? m_model3DPreview->firstGeneration
                                        : m_previewRequestGeneration + 1;
    m_model3DPreview              = std::make_shared<Model3DPreview>(
        Model3DPreview{_index, std::move(_parameters), firstGeneration, _copy});
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
        pixelEditBusy() || transformActive())
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
                                      const QString &_label, bool _copy)
{
    const auto &current = m_layers[_index].parameters;
    if (!_copy && current == _parameters)
        return true;
    if (deferEdit(_label, std::numeric_limits<qint64>::max(),
                  [_index, _parameters, _label, _copy](Document &worker) {
                      worker.applyModel3DParameters(_index, _parameters, _label, _copy);
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
        if (i == _index)
            layer.parameters = _parameters;
        layer.parameters["light3d"] = light;
        const auto keepGoing = [this, completed, total] { return operationStep(completed, total); };
        layer.image =
            Model3D::render(*layer.model3D, layer.parameters, m_size, workingFormat(), keepGoing);
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

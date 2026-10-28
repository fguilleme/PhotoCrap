#include "Document.h"
#include "SelectionGeometry.h"
#include <QtConcurrent>

void Document::commitSelectionEdit(const Snapshot &_before, const QString &_label)
{
    if (_before.selectionRegion == m_selectionRegion &&
        _before.selectionMask.cacheKey() == m_selectionMask.cacheKey() &&
        _before.activePenPath == m_activePenPath)
        return;
    auto before          = _before;
    before.label         = _label;
    before.selectionOnly = true;
    m_undo.append(before);
    m_redo.clear();
    // Selection history has an identity of its own but does not dirty saved pixels/paths.
    m_stateId = m_nextId++;
    trimHistory();
    emit historyChanged();
}

void Document::finishSelectionGesture()
{
    if (!m_selectionGestureRecorded)
        return;
    const auto before          = m_beforeSelectionGesture;
    m_beforeSelectionGesture   = {};
    m_selectionGestureRecorded = false;
    // These tools stage a temporary selection as part of one retouch operation.
    if (m_tool == "patch" || m_tool == "contentAwareMove")
        return;
    commitSelectionEdit(before, tr("Modifier la sélection"));
}

void Document::selectAll()
{
    endStroke();
    if (m_polygonLassoActive)
        cancelPolygonLasso();
    const auto before = snapshot({});
    m_selectionMask   = {};
    m_selection       = QRect(QPoint{}, m_size);
    m_selectionRegion = QRegion(m_selection);
    m_selectionShape  = 0;
    m_selectionPath.clear();
    m_polygonLassoActive = false;
    commitSelectionEdit(before, tr("Tout sélectionner"));
    emit selectionChanged();
}

void Document::deselect()
{
    endStroke();
    if (m_polygonLassoActive)
        cancelPolygonLasso();
    const auto before = snapshot({});
    if (!m_selectionRegion.isEmpty()) {
        m_lastSelectionRegion  = m_selectionRegion;
        m_lastSelectionMask    = m_selectionMask;
        m_lastSelectionOutline = m_selectionOutline;
    }
    m_selectionMask   = {};
    m_selection       = {};
    m_selectionRegion = {};
    m_selectionShape  = 0;
    m_selectionPath.clear();
    m_polygonLassoActive = false;
    commitSelectionEdit(before, tr("Désélectionner"));
    emit selectionChanged();
}

void Document::reselect()
{
    endStroke();
    if (m_polygonLassoActive)
        cancelPolygonLasso();
    if (!canReselect())
        return;
    const auto before     = snapshot({});
    m_selectionMask       = m_lastSelectionMask;
    m_selectionOutline    = m_lastSelectionOutline;
    m_selectionRegion     = m_lastSelectionRegion & QRegion(QRect(QPoint{}, m_size));
    m_selectionMaskRegion = m_selectionRegion;
    m_selection           = m_selectionRegion.boundingRect();
    m_selectionShape      = m_selectionRegion.isEmpty() ? 0 : 6;
    m_selectionPath.clear();
    m_polygonLassoActive = false;
    commitSelectionEdit(before, tr("Resélectionner"));
    emit selectionChanged();
}

void Document::invertSelection()
{
    endStroke();
    if (m_polygonLassoActive)
        cancelPolygonLasso();
    if (!m_selectionMask.isNull()) {
        modifySelection("inverse", 1);
        return;
    }
    const auto before      = snapshot({});
    m_lastSelectionRegion  = m_selectionRegion;
    m_lastSelectionMask    = m_selectionMask;
    m_lastSelectionOutline = m_selectionOutline;
    m_selectionRegion      = QRegion(QRect(QPoint{}, m_size)).subtracted(m_selectionRegion);
    m_selection            = m_selectionRegion.boundingRect();
    m_selectionShape       = m_selectionRegion.isEmpty() ? 0 : 6;
    m_selectionPath.clear();
    m_polygonLassoActive = false;
    commitSelectionEdit(before, tr("Intervertir la sélection"));
    emit selectionChanged();
}

void Document::setSelectionEdgesVisible(bool _visible)
{
    if (m_selectionEdgesVisible == _visible)
        return;
    m_selectionEdgesVisible = _visible;
    emit selectionDisplayChanged();
}

void Document::cancelSelectionOperation()
{
    if (!m_selectionCancel)
        return;
    m_selectionCancel->store(true);
    m_selectionCancel.reset();
    emit selectionOperationChanged();
}

void Document::modifySelection(const QString &_operation, int _amount)
{
    endStroke();
    if (!hasSelection())
        return;
    if ((_operation != "expand" && _operation != "contract" && _operation != "border" &&
         _operation != "smooth" && _operation != "feather" && _operation != "inverse") ||
        _amount < 1 ||
        _amount > (_operation == "feather"  ? 1000
                   : _operation == "border" ? 200
                                            : 100)) {
        fail(tr("Paramètres de modification de sélection invalides."));
        return;
    }
    cancelSelectionOperation();
    if (_operation == "feather" || !m_selectionMask.isNull()) {
        const auto before      = snapshot({});
        const auto session     = m_documentSession;
        auto       cancel      = std::make_shared<std::atomic_bool>(false);
        m_selectionCancel      = cancel;
        m_selectionTaskStateId = m_stateId;
        emit selectionOperationChanged();
        auto watcher = new QFutureWatcher<SelectionCoverage::Result>(this);
        connect(watcher, &QFutureWatcher<SelectionCoverage::Result>::finished, this,
                [this, watcher, before, session, cancel, _operation] {
                    const auto result = watcher->result();
                    watcher->deleteLater();
                    if (m_selectionCancel != cancel)
                        return;
                    cancelSelectionOperation();
                    if (before.id != m_stateId || session != m_documentSession)
                        return;
                    if (!result.valid) {
                        if (!result.error.isEmpty())
                            fail(tr("Impossible d’adoucir cette sélection."));
                        return;
                    }
                    if (_operation == "inverse") {
                        m_lastSelectionRegion  = before.selectionRegion;
                        m_lastSelectionMask    = before.selectionMask;
                        m_lastSelectionOutline = before.selectionOutline;
                    }
                    applySelectionCoverage(result);
                    const QString label = _operation == "inverse"   ? tr("Intervertir la sélection")
                                          : _operation == "feather" ? tr("Contour progressif")
                                          : _operation == "expand"  ? tr("Dilater la sélection")
                                          : _operation == "contract" ? tr("Contracter la sélection")
                                          : _operation == "border"   ? tr("Frange de sélection")
                                                                     : tr("Lisser la sélection");
                    commitSelectionEdit(before, label);
                    emit selectionChanged();
                    if (!result.support.isEmpty() && result.outline.isEmpty())
                        emit statusMessage(tr("Aucun pixel n’est sélectionné à plus de 50 %. La "
                                              "sélection reste active."));
                });
        watcher->setFuture(QtConcurrent::run([before, size = m_size, _amount, cancel, _operation] {
            if (_operation == "inverse") {
                QImage mask = before.selectionMask;
                mask.invertPixels();
                return SelectionCoverage::describe(mask, cancel);
            }
            return _operation == "feather"
                       ? SelectionCoverage::feather(before.selectionRegion, before.selectionMask,
                                                    size, _amount, cancel)
                       : SelectionCoverage::modify(before.selectionMask, _operation, _amount,
                                                   cancel);
        }));
        return;
    }
    const auto    before   = snapshot({});
    const auto    size     = m_size;
    const auto    session  = m_documentSession;
    const QString label    = _operation == "expand"     ? tr("Dilater la sélection")
                             : _operation == "contract" ? tr("Contracter la sélection")
                             : _operation == "border"   ? tr("Frange de sélection")
                                                        : tr("Lisser la sélection");
    auto          cancel   = std::make_shared<std::atomic_bool>(false);
    m_selectionCancel      = cancel;
    m_selectionTaskStateId = before.id;
    emit selectionOperationChanged();
    auto watcher = new QFutureWatcher<SelectionGeometry::Result>(this);
    connect(watcher, &QFutureWatcher<SelectionGeometry::Result>::finished, this,
            [this, watcher, before, session, cancel, label] {
                const auto result = watcher->result();
                watcher->deleteLater();
                if (m_selectionCancel != cancel)
                    return;
                cancelSelectionOperation();
                if (session != m_documentSession || before.id != m_stateId ||
                    before.selectionRegion != m_selectionRegion)
                    return;
                if (!result.valid) {
                    if (!result.error.isEmpty())
                        fail(result.error == "complexity"
                                 ? tr("Sélection trop complexe pour cette opération.")
                                 : tr("Mémoire insuffisante pour modifier la sélection."));
                    return;
                }
                m_selectionMask   = {};
                m_selectionRegion = result.region;
                m_selection       = result.region.boundingRect();
                m_selectionShape  = result.region.isEmpty() ? 0 : 6;
                m_selectionPath.clear();
                m_polygonLassoActive = false;
                commitSelectionEdit(before, label);
                emit selectionChanged();
            });
    watcher->setFuture(
        QtConcurrent::run([selection = before.selectionRegion, size, _operation, _amount, cancel] {
            return SelectionGeometry::modify(selection, size, _operation, _amount, cancel);
        }));
}

void Document::selectSimilarLayers()
{
    if (m_active < 0 || m_active >= m_layers.size())
        return;
    const QString kind = m_layers[m_active].kind;
    m_selectedLayers.clear();
    for (int i = 0; i < m_layers.size(); ++i)
        if (m_layers[i].kind == kind)
            m_selectedLayers.insert(i);
    emit layersChanged();
}

#pragma once
#include "Document.h"
#include "PenPanelSmoke.h"
#include "PenStrokeSmoke.h"
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

inline bool runPenUiSmoke(QQuickWindow *_window, Document &_document)
{
    auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Pen UI check failed: %s", _message);
        return _ok;
    };
    auto area     = _window->findChild<QQuickItem *>("drawingArea");
    auto overlay  = _window->findChild<QQuickItem *>("penOverlay");
    auto options  = _window->findChild<QQuickItem *>("penOptions");
    auto viewport = _window->findChild<QQuickItem *>("viewport");
    if (!require(area && overlay && options && viewport, "pen controls and overlay exist"))
        return false;
    auto handles = overlay->findChild<QQuickItem *>("penHandlesOverlay");
    auto band    = overlay->findChild<QQuickItem *>("penRubberBandOverlay");
    if (!require(handles && band, "pen handles and provisional curve use separate layers"))
        return false;
    _document.newDocument(300, 200, true);
    _document.setTool("brush");
    _window->setProperty("zoom", 1.);
    area->forceActiveFocus();
    QTest::keyClick(_window, Qt::Key_P);
    QTest::qWait(30);
    if (!require(_document.tool() == "pen" && options->isVisible(),
                 "P activates the available Pen tool and options"))
        return false;
    const auto scene = [&](qreal _x, qreal _y) {
        return area->mapToScene(QPointF(_x, _y)).toPoint();
    };
    QTest::mouseMove(_window, scene(30, 40));
    QTest::qWait(20);
    if (!require(QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor,
                 "Pen installs its nib cursor on the canvas"))
        return false;
    const auto newPathCursor = QGuiApplication::overrideCursor()->pixmap().toImage();
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, scene(30, 40));
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, scene(130, 40));
    QTest::mouseMove(_window, scene(155, 75), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, scene(155, 75));
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, scene(130, 140));
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, scene(30, 40));
    if (!require(_document.penPaths().size() == 1 &&
                     _document.penPaths()[0].toMap()["closed"].toBool() &&
                     !_document.penDrawing() &&
                     _document.composite().pixelColor(60, 60).alpha() == 0,
                 "pointer creates and closes a retained cubic path without painting"))
        return false;
    const auto points = _document.penPaths()[0].toMap()["points"].toList();
    if (!require(points.size() == 3 && points[1].toMap()["outX"].toReal() > 140,
                 "click-drag creates direction handles"))
        return false;
    QTest::mouseMove(_window, scene(80, 90));
    QTest::qWait(20);
    const auto addPointCursor = QGuiApplication::overrideCursor()->pixmap().toImage();
    const auto addPointTip    = QGuiApplication::overrideCursor()->hotSpot();
    QTest::mouseMove(_window, scene(30, 40));
    QTest::qWait(20);
    const auto deletePointCursor = QGuiApplication::overrideCursor()->pixmap().toImage();
    if (!require(
            newPathCursor != addPointCursor && newPathCursor != deletePointCursor &&
                addPointCursor != deletePointCursor && addPointTip == QPoint(3, 29) &&
                QGuiApplication::overrideCursor()->hotSpot() == QPoint(3, 29),
            "new path, add anchor and delete anchor have distinct cursors with the same nib tip"))
        return false;
    QSignalSpy editedHandlePaints(handles, SIGNAL(painted()));
    QTest::mousePress(_window, Qt::LeftButton, Qt::ControlModifier, scene(130, 40));
    QTest::mouseMove(_window, scene(160, 50), 20);
    if (!require(QTest::qWaitFor(
                     [&] {
                         return handles->isVisible() && (handles->property("gpuHandles").toBool()
                                                             ? editedHandlePaints.isEmpty()
                                                             : !editedHandlePaints.isEmpty());
                     },
                     3000),
                 "real anchor dragging updates handles with the expected GPU/software renderer"))
        return false;
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::ControlModifier, scene(160, 50));
    const auto moved = _document.penPaths()[0].toMap()["points"].toList()[1].toMap();
    if (qAbs(moved["x"].toReal() - 160) >= 1 || qAbs(moved["y"].toReal() - 50) >= 1)
        qWarning() << "Pen drag position" << moved << "tool" << _document.tool();
    if (!require(_document.tool() == "pen" && qAbs(moved["x"].toReal() - 160) < 1 &&
                     qAbs(moved["y"].toReal() - 50) < 1,
                 "Command/Ctrl drags an anchor rather than moving image pixels"))
        return false;
    _document.undo();
    area->forceActiveFocus();
    QTest::mouseMove(_window, scene(130, 40));
    const auto normalCursor = QGuiApplication::overrideCursor()->pixmap().cacheKey();
    QTest::keyPress(_window, Qt::Key_Control);
    if (!require(_window->property("primaryPressed").toBool() &&
                     QGuiApplication::overrideCursor()->pixmap().cacheKey() != normalCursor &&
                     QGuiApplication::overrideCursor()->hotSpot() == QPoint(3, 3),
                 "holding Command/Ctrl switches to an arrow with its hotspot at the visible tip")) {
        QTest::keyRelease(_window, Qt::Key_Control);
        return false;
    }
    // Key state remains authoritative when a pointer event lacks its modifier snapshot.
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, scene(130, 40));
    QTest::mouseMove(_window, scene(160, 60), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, scene(160, 60));
    QTest::keyRelease(_window, Qt::Key_Control);
    const auto heldMove = _document.penPaths()[0].toMap()["points"].toList()[1].toMap();
    if (!require(qAbs(heldMove["x"].toReal() - 160) < 1 && qAbs(heldMove["y"].toReal() - 60) < 1,
                 "held Command/Ctrl moves an anchor even without mouse-event modifiers"))
        return false;
    _document.undo();
    QTest::keyClick(_window, Qt::Key_Return, Qt::ControlModifier);
    if (!require(_document.hasSelection() && _document.activePenPath() == -1 &&
                     !overlay->isVisible(),
                 "Command/Ctrl+Return shows only the selection and retains the saved path"))
        return false;
    _document.setActivePenPath(0);
    if (!require(overlay->isVisible(), "choosing the saved path restores its editing overlay"))
        return false;
    auto selectionButton = _window->findChild<QObject *>("penSelectionButton");
    if (!require(selectionButton && QMetaObject::invokeMethod(selectionButton, "clicked") &&
                     _document.hasSelection() && !overlay->isVisible() &&
                     _document.penPaths().size() == 1,
                 "Selection button hides anchors and handles without deleting the path"))
        return false;
    _document.setActivePenPath(0);
    QTest::mouseClick(_window, Qt::RightButton, Qt::NoModifier, scene(70, 80));
    QTest::qWait(30);
    auto menu  = _window->findChild<QObject *>("penContextMenu");
    auto brush = _window->findChild<QObject *>("brushSettingsDialog");
    if (!require(menu && menu->property("visible").toBool() && !brush->property("visible").toBool(),
                 "right-click opens path commands rather than brush properties"))
        return false;
    QMetaObject::invokeMethod(menu, "close");
    _window->setProperty("zoom", 8.);
    QTest::qWait(30);
    if (!require(overlay->width() <= viewport->width() + 2 &&
                     overlay->height() <= viewport->height() + 2,
                 "zoomed path overlay allocation stays limited to the viewport"))
        return false;
    _window->setProperty("zoom", 1.);
    area->forceActiveFocus();
    QTest::keyClick(_window, Qt::Key_P, Qt::ShiftModifier);
    if (!require(_document.tool() == "convertPoint", "Shift+P skips the retired add/delete tools"))
        return false;
    _document.setTool("pen");
    _document.newPenPath();
    QSignalSpy initialPathPaints(overlay, SIGNAL(painted()));
    QSignalSpy initialHandlePaints(handles, SIGNAL(painted()));
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, scene(20, 160));
    _document.setPenRubberBand(true);
    if (!require(QTest::qWaitFor(
                     [&] {
                         return (overlay->property("gpuBackend").toBool() ||
                                 !initialPathPaints.isEmpty()) &&
                                (handles->property("gpuHandles").toBool() ||
                                 !initialHandlePaints.isEmpty());
                     },
                     3000),
                 "initial anchor rendering completes before measuring hover repaints"))
        return false;
    QSignalSpy pathPaints(overlay, SIGNAL(painted()));
    QSignalSpy handlePaints(handles, SIGNAL(painted()));
    QSignalSpy bandPaints(band, SIGNAL(painted()));
    QSignalSpy primitives(handles, SIGNAL(primitivesChanged()));
    QTest::mouseMove(_window, scene(60, 150));
    const bool hoverRendered = QTest::qWaitFor(
        [&] {
            return band->isVisible() &&
                   (band->property("gpuBand").toBool() || !bandPaints.isEmpty());
        },
        3000);
    if (!hoverRendered || !pathPaints.isEmpty() || !handlePaints.isEmpty() || !primitives.isEmpty())
        qWarning() << "Pen hover diagnostics" << _document.penDrawing() << _document.penHover()
                   << band->isVisible() << band->property("gpuBand") << pathPaints.size()
                   << handlePaints.size() << primitives.size() << bandPaints.size();
    if (!require(
            hoverRendered && pathPaints.isEmpty() && handlePaints.isEmpty() &&
                primitives.isEmpty() &&
                (band->property("gpuBand").toBool() ? bandPaints.isEmpty() : !bandPaints.isEmpty()),
            "pointer hover updates the provisional curve without repainting the retained path"))
        return false;
    QTest::keyClick(_window, Qt::Key_Escape);
    if (!require(!_document.penDrawing() && _document.penPaths().size() == 2,
                 "Escape finishes an open path without deleting it"))
        return false;
    if (!require(!band->isVisible(), "Escape hides the provisional curve"))
        return false;
    auto pathsDock = _window->findChild<QObject *>("pathsDockPanel");
    auto pathList  = _window->findChild<QQuickItem *>("pathList");
    auto showPaths = _window->findChild<QObject *>("showPathsPanelButton");
    if (!require(pathsDock && pathList && showPaths &&
                     _window->findChild<QObject *>("pathsPanelMenuItem"),
                 "Paths is an independent dock with a View menu entry and a toolbar shortcut"))
        return false;
    QMetaObject::invokeMethod(showPaths, "clicked");
    QTest::qWait(30);
    if (!require(pathsDock->property("visible").toBool() &&
                     pathList->property("count").toInt() == 2,
                 "the toolbar opens the Paths tab and lists the retained paths"))
        return false;
    _document.setActivePenPath(0);
    _document.setTool("brush");
    if (!require(overlay->isVisible(),
                 "a path selected from the panel stays visible with another tool"))
        return false;
    auto selectionFromPanel = _window->findChild<QObject *>("pathSelectionPanelButton");
    QMetaObject::invokeMethod(selectionFromPanel, "clicked");
    if (!require(_document.hasSelection() && _document.activePenPath() == -1 &&
                     !overlay->isVisible(),
                 "the panel selection action keeps only marching ants on the canvas"))
        return false;
    auto newPath = _window->findChild<QObject *>("newPathPanelButton");
    QMetaObject::invokeMethod(newPath, "clicked");
    if (!require(_document.penPaths().size() == 3 && _document.activePenPath() == 2,
                 "the Paths plus button creates and selects an empty retained path"))
        return false;
    QMetaObject::invokeMethod(pathsDock, "setPanelFloating", Q_ARG(QVariant, true));
    QTest::qWait(30);
    if (!require(pathList->window() != _window && pathList->window()->isVisible(),
                 "Paths content follows its floating window"))
        return false;
    QMetaObject::invokeMethod(pathsDock, "setPanelFloating", Q_ARG(QVariant, false));
    QTest::qWait(30);
    if (!require(pathList->window() == _window, "Paths docks back without losing its list"))
        return false;
    _window->requestActivate();
    area->forceActiveFocus();
    QTest::keyClick(_window, Qt::Key_A);
    if (!require(_document.tool() == "pathSelection",
                 "A activates the available Path Selection tool"))
        return false;
    _document.setActivePenPath(0);
    QTest::keyClick(_window, Qt::Key_Escape);
    if (!require(_document.activePenPath() == -1,
                 "Escape deselects a finished path without deleting it"))
        return false;
    _document.newDocument(200, 160, true, 16);
    _document.setTool("pen");
    auto modeCombo = _window->findChild<QObject *>("penModeCombo");
    if (!require(modeCombo, "Pen has a Shape/Path drawing-mode selector"))
        return false;
    modeCombo->setProperty("currentIndex", 0);
    QMetaObject::invokeMethod(modeCombo, "activated", Q_ARG(int, 0));
    if (!require(_document.penMode() == "shape", "Shape mode is selectable from the options bar"))
        return false;
    area->forceActiveFocus();
    for (const auto &point : {QPoint(20, 20), QPoint(130, 20), QPoint(130, 130), QPoint(20, 20)})
        QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, scene(point.x(), point.y()));
    if (!require(_document.layers().size() == 2 &&
                     _document.layers()[0].toMap()["kind"].toString() == "shape" &&
                     _document.layerImage(1).pixelColor(100, 50).alpha() > 0,
                 "native pointer drawing creates a filled, retained shape layer in 16 bits"))
        return false;
    const auto fillControl   = _window->findChild<QQuickItem *>("penShapeFillColor");
    const auto strokeControl = _window->findChild<QQuickItem *>("penShapeStrokeWidth");
    if (!require(fillControl && strokeControl && fillControl->isVisible() &&
                     strokeControl->isVisible(),
                 "Shape mode exposes fill, stroke and editable stroke width"))
        return false;
    _document.setPenShapeStyle("fillColor", "#ff2040e0");
    if (!require(_document.layerImage(1).pixelColor(100, 50) == QColor("#2040e0"),
                 "the displayed shape updates when its retained fill changes"))
        return false;
    _document.setActiveLayer(1);
    QMetaObject::invokeMethod(_window, "openActiveLayerProperties");
    if (!require(_document.penMode() == "shape" && _document.activePenPath() >= 0 &&
                     !_window->property("modalOpen").toBool(),
                 "shape properties reopen vector editing rather than the adjustment dialog"))
        return false;
    modeCombo->setProperty("currentIndex", 1);
    QMetaObject::invokeMethod(modeCombo, "activated", Q_ARG(int, 1));
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, scene(10, 150));
    if (!require(_document.penMode() == "path" && _document.layers().size() == 2,
                 "returning to Path mode creates an outline without another shape layer"))
        return false;
    if (!runPenPanelEditingSmoke(_window, _document) ||
        !runPenStrokeVisibilitySmoke(_window, _document))
        return false;
    _document.setPenRubberBand(false);
    _document.newDocument(100, 80, true);
    _document.setTool("pen");
    _document.beginPenGesture(20, 20);
    _document.updatePenGesture(30, 30);
    const bool closed = _window->close();
    auto       guard  = _window->findChild<QObject *>("unsavedDialog");
    if (!require(!closed && _window->isVisible() && guard->property("visible").toBool() &&
                     _document.modified(),
                 "closing commits a pending pen gesture before checking for unsaved changes"))
        return false;
    QMetaObject::invokeMethod(_window->findChild<QObject *>("unsavedCancelButton"), "clicked");
    QTest::qWait(40);
    _document.setTool("brush");
    return true;
}

#pragma once
#include "Document.h"
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
    QTest::mousePress(_window, Qt::LeftButton, Qt::ControlModifier, scene(130, 40));
    QTest::mouseMove(_window, scene(160, 50), 20);
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
    if (!require(_document.tool() == "addAnchor", "Shift+P cycles only implemented Pen variants"))
        return false;
    _document.setTool("pen");
    _document.newPenPath();
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, scene(20, 160));
    QTest::keyClick(_window, Qt::Key_Escape);
    if (!require(!_document.penDrawing() && _document.penPaths().size() == 2,
                 "Escape finishes an open path without deleting it"))
        return false;
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

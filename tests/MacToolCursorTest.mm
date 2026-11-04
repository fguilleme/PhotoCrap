#include "MacToolCursorTest.h"
#include "CanvasItem.h"
#include "Document.h"
#include "PenPanelSmoke.h"
#include "PenStrokeSmoke.h"

#include <QCursor>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QtTest>

#import <AppKit/AppKit.h>

bool macTextCursorIsCurrent()
{
    return [[NSCursor currentCursor] isEqual:[NSCursor IBeamCursor]];
}

bool runMacPenInputTest(QQuickWindow *_window, Document &_document)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return false;
    const QPoint previousPointer = QCursor::pos();
    const auto   restorePointer  = qScopeGuard([&] { QCursor::setPos(previousPointer); });
    _window->requestActivate();
    [NSApp activateIgnoringOtherApps:YES];
    _document.newDocument(300, 200, true);
    _document.setTool("pen");
    _window->setProperty("zoom", 1.);
    auto area = _window->findChild<QQuickItem *>("drawingArea");
    if (!area)
        return false;
    area->forceActiveFocus();
    QTest::qWait(100);
    NSView   *view         = reinterpret_cast<NSView *>(_window->winId());
    NSWindow *nativeWindow = [view window];
    auto      pointer      = [&](NSEventType _type, QPointF _point, NSEventModifierFlags _flags) {
        const auto    scene = area->mapToScene(_point);
        const NSPoint point =
            [view convertPoint:NSMakePoint(scene.x(), [view isFlipped]
                                                          ? scene.y()
                                                          : [view bounds].size.height - scene.y())
                        toView:nil];
        NSEvent *event = [NSEvent mouseEventWithType:_type
                                            location:point
                                       modifierFlags:_flags
                                           timestamp:NSProcessInfo.processInfo.systemUptime
                                        windowNumber:[nativeWindow windowNumber]
                                             context:nil
                                         eventNumber:0
                                          clickCount:1
                                            pressure:1];
        [NSApp sendEvent:event];
        QTest::qWait(20);
    };
    auto command = [&](bool _down) {
        NSEvent *event = [NSEvent keyEventWithType:NSEventTypeFlagsChanged
                                          location:NSZeroPoint
                                     modifierFlags:_down ? NSEventModifierFlagCommand : 0
                                         timestamp:NSProcessInfo.processInfo.systemUptime
                                      windowNumber:[nativeWindow windowNumber]
                                           context:nil
                                        characters:@""
                       charactersIgnoringModifiers:@""
                                         isARepeat:NO
                                           keyCode:55];
        [NSApp sendEvent:event];
        QTest::qWait(20);
    };
    for (const QPointF point : {QPointF(30, 40), QPointF(130, 40), QPointF(130, 140)}) {
        pointer(NSEventTypeLeftMouseDown, point, 0);
        pointer(NSEventTypeLeftMouseUp, point, 0);
    }
    _document.finishPenPath();
    QCursor::setPos(_window->mapToGlobal(area->mapToScene(QPointF(130, 40)).toPoint()));
    pointer(NSEventTypeMouseMoved, QPointF(130, 40), 0);
    if (!QGuiApplication::overrideCursor()) {
        qWarning("Pen cursor missing before native Command test");
        return false;
    }
    const auto cursorBefore = QGuiApplication::overrideCursor()->pixmap().cacheKey();
    command(true);
    if (!QGuiApplication::overrideCursor()) {
        command(false);
        qWarning("Pen cursor missing after native Command test");
        return false;
    }
    const NSPoint nativeTip = [[NSCursor currentCursor] hotSpot];
    const bool    changedCursor =
        QGuiApplication::overrideCursor()->pixmap().cacheKey() != cursorBefore;
    const bool alignedTip = QGuiApplication::overrideCursor()->hotSpot() == QPoint(3, 3) &&
                            qAbs(nativeTip.x - 3) < .1 && qAbs(nativeTip.y - 3) < .1;
    pointer(NSEventTypeLeftMouseDown, QPointF(130, 40), NSEventModifierFlagCommand);
    pointer(NSEventTypeLeftMouseDragged, QPointF(160, 60), NSEventModifierFlagCommand);
    pointer(NSEventTypeLeftMouseUp, QPointF(160, 60), NSEventModifierFlagCommand);
    command(false);
    const auto paths       = _document.penPaths();
    const auto points      = paths.isEmpty() ? QVariantList{} : paths[0].toMap()["points"].toList();
    const auto moved       = points.size() > 1 ? points[1].toMap() : QVariantMap{};
    const bool movedAnchor = points.size() == 3 && qAbs(moved["x"].toReal() - 160) < 1 &&
                             qAbs(moved["y"].toReal() - 60) < 1;
    if (!changedCursor || !alignedTip || !movedAnchor)
        qWarning() << "Native Command Pen input" << "cursor changed" << changedCursor
                   << "visible arrow tip aligned" << alignedTip << "Cocoa hotspot" << nativeTip.x
                   << nativeTip.y << "anchor moved" << movedAnchor << "points" << points
                   << "modifiers"
                   << _window->findChild<QObject *>("toolInput")->property("modifiers");
    if (!changedCursor || !alignedTip || !movedAnchor)
        return false;
    _document.setTool("pathSelection");
    pointer(NSEventTypeMouseMoved, QPointF(160, 60), 0);
    pointer(NSEventTypeLeftMouseDown, QPointF(160, 60), 0);
    pointer(NSEventTypeLeftMouseDragged, QPointF(170, 65), 0);
    pointer(NSEventTypeLeftMouseUp, QPointF(170, 65), 0);
    const auto translated = _document.penPaths()[0].toMap()["points"].toList();
    if (translated[0].toMap()["x"].toReal() != 40 || translated[1].toMap()["x"].toReal() != 170 ||
        translated[2].toMap()["x"].toReal() != 140) {
        qWarning("Native black-arrow dragging must move the entire path");
        return false;
    }
    _document.setActivePenPath(0);
    _document.setTool("directSelection");
    pointer(NSEventTypeMouseMoved, QPointF(170, 65), 0);
    command(true);
    pointer(NSEventTypeLeftMouseDown, QPointF(170, 65), NSEventModifierFlagCommand);
    pointer(NSEventTypeLeftMouseDragged, QPointF(180, 75), NSEventModifierFlagCommand);
    pointer(NSEventTypeLeftMouseUp, QPointF(180, 75), NSEventModifierFlagCommand);
    command(false);
    const auto edited = _document.penPaths()[0].toMap()["points"].toList();
    if (edited[1].toMap()["x"].toReal() != 180 || edited[0].toMap()["x"].toReal() != 40 ||
        edited[2].toMap()["x"].toReal() != 140) {
        qWarning("Native white-arrow dragging must edit the chosen anchor");
        return false;
    }
    QMetaObject::invokeMethod(_window, "setDockPanelVisible", Q_ARG(QVariant, "paths"),
                              Q_ARG(QVariant, true));
    QTest::qWait(50);
    const auto pathsPanel = _window->findChild<QQuickItem *>("pathsDockPanel");
    const auto pathList   = _window->findChild<QQuickItem *>("pathList");
    if (!pathsPanel || !pathsPanel->isVisible() || !pathList ||
        pathList->property("count").toInt() != 1) {
        qWarning("Native Paths tab must display the edited path");
        return false;
    }
    _document.newDocument(300, 200, true, 16);
    _document.setTool("pen");
    _document.setPenMode("shape");
    _document.setPenShapeStyle("fillColor", "#ffca4050");
    QTest::qWait(30);
    for (const auto &point : {QPointF(30, 40), QPointF(130, 40), QPointF(130, 140),
                              QPointF(30, 140), QPointF(30, 40)}) {
        pointer(NSEventTypeLeftMouseDown, point, 0);
        pointer(NSEventTypeLeftMouseUp, point, 0);
    }
    if (_document.layers().size() != 2 ||
        _document.layers()[0].toMap()["kind"].toString() != "shape") {
        qWarning("Native Shape-mode drawing must create an editable shape layer");
        return false;
    }
    const auto shapeBefore = _document.layerImage(1);
    command(true);
    pointer(NSEventTypeLeftMouseDown, QPointF(130, 40), NSEventModifierFlagCommand);
    pointer(NSEventTypeLeftMouseDragged, QPointF(160, 60), NSEventModifierFlagCommand);
    pointer(NSEventTypeLeftMouseUp, QPointF(160, 60), NSEventModifierFlagCommand);
    command(false);
    if (_document.layerImage(1) == shapeBefore) {
        qWarning("Native Command dragging must update the retained shape pixels");
        return false;
    }
    _document.undo();
    if (_document.layerImage(1) != shapeBefore) {
        qWarning("Native shape editing must restore its geometry and pixels on Undo");
        return false;
    }
    _document.redo();
    _document.setPenShapeStyle("fillColor", "#ff3060dc");
    if (_document.layerImage(1).pixelColor(80, 80) != QColor("#3060dc")) {
        qWarning("Native shape fill controls must update its displayed content");
        return false;
    }
    if (!runPenPanelEditingSmoke(_window, _document) ||
        !runPenStrokeVisibilitySmoke(_window, _document))
        return false;
    _document.editPenShape();
    _window->setProperty("zoom", .13);
    // Let delegate incubation and Canvas thumbnails complete before the frame capture.
    QTest::qWait(80);
    return true;
}

bool runMacToolCursorTest(QQuickWindow *_window)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return false;
    auto artboard = _window->findChild<QQuickItem *>("artboard");
    auto info     = _window->findChild<QObject *>("informationDockPanel");
    auto canvas   = _window->findChild<CanvasItem *>("imageCanvas");
    if (!artboard || !info || !canvas)
        return false;
    const QPoint previousPosition = QCursor::pos();
    _window->requestActivate();
    [NSApp activateIgnoringOtherApps:YES];
    QTest::qWait(200);
    const QPoint canvasPoint = _window->mapToGlobal(
        artboard->mapToScene(QPointF(artboard->width() / 4, artboard->height() / 2)).toPoint());
    // Use real pointer entry and Cocoa's current cursor. Offscreen tests only
    // inspect Qt's override stack, which can be correct while macOS shows an arrow.
    bool passed = true;
    for (const QString &tool :
         {QStringLiteral("picker"), QStringLiteral("colorSampler"), QStringLiteral("ruler")}) {
        QCursor::setPos(canvasPoint);
        QTest::qWait(40);
        QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "measure"),
                                  Q_ARG(QVariant, tool));
        // Reproduce AppKit overwriting the cursor at the end of the input event.
        [[NSCursor crosshairCursor] set];
        const QPoint expected      = tool == "ruler" ? QPoint(8, 8) : QPoint(3, 29);
        const auto   cursorMatches = [&] {
            const NSPoint point = [[NSCursor currentCursor] hotSpot];
            return _window->isActive() && QGuiApplication::overrideCursor() &&
                   QGuiApplication::overrideCursor()->hotSpot() == expected &&
                   qAbs(point.x - expected.x()) < .1 && qAbs(point.y - expected.y()) < .1;
        };
        const bool    cursorReady   = QTest::qWaitFor(cursorMatches, 1000);
        const NSPoint nativeHotspot = [[NSCursor currentCursor] hotSpot];
        if (!cursorReady) {
            qWarning() << "First native tool cursor is incorrect:" << tool << "active"
                       << _window->isActive() << "Qt hotspot"
                       << (QGuiApplication::overrideCursor()
                               ? QGuiApplication::overrideCursor()->hotSpot()
                               : QPoint(-1, -1))
                       << "Cocoa hotspot" << nativeHotspot.x << nativeHotspot.y << "pointer"
                       << QCursor::pos() << "canvas point" << canvasPoint << "drawing hover"
                       << _window->findChild<QObject *>("drawingArea")->property("containsMouse");
            passed = false;
            break;
        }
        // Isolate a reset occurring after our handler but before the event loop
        // resumes, without relying on a subsequent hover or QML callback.
        canvas->setSystemCursor(Qt::CrossCursor, true, tool, 0);
        [[NSCursor crosshairCursor] set];
        const bool cursorRecovered = QTest::qWaitFor(
            [&] {
                const NSPoint point = [[NSCursor currentCursor] hotSpot];
                return qAbs(point.x - expected.x()) < .1 && qAbs(point.y - expected.y()) < .1;
            },
            1000);
        const NSPoint recovered = [[NSCursor currentCursor] hotSpot];
        if (!cursorRecovered) {
            qWarning() << "Native cursor did not recover after AppKit reset:" << tool << recovered.x
                       << recovered.y << "Qt hotspot"
                       << (QGuiApplication::overrideCursor()
                               ? QGuiApplication::overrideCursor()->hotSpot()
                               : QPoint(-1, -1))
                       << "active" << _window->isActive() << "canvas hover"
                       << _window->findChild<QObject *>("drawingArea")->property("containsMouse")
                       << "pointer" << QCursor::pos() << "target" << canvasPoint;
            passed = false;
            break;
        }
        // Close and show Info while the pointer stays over the image.
        QMetaObject::invokeMethod(_window, "setDockPanelVisible", Q_ARG(QVariant, "information"),
                                  Q_ARG(QVariant, false));
        QMetaObject::invokeMethod(_window, "showInformationPanel");
        [[NSCursor crosshairCursor] set];
        const bool    infoCursorReady = QTest::qWaitFor(cursorMatches, 1000);
        const NSPoint reopenedHotspot = [[NSCursor currentCursor] hotSpot];
        if (!infoCursorReady) {
            qWarning() << "Opening Info resets the native tool cursor:" << tool << reopenedHotspot.x
                       << reopenedHotspot.y << "active" << _window->isActive() << "Qt hotspot"
                       << (QGuiApplication::overrideCursor()
                               ? QGuiApplication::overrideCursor()->hotSpot()
                               : QPoint(-1, -1))
                       << "pointer" << QCursor::pos() << "canvas point" << canvasPoint
                       << "drawing hover"
                       << _window->findChild<QObject *>("drawingArea")->property("containsMouse");
            passed = false;
            break;
        }
    }
    QCursor::setPos(previousPosition);
    return passed;
}

void activateMacTestWindow(QQuickWindow *window)
{
    [NSApp activateIgnoringOtherApps:YES];
    NSView *view = reinterpret_cast<NSView *>(window->winId());
    [[view window] makeKeyAndOrderFront:nil];
    window->requestActivate();
}

void sendMacTestMouseButton(QQuickWindow *_window, const QPointF &_scene, bool _down)
{
    NSView       *view         = reinterpret_cast<NSView *>(_window->winId());
    NSWindow     *nativeWindow = [view window];
    const NSPoint point =
        [view convertPoint:NSMakePoint(_scene.x(), [view isFlipped]
                                                       ? _scene.y()
                                                       : [view bounds].size.height - _scene.y())
                    toView:nil];
    NSEvent *event =
        [NSEvent mouseEventWithType:_down ? NSEventTypeLeftMouseDown : NSEventTypeLeftMouseUp
                           location:point
                      modifierFlags:0
                          timestamp:NSProcessInfo.processInfo.systemUptime
                       windowNumber:[nativeWindow windowNumber]
                            context:nil
                        eventNumber:0
                         clickCount:1
                           pressure:_down ? 1 : 0];
    [NSApp sendEvent:event];
    QTest::qWait(25);
}

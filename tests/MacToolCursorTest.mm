#include "MacToolCursorTest.h"
#include "CanvasItem.h"
#include "Document.h"

#include <QCursor>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QtTest>

#import <AppKit/AppKit.h>

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
    return changedCursor && alignedTip && movedAnchor;
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

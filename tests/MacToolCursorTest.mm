#include "MacToolCursorTest.h"

#include <QCursor>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#import <AppKit/AppKit.h>

bool runMacToolCursorTest(QQuickWindow *_window)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return false;
    auto artboard = _window->findChild<QQuickItem *>("artboard");
    auto info     = _window->findChild<QObject *>("informationDockPanel");
    if (!artboard || !info)
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
        QTest::qWait(100);
        const QPoint  expected      = tool == "ruler" ? QPoint(8, 8) : QPoint(3, 29);
        const NSPoint nativeHotspot = [[NSCursor currentCursor] hotSpot];
        if (!_window->isActive() || !QGuiApplication::overrideCursor() ||
            QGuiApplication::overrideCursor()->hotSpot() != expected ||
            qAbs(nativeHotspot.x - expected.x()) > 0.1 ||
            qAbs(nativeHotspot.y - expected.y()) > 0.1) {
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
        // Close and show Info while the pointer stays over the image.
        QMetaObject::invokeMethod(_window, "setDockPanelVisible", Q_ARG(QVariant, "information"),
                                  Q_ARG(QVariant, false));
        QMetaObject::invokeMethod(_window, "showInformationPanel");
        QTest::qWait(150);
        const NSPoint reopenedHotspot = [[NSCursor currentCursor] hotSpot];
        if (!_window->isActive() || qAbs(reopenedHotspot.x - expected.x()) > 0.1 ||
            qAbs(reopenedHotspot.y - expected.y()) > 0.1) {
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

#pragma once
#include "OverlayReference.h"
#include "render/CanvasFonts.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtTest>
#include <memory>

inline bool runViewportRulerSmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Viewport ruler check failed: %s", _message);
        return _ok;
    };
    const bool hardware =
        QSGRendererInterface::isApiRhiBased(_window->rendererInterface()->graphicsApi());
    QQmlComponent component(qmlEngine(_window));
    component.setData(R"QML(
import QtQuick
import PhotoCrap
Item {
    id: fixture
    width: 512; height: 320; clip: true
    property bool referenceVisible: false
    property bool showRulers: true
    property real hLength: 16384
    property real vLength: 8192
    property real rulerZoom: 1
    property real hOffset: 20
    property real vOffset: 35
    property real hWidth: 512
    property real vHeight: 256
    property real rulerOpacity: 1
    Rectangle { anchors.fill: parent; color: "#5a7896" }
    component LegacyRuler: Canvas {
        id: legacy
        property bool vertical: false
        property real documentLength: 0
        property real zoom: 1
        property real offset: 0
        property string paintFont: ""
        onPaint: {
            // Clear the whole backing edge so repaint counts cannot accumulate coverage.
            const c=getContext("2d");c.reset();c.clearRect(0,0,Math.ceil(width),Math.ceil(height));c.fillStyle="#20262e";c.fillRect(0,0,width,height)
            c.fillStyle="#788491";c.strokeStyle="#46505b";c.font="9px sans-serif"
            paintFont=c.font
            const step=zoom<.3?500:zoom<.65?200:zoom<1.5?100:50
            for(let i=0;i<=documentLength;i+=step) {
                const at=offset+i*zoom
                if(vertical) {
                    c.save();c.translate(11,at+4);c.rotate(-Math.PI/2);c.fillText(i,0,0);c.restore()
                    c.beginPath();c.moveTo(17,at);c.lineTo(24,at);c.stroke()
                } else {
                    c.fillText(i,at+4,12)
                    c.beginPath();c.moveTo(at,17);c.lineTo(at,24);c.stroke()
                }
            }
        }
        onDocumentLengthChanged: if(visible)requestPaint()
        onZoomChanged: if(visible)requestPaint()
        onOffsetChanged: if(visible)requestPaint()
        onVisibleChanged: if(visible)requestPaint()
    }
    ViewportRuler {
        objectName: "rulerTestHorizontal"
        width: hWidth; height: 24
        documentLength: hLength;zoom:rulerZoom;offset:hOffset
        opacity:rulerOpacity
        visible:showRulers && !referenceVisible
    }
    ViewportRuler {
        objectName: "rulerTestVertical"
        y:32;width:24;height:vHeight
        vertical:true
        documentLength:vLength;zoom:rulerZoom;offset:vOffset
        opacity:rulerOpacity
        visible:showRulers && !referenceVisible
    }
    LegacyRuler {
        objectName: "rulerReferenceHorizontal"
        width:hWidth;height:24
        documentLength:hLength;zoom:rulerZoom;offset:hOffset
        opacity:rulerOpacity
        visible:showRulers && referenceVisible
    }
    LegacyRuler {
        y:32;width:24;height:vHeight;vertical:true
        documentLength:vLength;zoom:rulerZoom;offset:vOffset
        opacity:rulerOpacity
        visible:showRulers && referenceVisible
    }
}
)QML",
                      QUrl());
    if (!require(component.isReady(), qPrintable(component.errorString())))
        return false;
    std::unique_ptr<QObject> object(component.create());
    auto                    *root = qobject_cast<QQuickItem *>(object.get());
    if (!require(root, "create the ruler fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *horizontal = root->findChild<QQuickItem *>("rulerTestHorizontal");
    auto *vertical   = root->findChild<QQuickItem *>("rulerTestVertical");
    if (!require(horizontal && vertical, "both ruler orientations exist"))
        return false;
    const auto frame = [&] {
        _window->update();
        QTest::qWait(50);
        return _window->grabWindow();
    };
    const auto countVisual = [&](QQuickItem *_parent, const QString &_name) {
        int                 count = 0;
        QList<QQuickItem *> pending{_parent};
        while (!pending.isEmpty()) {
            auto *item = pending.takeLast();
            if (item->objectName() == _name)
                ++count;
            pending.append(item->childItems());
        }
        return count;
    };
    const auto matchesReference = [&] {
        root->setProperty("referenceVisible", true);
        const auto reference = frame();
        root->setProperty("referenceVisible", false);
        const auto  actual = frame();
        const auto  origin = root->mapToScene(QPointF{});
        const qreal ratio  = qreal(actual.width()) / _window->width();
        const QRect bounds(qFloor(origin.x() * ratio), qFloor(origin.y() * ratio),
                           qFloor(root->width() * ratio), qFloor(root->height() * ratio));
        return compareOverlayReference(actual, reference, bounds, hardware,
                                       "/tmp/photocrap-viewport-ruler");
    };
    const auto bounded = [&](QQuickItem *_ruler) {
        const qreal extent =
            _ruler->property("vertical").toBool() ? _ruler->height() : _ruler->width();
        const auto spacing = _ruler->property("spacing").toReal();
        const auto margin  = _ruler->property("labelMargin").toReal();
        const int  count   = _ruler->property("tickCount").toInt();
        return count >= 0 && count <= (extent + 2 * margin) / spacing + 2 &&
               (!_ruler->property("gpuRuler").toBool() ||
                (countVisual(_ruler, "viewportRulerLabel") == count &&
                 countVisual(_ruler, "viewportRulerTick") == count));
    };
    frame();
    const bool  initialMatches = matchesReference();
    CanvasFonts fonts;
    const auto  referenceFont =
        root->findChild<QObject *>("rulerReferenceHorizontal")->property("paintFont").toString();
    if (!require(fonts.ruler().toString() == referenceFont,
                 "native labels resolve the same platform font as Canvas CSS"))
        return false;
    if (!require(horizontal->property("rasterActive").toBool() != hardware &&
                     vertical->property("rasterActive").toBool() != hardware &&
                     bounded(horizontal) && bounded(vertical) && initialMatches,
                 "visible graduations, labels and vertical orientation match the original rulers"))
        return false;
    QSignalSpy hPaints(horizontal, SIGNAL(painted())), vPaints(vertical, SIGNAL(painted()));
    if (!require(hPaints.isValid() && vPaints.isValid(), "observe ruler raster paints"))
        return false;
    root->setProperty("hOffset", -350.25);
    root->setProperty("vOffset", -199.75);
    frame();
    if (!require((hardware ? hPaints.isEmpty() && vPaints.isEmpty()
                           : !hPaints.isEmpty() && !vPaints.isEmpty()) &&
                     bounded(horizontal) && bounded(vertical) && matchesReference(),
                 "fractional scrolling updates scene items without raster painting on GPU"))
        return false;
    for (qreal z : {.02, .299, .3, .649, .65, 1.499, 1.5, 4., 16.}) {
        root->setProperty("rulerZoom", z);
        frame();
        if (!require(bounded(horizontal) && bounded(vertical) && matchesReference(),
                     "zoom steps preserve label spacing and cull offscreen ticks"))
            return false;
    }
    root->setProperty("rulerZoom", 1.);
    root->setProperty("rulerOpacity", .37);
    frame();
    if (!require(matchesReference(), "native ruler opacity composes the full band once"))
        return false;
    root->setProperty("rulerOpacity", 1.);
    root->setProperty("hWidth", 367.5);
    root->setProperty("vHeight", 211.25);
    frame();
    if (!require(
            !horizontal->property("gpuRuler").toBool() &&
                !vertical->property("gpuRuler").toBool() && matchesReference(),
            "fractional viewport dimensions preserve ruler projection via bounded raster fallback"))
        return false;
    root->setProperty("rulerOpacity", .37);
    frame();
    if (!require(matchesReference(),
                 "ruler opacity is applied after labels and background compose"))
        return false;
    root->setProperty("rulerOpacity", 1.);
    root->setProperty("hLength", 0.);
    root->setProperty("vLength", 0.);
    root->setProperty("hOffset", 20.);
    root->setProperty("vOffset", 35.);
    frame();
    if (!require(horizontal->property("tickCount").toInt() == 1 &&
                     vertical->property("tickCount").toInt() == 1 && matchesReference(),
                 "zero-sized documents retain the origin graduation"))
        return false;
    root->setProperty("hLength", 16384.);
    root->setProperty("vLength", 8192.);
    root->setProperty("hOffset", 100000.);
    root->setProperty("vOffset", 100000.);
    frame();
    if (!require(horizontal->property("tickCount").toInt() == 0 &&
                     vertical->property("tickCount").toInt() == 0 && matchesReference(),
                 "fully offscreen documents leave only the ruler background"))
        return false;
    root->setProperty("showRulers", false);
    root->setProperty("hOffset", -125.5);
    root->setProperty("vOffset", -250.25);
    frame();
    root->setProperty("showRulers", true);
    frame();
    if (!require(matchesReference(), "restored rulers use the latest view position"))
        return false;
    root->setProperty("hWidth", 512.);
    root->setProperty("vHeight", 256.);
    // Do not render the legacy two-million-tick loop: verify the bounded model directly.
    root->setProperty("hLength", 1000000000.);
    root->setProperty("vLength", 1000000000.);
    root->setProperty("rulerZoom", .02);
    root->setProperty("hOffset", -19999750.);
    root->setProperty("vOffset", -19999750.);
    frame();
    if (!require(bounded(horizontal) && bounded(vertical) &&
                     horizontal->property("firstTick").toInt() > 1000000 &&
                     horizontal->property("tickCount").toInt() < 150 &&
                     vertical->property("tickCount").toInt() < 150,
                 "huge document coordinates create only a viewport-sized tick model"))
        return false;
    qInfo("Viewport ruler checks passed");
    return true;
}

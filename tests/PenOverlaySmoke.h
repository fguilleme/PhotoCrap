#pragma once
#include "Document.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>
#include <memory>

inline bool runPenOverlaySmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Pen overlay check failed: %s", _message);
        return _ok;
    };
    Document      document;
    QQmlComponent component(qmlEngine(_window));
    component.setData(R"(
import QtQuick
import PhotoCrap
Item {
    id: fixture
    required property var document
    width: 512; height: 320
    property bool referenceVisible: false
    property real zoom: 1
    property real panX: 0
    property real panY: 0
    property real viewRotation: 0
    Rectangle { anchors.fill: parent; color: "#5a7896" }
component LegacyPenOverlay: ScreenOverlayCanvas {
    id: root
    required property var document
    enabled: false
    antialiasing: true
    onVisibleChanged: if(visible)requestPaint()
    onPaint: {
        const c=getContext("2d")
        c.reset();c.clearRect(0,0,width,height)
        const paths=document.penPaths,index=document.activePenPath
        if(index<0 || index>=paths.length)return
        const path=paths[index],p=path.points
        if(!p.length)return
        c.translate(-x,-y)
        function segment(a,b) { c.bezierCurveTo(a.outX*zoom,a.outY*zoom,b.inX*zoom,b.inY*zoom,b.x*zoom,b.y*zoom) }
        c.beginPath();c.moveTo(p[0].x*zoom,p[0].y*zoom)
        for(let i=1;i<p.length;++i)segment(p[i-1],p[i])
        if(path.closed)segment(p[p.length-1],p[0])
        c.strokeStyle="#1a2b3b";c.lineWidth=3;c.stroke()
        c.strokeStyle="#73c5ff";c.lineWidth=1.5;c.stroke()
        if(document.penDrawing && document.penRubberBand && !path.closed) {
            const last=p[p.length-1],to=document.penHover
            c.beginPath();c.moveTo(last.x*zoom,last.y*zoom)
            c.bezierCurveTo(last.outX*zoom,last.outY*zoom,to.x*zoom,to.y*zoom,to.x*zoom,to.y*zoom)
            c.setLineDash([4,3]);c.strokeStyle="#73c5ff";c.lineWidth=1.5;c.stroke();c.setLineDash([])
        }
        for(let i=0;i<p.length;++i) {
            const a=p[i],px=a.x*zoom,py=a.y*zoom
            if(a.selected)for(const h of [{x:a.inX*zoom,y:a.inY*zoom},{x:a.outX*zoom,y:a.outY*zoom}]) {
                if(Math.hypot(h.x-px,h.y-py)<.1)continue
                c.beginPath();c.moveTo(px,py);c.lineTo(h.x,h.y);c.strokeStyle="#73c5ff";c.lineWidth=1;c.stroke()
                c.beginPath();c.arc(h.x,h.y,3,0,2*Math.PI);c.fillStyle="#eef8ff";c.fill();c.strokeStyle="#1b4968";c.stroke()
            }
            c.fillStyle=a.selected?"#73c5ff":"#ffffff";c.fillRect(px-3,py-3,6,6)
            c.strokeStyle="#183e59";c.lineWidth=1;c.strokeRect(px-3,py-3,6,6)
        }
    }
    Connections {
        target: root.document
        function onPenPathsChanged(){root.requestPaint()}
        function onPenHoverChanged(){if(root.document.penRubberBand)root.requestPaint()}
        function onPenOptionsChanged(){root.requestPaint()}
    }
    onZoomChanged: requestPaint()
}

    Flickable {
        id: view
        anchors.fill: parent; clip: true; interactive: false
        contentWidth: board.width; contentHeight: board.height
        contentX: panX; contentY: panY
        Item {
            id: board
            objectName: "penTestBoard"
            width: document.width*fixture.zoom; height: document.height*fixture.zoom
            rotation: viewRotation; transformOrigin: Item.TopLeft
            PenOverlay {
                objectName: "penTestOverlay"
                document: fixture.document; viewportItem: view; zoom: fixture.zoom
                visible: !referenceVisible && document.activePenPath>=0
            }
            LegacyPenOverlay {
                objectName: "penTestReference"
                document: fixture.document; viewportItem: view; zoom: fixture.zoom
                visible: referenceVisible && document.activePenPath>=0
            }
        }
    }
}
)",
                      QUrl());
    if (!require(component.isReady(), qPrintable(component.errorString())))
        return false;
    std::unique_ptr<QObject> object(
        component.createWithInitialProperties({{"document", QVariant::fromValue(&document)}}));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!require(root, "create the pen overlay fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *overlay = root->findChild<QQuickItem *>("penTestOverlay");
    auto *handles = root->findChild<QQuickItem *>("penHandlesOverlay");
    auto *band    = root->findChild<QQuickItem *>("penRubberBandOverlay");
    if (!require(overlay && handles && band, "pen overlay layers exist"))
        return false;
    const auto frame = [&] {
        _window->update();
        QTest::qWait(50);
        return _window->grabWindow();
    };
    const auto matchesReference = [&] {
        root->setProperty("referenceVisible", true);
        const auto reference = frame();
        root->setProperty("referenceVisible", false);
        const auto  actual = frame();
        const qreal ratio  = qreal(actual.width()) / _window->width();
        const auto  origin = root->mapToScene(QPointF{});
        const QRect bounds(qFloor(origin.x() * ratio), qFloor(origin.y() * ratio),
                           qFloor(root->width() * ratio), qFloor(root->height() * ratio));
        for (int y = bounds.top(); y <= bounds.bottom(); ++y)
            for (int x = bounds.left(); x <= bounds.right(); ++x) {
                const auto a = actual.pixelColor(x, y), b = reference.pixelColor(x, y);
                if (qAbs(a.red() - b.red()) > 4 || qAbs(a.green() - b.green()) > 4 ||
                    qAbs(a.blue() - b.blue()) > 4) {
                    qWarning() << "Pen reference difference" << QPoint(x, y) << a << b;
                    return false;
                }
            }
        return true;
    };
    QSignalSpy pathPaints(overlay, SIGNAL(painted()));
    QSignalSpy handlePaints(handles, SIGNAL(painted()));
    QSignalSpy bandPaints(band, SIGNAL(painted()));
    QSignalSpy snapshots(overlay, SIGNAL(activePathChanged()));
    if (!require(pathPaints.isValid() && handlePaints.isValid() && bandPaints.isValid() &&
                     snapshots.isValid(),
                 "observe layer painting and native path snapshots"))
        return false;
    for (int bits : {8, 16}) {
        if (!require(document.newDocument(512, 320, false, bits),
                     "create the pen reference document"))
            return false;
        document.setTool("pen");
        document.setPenRubberBand(true);
        for (const auto point : {QPointF(80, 70), QPointF(180, 70), QPointF(240, 220)}) {
            document.beginPenGesture(point.x(), point.y());
            if (point.x() > 100)
                document.updatePenGesture(point.x() + 30, point.y() + 30);
            document.endPenGesture();
        }
        document.updatePenHover(340, 130);
        const auto original = document.layerImage(0);
        const auto history  = document.historyIndex();
        frame();
        if (!require(matchesReference(),
                     "separate curve, rubber band and handles match the original drawing"))
            return false;
        pathPaints.clear();
        handlePaints.clear();
        bandPaints.clear();
        snapshots.clear();
        const auto beforeHover = frame();
        for (const auto point : {QPointF(300, 100), QPointF(110, 60), QPointF(300, 170)}) {
            document.updatePenHover(point.x(), point.y());
            frame();
        }
        if (!require(
                pathPaints.isEmpty() && handlePaints.isEmpty() && snapshots.isEmpty() &&
                    bandPaints.count() >= 3 && frame() != beforeHover && band->width() < 512 &&
                    band->height() < 320 && document.historyIndex() == history &&
                    document.layerImage(0) == original,
                "hover repaints only the local provisional curve without path conversion or edits"))
            return false;
        if (!require(matchesReference(),
                     "a moved provisional curve preserves draw order at crossings"))
            return false;
        pathPaints.clear();
        handlePaints.clear();
        bandPaints.clear();
        document.setPenRubberBand(false);
        frame();
        document.updatePenHover(200, 90);
        frame();
        if (!require(!band->isVisible() && pathPaints.isEmpty() && handlePaints.isEmpty() &&
                         bandPaints.isEmpty(),
                     "disabling the guide avoids all hover paints"))
            return false;
        if (!require(matchesReference(), "hiding the guide clears it without changing the path"))
            return false;
        document.setPenRubberBand(true);
        frame();
        pathPaints.clear();
        handlePaints.clear();
        document.beginPenGesture(80, 70, 6, true);
        document.updatePenGesture(90, 85);
        document.endPenGesture();
        frame();
        if (!require(!pathPaints.isEmpty() && !handlePaints.isEmpty() && matchesReference(),
                     "anchor edits invalidate curve and handle rasters"))
            return false;
        document.undo();
        frame();
        if (!require(matchesReference(), "undo restores curve, handles and guide"))
            return false;
        for (qreal zoom : {.5, 2., 8.}) {
            root->setProperty("zoom", zoom);
            root->setProperty("panX", zoom > 1 ? 100 * zoom : 0);
            root->setProperty("panY", zoom > 1 ? 60 * zoom : 0);
            frame();
            if (!require(
                    overlay->width() <= 512 && overlay->height() <= 320 &&
                        handles->width() <= 512 && handles->height() <= 320 &&
                        band->width() <= 512 && band->height() <= 320 && matchesReference(),
                    "all pen layers stay bounded and preserve pixels through zoom and scrolling"))
                return false;
        }
        root->setProperty("zoom", 8.);
        root->setProperty("panX", 0);
        root->setProperty("panY", 0);
        document.updatePenHover(20, 20);
        frame();
        if (!require((overlay->width() == 0 || overlay->height() == 0) && band->width() > 0 &&
                         band->height() > 0 && matchesReference(),
                     "a provisional curve remains visible when its retained path is offscreen"))
            return false;
        root->setProperty("zoom", 1.);
        root->setProperty("panX", 0);
        root->setProperty("panY", 0);
        root->setProperty("viewRotation", 12);
        frame();
        if (!require(overlay->width() <= 610 && overlay->height() <= 610 &&
                         handles->width() <= 610 && handles->height() <= 610 && band->isVisible(),
                     "rotated pen layers keep their visible bounds"))
            return false;
        root->setProperty("viewRotation", 0);
        document.closePenPath();
        frame();
        if (!require(!band->isVisible() && matchesReference(),
                     "closing the path drops the provisional curve"))
            return false;
        pathPaints.clear();
        handlePaints.clear();
        bandPaints.clear();
        snapshots.clear();
        document.updatePenHover(300, 180);
        frame();
        if (!require(pathPaints.isEmpty() && handlePaints.isEmpty() && bandPaints.isEmpty() &&
                         snapshots.isEmpty(),
                     "closed paths do not repaint or reconvert on hover"))
            return false;
        document.setActivePenPath(-1);
        frame();
        if (!require(!overlay->isVisible() && !band->isVisible() && !handles->isVisible() &&
                         document.layerImage(0) == original,
                     "hiding the path clears all layers and preserves pixels"))
            return false;
    }
    return true;
}

#pragma once
#include "Document.h"
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>
#include <memory>

inline bool runSliceOverlaySmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Slice overlay check failed: %s", _message);
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
component LegacySliceOverlay: ScreenOverlayCanvas {
    id: root
    required property var document
    enabled: false
    visible: document.tool==="slice" || document.tool==="sliceSelect"
    antialiasing: true
    function cursorAt(x,y) {
        if(document.tool!=="sliceSelect") return Qt.CrossCursor
        const slice=document.activeSlice>=0 ? document.slices[document.activeSlice] : null
        if(slice) {
            const l=slice.x*zoom,t=slice.y*zoom,r=l+slice.width*zoom,b=t+slice.height*zoom
            const handles=[{x:l,y:t},{x:r,y:t},{x:r,y:b},{x:l,y:b},
                           {x:(l+r)/2,y:t},{x:r,y:(t+b)/2},{x:(l+r)/2,y:b},{x:l,y:(t+b)/2}]
            for(let i=0;i<8;i++) if(Math.hypot(x-handles[i].x,y-handles[i].y)<=10)
                return i<4 ? (i%2===0 ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor)
                           : (i%2===0 ? Qt.SizeVerCursor : Qt.SizeHorCursor)
        }
        for(const s of document.slices)
            if(!s.automatic && x>=s.x*zoom && y>=s.y*zoom && x<=(s.x+s.width)*zoom && y<=(s.y+s.height)*zoom)
                return Qt.SizeAllCursor
        return Qt.ArrowCursor
    }
    onPaint: {
        const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
        for(const r of document.slices) {
            const selected=r.index===document.activeSlice && !r.automatic
            const x=r.x*zoom-root.x,y=r.y*zoom-root.y,w=r.width*zoom,h=r.height*zoom
            c.lineWidth=1;c.strokeStyle=r.automatic ? "#adb3bc" : selected ? "#ffb664" : "#67c5ff"
            c.strokeRect(x+.5,y+.5,w-1,h-1)
            c.fillStyle=r.automatic ? "#515964" : selected ? "#bd6c16" : "#256797"
            c.fillRect(x+1,y+1,26,18);c.fillStyle="#ffffff";c.font="11px sans-serif"
            c.fillText(String(r.index+1).padStart(2,"0"),x+5,y+14)
            if(selected && document.tool==="sliceSelect") {
                for(const p of [{x:x,y:y},{x:x+w,y:y},{x:x+w,y:y+h},{x:x,y:y+h},
                                {x:x+w/2,y:y},{x:x+w,y:y+h/2},{x:x+w/2,y:y+h},{x:x,y:y+h/2}]) {
                    c.fillStyle="#ffb664";c.fillRect(p.x-3,p.y-3,6,6)
                    c.strokeStyle="#101419";c.strokeRect(p.x-3,p.y-3,6,6)
                }
            }
        }
        const draft=document.sliceDraft
        if(draft.width>0 && draft.height>0) {
            c.strokeStyle="#ffffff";c.lineWidth=2
            c.strokeRect(draft.x*zoom-root.x,draft.y*zoom-root.y,draft.width*zoom,draft.height*zoom)
        }
    }
    onZoomChanged:requestPaint()
    onVisibleChanged:requestPaint()
    Connections { target:root.document;function onSlicesChanged(){root.requestPaint()} function onSettingsChanged(){root.requestPaint()} }
}

    Flickable {
        id: view
        anchors.fill: parent; clip: true; interactive: false
        contentWidth: board.width; contentHeight: board.height
        contentX: panX; contentY: panY
        Item {
            id: board
            width: document.width*fixture.zoom; height: document.height*fixture.zoom
            rotation: viewRotation; transformOrigin: Item.TopLeft
            SliceOverlay {
                objectName: "sliceTestOverlay"
                document: fixture.document; viewportItem: view; zoom: fixture.zoom
                visible: !referenceVisible && sliceToolActive
            }
            LegacySliceOverlay {
                document: fixture.document; viewportItem: view; zoom: fixture.zoom
                visible: referenceVisible && (document.tool==="slice" || document.tool==="sliceSelect")
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
    if (!require(root, "create the slice overlay fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *overlay = root->findChild<QQuickItem *>("sliceTestOverlay");
    if (!require(overlay, "slice overlay exists"))
        return false;
    const auto frame = [&] {
        _window->update();
        QTest::qWait(50);
        return _window->grabWindow();
    };
    const auto sequence = [&](const char *_name) {
        const auto value = overlay->property(_name);
        return value.metaType() == QMetaType::fromType<QJSValue>()
                   ? value.value<QJSValue>().toVariant().toList()
                   : value.toList();
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
                    qWarning() << "Slice reference difference" << QPoint(x, y) << a << b;
                    return false;
                }
            }
        return true;
    };
    QSignalSpy paints(overlay, SIGNAL(painted())),
        snapshots(overlay, SIGNAL(sliceRecordsChanged()));
    if (!require(paints.isValid() && snapshots.isValid(),
                 "observe slice paints and metadata snapshots"))
        return false;
    for (int bits : {8, 16}) {
        if (!require(document.newDocument(4096, 3072, true, bits),
                     "create the slice reference document"))
            return false;
        document.setSliceShowAuto(false);
        document.setTool("slice");
        for (int row = 0; row < 12; ++row)
            for (int column = 0; column < 12; ++column) {
                const int x = 20 + column * 310, y = 20 + row * 250;
                document.beginSliceGesture(x, y);
                document.updateSliceGesture(x + 40, y + 30);
                document.endSliceGesture();
            }
        document.beginSliceGesture(600, 100);
        document.updateSliceGesture(602, 103);
        document.endSliceGesture();
        document.setSliceShowAuto(true);
        document.setTool("sliceSelect");
        const auto pixels  = document.layerImage(0);
        const auto all     = document.slices();
        const auto history = document.historyIndex();
        frame();
        if (!require(sequence("sliceRecords").size() == all.size() &&
                         sequence("visibleSlices").size() < all.size() / 4 && matchesReference(),
                     "many offscreen slices are culled without changing visible labels, borders "
                     "and handles"))
            return false;
        paints.clear();
        snapshots.clear();
        const auto cursorAt = [&](qreal _x, qreal _y) {
            QVariant result;
            if (!QMetaObject::invokeMethod(overlay, "cursorAt", Q_RETURN_ARG(QVariant, result),
                                           Q_ARG(QVariant, QVariant(_x)),
                                           Q_ARG(QVariant, QVariant(_y))))
                return -1;
            return result.toInt();
        };
        for (int i = 0; i < 64; ++i) {
            if (!require(cursorAt(600, 100) == int(Qt::SizeFDiagCursor) &&
                             cursorAt(40, 35) == int(Qt::SizeAllCursor) &&
                             cursorAt(300, 180) == int(Qt::ArrowCursor),
                         "cached slice hit testing preserves handles, bodies and empty regions"))
                return false;
        }
        frame();
        if (!require(
                paints.isEmpty() && snapshots.isEmpty() &&
                    document.slices().constData() == all.constData(),
                "cursor queries reuse metadata without automatic-region rebuilding or repainting"))
            return false;
        document.setBrushOpacity(.42);
        frame();
        if (!require(paints.isEmpty() && snapshots.isEmpty(),
                     "unrelated tool settings do not repaint slice overlays"))
            return false;
        document.setTool("slice");
        frame();
        if (!require(!paints.isEmpty() && matchesReference(),
                     "switching slice tools refreshes handle visibility"))
            return false;
        document.setTool("sliceSelect");
        root->setProperty("panX", 620);
        root->setProperty("panY", 80);
        frame();
        bool labelKept = false;
        for (const auto &value : sequence("visibleSlices"))
            labelKept |= value.toMap()["index"].toInt() == 144;
        if (!require(labelKept && matchesReference(),
                     "offscreen tiny slice labels remain visible when they extend into the view"))
            return false;
        root->setProperty("panX", 0);
        root->setProperty("panY", 0);
        document.beginSliceGesture(40, 35, 0);
        for (int i = 0; i < 4; ++i) {
            document.updateSliceGesture(50 + i, 40);
            frame();
            if (!require(document.slices().constData() == all.constData(),
                         "draft drags keep the committed metadata buffer"))
                return false;
        }
        if (!require(matchesReference(), "live slice drafts preserve their original drawing"))
            return false;
        document.endSliceGesture(true);
        frame();
        if (!require(document.historyIndex() == history && matchesReference(),
                     "cancelling a slice draft restores the display without history"))
            return false;
        document.beginSliceGesture(40, 35, 0);
        document.updateSliceGesture(50, 40);
        document.endSliceGesture();
        frame();
        if (!require(document.slices().constData() != all.constData() && matchesReference(),
                     "committing slice geometry invalidates metadata and display"))
            return false;
        document.undo();
        frame();
        if (!require(document.slices() == all && matchesReference(),
                     "undo restores the cached slice description values"))
            return false;
        document.setSliceShowAuto(false);
        frame();
        if (!require(matchesReference(), "hiding automatic slices updates the overlay"))
            return false;
        document.setSliceShowAuto(true);
        for (qreal zoom : {.25, 1., 8.}) {
            root->setProperty("zoom", zoom);
            root->setProperty("panX", 620 * zoom);
            root->setProperty("panY", 80 * zoom);
            frame();
            if (!require(overlay->width() <= 512 && overlay->height() <= 320 && matchesReference(),
                         "zoom and scrolling preserve slice pixels within a bounded surface"))
                return false;
        }
        root->setProperty("zoom", 1.);
        root->setProperty("panX", 0);
        root->setProperty("panY", 0);
        root->setProperty("viewRotation", 12);
        frame();
        if (!require(overlay->width() <= 610 && overlay->height() <= 610 && matchesReference(),
                     "slice culling respects the rotated visible area"))
            return false;
        root->setProperty("viewRotation", 0);
        document.setTool("brush");
        frame();
        if (!require(!overlay->isVisible() && sequence("sliceRecords").isEmpty() &&
                         document.layerImage(0) == pixels && document.channelBits() == bits,
                     "leaving slice tools drops the QML snapshot and preserves native pixels"))
            return false;
    }
    return true;
}

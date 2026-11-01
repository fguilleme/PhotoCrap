#pragma once
#include "CropShadeItem.h"
#include "Document.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtTest>
#include <memory>

inline bool runCropOverlaySmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Crop overlay check failed: %s", _message);
        return _ok;
    };
    const bool hardware =
        QSGRendererInterface::isApiRhiBased(_window->rendererInterface()->graphicsApi());
    Document      document;
    QQmlComponent component(qmlEngine(_window));
    component.setData(R"QML(
import QtQuick
import PhotoCrap
Item {
    id: fixture
    required property var document
    width: 512; height: 320
    property bool referenceVisible: false
    property bool grid: true
    property color backgroundColor: "#5a7896"
    property real zoom: 1
    property real panX: 0
    property real panY: 0
    property real viewRotation: 0
    Rectangle { anchors.fill: parent; color: backgroundColor }

component LegacyCropOverlay: ScreenOverlayCanvas {
    id: root
    required property var document
    property bool showGrid: true
    property bool perspective: document.tool === "perspectiveCrop"
    enabled: false
    visible: document.cropPoints.length === 4
    antialiasing: true
    function cursorAt(x,y) {
        const points=document.cropPoints
        if(points.length!==4) return Qt.CrossCursor
        for(let i=0;i<4;i++) {
            if(Math.hypot(x-points[i].x*zoom,y-points[i].y*zoom)<=10)
                return perspective ? Qt.CrossCursor : i%2===0 ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
            if(!perspective) {
                const p=points[i],q=points[(i+1)%4]
                if(Math.hypot(x-(p.x+q.x)*zoom/2,y-(p.y+q.y)*zoom/2)<=10)
                    return i%2===0 ? Qt.SizeVerCursor : Qt.SizeHorCursor
            }
        }
        let inside=false
        for(let i=0,j=3;i<4;j=i++) {
            const p=points[i],q=points[j],px=x/zoom,py=y/zoom
            if((p.y>py)!==(q.y>py) && px<(q.x-p.x)*(py-p.y)/(q.y-p.y)+p.x) inside=!inside
        }
        return inside ? Qt.SizeAllCursor : Qt.CrossCursor
    }
    onPaint: {
        const points=document.cropPoints
        const c=getContext("2d");c.reset();c.clearRect(0,0,width,height)
        if(points.length!==4) return
        const p=points.map(function(pt){return {x:pt.x*zoom-root.x,y:pt.y*zoom-root.y}})
        // The reverse-wound frame leaves the retained area unobscured.
        c.beginPath();c.rect(0,0,width,height)
        c.moveTo(p[3].x,p[3].y)
        for(let i=2;i>=0;i--) c.lineTo(p[i].x,p[i].y)
        c.closePath();c.fillStyle="rgba(0,0,0,.55)";c.fill()
        c.beginPath();c.moveTo(p[0].x,p[0].y)
        for(let i=1;i<4;i++) c.lineTo(p[i].x,p[i].y)
        c.closePath();c.lineWidth=3;c.strokeStyle="rgba(0,0,0,.9)";c.stroke()
        c.lineWidth=1;c.strokeStyle="#ffffff";c.stroke()
        if(showGrid) {
            c.beginPath()
            for(let i=1;i<3;i++) {
                const t=i/3
                c.moveTo(p[0].x+(p[1].x-p[0].x)*t,p[0].y+(p[1].y-p[0].y)*t)
                c.lineTo(p[3].x+(p[2].x-p[3].x)*t,p[3].y+(p[2].y-p[3].y)*t)
                c.moveTo(p[0].x+(p[3].x-p[0].x)*t,p[0].y+(p[3].y-p[0].y)*t)
                c.lineTo(p[1].x+(p[2].x-p[1].x)*t,p[1].y+(p[2].y-p[1].y)*t)
            }
            c.lineWidth=2;c.strokeStyle="rgba(0,0,0,.65)";c.stroke()
            c.lineWidth=1;c.strokeStyle="rgba(255,255,255,.7)";c.stroke()
        }
        const handles=p.slice()
        if(!perspective) for(let i=0;i<4;i++) handles.push({x:(p[i].x+p[(i+1)%4].x)/2,y:(p[i].y+p[(i+1)%4].y)/2})
        for(const pt of handles) {
            c.fillStyle="#ffffff";c.fillRect(pt.x-4,pt.y-4,8,8)
            c.lineWidth=1;c.strokeStyle="#111111";c.strokeRect(pt.x-4,pt.y-4,8,8)
        }
    }
    onZoomChanged: requestPaint()
    onShowGridChanged: requestPaint()
    onPerspectiveChanged: requestPaint()
    onVisibleChanged: requestPaint()
    Connections { target: root.document; function onCropChanged() { root.requestPaint() } }
}

    Flickable {
        id: view
        anchors.fill: parent
        clip: true
        interactive: false
        contentWidth: board.width; contentHeight: board.height
        contentX: fixture.panX; contentY: fixture.panY
        Item {
            id: board
            width: document.width*fixture.zoom; height: document.height*fixture.zoom
            rotation: fixture.viewRotation; transformOrigin: Item.TopLeft
            CropOverlay {
                objectName: "cropTestOverlay"
                document: fixture.document; viewportItem: view
                zoom: fixture.zoom; showGrid: fixture.grid
                visible: !fixture.referenceVisible && document.cropPoints.length === 4
            }
            LegacyCropOverlay {
                document: fixture.document; viewportItem: view
                zoom: fixture.zoom; showGrid: fixture.grid
                visible: fixture.referenceVisible && document.cropPoints.length === 4
            }
        }
    }
}
)QML",
                      QUrl());
    if (!require(component.isReady(), qPrintable(component.errorString())))
        return false;
    std::unique_ptr<QObject> object(
        component.createWithInitialProperties({{"document", QVariant::fromValue(&document)}}));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!require(root, "create the crop overlay fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *overlay = root->findChild<QQuickItem *>("cropTestOverlay");
    auto *shade   = root->findChild<CropShadeItem *>("cropShade");
    if (!require(overlay && shade, "crop guides and native shade exist"))
        return false;
    const auto frame = [&] {
        _window->update();
        QTest::qWait(50);
        return _window->grabWindow();
    };
    const auto visualItem = [&](const QString &_name) -> QQuickItem * {
        QList<QQuickItem *> pending{root};
        while (!pending.isEmpty()) {
            auto *item = pending.takeLast();
            if (item->objectName() == _name)
                return item;
            pending.append(item->childItems());
        }
        return nullptr;
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
        const auto  filtered = [&](const QImage &_image, int _x, int _y) {
            int rgb[3] = {0, 0, 0}, count = 0;
            for (int yy = qMax(bounds.top(), _y - 2); yy <= qMin(bounds.bottom(), _y + 2); ++yy)
                for (int xx = qMax(bounds.left(), _x - 2); xx <= qMin(bounds.right(), _x + 2);
                     ++xx) {
                    const auto c = _image.pixelColor(xx, yy);
                    rgb[0] += c.red();
                    rgb[1] += c.green();
                    rgb[2] += c.blue();
                    ++count;
                }
            return QColor(rgb[0] / count, rgb[1] / count, rgb[2] / count);
        };
        const auto near = [](QColor _a, QColor _b, int _tolerance) {
            return qAbs(_a.red() - _b.red()) <= _tolerance &&
                   qAbs(_a.green() - _b.green()) <= _tolerance &&
                   qAbs(_a.blue() - _b.blue()) <= _tolerance;
        };
        for (int y = bounds.top(); y <= bounds.bottom(); ++y)
            for (int x = bounds.left(); x <= bounds.right(); ++x) {
                const auto a = actual.pixelColor(x, y), b = reference.pixelColor(x, y);
                // The old rotated Canvas is antialiased then texture-filtered;
                // GPU paths are antialiased once. A small common low-pass filter
                // compares their geometry/coverage without requiring identical AA.
                if (!near(a, b, 4)) {
                    if (hardware && near(filtered(actual, x, y), filtered(reference, x, y), 12))
                        continue;
                    qWarning() << "Crop reference difference" << QPoint(x, y) << a << b;
                    actual.save("/tmp/photocrap-crop-actual.png");
                    reference.save("/tmp/photocrap-crop-reference.png");
                    return false;
                }
            }
        return true;
    };
    const auto makeRectangle = [&](QPointF _a, QPointF _b) {
        document.cancelCrop();
        document.beginCropGesture(_a.x(), _a.y(), 0);
        document.updateCropGesture(_b.x(), _b.y());
        document.endCropGesture();
    };
    for (int bits : {8, 16}) {
        if (!require(document.newDocument(2048, 1536, true, bits),
                     "create the crop reference document"))
            return false;
        document.setTool("crop");
        root->setProperty("backgroundColor", QColor("#5a7896"));
        makeRectangle({80, 60}, {260, 220});
        const auto pixels  = document.layerImage(0);
        const auto history = document.historyIndex();
        frame();
        auto      *guides = visualItem("cropGpuGuides");
        auto      *raster = visualItem("cropRasterGuides");
        const bool vectorGuides =
            hardware && guides && guides->property("curveRendererAvailable").toBool();
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        if (!require(!hardware || vectorGuides, "current RHI kits must use GPU crop guides"))
            return false;
#endif
        if (!require(vectorGuides ? guides && !raster : raster != nullptr,
                     "hardware creates retained guides without a Canvas; software keeps the raster "
                     "fallback"))
            return false;
        if (vectorGuides) {
            auto *shape = guides->findChild<QQuickItem *>("cropGuideShape");
            if (!require(shape && shape->property("rendererType") ==
                                      shape->property("preferredRendererType"),
                         "crop paths use the requested GPU curve renderer"))
                return false;
        }
        if (!require(overlay->property("gpuShade").toBool() == hardware &&
                         shade->shaderActive() == hardware && matchesReference(),
                     "GPU crop shade and guides match the original raster overlay"))
            return false;
        if (hardware && !require(overlay->width() * overlay->height() < 512 * 320 &&
                                     shade->geometryUploads() > 0,
                                 "hardware keeps a four-vertex veil and bounded retained guides"))
            return false;
        QSignalSpy rasterPaints(overlay, SIGNAL(painted()));
        const auto geometry = shade->geometryUploads(), parameters = shade->parameterUpdates();
        document.beginCropGesture(160, 140, 0);
        document.updateCropGesture(170, 150);
        document.endCropGesture();
        frame();
        if (!require(vectorGuides ? rasterPaints.isEmpty() : !rasterPaints.isEmpty(),
                     "GPU crop movement avoids raster paints while the fallback still updates"))
            return false;
        if (!require((!hardware || (shade->geometryUploads() == geometry &&
                                    shade->parameterUpdates() > parameters)) &&
                         document.layerImage(0) == pixels && document.historyIndex() == history &&
                         matchesReference(),
                     "moving the crop only updates shade parameters without rebuilding geometry or "
                     "editing pixels"))
            return false;
        QSignalSpy paints(overlay, SIGNAL(painted())), snapshots(overlay, SIGNAL(pointsChanged()));
        for (int i = 0; i < 64; ++i) {
            QVariant cursor;
            if (!require(QMetaObject::invokeMethod(
                             overlay, "cursorAt", Q_RETURN_ARG(QVariant, cursor),
                             Q_ARG(QVariant, QVariant(90.)), Q_ARG(QVariant, QVariant(70.))) &&
                             cursor.toInt() == int(Qt::SizeFDiagCursor),
                         "crop cursor uses the current retained corner"))
                return false;
        }
        frame();
        if (!require(paints.isEmpty() && snapshots.isEmpty(),
                     "crop hover reuses native point snapshots without repainting"))
            return false;
        root->setProperty("grid", false);
        frame();
        if (!require(matchesReference(), "hiding the grid preserves crop shading"))
            return false;
        root->setProperty("grid", true);
        root->setProperty("backgroundColor", QColor(Qt::white));
        frame();
        if (!require(matchesReference(), "bright backgrounds preserve shade and border blending"))
            return false;
        document.setTool("perspectiveCrop");
        makeRectangle({70, 60}, {330, 240});
        document.beginCropGesture(70, 60, 6);
        document.updateCropGesture(95, 80);
        document.endCropGesture();
        frame();
        if (!require(document.hasCrop() && matchesReference(),
                     "a skewed perspective crop keeps its retained opening and grid"))
            return false;
        for (qreal zoom : {.5, 2., 8.}) {
            root->setProperty("zoom", zoom);
            root->setProperty("panX", zoom > 1 ? 100 * zoom : 0);
            root->setProperty("panY", zoom > 1 ? 60 * zoom : 0);
            frame();
            if (!require(overlay->width() <= 512 && overlay->height() <= 320 && matchesReference(),
                         "crop shade and guides preserve pixels through zoom and clipping"))
                return false;
        }
        root->setProperty("zoom", 1.);
        root->setProperty("panX", 0);
        root->setProperty("panY", 0);
        root->setProperty("viewRotation", 12);
        frame();
        if (!require(overlay->width() <= 610 && shade->width() <= 610 && matchesReference(),
                     "crop shade respects rotation and viewport clipping"))
            return false;
        root->setProperty("viewRotation", 0);
        document.setTool("crop");
        makeRectangle({600, 500}, {900, 800});
        frame();
        if (!require((!hardware || overlay->width() == 0 || overlay->height() == 0) &&
                         matchesReference(),
                     "a crop outside the view still shades the viewport through its native child"))
            return false;
        document.cancelCrop();
        document.beginCropGesture(40, 40, 0);
        frame();
        if (!require(matchesReference(),
                     "the initial degenerate frame shades the whole image safely"))
            return false;
        document.endCropGesture();
        document.cancelCrop();
        frame();
        if (!require(!overlay->isVisible() && !shade->isVisible() &&
                         document.layerImage(0) == pixels && document.channelBits() == bits,
                     "cancelling crop clears the veil without touching native image precision"))
            return false;
    }
    return true;
}

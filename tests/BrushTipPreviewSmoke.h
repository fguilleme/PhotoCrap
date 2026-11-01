#pragma once
#include "OverlayReference.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtTest>
#include <memory>

inline bool runBrushTipPreviewSmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Brush tip preview check failed: %s", _message);
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
    width:512;height:320;clip:true
    property bool referenceVisible:false
    property bool showPreview:true
    property real previewWidth:400
    property real previewHeight:105
    property real previewOpacity:1
    property real previewRotation:0
    property point position:Qt.point(45,85)
    property real brushSize:32
    property real hardness:.8
    property real brushOpacity:1
    property real flow:1
    property real spacing:.25
    property real angle:0
    property real roundness:1
    Rectangle {anchors.fill:parent;color:"#597a96"}
    BrushTipPreview {
        objectName:"tipTestPreview"
        x:fixture.position.x;y:fixture.position.y
        width:fixture.previewWidth;height:fixture.previewHeight
        opacity:fixture.previewOpacity;rotation:fixture.previewRotation
        visible:fixture.showPreview && !fixture.referenceVisible
        brushSize:fixture.brushSize;hardness:fixture.hardness
        brushOpacity:fixture.brushOpacity;flow:fixture.flow;spacing:fixture.spacing
        angle:fixture.angle;roundness:fixture.roundness
    }
    Canvas {
        x:fixture.position.x;y:fixture.position.y
        width:fixture.previewWidth;height:fixture.previewHeight
        opacity:fixture.previewOpacity;rotation:fixture.previewRotation
        visible:fixture.showPreview && fixture.referenceVisible
        onVisibleChanged:if(visible)requestPaint()
        onPaint: {
            const c=getContext("2d");c.reset();c.clearRect(0,0,Math.ceil(width),Math.ceil(height))
            c.fillStyle="#151a20";c.fillRect(0,0,width,height)
            const radius=Math.min(fixture.brushSize/2,38)
            const spacing=Math.max(1,fixture.brushSize*fixture.spacing)
            c.globalAlpha=fixture.brushOpacity
            for(let x=28;x<width-20;x+=spacing) {
                c.save();c.translate(x,height/2);c.rotate(fixture.angle*Math.PI/180);c.scale(1,fixture.roundness)
                const g=c.createRadialGradient(0,0,0,0,0,radius)
                g.addColorStop(0,"rgba(190,236,203,"+fixture.flow+")")
                g.addColorStop(Math.min(.999,fixture.hardness),"rgba(190,236,203,"+fixture.flow+")")
                g.addColorStop(1,"rgba(190,236,203,0)")
                c.fillStyle=g;c.beginPath();c.arc(0,0,radius,0,Math.PI*2);c.fill();c.restore()
            }
        }
    }
}
)QML",
                      QUrl());
    if (!require(component.isReady(), qPrintable(component.errorString())))
        return false;
    std::unique_ptr<QObject> object(component.create());
    auto                    *root = qobject_cast<QQuickItem *>(object.get());
    if (!require(root, "create fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *preview = root->findChild<QQuickItem *>("tipTestPreview");
    if (!require(preview, "create preview"))
        return false;
    const auto frame = [&] {
        _window->update();
        QTest::qWait(50);
        return _window->grabWindow();
    };
    const auto visualObject = [&](const QString &_name) -> QQuickItem * {
        QList<QQuickItem *> pending{preview};
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
        const auto  origin = root->mapToScene(QPointF{});
        const qreal ratio  = qreal(actual.width()) / _window->width();
        const QRect bounds(qFloor(origin.x() * ratio), qFloor(origin.y() * ratio),
                           qFloor(root->width() * ratio), qFloor(root->height() * ratio));
        return compareOverlayReference(actual, reference, bounds, hardware,
                                       "/tmp/photocrap-brush-tip");
    };
    frame();
    if (!require(preview->property("gpuPreview").toBool() == hardware &&
                     preview->property("rasterActive").toBool() != hardware && matchesReference(),
                 "default stroke uses the expected backend and matches the legacy drawing"))
        return false;
    struct Tip {
        qreal size, hardness, opacity, flow, spacing, angle, roundness;
    };
    const Tip tips[] = {{32, 0., 1., 1., .25, 0., 1.},       {32, 1., 1., 1., .25, 0., 1.},
                        {50, .4, .6, .35, .6, 37., .4},      {76, .7, .45, .6, 1., -80., .2},
                        {500, .8, 1., 1., .25, 0., 1.},      {32, .8, 0., 1., .25, 0., 1.},
                        {32, .9, .25, .01, .25, 0., 1.},     {1, .4, .7, .5, .25, 0., 1.},
                        {76, 1., 1., 1., .01, 17., 1.},      {76, .4, .02, .01, .01, 0., 1.},
                        {76, .5, .8, .5, .25, 75., .01},     {32, .4, .7, .5, 10., -17., .5},
                        {32, .4, .7, .5, .25, -17., .5},     {32, .5, .04, .1, .0625, 0., 1.},
                        {32, 1., .25, .3, .0625, 45., .125}, {76, .9, .7, .45, .0625, -150., .5},
                        {32, .001, 1., .7, .3, 90., 1.},     {32, .999, 1., .7, .3, 90., 1.}};
    for (const auto &tip : tips) {
        root->setProperty("brushSize", tip.size);
        root->setProperty("hardness", tip.hardness);
        root->setProperty("brushOpacity", tip.opacity);
        root->setProperty("flow", tip.flow);
        root->setProperty("spacing", tip.spacing);
        root->setProperty("angle", tip.angle);
        root->setProperty("roundness", tip.roundness);
        frame();
        if (!require(matchesReference(),
                     "soft/hard, rotated, flat, large, transparent and dense tips match")) {
            qWarning() << tip.size << tip.hardness << tip.opacity << tip.flow << tip.spacing
                       << tip.angle << tip.roundness;
            return false;
        }
    }
    root->setProperty("brushSize", 32.);
    root->setProperty("roundness", 1.);
    root->setProperty("spacing", .25);
    frame();
    QSignalSpy                  paints(preview, SIGNAL(painted()));
    auto                       *shader = visualObject("brushTipPreviewShader");
    std::unique_ptr<QSignalSpy> sizes;
    if (hardware) {
        if (!require(shader && !preview->property("shaderFailed").toBool(),
                     "native shader exists without error"))
            return false;
        sizes = std::make_unique<QSignalSpy>(shader, SIGNAL(previewSizeChanged()));
        if (!require(sizes->isValid(), "observe geometry size"))
            return false;
    }
    for (qreal value : {.1, .7, 1.}) {
        root->setProperty("brushOpacity", value);
        root->setProperty("angle", value * 90);
        root->setProperty("hardness", value);
        frame();
    }
    if (!require(hardware ? (paints.isEmpty() && sizes->isEmpty() &&
                             visualObject("brushTipPreviewShader") == shader)
                          : !paints.isEmpty(),
                 "GPU edits preserve the rectangle without raster paints; software edits repaint"))
        return false;
    root->setProperty("previewOpacity", .35);
    root->setProperty("previewRotation", 11.);
    frame();
    if (!require(matchesReference(), "inherited opacity and rotation are preserved"))
        return false;
    root->setProperty("previewRotation", 0.);
    root->setProperty("previewWidth", 329.5);
    root->setProperty("previewHeight", 91.25);
    frame();
    if (!require(!preview->property("gpuPreview").toBool() && matchesReference(),
                 "fractional sizes use raster projection"))
        return false;
    root->setProperty("previewWidth", 330.);
    root->setProperty("previewHeight", 91.);
    frame();
    if (!require(preview->property("gpuPreview").toBool() == hardware && matchesReference(),
                 "GPU return removes the old raster"))
        return false;
    root->setProperty("previewWidth", 40.);
    frame();
    if (!require(matchesReference(), "narrow previews contain only the background"))
        return false;
    const auto hiddenPaints = paints.count();
    root->setProperty("showPreview", false);
    root->setProperty("brushSize", 76.);
    root->setProperty("spacing", .01);
    frame();
    if (!require(paints.count() == hiddenPaints, "hidden changes do not rasterize"))
        return false;
    root->setProperty("showPreview", true);
    root->setProperty("previewWidth", 360.);
    frame();
    if (!require(matchesReference(), "reopening uses settings changed while hidden"))
        return false;
    qInfo("Brush tip preview checks passed");
    return true;
}

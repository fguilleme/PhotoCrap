#pragma once
#include "OverlayReference.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtTest>
#include <memory>

inline bool runColorWheelSmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Color wheel check failed: %s", _message);
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
    width: 512; height: 420; clip: true
    property real value: .16
    property real shadeWidth: 230
    property real shadeHeight: 230
    property real shadeOpacity: 1
    property real shadeRotation: 0
    property point shadePosition: Qt.point(48,30)
    property bool referenceVisible: false
    property bool showShade: true
    property bool showEditor: false
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position:0; color:"#c23a7b" }
            GradientStop { position:1; color:"#37bbd9" }
        }
    }
    ColorWheelShade {
        objectName: "shadeTestItem"
        x:fixture.shadePosition.x;y:fixture.shadePosition.y
        width:fixture.shadeWidth;height:fixture.shadeHeight
        value:fixture.value;opacity:fixture.shadeOpacity;rotation:fixture.shadeRotation
        visible:fixture.showShade && !fixture.referenceVisible && !fixture.showEditor
    }
    Canvas {
        id: reference
        x:fixture.shadePosition.x;y:fixture.shadePosition.y
        width:fixture.shadeWidth;height:fixture.shadeHeight
        opacity:fixture.shadeOpacity;rotation:fixture.shadeRotation
        visible:fixture.showShade && fixture.referenceVisible && !fixture.showEditor
        onVisibleChanged: if(visible)requestPaint()
        onPaint: {
            const c=getContext("2d")
            c.reset();c.clearRect(0,0,Math.ceil(width),Math.ceil(height))
            const center=width/2,radius=Math.min(width,height)/2-9
            if(radius<=0)return
            c.beginPath();c.arc(center,center,radius,0,Math.PI*2)
            c.fillStyle="rgba(0,0,0,"+(1-fixture.value)+")";c.fill()
        }
    }
    GradientMapEditor {
        id: editor
        objectName:"wheelTestEditor"
        x:48;y:12;width:380;height:380
        visible:fixture.showEditor
    }
}
)QML",
                      QUrl());
    if (!require(component.isReady(), qPrintable(component.errorString())))
        return false;
    std::unique_ptr<QObject> object(component.create());
    auto                    *root = qobject_cast<QQuickItem *>(object.get());
    if (!require(root, "create color wheel fixture"))
        return false;
    root->setParentItem(_window->contentItem());
    root->setPosition({40, 120});
    root->setZ(2000);
    auto *shade  = root->findChild<QQuickItem *>("shadeTestItem");
    auto *editor = root->findChild<QQuickItem *>("wheelTestEditor");
    if (!require(shade && editor, "shade and editor exist"))
        return false;
    const auto frame = [&] {
        _window->update();
        QTest::qWait(50);
        return _window->grabWindow();
    };
    const auto visualObject = [&](QQuickItem *_parent, const QString &_name) -> QQuickItem * {
        QList<QQuickItem *> pending{_parent};
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
                                       "/tmp/photocrap-color-wheel");
    };
    // Verify actual native output, independently of ShaderEffect's status:
    // Qt may already have cached these QSB files while building the main UI.
    root->setProperty("value", 1.);
    const auto background = frame();
    root->setProperty("value", .16);
    const auto   first       = frame();
    const auto   centerPoint = shade->mapToScene(QPointF(shade->width() / 2, shade->width() / 2));
    const qreal  pixelRatio  = qreal(first.width()) / _window->width();
    const QPoint centerPixel(qFloor(centerPoint.x() * pixelRatio),
                             qFloor(centerPoint.y() * pixelRatio));
    const auto   under = background.pixelColor(centerPixel), over = first.pixelColor(centerPixel);
    if (!require(qAbs(over.red() - qRound(under.red() * .16)) <= 3 &&
                     qAbs(over.green() - qRound(under.green() * .16)) <= 3 &&
                     qAbs(over.blue() - qRound(under.blue() * .16)) <= 3,
                 "the disk actually darkens its background by the selected value"))
        return false;
    if (!require(shade->property("gpuShade").toBool() == hardware &&
                     shade->property("rasterActive").toBool() != hardware && matchesReference(),
                 "native and software shades match the original disk"))
        return false;
    QSignalSpy paints(shade, SIGNAL(painted()));
    auto      *shader = qobject_cast<QQuickItem *>(visualObject(shade, "colorWheelShadeShader"));
    std::unique_ptr<QSignalSpy> sizeChanges;
    if (hardware) {
        if (!require(shader && !shade->property("shaderFailed").toBool(),
                     "the recreated shader exists without error"))
            return false;
        sizeChanges = std::make_unique<QSignalSpy>(shader, SIGNAL(shadeSizeChanged()));
        if (!require(sizeChanges->isValid(), "observe the disk size"))
            return false;
    }
    for (qreal value : {0., .1, .5, 1., .96, .16}) {
        root->setProperty("value", value);
        frame();
        if (hardware && !require(qAbs(shader->opacity() - (1 - value)) < .00001 &&
                                     visualObject(shade, "colorWheelShadeShader") == shader &&
                                     sizeChanges->isEmpty() && paints.isEmpty(),
                                 "brightness updates opacity without geometry or raster paints"))
            return false;
    }
    if (!require(hardware || !paints.isEmpty(), "software shade really paints"))
        return false;
    for (qreal value : {0., .1, .5, 1., .96, .16}) {
        root->setProperty("value", value);
        frame();
        if (!require(matchesReference(),
                     "black, transparent and intermediate disks preserve coverage"))
            return false;
    }
    root->setProperty("shadeOpacity", .37);
    frame();
    if (!require(matchesReference(), "inherited opacity is preserved"))
        return false;
    root->setProperty("shadeOpacity", 1.);
    root->setProperty("shadeRotation", 17.);
    frame();
    if (!require(matchesReference(), "rotated disk keeps its center and radius"))
        return false;
    root->setProperty("shadeRotation", 0.);
    root->setProperty("shadeWidth", 264.);
    root->setProperty("shadeHeight", 220.);
    frame();
    if (!require(matchesReference(), "non-square bounds keep the legacy center"))
        return false;
    root->setProperty("shadeWidth", 239.5);
    root->setProperty("shadeHeight", 231.25);
    frame();
    if (!require(!shade->property("gpuShade").toBool() && matchesReference(),
                 "fractional dimensions retain the original Canvas projection"))
        return false;
    const int hiddenPaints = paints.count();
    root->setProperty("showShade", false);
    root->setProperty("value", .73);
    frame();
    if (!require(paints.count() == hiddenPaints, "hidden brightness changes do not paint"))
        return false;
    root->setProperty("shadeWidth", 240.);
    root->setProperty("shadeHeight", 231.);
    root->setProperty("showShade", true);
    frame();
    if (!require(shade->property("gpuShade").toBool() == hardware && matchesReference(),
                 "returning from raster clears stale transparent shading"))
        return false;
    root->setProperty("shadeWidth", 18.);
    root->setProperty("shadeHeight", 18.);
    frame();
    if (!require(matchesReference(), "zero-radius shade leaves its background intact"))
        return false;
    // The real editor must not repaint its fixed spectrum after HSV synchronisation.
    root->setProperty("showEditor", true);
    const auto shown       = frame();
    auto      *wheel       = editor->findChild<QQuickItem *>("gradientMapWheel");
    auto      *editorShade = editor->findChild<QQuickItem *>("gradientMapValueShade");
    if (!require(wheel && editorShade, "editor uses the shared shade"))
        return false;
    const auto  center = wheel->mapToScene(QPointF(wheel->width() / 2, wheel->width() / 2));
    const qreal ratio  = qreal(shown.width()) / _window->width();
    const auto  centerColor =
        shown.pixelColor(qFloor(center.x() * ratio), qFloor(center.y() * ratio));
    const int expectedGray = qRound(editor->property("shadowValue").toReal() * 255);
    if (!require(qAbs(centerColor.red() - expectedGray) <= 3 &&
                     qAbs(centerColor.green() - expectedGray) <= 3 &&
                     qAbs(centerColor.blue() - expectedGray) <= 3,
                 "the spectrum is actually painted before its cache is reused"))
        return false;
    QSignalSpy spectrumPaints(wheel, SIGNAL(painted()));
    QSignalSpy editorShadePaints(editorShade, SIGNAL(painted()));
    editor->setProperty("shadowColor", "#d32874");
    editor->setProperty("highlightColor", "#1a7bb4");
    frame();
    editor->setProperty("activeHandle", 1);
    frame();
    if (!require(spectrumPaints.isEmpty() &&
                     qAbs(editorShade->property("value").toReal() -
                          editor->property("highlightValue").toReal()) < .00001,
                 "endpoint and active-handle changes reuse the spectrum and update the shade"))
        return false;
    if (hardware &&
        !require(editorShade->property("gpuShade").toBool() && editorShadePaints.isEmpty(),
                 "the editor uses opacity-only hardware updates"))
        return false;
    if (!hardware &&
        !require(!editorShadePaints.isEmpty(), "editor software fallback follows brightness"))
        return false;
    qInfo("Color wheel checks passed");
    return true;
}

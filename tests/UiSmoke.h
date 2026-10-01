#pragma once
#include <QtTest>
#include <QQuickItem>
#include <QQuickWindow>
#include "Document.h"

// Exercise pointer delivery through QML, keyboard focus and the unsaved guard.
// Compiled only with BUILD_TESTING; production builds need no Qt Test runtime.
inline bool runUiSmoke(QQuickWindow *window,Document &document) {
    auto require=[](bool ok,const char *message){if(!ok) qWarning("UI check failed: %s",message); return ok;};
    auto area=window->findChild<QQuickItem*>("drawingArea");
    if(!require(area,"drawing area exists")) return false;
    document.newDocument(300,200,true);
    QTest::qWait(100);
    document.setColor(Qt::red); document.setBrushSize(16); document.setTool("brush");
    QPoint start=area->mapToScene(QPointF(area->width()/3,area->height()/2)).toPoint();
    QPoint end=area->mapToScene(QPointF(area->width()*2/3,area->height()/2)).toPoint();
    QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);
    QTest::mouseMove(window,end,20);
    QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,end);
    if(!require(document.canUndo() && document.composite().pixelColor(100,100).red()>200 && document.composite().pixelColor(100,100).alpha()>0,"pointer paints document")) return false;
    document.undo();
    if(!require(document.composite().pixelColor(100,100).alpha()==0,"gesture undoes")) return false;
    document.redo();
    QMetaObject::invokeMethod(window,"openLayerContextMenu");QTest::qWait(100);
    if(!require(window->property("layerContextMenuVisible").toBool(),"layer context menu opens")) return false;
    QMetaObject::invokeMethod(window,"closeLayerContextMenu");
    document.setTool("text");
    auto textField=window->findChild<QQuickItem*>("textField");
    if(!require(textField,"text field exists")) return false;
    textField->forceActiveFocus();
    QTest::keyClick(window,Qt::Key_B);
    if(!require(document.tool()=="text","typing B does not switch tools")) return false;
    auto preferences=window->findChild<QObject*>("preferencesDialog");
    if(!require(preferences,"preferences dialog exists")) return false;
    QMetaObject::invokeMethod(preferences,"open");
    QTest::qWait(150);
    if(!require(preferences->property("visible").toBool(),"keyboard settings dialog opens")) return false;
    auto shortcutList=window->findChild<QObject*>("shortcutList");
    if(!require(shortcutList && shortcutList->property("count").toInt()==33,"keyboard settings list is populated")) return false;
    QMetaObject::invokeMethod(preferences,"close");
    auto brushSettings=window->findChild<QObject*>("brushSettingsDialog");
    if(!require(brushSettings,"brush settings dialog exists")) return false;
    QMetaObject::invokeMethod(brushSettings,"open");QTest::qWait(100);
    if(!require(brushSettings->property("visible").toBool(),"brush settings dialog opens")) return false;
    QMetaObject::invokeMethod(brushSettings,"close");
    QMetaObject::invokeMethod(window,"guard",Q_ARG(QVariant,QVariant("new")));
    QTest::qWait(200);
    auto guard=window->findChild<QObject*>("unsavedDialog");
    if(!require(guard && guard->property("visible").toBool(),"unsaved changes are guarded")) return false;
    QMetaObject::invokeMethod(guard,"close");
    QTest::qWait(150);
    document.newDocument(300,200,true);
    QMetaObject::invokeMethod(window,"guard",Q_ARG(QVariant,QVariant("new")));
    QTest::qWait(150);
    auto dialog=window->findChild<QObject*>("newDialog");
    if(!require(dialog && dialog->property("visible").toBool() && dialog->property("height").toReal()>200,"new document dialog has content")) return false;
    QMetaObject::invokeMethod(dialog,"reject");
    document.addAdjustmentLayer("brightnessContrast");
    auto layerProperties=window->findChild<QObject*>("layerPropertiesDialog");
    if(!require(layerProperties,"layer properties dialog exists")) return false;
    QMetaObject::invokeMethod(layerProperties,"open");
    QTest::qWait(150);
    if(!require(layerProperties->property("visible").toBool(),"adjustment properties dialog opens")) return false;
    QMetaObject::invokeMethod(layerProperties,"close");
    document.createDemo(); document.setTool("brush");
    QTest::qWait(200);
    qInfo("UI interaction checks passed");
    return true;
}

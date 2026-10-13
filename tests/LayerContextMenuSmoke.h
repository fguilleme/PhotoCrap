#pragma once
#include "Document.h"
#include <QJSValue>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

inline bool runLayerContextMenuSmoke(QQuickWindow *_window, Document &_document)
{
    auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Layer menu check failed: %s", _message);
        return _ok;
    };
    auto layerMenu       = _window->findChild<QObject *>("layerContextMenu");
    auto maskMenu        = _window->findChild<QObject *>("maskContextMenu");
    auto properties      = _window->findChild<QObject *>("layerPropertiesDialog");
    auto lock            = _window->findChild<QObject *>("contextLayerLock");
    auto layerProperties = _window->findChild<QObject *>("contextLayerProperties");
    auto maskProperties  = _window->findChild<QObject *>("contextMaskProperties");
    auto maskInvert      = _window->findChild<QObject *>("contextMaskInvert");
    auto maskDelete      = _window->findChild<QObject *>("contextMaskDelete");
    auto maskEnabled     = _window->findChild<QObject *>("contextMaskEnabled");
    auto maskView        = _window->findChild<QObject *>("contextMaskView");
    if (!require(layerMenu && maskMenu && properties && lock && layerProperties && maskProperties &&
                     maskInvert && maskDelete && maskEnabled && maskView,
                 "separate layer and mask actions exist"))
        return false;
    for (auto menu : {layerMenu, maskMenu}) {
        int unavailable = 0;
        for (auto item : menu->findChildren<QObject *>()) {
            if (!item->property("text").toString().endsWith("(NYI)"))
                continue;
            ++unavailable;
            if (!require(!item->property("enabled").toBool(), "NYI actions are disabled"))
                return false;
        }
        if (!require(unavailable >= 4, "missing commands are explicitly inventoried"))
            return false;
    }
    _document.newDocument(100, 80, true);
    _document.addLayerMask(true);
    _document.addLayer();
    _document.addLayerMask(false);
    QMetaObject::invokeMethod(_window, "selectDockPanel", Q_ARG(QVariant, "layers"));
    QMetaObject::invokeMethod(_window, "moveDockPanelTo", Q_ARG(QVariant, "layers"),
                              Q_ARG(QVariant, "color"), Q_ARG(QVariant, false), Q_ARG(QVariant, 0));
    auto layerList = _window->findChild<QObject *>("layerListView");
    if (layerList)
        QMetaObject::invokeMethod(layerList, "positionViewAtBeginning");
    QTest::qWait(100);
    auto thumbnail = [&](int _index, const char *_name) -> QQuickItem * {
        QList<QQuickItem *> pending{qobject_cast<QQuickItem *>(layerList)};
        while (!pending.isEmpty()) {
            auto row = pending.takeLast();
            if (!row)
                continue;
            pending.append(row->childItems());
            if (row->objectName() != "layerDelegate")
                continue;
            auto model = row->property("modelData");
            if (model.metaType() == QMetaType::fromType<QJSValue>())
                model = model.value<QJSValue>().toVariant();
            if (row->isVisible() && model.toMap().value("layerIndex", -1).toInt() == _index)
                return row->findChild<QQuickItem *>(_name);
        }
        return nullptr;
    };
    auto target = thumbnail(0, "maskThumbnailMouse");
    if (!require(target && target->isVisible(), "mask thumbnail is available for a pointer test"))
        return false;
    const QPoint point =
        target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint();
    const int history = _document.historyIndex();
    QTest::mouseClick(_window, Qt::RightButton, Qt::NoModifier, point);
    QTest::qWait(50);
    if (!require(
            maskMenu->property("visible").toBool() && !layerMenu->property("visible").toBool() &&
                !properties->property("visible").toBool() && _document.activeLayer() == 0 &&
                _document.editingMask() && _document.historyIndex() == history,
            "right-click targets the clicked mask without opening properties or adding history"))
        return false;
    QMetaObject::invokeMethod(maskMenu, "close");
    QMetaObject::invokeMethod(maskInvert, "triggered");
    if (!require(_document.maskImage(0).pixelColor(0, 0).red() == 0 &&
                     _document.maskImage(1).pixelColor(0, 0).red() == 0,
                 "mask inversion acts on the clicked layer"))
        return false;
    _document.undo();
    if (!require(_document.maskImage(0).pixelColor(0, 0).red() == 255,
                 "mask inversion is undoable"))
        return false;
    QMetaObject::invokeMethod(maskView, "triggered");
    if (!require(_document.maskOnlyLayer() == 0, "mask menu displays the mask alone"))
        return false;
    QMetaObject::invokeMethod(maskView, "triggered");
    QMetaObject::invokeMethod(maskProperties, "triggered");
    QTest::qWait(50);
    if (!require(properties->property("visible").toBool() && _document.editingMask(),
                 "mask properties action opens the mask editor"))
        return false;
    properties->setProperty("keepChanges", true);
    QMetaObject::invokeMethod(properties, "accept");
    QMetaObject::invokeMethod(lock, "triggered");
    if (!require(!maskInvert->property("enabled").toBool() &&
                     !maskDelete->property("enabled").toBool() &&
                     !maskEnabled->property("enabled").toBool() &&
                     maskProperties->property("enabled").toBool(),
                 "locked masks allow inspection but disable mutation commands"))
        return false;
    QMetaObject::invokeMethod(lock, "triggered");
    if (!require(maskInvert->property("enabled").toBool(), "unlock re-enables mask editing"))
        return false;
    QMetaObject::invokeMethod(maskDelete, "triggered");
    if (!require(_document.maskImage(0).isNull(), "delete removes only the mask"))
        return false;
    _document.undo();
    if (!require(!_document.maskImage(0).isNull(), "mask deletion is undoable"))
        return false;
    target = thumbnail(0, "layerThumbnailMouse");
    if (!require(target, "layer thumbnail has its own context target"))
        return false;
    QTest::mouseClick(
        _window, Qt::RightButton, Qt::NoModifier,
        target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint());
    QTest::qWait(50);
    if (!require(layerMenu->property("visible").toBool() &&
                     !maskMenu->property("visible").toBool() &&
                     !layerProperties->property("enabled").toBool(),
                 "pixel thumbnail opens layer menu with no misleading mask-properties action"))
        return false;
    QMetaObject::invokeMethod(layerMenu, "close");
    _document.selectLayer(1, true);
    if (!require(_document.selectedLayerCount() == 2 && !lock->property("enabled").toBool(),
                 "single-layer actions do not silently act on one member of a multi-selection"))
        return false;
    _document.addAdjustmentLayer("light");
    if (!require(layerProperties->property("enabled").toBool(),
                 "adjustment layer exposes its properties"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "triggered");
    QTest::qWait(50);
    if (!require(properties->property("visible").toBool() && !_document.editingMask(),
                 "layer properties action opens adjustment controls"))
        return false;
    properties->setProperty("keepChanges", true);
    QMetaObject::invokeMethod(properties, "accept");
    return true;
}

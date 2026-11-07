#pragma once
#include "Document.h"
#include <QQuickWindow>
#include <QtTest>

inline bool runLayerMergeSmoke(QQuickWindow *_window, Document &_document)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Layer merge UI check failed: %s", _message);
        return _ok;
    };
    _document.setBackgroundEditThreshold(-1);
    _document.newDocument(64, 48, true, 16);
    _document.setColor(Qt::red);
    _document.fillWithColor(true);
    _document.addLayer();
    _document.setTool("brush");
    _document.setColor(Qt::blue);
    _document.setBrushSize(20);
    _document.beginStroke(30, 20);
    _document.endStroke();
    const auto expected = _document.composite();
    QTest::qWait(50);
    auto *merge   = _window->findChild<QObject *>("mergeLayersAction");
    auto *flatten = _window->findChild<QObject *>("flattenDialog");
    if (!require(merge && flatten && merge->property("enabled").toBool(),
                 "merge actions exist and are enabled"))
        return false;
    QTest::keyClick(_window, Qt::Key_E, Qt::ControlModifier);
    if (!require(_document.layers().size() == 1 && _document.composite() == expected,
                 "Command/Ctrl-E merges downward"))
        return false;
    QTest::keyClick(_window, Qt::Key_Z, Qt::ControlModifier);
    _document.selectLayer(0, true);
    QTest::keyClick(_window, Qt::Key_E, Qt::ControlModifier | Qt::AltModifier);
    if (!require(_document.layers().size() == 3 && _document.layerImage(2) == expected,
                 "Command/Ctrl-Option/Alt-E stamps selected layers"))
        return false;
    QTest::keyClick(_window, Qt::Key_Z, Qt::ControlModifier);
    QTest::keyClick(_window, Qt::Key_E, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
    if (!require(_document.layers().size() == 3 && _document.layerImage(2) == expected,
                 "Command/Ctrl-Option/Alt-Shift-E stamps visible layers"))
        return false;
    QTest::keyClick(_window, Qt::Key_Z, Qt::ControlModifier);
    QTest::keyClick(_window, Qt::Key_E, Qt::ControlModifier | Qt::ShiftModifier);
    if (!require(_document.layers().size() == 1 && _document.composite() == expected,
                 "Command/Ctrl-Shift-E merges visible layers"))
        return false;
    _document.undo();
    _document.setLayerVisible(0, false);
    const int history = _document.historyIndex();
    QMetaObject::invokeMethod(_window, "requestFlatten");
    QTest::qWait(40);
    if (!require(flatten->property("visible").toBool(), "flatten asks about hidden layers"))
        return false;
    QMetaObject::invokeMethod(flatten, "reject");
    if (!require(_document.layers().size() == 2 && _document.historyIndex() == history,
                 "cancel keeps hidden layers and history"))
        return false;
    QMetaObject::invokeMethod(_window, "requestFlatten");
    QMetaObject::invokeMethod(flatten, "accept");
    QTest::qWait(40);
    if (!require(_document.layers().size() == 1 &&
                     _document.layerImage(0).pixelColor(0, 0) == QColor(Qt::white),
                 "flatten confirms and makes transparent areas white"))
        return false;
    _document.setBackgroundEditThreshold(4LL * 1024 * 1024);
    return true;
}

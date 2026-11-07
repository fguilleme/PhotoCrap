#pragma once
#include "Document.h"
#include <QCursor>
#include <QMouseEvent>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>
#include <memory>

inline bool runValueScrubberSmoke(QQuickWindow *_window, Document &_document)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Value scrubber check failed: %s", _message);
        return _ok;
    };
    _document.setTool("brush");
    _document.setBrushSize(24);
    QTest::qWait(40);
    auto *label    = _window->findChild<QQuickItem *>("brushSizeLabel");
    auto *field    = _window->findChild<QQuickItem *>("brushSizeTop");
    auto *scrubber = label ? label->findChild<QQuickItem *>("valueScrubber") : nullptr;
    if (!require(label && field && scrubber && label->isVisible(),
                 "the brush size title has a scrubber"))
        return false;
    const auto point =
        scrubber->mapToScene(QPointF(scrubber->width() / 2, scrubber->height() / 2)).toPoint();
    const auto move = [&](const QPoint &_position, Qt::KeyboardModifiers _modifiers = {}) {
        QMouseEvent event(QEvent::MouseMove, QPointF(_position),
                          QPointF(_window->mapToGlobal(_position)), Qt::NoButton, Qt::LeftButton,
                          _modifiers);
        QCoreApplication::sendEvent(_window, &event);
    };
    QTest::mouseMove(_window, point);
    if (!require(scrubber->cursor().shape() == Qt::OpenHandCursor,
                 "hovering a numeric title shows a hand"))
        return false;
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, point);
    move(point + QPoint(20, 0));
    if (!require(_document.brushSize() == 34 && scrubber->cursor().shape() == Qt::ClosedHandCursor,
                 "a horizontal drag changes brush size and grabs the cursor"))
        return false;
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, point + QPoint(20, 0));
    _document.setBrushSize(77);
    if (!require(field->property("value").toInt() == 77,
                 "scrubbing preserves the numeric model binding"))
        return false;
    for (auto modifiers :
         {Qt::KeyboardModifiers(Qt::ShiftModifier), Qt::KeyboardModifiers(Qt::AltModifier)}) {
        _document.setBrushSize(24);
        QTest::mousePress(_window, Qt::LeftButton, modifiers, point);
        move(point + QPoint(20, 0), modifiers);
        QTest::mouseRelease(_window, Qt::LeftButton, modifiers, point + QPoint(20, 0));
        if (!require(_document.brushSize() == (modifiers.testFlag(Qt::ShiftModifier) ? 124 : 25),
                     "Shift accelerates and Alt refines the adjustment"))
            return false;
    }
    _document.setBrushSize(495);
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, point);
    move(point + QPoint(100, 0));
    if (!require(_document.brushSize() == 500, "scrubbing clamps to the field maximum"))
        return false;
    move(point + QPoint(96, 0));
    if (!require(_document.brushSize() == 498, "reversing at the limit reacts immediately"))
        return false;
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, point + QPoint(96, 0));
    _document.setBrushSize(37);
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, point);
    move(point + QPoint(20, 0));
    QTest::keyClick(_window, Qt::Key_Escape);
    if (!require(_document.brushSize() == 37 && !scrubber->property("pressed").toBool(),
                 "Escape restores the starting value and releases the gesture"))
        return false;
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, point + QPoint(20, 0));
    field->setEnabled(false);
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, point);
    const bool disabledOk = _document.brushSize() == 37 && !scrubber->property("pressed").toBool();
    field->setEnabled(true);
    if (!require(disabledOk, "disabled numeric controls cannot be scrubbed"))
        return false;

    QQmlComponent component(qmlEngine(_window));
    component.setData(R"QML(
import QtQuick
import PhotoCrap
Item {
    width:360;height:280;z:10000
    property real number:.3
    property int starts:0
    property int commits:0
    LightEditor {
        anchors.fill:parent
        parameters:({exposure:parent.number})
        onEditStarted: parent.starts++
        onPreviewed: function(key,value) { if(key==="exposure")parent.number=value }
        onCommitted: function(key,value) { if(key==="exposure"){parent.number=value;parent.commits++} }
    }
}
)QML",
                      QUrl("qrc:/qt/qml/PhotoCrap/qml/ValueScrubberFixture.qml"));
    std::unique_ptr<QObject> object(component.create());
    auto *                   fixture = qobject_cast<QQuickItem *>(object.get());
    if (!require(fixture, qPrintable(component.errorString())))
        return false;
    fixture->setParentItem(_window->contentItem());
    fixture->setX(20);
    fixture->setY(120);
    QTest::qWait(40);
    QQuickItem *        exposure = nullptr;
    QList<QQuickItem *> pending{fixture};
    while (!pending.isEmpty() && !exposure) {
        auto *item = pending.takeLast();
        if (item->objectName() == "lightScrub_exposure")
            exposure = item;
        pending.append(item->childItems());
    }
    if (!require(exposure, "the decimal light control exposes its draggable title"))
        return false;
    const auto start =
        exposure->mapToScene(QPointF(exposure->width() / 2, exposure->height() / 2)).toPoint();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, start);
    move(start + QPoint(10, 0));
    move(start + QPoint(20, 0));
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, start + QPoint(20, 0));
    if (!require(qAbs(fixture->property("number").toDouble() - 1.3) < .0001 &&
                     fixture->property("starts").toInt() == 1 &&
                     fixture->property("commits").toInt() == 1,
                 "decimal scrubbing previews continuously and commits only once"))
        return false;
    return true;
}

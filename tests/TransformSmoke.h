#pragma once
#include "Document.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

inline bool runTransformSmoke(QQuickWindow *_window, Document &_document)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Transform UI check failed: %s", _message);
        return _ok;
    };
    auto *area     = _window->findChild<QQuickItem *>("drawingArea");
    auto *artboard = _window->findChild<QQuickItem *>("artboard");
    auto *frame    = _window->findChild<QQuickItem *>("transformOverlay");
    auto *field    = _window->findChild<QQuickItem *>("transformField_x");
    auto *menu     = _window->findChild<QObject *>("layerTransformMenu");
    if (!require(area && artboard && frame && field && menu, "menu, frame and options exist"))
        return false;

    _document.newDocument(160, 120, true);
    _document.setTool("brush");
    _document.setBrushSize(24);
    _document.setColor(Qt::red);
    _document.beginStroke(42, 49);
    _document.endStroke();
    const auto history = _document.historyIndex();
    _window->setProperty("zoom", 1.);
    area->forceActiveFocus();
    QTest::keyClick(_window, Qt::Key_T, Qt::ControlModifier);
    QTest::qWait(20);
    if (!require(_document.transformActive() && frame->isVisible() && field->isVisible(),
                 "the shortcut opens Free Transform"))
        return false;
    const QPoint start   = artboard->mapToScene(QPointF(42, 49)).toPoint();
    const qreal  beforeX = _document.transformOptions()["x"].toReal();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(_window, start + QPoint(10, 0), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, start + QPoint(10, 0));
    if (!require(_document.transformOptions()["x"].toReal() == beforeX + 10,
                 "dragging the frame moves its center"))
        return false;
    field->forceActiveFocus();
    field->setProperty("text", QStringLiteral("75"));
    QTest::keyClick(_window, Qt::Key_Return);
    if (!require(_document.transformActive() && _document.transformOptions()["x"].toReal() == 75 &&
                     _document.historyIndex() == history,
                 "first Return commits the numeric field without applying the transform"))
        return false;
    QTest::keyClick(_window, Qt::Key_Return);
    if (!require(!_document.transformActive() && _document.historyIndex() == history + 1 &&
                     _document.layerImage(0).pixelColor(75, 49).red() > 240 &&
                     _document.layerImage(0).pixelColor(42, 49).alpha() == 0,
                 "second Return applies one history step and moves the painted pixels"))
        return false;
    QTest::keyClick(_window, Qt::Key_T, Qt::ControlModifier);
    if (!require(_document.transformActive(), "another session can begin"))
        return false;
    QTest::keyClick(_window, Qt::Key_Escape);
    return require(!_document.transformActive() && _document.historyIndex() == history + 1,
                   "Escape cancels without recording an edit");
}

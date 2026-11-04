#pragma once
#include "Document.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

// Exercise the panel click itself before testing canvas gestures. Calling
// setActivePenPath directly would miss a stale brush/Pen/black-arrow tool.
inline bool runPenPanelEditingSmoke(QQuickWindow *_window, Document &_document)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Pen panel check failed: %s", _message);
        return _ok;
    };
    auto dock = _window->findChild<QObject *>("pathsDockPanel");
    auto area = _window->findChild<QQuickItem *>("drawingArea");
    auto list = _window->findChild<QQuickItem *>("pathList");
    if (!require(dock && area && list, "Paths dock and drawing area exist"))
        return false;
    const auto floating = dock->property("floating").toBool();
    QMetaObject::invokeMethod(_window, "setDockPanelVisible", Q_ARG(QVariant, "paths"),
                              Q_ARG(QVariant, true));
    QMetaObject::invokeMethod(dock, "setPanelFloating", Q_ARG(QVariant, true));
    for (bool shape : {false, true}) {
        for (const QString &previousTool :
             {QStringLiteral("brush"), QStringLiteral("pen"), QStringLiteral("pathSelection")}) {
            _document.newDocument(300, 200, true, 16);
            _document.setTool("pen");
            _document.setPenMode(shape ? "shape" : "path");
            _document.beginPenGesture(30, 40, 2);
            _document.endPenGesture();
            _document.beginPenGesture(130, 40, 2);
            _document.updatePenGesture(160, 70);
            _document.endPenGesture();
            _document.beginPenGesture(130, 140, 2);
            _document.endPenGesture();
            _document.closePenPath();
            const auto image = shape ? _document.layerImage(1) : QImage{};
            _document.setTool(previousTool);
            _window->setProperty("zoom", 1.);
            QTest::qWait(25);
            QQuickItem *row      = nullptr;
            const bool  rowReady = QTest::qWaitFor(
                [&] {
                    row = list->property("currentItem").value<QQuickItem *>();
                    return row && row->isVisible() && row->width() > 0;
                },
                2000);
            if (!require(rowReady, "path row is visible and can be clicked")) {
                qWarning() << "Path row state" << shape << previousTool << row
                           << list->property("count") << list->property("currentIndex")
                           << dock->property("collapsed") << dock->property("temporarilyHidden");
                return false;
            }
            const auto panelHistory = _document.historyIndex();
            QTest::mouseClick(
                row->window(), Qt::LeftButton, Qt::NoModifier,
                row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint());
            if (!require(_document.tool() == "directSelection" && _document.activePenPath() == 0 &&
                             _document.historyIndex() == panelHistory,
                         "clicking a path enters direct editing without a history step"))
                return false;
            _window->requestActivate();
            area->forceActiveFocus();
            QTest::qWait(25);
            const auto point = [&](qreal _x, qreal _y) {
                return area->mapToScene(QPointF(_x, _y)).toPoint();
            };
            QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, point(130, 40));
            auto nodes = _document.penPaths()[0].toMap()["points"].toList();
            if (!require(nodes.size() == 3 && nodes[1].toMap()["selected"].toBool(),
                         "the anchor is selected instead of removed or painted"))
                return false;
            QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, point(160, 70));
            QTest::mouseMove(_window, point(180, 90), 20);
            QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, point(180, 90));
            nodes           = _document.penPaths()[0].toMap()["points"].toList();
            const auto node = nodes[1].toMap();
            if (!require(
                    nodes.size() == 3 && qAbs(node["outX"].toReal() - 180) < 1 &&
                        qAbs(node["outY"].toReal() - 90) < 1 && node["x"].toReal() == 130 &&
                        node["y"].toReal() == 40 && _document.historyIndex() == panelHistory + 1,
                    "the chosen handle moves without moving the anchor or creating an extra point"))
                return false;
            if (!require(!shape || _document.layerImage(1) != image,
                         "shape pixels follow the edited handle"))
                return false;
            _document.undo();
            if (!require(_document.penPaths()[0]
                                     .toMap()["points"]
                                     .toList()[1]
                                     .toMap()["outX"]
                                     .toReal() == 160 &&
                             (!shape || _document.layerImage(1) == image),
                         "Undo restores the handle and shape pixels"))
                return false;
            _document.redo();
            const auto restoredNodes = _document.penPaths()[0].toMap()["points"].toList();
            if (!require(restoredNodes == nodes,
                         "Redo restores the exact edited anchors and handles")) {
                qWarning() << "Panel redo state" << shape << previousTool << restoredNodes << nodes;
                return false;
            }
        }
    }
    // Modified clicks still load a selection without changing the painting tool.
    _document.setTool("brush");
    QTest::qWait(25);
    QQuickItem *row      = nullptr;
    const bool  rowReady = QTest::qWaitFor(
        [&] {
            row = list->property("currentItem").value<QQuickItem *>();
            return row && row->isVisible();
        },
        2000);
    if (!require(rowReady, "the shape row remains present after editing"))
        return false;
    QTest::mouseClick(row->window(), Qt::LeftButton, Qt::ControlModifier,
                      row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint());
    if (!require(_document.tool() == "brush" && _document.hasSelection() &&
                     _document.activePenPath() == -1,
                 "Command/Ctrl-click loads the selection and preserves the brush"))
        return false;
    QMetaObject::invokeMethod(dock, "setPanelFloating", Q_ARG(QVariant, floating));
    _document.setPenMode("path");
    _window->requestActivate();
    area->forceActiveFocus();
    return true;
}

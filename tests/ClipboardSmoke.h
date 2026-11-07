#pragma once
#include "Document.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

inline bool runClipboardSmoke(QQuickWindow *window, Document &document)
{
    const auto require = [](bool ok, const char *message) {
        if (!ok)
            qWarning("Clipboard UI check failed: %s", message);
        return ok;
    };
    auto *area = window->findChild<QQuickItem *>("drawingArea");
    document.setBackgroundEditThreshold(-1);
    if (!document.newDocument(160, 120, true, 16) || !area)
        return false;
    document.setColor(Qt::red);
    document.fillWithColor(true);
    document.setTool("ellipticalSelect");
    document.setSelectionAntialias(false);
    window->setProperty("zoom", 1.);
    QTest::qWait(80);
    const QPoint a = area->mapToScene(QPointF(20, 20)).toPoint();
    const QPoint b = area->mapToScene(QPointF(70, 70)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, a);
    QTest::mouseMove(window, b, 30);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, b);
    if (!require(document.hasSelection(), "a real mouse gesture selects an ellipse"))
        return false;
    const QRect  bounds = document.selectionRegion().boundingRect();
    const QImage before = document.layerImage(0);
    QTest::keyClick(window, Qt::Key_X, Qt::ControlModifier);
    if (!require(!document.hasSelection() &&
                     document.layerImage(0).pixelColor(bounds.center()).alpha() == 0,
                 "Command/Ctrl-X cuts the selected pixels"))
        return false;
    QTest::keyClick(window, Qt::Key_V, Qt::ControlModifier);
    if (!require(document.layers().size() == 2 && document.activeLayer() == 1,
                 "Command/Ctrl-V pastes into a selected new layer"))
        return false;
    const QImage pasted = document.layerImage(1);
    if (!require(pasted.pixelColor(0, 0).alpha() == 0 && pasted.pixelColor(80, 60).alpha() == 255 &&
                     document.layerImage(0).pixelColor(bounds.topLeft()).alpha() == 255,
                 "ellipse corners remain transparent in the paste and untouched in the source"))
        return false;
    QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
    QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
    if (!require(document.layers().size() == 1 && document.layerImage(0) == before &&
                     document.hasSelection(),
                 "two keyboard undos restore the original pixels and selection"))
        return false;
    document.setBackgroundEditThreshold(0);
    QTest::keyClick(window, Qt::Key_X, Qt::ControlModifier);
    QTest::keyClick(window, Qt::Key_V, Qt::ControlModifier);
    if (!require(
            QTest::qWaitFor(
                [&] { return document.layers().size() == 2 && !document.operationBusy(); }, 10000),
            "immediate Command/Ctrl-V waits for a pending cut and pastes once"))
        return false;
    document.setBackgroundEditThreshold(4LL * 1024 * 1024);
    document.setSelectionAntialias(true);
    return true;
}

#pragma once
#include "Document.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

inline bool runSelectionUiSmoke(QQuickWindow *_window, Document &_document)
{
    auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Selection UI check failed: %s", _message);
        return _ok;
    };
    auto area     = _window->findChild<QQuickItem *>("drawingArea");
    auto menu     = _window->findChild<QObject *>("selectionMenu");
    auto reselect = _window->findChild<QObject *>("reselectAction");
    auto inverse  = _window->findChild<QObject *>("inverseSelectionAction");
    auto expand   = _window->findChild<QObject *>("expandSelectionAction");
    auto edges    = _window->findChild<QObject *>("selectionEdgesAction");
    auto dialog   = _window->findChild<QObject *>("selectionModifyDialog");
    auto amount   = _window->findChild<QObject *>("selectionModifyAmount");
    if (!require(area && menu && reselect && inverse && expand && edges && dialog && amount,
                 "selection controls exist"))
        return false;
    _document.newDocument(240, 180, false);
    _document.setTool("select");
    _window->setProperty("zoom", 1.);
    area->forceActiveFocus();
    if (!require(!reselect->property("enabled").toBool() && !expand->property("enabled").toBool(),
                 "commands reflect the absence of a selection"))
        return false;
    _document.beginStroke(40, 40);
    _document.continueStroke(100, 100);
    _document.endStroke();
    const auto original = _document.selectionRegion();
    QTest::keyClick(_window, Qt::Key_D, Qt::ControlModifier);
    if (!require(!_document.hasSelection() && reselect->property("enabled").toBool(),
                 "Deselect enables Reselect"))
        return false;
    QTest::keyClick(_window, Qt::Key_D, Qt::ControlModifier | Qt::ShiftModifier);
    if (!require(_document.selectionRegion() == original && expand->property("enabled").toBool(),
                 "Reselect shortcut remains active while the menu is closed"))
        return false;
    QTest::keyClick(_window, Qt::Key_I, Qt::ControlModifier | Qt::ShiftModifier);
    if (!require(_document.selectionRegion() == QRegion(QRect(0, 0, 240, 180)).subtracted(original),
                 "Inverse shortcut uses the menu action"))
        return false;
    _document.undo();
    const int history = _document.historyIndex();
    if (!require(QMetaObject::invokeMethod(expand, "trigger") &&
                     dialog->property("visible").toBool(),
                 "Modify opens a parameter dialog"))
        return false;
    amount->setProperty("value", 5);
    QMetaObject::invokeMethod(dialog, "reject");
    if (!require(_document.selectionRegion() == original && _document.historyIndex() == history,
                 "Cancel leaves the selection unchanged"))
        return false;
    QMetaObject::invokeMethod(expand, "trigger");
    amount->setProperty("value", 5);
    QMetaObject::invokeMethod(dialog, "accept");
    if (!require(QTest::qWaitFor([&] { return !_document.selectionBusy(); }, 5000),
                 "asynchronous modification completes"))
        return false;
    if (!require(_document.selectionContains(35, 65) && _document.historyIndex() == history + 1,
                 "Expand changes only the selection and creates one undo"))
        return false;
    _document.undo();
    _document.invertSelection(); // General regions are drawn by ImageCanvas, not a QML rectangle.
    QTest::qWait(40);
    const auto visible = _window->grabWindow();
    QMetaObject::invokeMethod(edges, "trigger");
    QTest::qWait(40);
    const auto  hidden = _window->grabWindow();
    const auto  rect   = area->mapRectToScene(QRectF(0, 0, 240, 180));
    const qreal sx     = qreal(visible.width()) / _window->width(),
                sy     = qreal(visible.height()) / _window->height();
    const QRect crop(qRound(rect.x() * sx), qRound(rect.y() * sy), qRound(rect.width() * sx),
                     qRound(rect.height() * sy));
    if (!require(!_document.selectionEdgesVisible() && _document.hasSelection() &&
                     visible.copy(crop) != hidden.copy(crop),
                 "hiding selection edges changes the rendered canvas without deselecting"))
        return false;
    _document.selectAll();
    if (!require(_document.selectionEdgesVisible(), "a new selection shows its edges again"))
        return false;
    _document.modifySelection("smooth", 100);
    area->forceActiveFocus();
    QTest::keyClick(_window, Qt::Key_Escape);
    if (!require(!_document.selectionBusy(), "Escape cancels the background operation"))
        return false;
    // Feather uses a real coverage mask, and its shortcut works outside the menu.
    QTest::keyClick(_window, Qt::Key_F6, Qt::ShiftModifier);
    if (!require(dialog->property("visible").toBool() &&
                     dialog->property("operation").toString() == "feather",
                 "Feather shortcut opens the radius dialog"))
        return false;
    amount->setProperty("value", 5);
    QMetaObject::invokeMethod(dialog, "accept");
    if (!require(QTest::qWaitFor([&] { return !_document.selectionBusy(); }, 5000),
                 "Feather finishes"))
        return false;
    _document.setTool("brush");
    _document.setColor(Qt::blue);
    _document.setBrushSize(80);
    _document.setBrushHardness(.5);
    _document.beginStroke(120, 90);
    _document.endStroke();
    const auto soft        = _document.selectionMask();
    const int  beforeColor = _document.historyIndex();
    auto       colorAction = _window->findChild<QObject *>("colorSelectionAction");
    auto       colorDialog = _window->findChild<QObject *>("colorSelectionDialog");
    if (!require(colorAction && colorDialog && QMetaObject::invokeMethod(colorAction, "trigger"),
                 "Color Range opens from the menu"))
        return false;
    if (!require(QTest::qWaitFor([&] { return _document.colorSelectionReady(); }, 5000),
                 "Color Range produces a preview"))
        return false;
    const auto findVisual = [&](auto &&_self, QQuickItem *_item,
                                const QString &_name) -> QQuickItem * {
        if (_item->objectName() == _name)
            return _item;
        for (auto *child : _item->childItems())
            if (auto *found = _self(_self, child, _name))
                return found;
        return nullptr;
    };
    QQuickItem *sampler = nullptr;
    if (!require(QTest::qWaitFor(
                     [&] {
                         sampler = findVisual(findVisual, _window->contentItem(),
                                              "colorSelectionSampler");
                         return sampler && sampler->width() > 0 &&
                                colorDialog->property("opened").toBool();
                     },
                     3000),
                 "Color Range image can be sampled")) {
        _window->grabWindow().save("/tmp/photocrap-color-selection-failure.png");
        return false;
    }
    const auto position =
        sampler->mapToScene(QPointF(sampler->width() / 2, sampler->height() / 2)).toPoint();
    QTest::mouseClick(_window, Qt::LeftButton, Qt::ShiftModifier, position);
    if (!require(_document.colorSelectionSampleCount() == 2 &&
                     QTest::qWaitFor([&] { return _document.colorSelectionReady(); }, 5000),
                 "Shift-click adds a color sample"))
        return false;
    auto imagePreview =
        findVisual(findVisual, _window->contentItem(), "colorSelectionImagePreview");
    auto maskPreview  = findVisual(findVisual, _window->contentItem(), "colorSelectionMaskPreview");
    auto zoomIn       = _window->findChild<QQuickItem *>("colorSelectionZoomIn");
    auto contiguous   = _window->findChild<QQuickItem *>("colorSelectionContiguous");
    auto canvasPicker = _window->findChild<QQuickItem *>("colorSelectionCanvasPicker");
    if (!require(imagePreview && maskPreview && zoomIn && contiguous && canvasPicker,
                 "zoom, connectivity and canvas picking controls exist"))
        return false;
    const double initialZoom = imagePreview->property("scaleFactor").toDouble();
    QTest::mouseClick(
        _window, Qt::LeftButton, Qt::NoModifier,
        zoomIn->mapToScene(QPointF(zoomIn->width() / 2, zoomIn->height() / 2)).toPoint());
    if (!require(imagePreview->property("scaleFactor").toDouble() > initialZoom &&
                     imagePreview->property("scaleFactor") == maskPreview->property("scaleFactor"),
                 "both previews zoom together"))
        return false;
    const double beforeWheel = imagePreview->property("scaleFactor").toDouble();
    QTest::wheelEvent(_window, position, QPoint(0, 120));
    if (!require(imagePreview->property("scaleFactor").toDouble() > beforeWheel &&
                     _document.colorSelectionSampleCount() == 2,
                 "the wheel zooms the preview without adding a sample"))
        return false;
    colorDialog->setProperty("previewZoom", 4.);
    QTest::qWait(30);
    const double initialCenter = colorDialog->property("previewCenterX").toDouble();
    QTest::mousePress(_window, Qt::MiddleButton, Qt::NoModifier, position);
    QTest::mouseMove(_window, position + QPoint(28, 0));
    QTest::mouseRelease(_window, Qt::MiddleButton, Qt::NoModifier, position + QPoint(28, 0));
    if (!require(colorDialog->property("previewCenterX").toDouble() < initialCenter &&
                     _document.colorSelectionSampleCount() == 2,
                 "panning the zoomed preview does not pick a color"))
        return false;
    QTest::mouseClick(
        _window, Qt::LeftButton, Qt::NoModifier,
        contiguous->mapToScene(QPointF(contiguous->width() / 2, contiguous->height() / 2))
            .toPoint());
    if (!require(
            contiguous->property("checked").toBool() &&
                QTest::qWaitFor([&] { return colorDialog->property("canApply").toBool(); }, 5000),
            "contiguous mode refreshes the selection"))
        return false;
    // Put the dialog to the right to expose the original canvas, without closing it.
    colorDialog->setProperty("x", _window->width() - colorDialog->property("width").toDouble() - 8);
    QTest::qWait(30);
    const auto nativePoint  = area->mapToScene(QPointF(120, 90)).toPoint();
    const auto nativePixels = _document.composite();
    QTest::mouseClick(_window, Qt::LeftButton, Qt::ShiftModifier, nativePoint);
    if (!require(colorDialog->property("visible").toBool() &&
                     _document.colorSelectionSampleCount() == 3 &&
                     QTest::qWaitFor([&] { return _document.colorSelectionReady(); }, 5000),
                 "clicking the main canvas samples without closing the dialog"))
        return false;
    QTest::mouseClick(_window, Qt::LeftButton, Qt::AltModifier, nativePoint);
    if (!require(colorDialog->property("visible").toBool() &&
                     _document.colorSelectionSampleCount() == 4 &&
                     QTest::qWaitFor([&] { return _document.colorSelectionReady(); }, 5000),
                 "Option/Alt subtracts a sample from the main canvas"))
        return false;
    QTest::wheelEvent(_window, nativePoint, QPoint(0, 120), QPoint(), Qt::ControlModifier);
    if (!require(_window->property("zoom").toDouble() > 1. &&
                     colorDialog->property("visible").toBool(),
                 "Ctrl/Command-wheel zooms the main canvas without closing Color Range"))
        return false;
    _window->setProperty("zoom", 2.);
    QTest::keyClick(_window, Qt::Key_1, Qt::ControlModifier);
    if (!require(qAbs(_window->property("zoom").toDouble() - 1.) < .001,
                 "canvas zoom shortcuts remain available during Color Range"))
        return false;
    if (!require(_document.historyIndex() == beforeColor &&
                     _document.composite().cacheKey() == nativePixels.cacheKey(),
                 "sampling and navigation leave document pixels and history untouched"))
        return false;
    _window->grabWindow().save("/tmp/photocrap-color-selection.png");
    QTest::keyClick(_window, Qt::Key_Escape);
    if (!require(!_document.colorSelectionActive() && _document.historyIndex() == beforeColor &&
                     _document.selectionMask() == soft,
                 "canceling Color Range keeps the existing selection"))
        return false;
    QMetaObject::invokeMethod(colorAction, "trigger");
    if (!require(QTest::qWaitFor([&] { return _document.colorSelectionReady(); }, 5000),
                 "Color Range can reopen"))
        return false;
    QMetaObject::invokeMethod(colorDialog, "accept");
    if (!require(!_document.colorSelectionActive() && _document.historyIndex() == beforeColor + 1,
                 "Color Range commits one selection"))
        return false;
    _document.setTool("brush");
    return true;
}

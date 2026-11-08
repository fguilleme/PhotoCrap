#pragma once
#include "ClipboardSmoke.h"
#include "Document.h"
#include "ValueScrubberSmoke.h"
#include "WorkspaceSettings.h"
#include "text/TextEditorController.h"
#include "text/TextLayer.h"
#ifdef Q_OS_MACOS
#include "MacToolCursorTest.h"
#endif
#include <QCursor>
#include <QGuiApplication>
#include <QJSValue>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

inline bool runToolLayerSmoke(QQuickWindow *_window, Document &_document,
                              WorkspaceSettings &_workspaceSettings)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Tool layer check failed: %s", _message);
        return _ok;
    };
    auto *area = _window->findChild<QQuickItem *>("drawingArea");
    if (!require(area, "the drawing area exists"))
        return false;
    if (!runClipboardSmoke(_window, _document) || !runValueScrubberSmoke(_window, _document))
        return false;
    // Creating content on an adjustment must select a new layer through real tool input.
    _document.newDocument(160, 120, true);
    _document.addAdjustmentLayer("brightnessContrast");
    _document.setTool("text");
    _document.setText("Layer");
    _document.setTextSize(18);
    _window->setProperty("zoom", 1.0);
    QTest::qWait(80);
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier,
                      area->mapToScene(QPointF(20, 20)).toPoint());
    if (!require(_document.layers().size() == 3 && _document.activeLayer() == 2 &&
                     _document.textEditing(),
                 "the text tool creates and selects an editable empty layer above an adjustment"))
        return false;
    QTest::qWait(50);
    for (const auto c : QStringLiteral("Layer"))
        QTest::keyClick(_window, Qt::Key(c.toUpper().unicode()),
                        c.isUpper() ? Qt::ShiftModifier : Qt::NoModifier);
    if (!require(_document.text() == "layer", "keyboard input edits the text on the image"))
        return false;
    auto *textInput = _window->findChild<QQuickItem *>("canvasTextEditor");
    if (!require(textInput, "the canvas text input exists"))
        return false;
    for (qreal zoom : {1., .5}) {
        _window->setProperty("zoom", zoom);
        QTest::qWait(40);
        QRectF caret;
        QMetaObject::invokeMethod(textInput, "positionToRectangle", Q_RETURN_ARG(QRectF, caret),
                                  Q_ARG(int, 2));
        QTest::mouseMove(_window, textInput->mapToScene(caret.center()).toPoint(), 20);
        QTest::qWait(80);
        if (!require(QGuiApplication::overrideCursor() &&
                         QGuiApplication::overrideCursor()->shape() == Qt::IBeamCursor,
                     "hovering editable text keeps the system text cursor at each zoom"))
            return false;
#ifdef Q_OS_MACOS
        if (QGuiApplication::platformName() == "cocoa" &&
            !require(macTextCursorIsCurrent(),
                     "the visible Cocoa cursor is the native text I-beam"))
            return false;
#endif
        QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier,
                          textInput->mapToScene(caret.center()).toPoint());
        if (!require(_document.textEditing() && _document.layers().size() == 3 &&
                         textInput->property("cursorPosition").toInt() == 2 &&
                         textInput->property("selectionStart").toInt() ==
                             textInput->property("selectionEnd").toInt(),
                     "clicking inside text positions the caret at each zoom without reopening it"))
            return false;
    }
    _window->setProperty("zoom", 1.0);
    QTest::qWait(40);
    const auto beforeMove = PhotoCrap::Text::transform(_document.textEditState());
    QRectF     textCaret;
    QMetaObject::invokeMethod(textInput, "positionToRectangle", Q_RETURN_ARG(QRectF, textCaret),
                              Q_ARG(int, 2));
    const auto textPoint = textInput->mapToScene(textCaret.center()).toPoint();
    QTest::mousePress(_window, Qt::LeftButton, Qt::ControlModifier, textPoint);
    if (!require(_window->property("canvasCursorShape").toInt() == Qt::SizeAllCursor,
                 "the cursor indicates text movement during Command/Ctrl-drag"))
        return false;
    QTest::mouseMove(_window, textPoint + QPoint(20, 10), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::ControlModifier, textPoint + QPoint(20, 10));
    const auto afterMove = PhotoCrap::Text::transform(_document.textEditState());
    if (!require(_document.textEditing() && afterMove.dx() == beforeMove.dx() + 20 &&
                     afterMove.dy() == beforeMove.dy() + 10 && _document.text() == "layer" &&
                     _document.layers().size() == 3,
                 "Command/Ctrl-drag moves the text while keeping its edit session and content"))
        return false;
    _document.finishTextEdit();
    const auto          originalTextImage = _document.layerImage(2);
    auto               *moveButton        = _window->findChild<QQuickItem *>("toolGroup_move");
    QList<QQuickItem *> toolItems{_window->contentItem()};
    while (!moveButton && !toolItems.isEmpty()) {
        auto *item = toolItems.takeLast();
        toolItems.append(item->childItems());
        if (item->objectName() == "toolGroup_move")
            moveButton = item;
    }
    if (!require(moveButton, "the Move tool button exists"))
        return false;
    QTest::mouseClick(
        _window, Qt::LeftButton, Qt::NoModifier,
        moveButton->mapToScene(QPointF(moveButton->width() / 2, moveButton->height() / 2))
            .toPoint());
    QTest::qWait(30);
    auto *textFrame = _window->findChild<QQuickItem *>("textLayerOverlay");
    if (!require(_document.tool() == "move" && textFrame && textFrame->isVisible(),
                 "the selected text has a visible frame with the Move tool"))
        return false;
    const auto movedPoint = textPoint + QPoint(20, 10);
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, movedPoint);
    QTest::mouseMove(_window, movedPoint + QPoint(10, 10), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, movedPoint + QPoint(10, 10));
    QTest::keyClick(_window, Qt::Key_Right);
    QTest::keyClick(_window, Qt::Key_Down, Qt::ShiftModifier);
    const auto toolMove = PhotoCrap::Text::transform(_document.textEditState());
    if (!require(!_document.textEditing() && toolMove.dx() == afterMove.dx() + 11 &&
                     toolMove.dy() == afterMove.dy() + 20 && _document.layers().size() == 3,
                 "the Move tool drags text and nudges it by one or ten pixels"))
        return false;
    _document.undo();
    _document.undo();
    _document.undo();
    if (!require(_document.layerImage(2) == originalTextImage,
                 "undo restores the text after drag and nudges"))
        return false;
    _document.setTool("text");
    _document.setActiveLayer(0);
    _window->setProperty("zoom", .5);
    QTest::qWait(40);
    const auto resumePoint = area->mapToScene(afterMove.map(QPointF(10, 10)) * .5).toPoint();
    QTest::mouseMove(_window, resumePoint, 20);
    QTest::qWait(20);
    if (!require(QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::IBeamCursor,
                 "the text tool uses an I-beam before clicking an existing text layer"))
        return false;
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, resumePoint);
    QTest::qWait(60);
    if (!require(_document.textEditing() && !_document.textGesture() &&
                     _document.activeLayer() == 2 && _document.layers().size() == 3 &&
                     _document.text() == "layer" &&
                     textInput->property("selectionStart").toInt() ==
                         textInput->property("selectionEnd").toInt() &&
                     textInput->property("activeFocus").toBool(),
                 "one canvas click reopens the text and keeps a focused caret after mouse release"))
        return false;
    _document.finishTextEdit(true);
    _window->setProperty("zoom", 1.0);
    _document.beginStroke(100, 80);
    _document.endStroke();
    _document.setText("other");
    _document.finishTextEdit(true);
    QTest::qWait(40);
    QMetaObject::invokeMethod(_window, "selectDockPanel", Q_ARG(QVariant, "layers"));
    QMetaObject::invokeMethod(_window, "moveDockPanelTo", Q_ARG(QVariant, "layers"),
                              Q_ARG(QVariant, "color"), Q_ARG(QVariant, false), Q_ARG(QVariant, 0));
    auto *layerList = _window->findChild<QQuickItem *>("layerListView");
    if (layerList)
        QMetaObject::invokeMethod(layerList, "positionViewAtBeginning");
    QTest::qWait(100);
    QQuickItem         *textThumbnail = nullptr;
    QList<QQuickItem *> pending{layerList};
    while (!pending.isEmpty()) {
        auto *row = pending.takeLast();
        if (!row)
            continue;
        pending.append(row->childItems());
        if (row->objectName() != "layerDelegate")
            continue;
        auto model = row->property("modelData");
        if (model.metaType() == QMetaType::fromType<QJSValue>())
            model = model.value<QJSValue>().toVariant();
        if (row->isVisible() && model.toMap().value("layerIndex", -1).toInt() == 2) {
            textThumbnail = row->findChild<QQuickItem *>("layerThumbnailMouse");
            break;
        }
    }
    if (!require(textThumbnail && textThumbnail->isVisible(), "the text thumbnail is visible"))
        return false;
    QTest::mouseDClick(
        _window, Qt::LeftButton, Qt::NoModifier,
        textThumbnail->mapToScene(QPointF(textThumbnail->width() / 2, textThumbnail->height() / 2))
            .toPoint());
    QTest::qWait(60);
    if (!require(_document.textEditing() && _document.layers().size() == 3,
                 "double-clicking the text thumbnail reopens the existing text without renaming or "
                 "creating"))
        return false;
    QTest::qWait(50);
    if (!require(_document.text() == "layer",
                 "reopening a layer does not replace it with stale editor content"))
        return false;
    auto *formatting = _window->findChild<TextEditorController *>("textFormatter");
    if (!require(formatting, "the character formatter exists"))
        return false;
    auto *typography = _window->findChild<QObject *>("textTypographyPanel");
    if (!require(typography, "the typography panel exists"))
        return false;
    QMetaObject::invokeMethod(_window, "openActiveLayerProperties");
    QTest::qWait(80);
    if (!require(typography->property("visible").toBool(), "character and paragraph controls open"))
        return false;
    auto *fontFamily = _window->findChild<QObject *>("textFontFamily");
    auto *fontStyle  = _window->findChild<QObject *>("textFontStyle");
    if (!require(fontFamily && fontStyle &&
                     fontFamily->property("editText").toString() ==
                         formatting->format(1).value("family").toString() &&
                     fontStyle->property("count").toInt() > 0,
                 "the font selector shows the actual text family and an available style"))
        return false;
    if (QCoreApplication::arguments().contains("--capture-text"))
        _window->grabWindow().save("/tmp/photocrap-text-editor.png");
    auto *fontSizeLabel = _window->findChild<QQuickItem *>("textFontSizeLabel");
    if (!require(fontSizeLabel && fontSizeLabel->isVisible(), "the font size title is draggable"))
        return false;
    const auto sizePoint =
        fontSizeLabel->mapToScene(QPointF(fontSizeLabel->width() / 2, fontSizeLabel->height() / 2))
            .toPoint();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, sizePoint);
    QTest::mouseMove(_window, sizePoint + QPoint(20, 0), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, sizePoint + QPoint(20, 0));
    if (!require(textInput->hasActiveFocus(),
                 "finishing a typography scrub restores canvas typing focus"))
        return false;
    auto *     fontSizeField = _window->findChild<QObject *>("textFontSize");
    const auto originalSize  = fontSizeField->property("value");
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, sizePoint);
    QTest::mouseMove(_window, sizePoint + QPoint(20, 0), 20);
    QTest::keyClick(_window, Qt::Key_Escape);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, sizePoint + QPoint(20, 0));
    if (!require(typography->property("visible").toBool() &&
                     fontSizeField->property("value") == originalSize,
                 "Escape cancels a numeric drag without closing its popup"))
        return false;
    QMetaObject::invokeMethod(typography, "close");
    QTest::qWait(40);
    formatting->apply({{"size", 32.}, {"bold", true}, {"baseline", 20.}}, 0, 3);
    QTest::qWait(50);
    if (!require(formatting->format(1).value("size").toInt() == 32,
                 "selected text retains its explicit pixel font size"))
        return false;
    if (!require(_document.layerImage(2) != originalTextImage && _document.layers().size() == 3,
                 "formatting selected characters updates the canvas cache without adding a layer"))
        return false;
    _document.finishTextEdit(true);
    if (!require(_document.layerImage(2) == originalTextImage,
                 "Cancel restores text and formatting together"))
        return false;
    _document.undo();
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "shape"),
                              Q_ARG(QVariant, "rectangle"));
    QTest::qWait(60);
    auto shapeMode = _window->findChild<QObject *>("shapeLayerMode");
    if (!require(shapeMode && shapeMode->property("currentIndex").toInt() == 0,
                 "shapes default to creating a new layer"))
        return false;
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier,
                      area->mapToScene(QPointF(10, 10)).toPoint());
    QTest::mouseMove(_window, area->mapToScene(QPointF(80, 80)).toPoint(), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier,
                        area->mapToScene(QPointF(80, 80)).toPoint());
    if (!require(_document.layers().size() == 3 && _document.activeLayer() == 2,
                 "drawing a shape creates one new layer through the real toolbar tool"))
        return false;
    shapeMode->setProperty("currentIndex", 1);
    QMetaObject::invokeMethod(shapeMode, "activated", Q_ARG(int, 1));
    if (!require(!_document.shapeNewLayer(), "the shape mode selector also exposes pixel editing"))
        return false;
    _document.setShapeNewLayer(true);
    auto autoLayerCheck = _window->findChild<QObject *>("autoCreatePaintLayerCheck");
    if (!require(autoLayerCheck && _document.autoCreatePaintLayer(),
                 "automatic paint layers are exposed in preferences and enabled by default"))
        return false;
    autoLayerCheck->setProperty("checked", false);
    QMetaObject::invokeMethod(autoLayerCheck, "toggled");
    if (!require(!_workspaceSettings.autoCreatePaintLayer() && !_document.autoCreatePaintLayer(),
                 "the automatic paint layer preference is wired to the document"))
        return false;
    autoLayerCheck->setProperty("checked", true);
    QMetaObject::invokeMethod(autoLayerCheck, "toggled");

    _document.newDocument(160, 120, true);
    _document.setTool("fill");
    QTest::qWait(40);
    auto *fill       = _window->findChild<QQuickItem *>("fillOptions");
    auto *contiguous = _window->findChild<QObject *>("fillContiguous");
    auto *tolerance  = _window->findChild<QObject *>("fillTolerance");
    if (!require(fill && fill->isVisible() && contiguous && tolerance &&
                     tolerance->property("value").toInt() == 32,
                 "the paint bucket exposes its tolerance and connectivity"))
        return false;
    contiguous->setProperty("checked", false);
    QMetaObject::invokeMethod(contiguous, "toggled");
    if (!require(!_document.property("fillContiguous").toBool(),
                 "the connectivity checkbox changes the fill behavior"))
        return false;
    contiguous->setProperty("checked", true);
    QMetaObject::invokeMethod(contiguous, "toggled");

    _document.newDocument(160, 120, false, 16);
    _document.addLayerMask();
    _document.setTool("gradient");
    _document.setProperty("gradientPreset", 2);
    _document.setBrushOpacity(1.);
    _window->setProperty("zoom", 1.0);
    QTest::qWait(60);
    const auto maskBefore = _document.maskImage(0), maskPixels = _document.layerImage(0);
    auto *     maskMode = _window->findChild<QQuickItem *>("gradientApplyMode");
    if (!require(maskMode && !maskMode->isEnabled(),
                 "a selected mask overrides gradient layer creation"))
        return false;
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier,
                      area->mapToScene(QPointF(20, 30)).toPoint());
    QTest::mouseMove(_window, area->mapToScene(QPointF(100, 30)).toPoint(), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier,
                        area->mapToScene(QPointF(100, 30)).toPoint());
    if (!require(_document.layers().size() == 1 && _document.editingMask() &&
                     _document.maskImage(0) != maskBefore && _document.layerImage(0) == maskPixels,
                 "a real gradient drag paints only the selected mask"))
        return false;
    _document.undo();
    _document.setColor(Qt::black);
    _document.setTool("text");
    QTest::qWait(40);
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier,
                      area->mapToScene(QPointF(20, 20)).toPoint());
    QTest::qWait(80);
    for (const auto c : QStringLiteral("mask"))
        QTest::keyClick(_window, Qt::Key(c.toUpper().unicode()));
    if (!require(_document.textEditing() && _document.text() == "mask" &&
                     _document.layers().size() == 1 && _document.maskImage(0) != maskBefore,
                 "canvas typing paints text into the mask without a text layer"))
        return false;
    _document.finishTextEdit(true);
    if (!require(_document.maskImage(0) == maskBefore && _document.layerImage(0) == maskPixels,
                 "cancelling text restores the original mask"))
        return false;

    _document.newDocument(160, 120, true);
    _document.setTool("gradient");
    QTest::qWait(60);
    auto *gradient = _window->findChild<QQuickItem *>("gradientOptions");
    auto *guide = _window->findChild<QQuickItem *>("gradientGuide");
    if (!require(gradient && gradient->isVisible() && guide && !guide->isVisible() &&
                     gradient->findChild<QObject *>("gradientPreset") &&
                     gradient->findChild<QObject *>("gradientMethod"),
                 "the gradient tool exposes its presets and interpolation method"))
        return false;
    const auto start = area->mapToScene(QPointF(20, 30)).toPoint();
    const auto end = area->mapToScene(QPointF(100, 30)).toPoint();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(_window, end, 20);
    if (!require(guide->isVisible() && _document.gradientDragging(),
                 "a real gradient drag shows the direction guide"))
        return false;
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, end);
    if (!require(guide->isVisible() && !_document.gradientDragging() &&
                     _document.dynamicGradientActive() && _document.layers().size() == 2,
                 "releasing the dynamic gradient retains the editable handles"))
        return false;
    auto *endHandle = guide->findChild<QQuickItem *>("gradientEndHandle");
    if (!require(endHandle && endHandle->isVisible(), "the gradient has an end handle"))
        return false;
    // The handle becomes enabled beneath the release position. Qt 6.8 may
    // suppress a synthetic move to that same position; leave and re-enter it
    // as a real hover gesture instead of relying on a frame's delivery timing.
    QTest::mouseMove(_window, area->mapToScene(QPointF(8, 105)).toPoint(), 20);
    const auto handlePoint = endHandle->mapToScene(QPointF(endHandle->width() / 2,
                                                         endHandle->height() / 2)).toPoint();
    QTest::mouseMove(_window, handlePoint, 20);
    if (!require(QTest::qWaitFor(
                     [] {
                         return QGuiApplication::overrideCursor() &&
                                QGuiApplication::overrideCursor()->shape() == Qt::SizeAllCursor;
                     },
                     3000),
                 "hovering a gradient handle changes the shared system cursor"))
        return false;
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, handlePoint);
    QTest::mouseMove(_window, handlePoint + QPoint(24, 14), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, handlePoint + QPoint(24, 14));
    if (!require(_document.gradientEnd().x() >= 120 && _document.gradientEnd().y() >= 40 &&
                     _document.layers().size() == 2,
                 "dragging a retained handle changes the same layer"))
        return false;

    _document.addGradientStop(.5);
    QTest::qWait(40);
    QQuickItem *colorStop = nullptr;
    QList<QQuickItem *> gradientItems{guide};
    while (!colorStop && !gradientItems.isEmpty()) {
        auto *item = gradientItems.takeLast();
        gradientItems.append(item->childItems());
        if (item->objectName() == "gradientColorStop_1")
            colorStop = item;
    }
    if (!require(_document.dynamicGradientStops().size() == 3 && colorStop,
                 "the intermediate gradient color control exists"))
        return false;
    const auto colorPoint = colorStop->mapToScene(QPointF(colorStop->width() / 2,
                                                         colorStop->height() / 2)).toPoint();
    QTest::mouseMove(_window, colorPoint, 20);
    QTest::qWait(80);
    if (!require(QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::PointingHandCursor,
                 "hovering a gradient color uses the pointing hand"))
        return false;
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, colorPoint);
    QTest::mouseMove(_window, colorPoint + QPoint(4, 2), 20);
    if (!require(QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::ClosedHandCursor,
                 "dragging a gradient color keeps the grabbing cursor"))
        return false;
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, colorPoint + QPoint(4, 2));
    QTest::mouseMove(_window, area->mapToScene(QPointF(8, 105)).toPoint(), 20);
    QTest::qWait(80);
    if (!require(QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::CrossCursor &&
                     !guide->property("pointerActive").toBool(),
                 "leaving the gradient controls restores the drawing cursor"))
        return false;

    _document.newDocument(160, 120, true);
    _document.addAdjustmentLayer("brightnessContrast");
    _document.setTool("rectangle");
    auto *properties = _window->findChild<QObject *>("layerPropertiesDialog");
    if (!require(properties, "the layer properties panel exists"))
        return false;
    QMetaObject::invokeMethod(properties, "open");
    QTest::qWait(80);
    _document.previewLayerParameter("brightness", 20.);
    _document.beginStroke(10, 10);
    _document.continueStroke(80, 80);
    _document.endStroke();
    QTest::qWait(160);
    if (!require(!properties->property("visible").toBool() && _document.layers().size() == 3,
                 "creating content closes the previous properties session without losing it"))
        return false;
    _document.undo();
    if (!require(_document.layers().size() == 2 && _document.layers()
                                                           .first()
                                                           .toMap()
                                                           .value("parameters")
                                                           .toMap()
                                                           .value("brightness")
                                                           .toDouble() == 20.,
                 "undo removes only the new shape and preserves the finished adjustment"))
        return false;
    return true;
}

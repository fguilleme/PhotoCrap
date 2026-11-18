#pragma once
#include "ClipboardSmoke.h"
#include "FontMenuSmoke.h"
#include "Document.h"
#include "ValueScrubberSmoke.h"
#include "WorkspaceSettings.h"
#include "ai/AiImageTypes.h"
#include "ai/SurfaceGeometry.h"
#include "text/TextEditorController.h"
#include "text/TextLayer.h"
#ifdef Q_OS_MACOS
#include "MacToolCursorTest.h"
#endif
#include <QCursor>
#include <QGuiApplication>
#include <QJSValue>
#include <QPainter>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
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
    _document.setTool("brush");
    _document.setActiveLayer(0);
    if (!require(!textFrame->isVisible(), "selecting a non-text layer hides the text frame"))
        return false;
    QTest::qWait(40);
    QMetaObject::invokeMethod(_window, "selectDockPanel", Q_ARG(QVariant, "layers"));
    QMetaObject::invokeMethod(_window, "moveDockPanelTo", Q_ARG(QVariant, "layers"),
                              Q_ARG(QVariant, "color"), Q_ARG(QVariant, false), Q_ARG(QVariant, 0));
    auto *layerList = _window->findChild<QQuickItem *>("layerListView");
    if (layerList)
        QMetaObject::invokeMethod(layerList, "positionViewAtBeginning");
    QTest::qWait(100);
    const auto layerRow = [layerList](int index) -> QQuickItem * {
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
            if (row->isVisible() && model.toMap().value("layerIndex", -1).toInt() == index)
                return row;
        }
        return nullptr;
    };
    auto *row           = layerRow(2);
    auto *textThumbnail = row ? row->findChild<QQuickItem *>("layerThumbnailMouse") : nullptr;
    if (!require(textThumbnail && textThumbnail->isVisible(), "the text thumbnail is visible"))
        return false;
    const auto thumbnailPoint =
        textThumbnail->mapToScene(QPointF(textThumbnail->width() / 2, textThumbnail->height() / 2))
            .toPoint();
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, thumbnailPoint);
    if (!require(QTest::qWaitFor(
                     [&] {
                         return _document.activeLayer() == 2 && _document.tool() == "text" &&
                                textFrame->isVisible() && !_document.textEditing() &&
                                !textInput->hasActiveFocus();
                     },
                     500),
                 "selecting a text layer activates the text tool and shows its frame")) {
        qWarning() << _document.activeLayer() << _document.tool() << textFrame->isVisible()
                   << _document.textEditing() << textInput->hasActiveFocus();
        return false;
    }
    QTest::mouseDClick(_window, Qt::LeftButton, Qt::NoModifier, thumbnailPoint);
    QTest::qWait(60);
    if (!require(_document.textEditing() && _document.layers().size() == 3,
                 "double-clicking the text thumbnail reopens the existing text without renaming or "
                 "creating"))
        return false;
    QTest::qWait(50);
    if (!require(_document.text() == "layer" && textInput->hasActiveFocus() &&
                     textInput->property("selectionStart").toInt() == 5 &&
                     textInput->property("selectionEnd").toInt() == 5,
                 "reopening text places a focused caret at the end without selecting it"))
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
    auto *fontStyle  = _window->findChild<QQuickItem *>("textFontStyle");
    if (!require(fontFamily && fontStyle &&
                     fontFamily->property("editText").toString() ==
                         formatting->format(1).value("family").toString() &&
                     fontStyle->isVisible() == (fontStyle->property("count").toInt() > 1),
                 "the font selector shows the actual family and hides a redundant style field"))
        return false;
    if (QCoreApplication::arguments().contains("--capture-text"))
        _window->grabWindow().save("/tmp/photocrap-text-editor.png");
    if (!require(checkFontMenu(_window, _document, typography, textInput, formatting),
                 "font hover is reversible and choosing a family changes only selected text"))
        return false;
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
    _document.setTool("pencil");
    QMetaObject::invokeMethod(_window, "activateShortcut", Q_ARG(QVariant, "selectBottomLayer"));
    if (!require(_document.activeLayer() == 0 && _document.tool() == "pencil",
                 "selecting an ordinary pixel layer keeps the current tool"))
        return false;
    QMetaObject::invokeMethod(_window, "activateShortcut", Q_ARG(QVariant, "selectLayerUp"));
    if (!require(_document.activeLayer() == 1 && _document.tool() == "gradient" &&
                     guide->isVisible() && _document.dynamicGradientActive(),
                 "selecting an existing gradient activates its tool and handles"))
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

    _document.newDocument(220, 160, true);
    _document.setTextSize(21);
    _window->setProperty("zoom", 1.0);
    const auto cancel = std::make_shared<std::atomic_bool>(false);
    const auto source = _document.prepareAiInput(false, 220, cancel, false, false, true).result();
    QImage     coverage(1, 1, QImage::Format_Grayscale8);
    coverage.fill(255);
    QImage normals(22, 16, QImage::Format_RGB888);
    normals.fill(QColor(128, 128, 255));
    QPainter plane(&normals);
    plane.fillRect(7, 0, 15, 16, QColor(165, 205, 219));
    plane.end();
    if (!require(_document.addSurfaceLayer(source, PixelPlane::uniform({220, 160}, coverage),
                                           normals, QPoint(24, 50), true),
                 "a folded text layer can be created for the canvas"))
        return false;
    _document.updateTextHtml("<b>Surface frame</b>");
    auto *outline  = _window->findChild<QQuickItem *>("surfaceTextOutline");
    auto *strength = _window->findChild<QQuickItem *>("surfaceTextStrength");
    if (!require(QTest::qWaitFor([&] { return outline && outline->isVisible(); }, 1500) &&
                     strength && strength->isVisible() &&
                     strength->property("value").toInt() == 100,
                 "folded text shows its outline and relief strength"))
        return false;
    const auto firstFrame = _document.textEditState().value("surfaceOutline").toList();
    _document.setSurfaceTextStrength(0.);
    if (!require(!outline->isVisible(), "zero strength restores the planar frame"))
        return false;
    _document.setSurfaceTextStrength(1.7);
    const auto newFrame = _document.textEditState().value("surfaceOutline").toList();
    if (!require(outline->isVisible() && newFrame != firstFrame && newFrame.size() > 4,
                 "changing the relief strength updates the outline"))
        return false;
    QTest::qWait(80);
    const auto anchor = PhotoCrap::Text::transform(_document.textEditState()).map(QPointF{});
    const qreal zoom = _window->property("zoom").toDouble();
    const auto borderPoint = outline->mapToScene(newFrame[2].toPointF() * zoom).toPoint();
    auto *frameDrag = _window->findChild<QQuickItem *>("textMoveMouse");
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, borderPoint);
    if (!require(frameDrag && frameDrag->property("moving").toBool(),
                 "the visible folded frame captures a press")) {
        qWarning() << borderPoint << newFrame[2] << zoom << (frameDrag ? frameDrag->size() : QSizeF{})
                   << (frameDrag ? frameDrag->mapFromScene(borderPoint) : QPointF{});
        return false;
    }
    QTest::mouseMove(_window, borderPoint + QPoint(8, 5), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, borderPoint + QPoint(8, 5));
    const auto moved = PhotoCrap::Text::transform(_document.textEditState()).map(QPointF{});
    if (!require(QLineF(moved, anchor + QPointF(8, 5) / zoom).length() < 1.,
                 "the visible folded frame can be dragged"))
        return false;
    _document.finishTextEdit();
    if (!require(outline->isVisible() && !strength->isVisible(),
                 "a selected folded layer keeps its frame when text editing ends"))
        return false;

    QTemporaryDir textProjectDir;
    const auto projectUrl = QUrl::fromLocalFile(textProjectDir.filePath("fabric.pcr"));
    if (!require(textProjectDir.isValid() && _document.saveProject(projectUrl),
                 "save a project with the folded text selected"))
        return false;
    _document.setTool("brush");
    _document.requestOpenFile(projectUrl);
    auto *typeButton = _window->findChild<QQuickItem *>("textTypographyButton");
    if (!require(QTest::qWaitFor([&] {
                     return !_document.operationBusy() && _document.tool() == "text" &&
                            _document.activeLayer() == 1 && outline->isVisible() &&
                            typeButton && typeButton->isVisible() && typeButton->isEnabled();
                 }, 3000) && !_document.textEditing() && !_document.modified(),
                 "reopening selected surface text activates its tool and formatting controls"))
        return false;
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier,
                      typeButton->mapToScene(typeButton->boundingRect().center()).toPoint());
    if (!require(QTest::qWaitFor([&] {
                     return typography->property("visible").toBool() && _document.textEditing();
                 }) && textInput->property("selectionStart") == textInput->property("selectionEnd"),
                 "formatting opens for the restored text without selecting its characters"))
        return false;
    constexpr int fabricCharacter = 11;
    QRectF fabricCaret;
    QMetaObject::invokeMethod(textInput, "positionToRectangle", Q_RETURN_ARG(QRectF, fabricCaret),
                              Q_ARG(int, fabricCharacter));
    QPolygonF caretOutline;
    for (const auto &point : _document.surfaceTextRect(fabricCaret))
        caretOutline.append(point.toPointF());
    if (!require(!caretOutline.isEmpty(), "the insertion caret is projected onto the fold"))
        return false;
    const QPointF insideText = caretOutline.boundingRect().center();
    QMetaObject::invokeMethod(typography, "close");
    _document.finishTextEdit(true);
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier,
                      area->mapToScene(insideText * _window->property("zoom").toReal()).toPoint());
    if (!require(QTest::qWaitFor([&] { return textInput->hasActiveFocus(); }) &&
                     textInput->property("cursorPosition").toInt() == fabricCharacter &&
                     _document.text() == "Surface frame" && _document.layerCount() == 2,
                 "clicking restored surface text focuses the existing letters"))
        return false;
    QTest::keyClick(_window, Qt::Key_X);
    if (!require(_document.text() == QStringLiteral("Surface frame").insert(fabricCharacter, 'x'),
                 "typing inserts a character in the restored text"))
        return false;
    _document.finishTextEdit(true);

    _document.newDocument(220, 160, true);
    _window->setProperty("zoom", 1.0);
    const auto twoFaces = _document.prepareAiInput(false, 220, cancel, false, false, true).result();
    const PhotoCrap::Ai::SurfaceCrease crease{QLineF(100, 130, 100, 30), QVector3D(0.f, 0.f, 1.f),
                                              QVector3D(.6f, 0.f, .8f)};
    if (!require(_document.addSurfaceLayer(twoFaces, PixelPlane::uniform({220, 160}, coverage),
                                           normals, QPoint(35, 60), false, .85,
                                           crease.parameters()),
                 "text can be placed across a wall edge"))
        return false;
    _document.updateTextHtml("<span style='font-size: 21px;'>WRAP AROUND</span>");
    _document.finishTextEdit();
    if (!require(outline->isVisible() &&
                     _document.textEditState().value("surfaceOutline").toList().size() == 7,
                 "the selected text frame turns at the wall edge"))
        return false;
    _document.editTextLayer(1);
    if (!require(QTest::qWaitFor([&] { return textInput->hasActiveFocus(); }, 1000),
                 "the projected text editor receives keyboard focus"))
        return false;
    const auto state   = _document.textEditState();
    const auto primary = PhotoCrap::Text::transform(state);
    const auto across  = PhotoCrap::Text::transform({{"transform", state.value("surfaceAcross")}});
    QRectF     caret;
    constexpr int character = 8;
    QMetaObject::invokeMethod(textInput, "positionToRectangle", Q_RETURN_ARG(QRectF, caret),
                              Q_ARG(int, character));
    const auto click = across.map(primary.map(caret.center()));
    if (!require(click.x() > 100. && click.x() < 220.,
                 "the caret test targets a letter on the second face"))
        return false;
    _document.finishTextEdit();
    _document.setActiveLayer(0);
    _document.setTool("brush");
    const auto toolReady = [&] {
        return _document.activeLayer() == 1 && _document.tool() == "text" &&
               !_document.textEditing() && _document.layerCount() == 2;
    };
    QMetaObject::invokeMethod(layerList, "positionViewAtBeginning");
    if (!require(QTest::qWaitFor([&] { return layerRow(1); }, 1000),
                 "the projected text row is visible"))
        return false;
    auto      *projectedRow = layerRow(1);
    const auto rowPoint =
        projectedRow->mapToScene({projectedRow->width() * .7, projectedRow->height() / 2})
            .toPoint();
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, rowPoint);
    if (!require(QTest::qWaitFor(toolReady, 500),
                 "selecting a projected text row activates the text tool")) {
        qWarning() << rowPoint << _document.activeLayer() << _document.tool()
                   << _document.textEditing() << _document.layerCount();
        return false;
    }
    _document.setTool("brush");
    auto *selectedRow = layerRow(1);
    auto *thumbnail =
        selectedRow ? selectedRow->findChild<QQuickItem *>("layerThumbnailMouse") : nullptr;
    if (!require(thumbnail && thumbnail->isVisible(), "the projected text thumbnail is visible"))
        return false;
    QTest::mouseClick(
        _window, Qt::LeftButton, Qt::NoModifier,
        thumbnail->mapToScene({thumbnail->width() / 2, thumbnail->height() / 2}).toPoint());
    if (!require(QTest::qWaitFor(toolReady, 500),
                 "clicking the projected text thumbnail activates the text tool"))
        return false;
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, area->mapToScene(click).toPoint());
    if (!require(QTest::qWaitFor(
                     [&] {
                         return _document.textEditing() && !_document.textGesture() &&
                                _document.activeLayer() == 1 && _document.layerCount() == 2 &&
                                textInput->hasActiveFocus() &&
                                textInput->property("cursorPosition").toInt() == character;
                     },
                     500),
                 "clicking the second face reopens the projected text with a focused caret"))
        return false;
    QTest::keyClick(_window, Qt::Key_Left, Qt::ShiftModifier);
    if (!require(textInput->property("selectionStart").toInt() == character - 1 &&
                     textInput->property("selectionEnd").toInt() == character,
                 "character selection still works while editing across the edge"))
        return false;
    _document.finishTextEdit();

    _document.newDocument(220, 160, true);
    const auto paintSource =
        _document.prepareAiInput(false, 220, cancel, false, false, true).result();
    if (!require(_document.addSurfaceLayer(paintSource, PixelPlane::uniform({220, 160}, coverage),
                                           normals),
                 "a projected paint layer can be created"))
        return false;
    _document.setActiveLayer(0);
    _document.setTool("text");
    QMetaObject::invokeMethod(_window, "activateShortcut", Q_ARG(QVariant, "selectTopLayer"));
    if (!require(_document.activeLayer() == 1 && _document.tool() == "brush",
                 "selecting a projected paint layer activates the brush"))
        return false;
    _document.setTool("pencil");
    QMetaObject::invokeMethod(_window, "activateMaskThumbnail", Q_ARG(QVariant, 1),
                              Q_ARG(QVariant, int(Qt::AltModifier)));
    if (!require(_document.editingMask() && _document.tool() == "pencil",
                 "selecting the mask keeps the chosen mask editing tool"))
        return false;
    QMetaObject::invokeMethod(_window, "activateShortcut", Q_ARG(QVariant, "selectTopLayer"));
    if (!require(!_document.editingMask() && _document.tool() == "brush",
                 "returning to layer content restores its tool"))
        return false;
    return true;
}

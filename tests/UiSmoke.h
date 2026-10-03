#pragma once
#include "Document.h"
#include "RecentFilesManager.h"
#include <QFile>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

// Exercise pointer delivery through QML, keyboard focus and the unsaved guard.
// Compiled only with BUILD_TESTING; production builds need no Qt Test runtime.
inline bool runUiSmoke(QQuickWindow *_window, Document &_document, RecentFilesManager &_recentFiles)
{
    auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("UI check failed: %s", _message);
        return _ok;
    };
    auto area = _window->findChild<QQuickItem *>("drawingArea");
    if (!require(area, "drawing area exists"))
        return false;
    auto panArea = _window->findChild<QQuickItem *>("panArea");
    if (!require(panArea, "pan area exists"))
        return false;
    auto viewport = _window->findChild<QQuickItem *>("viewport");
    auto artboard = _window->findChild<QQuickItem *>("artboard");
    if (!require(viewport && artboard, "viewport and artboard exist"))
        return false;
    _document.newDocument(300, 200, true);
    QTest::qWait(100);
    QTemporaryDir recentDir;
    if (!require(recentDir.isValid(), "temporary recent-files directory exists"))
        return false;
    QFile recentFile(recentDir.filePath("A very long recent project filename that must preserve "
                                        "both ends of its complete path in the menu.pcr"));
    if (!require(recentFile.open(QIODevice::WriteOnly), "temporary recent file can be created"))
        return false;
    recentFile.write("test");
    recentFile.close();
    _recentFiles.add(QUrl::fromLocalFile(recentFile.fileName()));
    QTest::qWait(30);
    auto          recentItem  = _window->findChild<QObject *>("recentFileItem");
    auto          recentLabel = _window->findChild<QObject *>("recentFilePathLabel");
    auto          recentMenu  = _window->findChild<QObject *>("recentFilesMenu");
    const QString recentText  = recentItem ? recentItem->property("text").toString() : QString();
    if (!require(_window->property("recentFileCount").toInt() == 1 && recentMenu && recentLabel &&
                     recentText.contains(QChar(0x2026)) && recentText.size() == 36 &&
                     recentLabel->property("text").toString() == recentText,
                 "recent-files menu renders a compact path with both ends preserved"))
        return false;
    QMetaObject::invokeMethod(recentMenu, "open");
    QTest::qWait(50);
    if (!require(recentMenu->property("width").toReal() <= 300.5,
                 "recent-files menu stays compact for long paths"))
        return false;
    QMetaObject::invokeMethod(recentMenu, "close");
    _recentFiles.clear();
    QTest::qWait(20);
    if (!require(_window->property("recentFileCount").toInt() == 0,
                 "recent-files list can be cleared"))
        return false;
    const QPointF wheelLocal(viewport->width() * .43, viewport->height() * .39);
    const QPoint  wheelScene = viewport->mapToScene(wheelLocal).toPoint();
    const qreal   oldZoom    = _window->property("zoom").toReal();
    const qreal   documentX =
        (viewport->property("contentX").toReal() + wheelLocal.x() - artboard->x()) / oldZoom;
    const qreal documentY =
        (viewport->property("contentY").toReal() + wheelLocal.y() - artboard->y()) / oldZoom;
    QTest::wheelEvent(_window, wheelScene, QPoint(0, 120), QPoint(), Qt::ControlModifier);
    const qreal newZoom = _window->property("zoom").toReal();
    const qreal zoomedDocumentX =
        (viewport->property("contentX").toReal() + wheelLocal.x() - artboard->x()) / newZoom;
    const qreal zoomedDocumentY =
        (viewport->property("contentY").toReal() + wheelLocal.y() - artboard->y()) / newZoom;
    if (!require(newZoom > oldZoom && qAbs(documentX - zoomedDocumentX) < .1 &&
                     qAbs(documentY - zoomedDocumentY) < .1,
                 "Ctrl+wheel zooms around the pointer"))
        return false;
    const qreal beforeVertical = viewport->property("contentY").toReal();
    QTest::wheelEvent(_window, wheelScene, QPoint(0, -120));
    if (!require(viewport->property("contentY").toReal() > beforeVertical, "wheel pans vertically"))
        return false;
    const qreal beforeHorizontal = viewport->property("contentX").toReal();
    const qreal stableVertical   = viewport->property("contentY").toReal();
    QTest::wheelEvent(_window, wheelScene, QPoint(0, -120), QPoint(), Qt::ShiftModifier);
    if (!require(viewport->property("contentX").toReal() > beforeHorizontal &&
                     qAbs(viewport->property("contentY").toReal() - stableVertical) < .01,
                 "Shift+wheel pans horizontally"))
        return false;
    viewport->forceActiveFocus();
    QTest::keyPress(_window, Qt::Key_Space);
    QTest::qWait(20);
    const qreal  beforeFreePanX = viewport->property("contentX").toReal(),
                 beforeFreePanY = viewport->property("contentY").toReal();
    const QPoint panStart =
        viewport->mapToScene(QPointF(viewport->width() / 2, viewport->height() / 2)).toPoint();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, panStart);
    QTest::mouseMove(_window, panStart - QPoint(30, 25), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, panStart - QPoint(30, 25));
    QTest::keyRelease(_window, Qt::Key_Space);
    QTest::qWait(20);
    if (!require(viewport->property("contentX").toReal() > beforeFreePanX &&
                     viewport->property("contentY").toReal() > beforeFreePanY,
                 "Space+drag pans freely"))
        return false;
    _document.setColor(Qt::red);
    _document.setBrushSize(16);
    _document.setTool("brush");
    QPoint start = area->mapToScene(QPointF(area->width() / 3, area->height() / 2)).toPoint();
    QPoint end   = area->mapToScene(QPointF(area->width() * 2 / 3, area->height() / 2)).toPoint();
    QTest::mouseMove(_window, start);
    QTest::qWait(20);
    if (!require(area->property("containsMouse").toBool() &&
                     panArea->property("cursorShape").toInt() == Qt::BlankCursor,
                 "brush cursor is not covered by the pan hand"))
        return false;
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(_window, end, 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, end);
    const qreal drawingZoom = _window->property("zoom").toReal();
    if (!require(_document.canUndo() && _document.composite().pixelColor(100, 100).red() > 200 &&
                     _document.composite().pixelColor(100, 100).alpha() > 0,
                 "pointer paints document"))
        return false;
    _document.undo();
    QTest::qWait(30);
    if (!require(_document.composite().pixelColor(100, 100).alpha() == 0 &&
                     qAbs(_window->property("zoom").toReal() - drawingZoom) < .001,
                 "gesture undoes without changing zoom"))
        return false;
    _document.redo();
    QMetaObject::invokeMethod(_window, "openLayerContextMenu");
    QTest::qWait(100);
    if (!require(_window->property("layerContextMenuVisible").toBool(), "layer context menu opens"))
        return false;
    QMetaObject::invokeMethod(_window, "closeLayerContextMenu");
    _document.setTool("text");
    auto textField = _window->findChild<QQuickItem *>("textField");
    if (!require(textField, "text field exists"))
        return false;
    textField->forceActiveFocus();
    QTest::keyClick(_window, Qt::Key_B);
    if (!require(_document.tool() == "text", "typing B does not switch tools"))
        return false;
    area->forceActiveFocus();
    _document.setTool("brush");
    _document.setColor(Qt::red);
    _document.setSecondaryColor(Qt::blue);
    _document.setBrushHardness(.5);
    auto topHardness = _window->findChild<QObject *>("brushHardnessTop");
    if (!require(topHardness && topHardness->property("visible").toBool() &&
                     topHardness->property("displayText").toString() == QStringLiteral("50") &&
                     topHardness->property("suffix").toString().contains(QLatin1Char('%')),
                 "brush hardness is available and its editable text excludes the percent sign"))
        return false;
    QTest::keyClick(_window, Qt::Key_D);
    QTest::qWait(20);
    if (!require(_document.color() == Qt::black && _document.secondaryColor() == Qt::white,
                 "D restores default colors"))
        return false;
    QTest::keyClick(_window, Qt::Key_X);
    QTest::qWait(20);
    if (!require(_document.color() == Qt::white && _document.secondaryColor() == Qt::black,
                 "X swaps foreground and background colors"))
        return false;
    QTest::keyClick(_window, Qt::Key_BracketLeft, Qt::ShiftModifier);
    QTest::qWait(20);
    if (!require(qAbs(_document.brushHardness() - .4) < .001, "Shift+[ decreases brush hardness"))
        return false;
    QTest::keyClick(_window, Qt::Key_BracketRight, Qt::ShiftModifier);
    QTest::qWait(20);
    if (!require(qAbs(_document.brushHardness() - .5) < .001, "Shift+] increases brush hardness"))
        return false;
    auto preferences = _window->findChild<QObject *>("preferencesDialog");
    if (!require(preferences, "preferences dialog exists"))
        return false;
    QMetaObject::invokeMethod(_window, "setInterfaceLanguage", Q_ARG(QVariant, QVariant("en")));
    QTest::qWait(100);
    if (!require(_window->property("interfaceLanguage").toString() == "en" &&
                     _window->property("newActionText").toString() == "New…",
                 "English interface loads immediately"))
        return false;
    QMetaObject::invokeMethod(_window, "setInterfaceLanguage", Q_ARG(QVariant, QVariant("fr")));
    QTest::qWait(100);
    if (!require(_window->property("newActionText").toString() == QString::fromUtf8("Nouveau…"),
                 "French interface restores immediately"))
        return false;
    QMetaObject::invokeMethod(_window, "openPreferences");
    QTest::qWait(150);
    auto brushSettings = _window->findChild<QObject *>("brushSettingsDialog");
    if (!require(preferences->property("visible").toBool() && brushSettings &&
                     !brushSettings->property("visible").toBool(),
                 "application preferences open without brush settings"))
        return false;
    _window->setProperty("preferencesPage", 1);
    QTest::qWait(50);
    auto languageCombo = _window->findChild<QObject *>("languageCombo");
    if (!require(languageCombo && languageCombo->property("count").toInt() == 2,
                 "interface settings list French and English"))
        return false;
    _window->setProperty("preferencesPage", 0);
    auto shortcutList = _window->findChild<QObject *>("shortcutList");
    if (!require(shortcutList && shortcutList->property("count").toInt() == 37,
                 "keyboard settings list is populated"))
        return false;
    QMetaObject::invokeMethod(preferences, "close");
    QMetaObject::invokeMethod(brushSettings, "open");
    QTest::qWait(100);
    if (!require(brushSettings->property("visible").toBool(), "brush settings dialog opens"))
        return false;
    auto brushCancel = _window->findChild<QObject *>("brushSettingsCancelButton");
    auto brushOk     = _window->findChild<QObject *>("brushSettingsOkButton");
    if (!require(brushCancel && brushOk, "brush settings provides Cancel and OK actions"))
        return false;
    auto brushOptionsButton = _window->findChild<QObject *>("brushOptionsButton");
    auto brushHardnessTop   = _window->findChild<QObject *>("brushHardnessTop");
    if (!require(brushOptionsButton && brushHardnessTop &&
                     qAbs(brushOptionsButton->property("height").toReal() -
                          brushHardnessTop->property("height").toReal()) < .5,
                 "toolbar buttons and numeric controls share one height"))
        return false;
    const qreal originalBrushSize = _document.brushSize();
    _document.setBrushSize(originalBrushSize + 12);
    QMetaObject::invokeMethod(brushSettings, "reject");
    QTest::qWait(80);
    if (!require(qAbs(_document.brushSize() - originalBrushSize) < .001,
                 "Cancel restores brush settings from before the dialog opened"))
        return false;
    QMetaObject::invokeMethod(brushSettings, "open");
    QTest::qWait(80);
    _document.setBrushSize(originalBrushSize + 8);
    brushSettings->setProperty("keepSettings", true);
    QMetaObject::invokeMethod(brushSettings, "accept");
    QTest::qWait(80);
    if (!require(qAbs(_document.brushSize() - originalBrushSize - 8) < .001,
                 "OK keeps the edited brush settings"))
        return false;
    QMetaObject::invokeMethod(_window, "guard", Q_ARG(QVariant, QVariant("new")));
    QTest::qWait(200);
    auto guard = _window->findChild<QObject *>("unsavedDialog");
    if (!require(guard && guard->property("visible").toBool(), "unsaved changes are guarded"))
        return false;
    QMetaObject::invokeMethod(guard, "close");
    QTest::qWait(150);
    _document.newDocument(300, 200, true);
    QMetaObject::invokeMethod(_window, "guard", Q_ARG(QVariant, QVariant("new")));
    QTest::qWait(150);
    auto dialog = _window->findChild<QObject *>("newDialog");
    if (!require(dialog && dialog->property("visible").toBool() &&
                     dialog->property("height").toReal() > 200,
                 "new document dialog has content"))
        return false;
    auto newCancel = _window->findChild<QObject *>("newDialogCancelButton");
    auto newOk     = _window->findChild<QObject *>("newDialogOkButton");
    if (!require(newCancel && newOk &&
                     newCancel->property("focusPolicy").toInt() == int(Qt::StrongFocus) &&
                     newOk->property("defaultButton").toBool(),
                 "dialog actions expose keyboard focus and a default button"))
        return false;
    QMetaObject::invokeMethod(newCancel, "forceActiveFocus");
    QTest::keyClick(_window, Qt::Key_Space);
    QTest::qWait(80);
    if (!require(!dialog->property("visible").toBool(),
                 "Space activates the focused dialog button"))
        return false;
    QMetaObject::invokeMethod(_window, "guard", Q_ARG(QVariant, QVariant("new")));
    QTest::qWait(80);
    QMetaObject::invokeMethod(newCancel, "forceActiveFocus");
    QTest::keyClick(_window, Qt::Key_Return);
    QTest::qWait(100);
    if (!require(!dialog->property("visible").toBool() && _document.width() == 1280 &&
                     _document.height() == 800,
                 "Return activates the default dialog button"))
        return false;
    _document.addAdjustmentLayer("brightnessContrast");
    auto layerProperties = _window->findChild<QObject *>("layerPropertiesDialog");
    if (!require(layerProperties, "layer properties dialog exists"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(150);
    if (!require(layerProperties->property("visible").toBool(),
                 "adjustment properties dialog opens"))
        return false;
    auto layerPropertiesCancel = _window->findChild<QObject *>("layerPropertiesCancelButton");
    auto layerPropertiesClose  = _window->findChild<QObject *>("layerPropertiesCloseButton");
    if (!require(layerPropertiesCancel && layerPropertiesClose,
                 "layer properties provides Cancel and Close actions"))
        return false;
    if (!require(_window->property("parameterSliderWidth").toReal() >
                     layerProperties->property("width").toReal() * .75,
                 "adjustment slider fills dialog width"))
        return false;
    const int previewHistory = _document.historyIndex();
    _document.beginLayerPreview();
    QMetaObject::invokeMethod(_window, "queueLayerPreview", Q_ARG(QVariant, QVariant("brightness")),
                              Q_ARG(QVariant, QVariant(10)));
    QMetaObject::invokeMethod(_window, "queueLayerPreview", Q_ARG(QVariant, QVariant("brightness")),
                              Q_ARG(QVariant, QVariant(20)));
    QMetaObject::invokeMethod(_window, "queueLayerPreview", Q_ARG(QVariant, QVariant("brightness")),
                              Q_ARG(QVariant, QVariant(30)));
    // Drive the coalescing boundary explicitly. Waiting for a short QML timer
    // makes this assertion depend on runner load rather than application state.
    QMetaObject::invokeMethod(_window, "flushLayerPreview");
    const QVariantMap previewLayer = _document.layers().first().toMap();
    if (!require(previewLayer.value("parameters").toMap().value("brightness").toInt() == 30 &&
                     _document.historyIndex() == previewHistory,
                 "rapid slider updates keep only the newest preview without adding undo steps"))
        return false;
    QMetaObject::invokeMethod(_window, "previewLayerValue", Q_ARG(QVariant, QVariant("brightness")),
                              Q_ARG(QVariant, QVariant(35)), Q_ARG(QVariant, QVariant(true)));
    if (!require(_document.historyIndex() == previewHistory + 1,
                 "releasing a slider commits one undo step"))
        return false;
    layerProperties->setProperty("keepChanges", true);
    QMetaObject::invokeMethod(layerProperties, "accept");
    _document.undo();
    _document.undo();
    _document.addAdjustmentLayer("curves");
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    auto curvesEditor = _window->findChild<QQuickItem *>("curvesEditor");
    auto levelsEditor = _window->findChild<QQuickItem *>("levelsEditor");
    if (!require(curvesEditor && curvesEditor->isVisible() && levelsEditor &&
                     !levelsEditor->isVisible(),
                 "curves adjustment uses its dedicated editor"))
        return false;
    auto curvesRoot = _window->findChild<QQuickItem *>("curvesEditorRoot");
    if (!require(curvesRoot, "curves editor root exists"))
        return false;
    const QPoint curveStart =
        curvesEditor->mapToScene(QPointF(curvesEditor->width() / 2, curvesEditor->height() / 2))
            .toPoint();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, curveStart);
    QTest::mouseMove(_window, curveStart + QPoint(24, -18), 20);
    QTest::qWait(80);
    QTest::mouseMove(_window, curveStart + QPoint(52, -36), 20);
    QTest::qWait(80);
    if (!require(curvesRoot->property("dragging").toBool(),
                 "curves keeps pointer capture while previews finish"))
        return false;
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, curveStart + QPoint(52, -36));
    QTest::qWait(100);
    QMetaObject::invokeMethod(layerProperties, "close");
    _document.undo();
    _document.addAdjustmentLayer("levels");
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    if (!require(levelsEditor->isVisible() && !curvesEditor->isVisible(),
                 "levels adjustment uses its dedicated editor"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "close");
    _document.createDemo();
    _document.setTool("brush");
    QTest::qWait(200);
    qInfo("UI interaction checks passed");
    return true;
}

#pragma once
#include "Document.h"
#include "LayerContextMenuSmoke.h"
#include "PenUiSmoke.h"
#include "RawTestImage.h"
#include "RecentFilesManager.h"
#include "SelectionUiSmoke.h"
#include "ShortcutManager.h"
#include "WorkspaceSettings.h"
#include <QFile>
#include <QOperatingSystemVersion>
#include <QPointingDevice>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>
#include <functional>

// Exercise pointer delivery through QML, keyboard focus and the unsaved guard.
// Compiled only with BUILD_TESTING; production builds need no Qt Test runtime.
inline bool runUiSmoke(QQuickWindow *_window, Document &_document, RecentFilesManager &_recentFiles,
                       WorkspaceSettings &_workspaceSettings, ShortcutManager &_shortcuts)
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
    auto wheelHandler = _window->findChild<QObject *>("wheelHandler");
    if (!require(wheelHandler, "wheel handler exists"))
        return false;
    auto viewport = _window->findChild<QQuickItem *>("viewport");
    auto artboard = _window->findChild<QQuickItem *>("artboard");
    if (!require(viewport && artboard, "viewport and artboard exist"))
        return false;
    QTemporaryDir rawDirectory;
    QFile         rawFile(rawDirectory.filePath(QString::fromUtf8("échantillon.DNG")));
    if (!require(rawFile.open(QIODevice::WriteOnly), "create a RAW fixture"))
        return false;
    rawFile.write(RawTestImage::dng());
    rawFile.close();
    auto rawPopup = _window->findChild<QObject *>("rawLoadPopup");
    if (!require(rawPopup && _document.rawFilePatterns().contains("*.DNG") &&
                     _document.rawFilePatterns().contains("*.cr3"),
                 "RAW dialog and camera patterns exist"))
        return false;
    if (!require(_document.openRawFile(QUrl::fromLocalFile(rawFile.fileName())),
                 "start asynchronous RAW open"))
        return false;
    QCoreApplication::processEvents();
    if (_document.rawLoading() && !require(rawPopup->property("visible").toBool() &&
                                               rawPopup->property("height").toInt() > 100,
                                           "RAW progress popup is usable"))
        return false;
    if (!require(QTest::qWaitFor([&] { return !_document.rawLoading(); }, 10000),
                 "RAW load finishes"))
        return false;
    if (!require(_document.channelBits() == 16 && _recentFiles.entries().size() > 0,
                 "RAW opens16-bit and enters recent files"))
        return false;
    _recentFiles.clear();
    _document.newDocument(300, 200, true);
    QTest::qWait(100);
    auto histogramPanel     = _window->findChild<QObject *>("histogramDockPanel");
    auto navigatorPanel     = _window->findChild<QObject *>("navigatorDockPanel");
    auto histogramCanvas    = _window->findChild<QQuickItem *>("colorHistogramCanvas");
    auto colorPanel         = _window->findChild<QObject *>("colorDockPanel");
    auto layersPanel        = _window->findChild<QObject *>("layersDockPanel");
    auto channelsPanel      = _window->findChild<QObject *>("channelsDockPanel");
    auto historyPanel       = _window->findChild<QObject *>("historyDockPanel");
    auto informationPanel   = _window->findChild<QObject *>("informationDockPanel");
    auto informationContent = _window->findChild<QQuickItem *>("informationPanelContent");
    if (!require(informationPanel && informationContent &&
                     !informationPanel->property("panelShown").toBool() &&
                     _window->findChild<QObject *>("informationPanelMenuItem"),
                 "Info is an independent panel, initially hidden, with a View menu entry"))
        return false;
    if (!require(histogramPanel && navigatorPanel && histogramCanvas && colorPanel && layersPanel &&
                     channelsPanel && historyPanel,
                 "dockable workspace panels exist"))
        return false;
    histogramPanel->setProperty("collapsed", true);
    if (!require(histogramPanel->property("collapsed").toBool(), "individual dock panel collapses"))
        return false;
    histogramPanel->setProperty("collapsed", false);
    QMetaObject::invokeMethod(_window, "groupDockPanels", Q_ARG(QVariant, "navigator"),
                              Q_ARG(QVariant, "histogram"));
    QTest::qWait(20);
    if (!require(navigatorPanel->property("visible").toBool() &&
                     !histogramPanel->property("visible").toBool(),
                 "dropping a panel header on another panel creates an active tab"))
        return false;
    QMetaObject::invokeMethod(_window, "selectDockPanel", Q_ARG(QVariant, "histogram"));
    QTest::qWait(20);
    if (!require(histogramPanel->property("visible").toBool() &&
                     !navigatorPanel->property("visible").toBool(),
                 "grouped dock tabs switch the visible panel"))
        return false;
    auto histogramUngroup = _window->findChild<QObject *>("histogramUngroupButton");
    if (!require(histogramUngroup && histogramUngroup->property("visible").toBool(),
                 "grouped tabs expose a visible ungroup command"))
        return false;
    QMetaObject::invokeMethod(histogramUngroup, "clicked");
    QTest::qWait(20);
    if (!require(histogramPanel->property("visible").toBool() &&
                     navigatorPanel->property("visible").toBool(),
                 "a grouped panel can return to the dock independently"))
        return false;
    QMetaObject::invokeMethod(_window, "moveDockPanelTo", Q_ARG(QVariant, "navigator"),
                              Q_ARG(QVariant, ""), Q_ARG(QVariant, true), Q_ARG(QVariant, 1));
    if (!require(navigatorPanel->property("dockColumn").toInt() == 1 &&
                     _window->property("dockWidth").toReal() >= 560.0,
                 "dragging horizontally creates a second visible panel column"))
        return false;
    QMetaObject::invokeMethod(_window, "moveDockPanelTo", Q_ARG(QVariant, "navigator"),
                              Q_ARG(QVariant, "history"), Q_ARG(QVariant, true),
                              Q_ARG(QVariant, 0));
    if (!require(navigatorPanel->property("dockColumn").toInt() == 0,
                 "second-column panel docks back into the first column"))
        return false;
    QMetaObject::invokeMethod(_window, "moveDockPanelTo", Q_ARG(QVariant, "color"),
                              Q_ARG(QVariant, "histogram"), Q_ARG(QVariant, false),
                              Q_ARG(QVariant, 0));
    const QVariantList reordered = _window->property("dockPanelOrder").toList();
    if (!require(!reordered.isEmpty() && reordered.first().toString() == "color" &&
                     reordered.indexOf("navigator") == reordered.indexOf("history") + 1,
                 "panels insert at the exact requested position in a dock column"))
        return false;
    auto layersHeader = _window->findChild<QQuickItem *>("layersDockHeader");
    if (!require(layersHeader && layersHeader->property("preventStealing").toBool(),
                 "the scrolling dock cannot steal an active panel drag"))
        return false;
    QMetaObject::invokeMethod(histogramPanel, "setPanelFloating", Q_ARG(QVariant, true));
    QTest::qWait(30);
    if (!require(histogramPanel->property("floating").toBool(), "dock panel enters floating state"))
        return false;
    if (!require(histogramCanvas->window() != _window,
                 "dock panel content moves into a floating tool window"))
        return false;
    _window->setProperty("panelsTemporarilyHidden", true);
    QTest::qWait(30);
    if (!require(!histogramCanvas->window()->isVisible() &&
                     histogramPanel->property("floating").toBool(),
                 "temporary hiding preserves the floating layout"))
        return false;
    _window->setProperty("panelsTemporarilyHidden", false);
    QTest::qWait(30);
    if (!require(histogramCanvas->window()->isVisible(),
                 "temporary hiding restores the floating window"))
        return false;
    QMetaObject::invokeMethod(histogramPanel, "setPanelFloating", Q_ARG(QVariant, false));
    QTest::qWait(30);
    if (!require(histogramCanvas->window() == _window,
                 "floating panel content docks back into the main window"))
        return false;
    _window->requestActivate();
    QTest::qWait(20);
    auto channelList = _window->findChild<QObject *>("channelList");
    if (!require(channelList && channelsPanel->property("visible").toBool(),
                 "color channels use an independent visible dock panel"))
        return false;
    QMetaObject::invokeMethod(_window, "setDockPanelVisible", Q_ARG(QVariant, "channels"),
                              Q_ARG(QVariant, false));
    QTest::qWait(20);
    if (!require(!channelsPanel->property("visible").toBool(),
                 "the View menu can hide an individual workspace panel"))
        return false;
    QMetaObject::invokeMethod(_window, "saveWorkspaceLayout");
    const QVariantMap savedWorkspace = _workspaceSettings.layout();
    if (!require(!savedWorkspace.isEmpty() &&
                     !savedWorkspace.value("visibility").toMap().value("channels").toBool() &&
                     savedWorkspace.value("order").toList().size() == 7,
                 "panel visibility and organization are saved in application settings"))
        return false;
    QMetaObject::invokeMethod(_window, "setDockPanelVisible", Q_ARG(QVariant, "channels"),
                              Q_ARG(QVariant, true));
    QTest::qWait(20);
    _document.setDisplayChannel(2);
    if (!require(_document.displayChannel() == 2, "green channel can be displayed on canvas"))
        return false;
    _document.setDisplayChannel(0);
    _document.addLayerMask(false);
    QTest::qWait(30);
    QMetaObject::invokeMethod(_window, "activateMaskThumbnail", Q_ARG(QVariant, 0),
                              Q_ARG(QVariant, int(Qt::AltModifier)));
    QTest::qWait(30);
    if (!require(_document.maskOnlyLayer() == _document.activeLayer(),
                 "Alt-clicking a mask thumbnail displays the mask alone"))
        return false;
    QMetaObject::invokeMethod(_window, "activateMaskThumbnail", Q_ARG(QVariant, 0),
                              Q_ARG(QVariant, int(Qt::AltModifier)));
    QTest::qWait(30);
    if (!require(_document.maskOnlyLayer() == -1, "a second Alt-click restores the composite view"))
        return false;
    auto backgroundFill = _window->findChild<QObject *>("backgroundFillShortcut");
    if (!require(backgroundFill, "background fill shortcut exists"))
        return false;
    _document.setSecondaryColor(Qt::white);
    QMetaObject::invokeMethod(backgroundFill, "activated");
    if (!require(_document.maskImage(_document.activeLayer()).pixelColor(10, 10).red() == 255,
                 "background fill shortcut fills the selected mask"))
        return false;
    auto maskColorFill = _window->findChild<QObject *>("maskColorFillShortcut");
    auto colorDialog   = _window->findChild<QObject *>("colorDialog");
    if (!require(maskColorFill && colorDialog, "mask color fill opens the shared color picker"))
        return false;
    QMetaObject::invokeMethod(maskColorFill, "activated");
    QTest::qWait(30);
    if (!require(colorDialog->property("visible").toBool(),
                 "Shift-Backspace opens a color picker for the selected mask"))
        return false;
    QMetaObject::invokeMethod(colorDialog, "reject");
    QTest::qWait(30);
    _document.setEditingMask(false);
    _window->requestActivate();
    QTest::qWait(20);
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
    // Real modifier presses and non-mouse devices exercise paths that a bare
    // synthetic Ctrl+wheel event over the image does not cover.
    const auto   savedTool    = _document.tool();
    const auto   savedZoom    = _window->property("zoom").toReal();
    const auto   savedScrollX = viewport->property("contentX").toReal();
    const auto   savedScrollY = viewport->property("contentY").toReal();
    const QPoint marginScene  = viewport->mapToScene(QPointF(8, 8)).toPoint();
    viewport->forceActiveFocus();
    QTest::keyPress(_window, Qt::Key_Control);
    const qreal beforeMarginZoom = _window->property("zoom").toReal();
    QTest::wheelEvent(_window, marginScene, QPoint(0, 120), QPoint(), Qt::ControlModifier);
    QTest::keyRelease(_window, Qt::Key_Control);
    if (!require(_window->property("zoom").toReal() > beforeMarginZoom,
                 "Ctrl+wheel zooms over the canvas margin"))
        return false;
    for (const QString &tool :
         {QStringLiteral("hand"), QStringLiteral("rotateView"), QStringLiteral("zoomTool")}) {
        _document.setTool(tool);
        QTest::qWait(10);
        const qreal beforeToolZoom = _window->property("zoom").toReal();
        QTest::wheelEvent(_window, wheelScene, QPoint(0, 120), QPoint(), Qt::ControlModifier);
        if (!require(_window->property("zoom").toReal() > beforeToolZoom,
                     "Ctrl+wheel zoom is independent of the active navigation tool"))
            return false;
    }
    _document.setTool(savedTool);
    QPointingDevice touchpad(
        QStringLiteral("Navigation test touchpad"), 0x504352, QInputDevice::DeviceType::TouchPad,
        QPointingDevice::PointerType::Finger, QInputDevice::Capability::Position, 2, 0);
    const qreal beforeTouchpadZoom = _window->property("zoom").toReal();
    QWheelEvent touchpadWheel(wheelScene, _window->mapToGlobal(wheelScene), QPoint(0, 24),
                              QPoint(0, 60), Qt::NoButton, Qt::ControlModifier, Qt::ScrollUpdate,
                              false, Qt::MouseEventNotSynthesized, &touchpad);
    QCoreApplication::sendEvent(_window, &touchpadWheel);
    QCoreApplication::processEvents();
    if (!require(_window->property("zoom").toReal() > beforeTouchpadZoom,
                 "Ctrl+touchpad scroll zooms with a touchpad device"))
        return false;
    const qreal beforePixelZoom = _window->property("zoom").toReal();
    QWheelEvent pixelWheel(wheelScene, _window->mapToGlobal(wheelScene), QPoint(0, -12), QPoint(),
                           Qt::NoButton, Qt::ControlModifier, Qt::ScrollUpdate, false,
                           Qt::MouseEventNotSynthesized, &touchpad);
    QCoreApplication::sendEvent(_window, &pixelWheel);
    QCoreApplication::processEvents();
    if (!require(_window->property("zoom").toReal() < beforePixelZoom,
                 "Ctrl+touchpad scroll also zooms with pixel-only deltas"))
        return false;
    _window->setProperty("zoom", savedZoom);
    viewport->setProperty("contentX", savedScrollX);
    viewport->setProperty("contentY", savedScrollY);
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
    if (!require(qAbs(_document.brushHardness() - .25) < .001, "Shift+[ decreases brush hardness"))
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
    if (!require(shortcutList && shortcutList->property("count").toInt() > 250,
                 "keyboard settings list is populated"))
        return false;
    auto shortcutStatus = _window->findChild<QObject *>("shortcutStatus");
    auto shortcutSearch = _window->findChild<QObject *>("shortcutSearch");
    if (!require(shortcutStatus && shortcutSearch,
                 "shortcut inventory has search and state filters"))
        return false;
    shortcutStatus->setProperty("currentIndex", 2);
    QTest::qWait(30);
    if (!require(shortcutList->property("count").toInt() > 80,
                 "NYI inventory lists missing commands"))
        return false;
    shortcutSearch->setProperty("text", "Fluidité");
    QTest::qWait(30);
    if (!require(shortcutList->property("count").toInt() == 1,
                 "shortcut search filters missing features"))
        return false;
    shortcutSearch->setProperty("text", "");
    shortcutStatus->setProperty("currentIndex", 0);
    _window->setProperty("preferencesPage", 2);
    QTest::qWait(80);
    auto toolSettingsList = _window->findChild<QObject *>("toolSettingsList");
    if (!require(toolSettingsList && toolSettingsList->property("visible").toBool(),
                 "toolbar settings page is available"))
        return false;
    _window->setProperty("preferencesPage", 3);
    QTest::qWait(50);
    auto scratchBudget = _window->findChild<QObject *>("scratchDiskBudget");
    auto clearScratch  = _window->findChild<QObject *>("clearScratchCacheButton");
    if (!require(scratchBudget && clearScratch && scratchBudget->property("visible").toBool() &&
                     scratchBudget->property("value").toInt() == 4,
                 "scratch cache settings expose the disk budget"))
        return false;
    _workspaceSettings.setScratchDiskGiB(3);
    QTest::qWait(20);
    if (!require(scratchBudget->property("value").toInt() == 3,
                 "scratch cache budget follows settings changes"))
        return false;
    QMetaObject::invokeMethod(clearScratch, "clicked");
    _workspaceSettings.setScratchDiskGiB(4);
    QMetaObject::invokeMethod(preferences, "close");
    QTest::qWait(80);
    _window->requestActivate();
    area->forceActiveFocus();
    _document.selectAll();
    QTest::keySequence(_window, QKeySequence("Ctrl+Shift+I"));
    QTest::qWait(30);
    if (!require(!_document.hasSelection(), "Adobe inverse-selection shortcut is active"))
        return false;
    QTest::keySequence(_window, QKeySequence("Ctrl+Shift+D"));
    QTest::qWait(30);
    if (!require(_document.hasSelection(), "Adobe reselect shortcut restores the selection"))
        return false;
    if (!require(_shortcuts.setSequence("invertSelection", "Ctrl+Alt+9"),
                 "new command can be customized"))
        return false;
    _document.selectAll();
    QTest::qWait(30);
    QTest::keySequence(_window, QKeySequence("Ctrl+Shift+I"));
    QTest::qWait(30);
    if (!require(_document.hasSelection(), "the former binding no longer executes"))
        return false;
    QTest::keySequence(_window, QKeySequence("Ctrl+Alt+9"));
    QTest::qWait(30);
    if (!require(!_document.hasSelection(), "custom binding executes immediately"))
        return false;
    _shortcuts.resetDefaults();
    _document.setTool("move");
    QTest::keySequence(_window, QKeySequence("Ctrl+T"));
    QTest::qWait(30);
    if (!require(_document.tool() == "move", "NYI bindings do not execute another tool"))
        return false;
    _document.setTool("brush");
    _document.deselect();
    QTest::keySequence(_window, QKeySequence("Ctrl+3"));
    QTest::qWait(30);
    if (!require(_document.displayChannel() == 1, "Adobe channel shortcut displays red"))
        return false;
    QTest::keySequence(_window, QKeySequence("Ctrl+2"));
    QTest::qWait(30);
    if (!require(_document.displayChannel() == 0, "Adobe composite shortcut restores RGB"))
        return false;
    _document.setTool("brush");
    QTest::keySequence(_window, QKeySequence("Alt+Shift+M"));
    QTest::qWait(30);
    if (!require(_document.brushBlend() == 3, "blend shortcut applies to the brush"))
        return false;
    _document.setBrushBlend(0);
    _document.setTool("sponge");
    QTest::keySequence(_window, QKeySequence("Alt+Shift+S"));
    QTest::qWait(30);
    if (!require(_document.spongeSaturate(), "Sponge shortcut takes priority over Screen blending"))
        return false;
    _document.setTool("dodge");
    QTest::keySequence(_window, QKeySequence("Alt+Shift+M"));
    QTest::qWait(30);
    if (!require(_document.retouchRange() == 1, "Dodge shortcut selects midtones"))
        return false;
    _document.setTool("brush");
    QTest::keySequence(_window, QKeySequence("Shift+Tab"));
    QTest::qWait(30);
    if (!require(_window->property("panelsTemporarilyHidden").toBool(),
                 "Shift-Tab hides panels temporarily"))
        return false;
    QTest::keySequence(_window, QKeySequence("Shift+Tab"));
    QTest::qWait(30);
    if (!require(!_window->property("panelsTemporarilyHidden").toBool(),
                 "Shift-Tab restores panels"))
        return false;
    auto preferencesAction = _window->findChild<QObject *>("preferencesAction");
    if (!require(preferencesAction, "application preferences action exists"))
        return false;
    QMetaObject::invokeMethod(preferencesAction, "trigger");
    QTest::qWait(80);
    if (!require(preferences->property("visible").toBool() &&
                     !brushSettings->property("visible").toBool(),
                 "preferences menu action opens application settings, not brush properties"))
        return false;
    QMetaObject::invokeMethod(preferences, "close");
    QTest::qWait(80);
    // Headless platforms may have no standard Preferences binding. The Cocoa
    // regression test below exercises Command-comma through the native menu.
    const auto preferencesShortcut = preferencesAction->property("shortcut").toString();
    if (!preferencesShortcut.isEmpty()) {
        QTest::keySequence(
            _window, QKeySequence::fromString(preferencesShortcut, QKeySequence::PortableText));
        QTest::qWait(80);
        if (!require(
                preferences->property("visible").toBool() &&
                    !brushSettings->property("visible").toBool(),
                "preferences keyboard shortcut opens application settings, not brush properties"))
            return false;
        QMetaObject::invokeMethod(preferences, "close");
        QTest::qWait(80);
    }
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "shape"),
                              Q_ARG(QVariant, "ellipse"));
    QTest::qWait(40);
    if (!require(_document.tool() == "ellipse", "a grouped tool can become active"))
        return false;
    QTest::keyClick(_window, Qt::Key_U);
    QTest::qWait(30);
    if (!require(_document.tool() == "ellipse", "group shortcut restores its active tool"))
        return false;
    QTest::keyClick(_window, Qt::Key_U, Qt::ShiftModifier);
    if (!require(_document.tool() == "triangle",
                 "Shift+U cycles to the next implemented visible shape"))
        return false;
    QTest::keyClick(_window, Qt::Key_U);
    if (!require(_document.tool() == "triangle",
                 "plain tool shortcut keeps the current group variant"))
        return false;
    _workspaceSettings.setUseShiftToCycleTools(false);
    QTest::keyClick(_window, Qt::Key_U);
    if (!require(_document.tool() == "polygon",
                 "tool cycling without Shift follows the preference"))
        return false;
    _workspaceSettings.setUseShiftToCycleTools(true);
    QTest::keyClick(_window, Qt::Key_B);
    QTest::keyPress(_window, Qt::Key_E);
    QTest::qWait(250);
    if (!require(_document.tool() == "eraser", "held E temporarily selects the eraser"))
        return false;
    QTest::keyRelease(_window, Qt::Key_E);
    if (!require(_document.tool() == "brush", "releasing held E restores the brush"))
        return false;
    QTest::keyClick(_window, Qt::Key_E);
    if (!require(_document.tool() == "eraser", "a short E press keeps the eraser"))
        return false;
    QTest::keyClick(_window, Qt::Key_Y);
    if (!require(_document.tool() == "historyBrush", "Y selects the history brush group"))
        return false;
    QTest::keyClick(_window, Qt::Key_B);
    QTest::keyClick(_window, Qt::Key_4);
    QTest::keyClick(_window, Qt::Key_5);
    if (!require(qAbs(_document.brushOpacity() - .45) < .001,
                 "rapid digit keys set precise brush opacity"))
        return false;
    QTest::keyClick(_window, Qt::Key_2, Qt::ShiftModifier);
    if (!require(qAbs(_document.brushFlow() - .2) < .001,
                 "Shift+digit sets brush flow independently"))
        return false;
    QTest::keyPress(_window, Qt::Key_Alt);
    if (!require(_window->property("temporaryPickerActive").toBool(),
                 "Option temporarily activates the brush eyedropper"))
        return false;
    QTest::keyRelease(_window, Qt::Key_Alt);
    if (!require(!_window->property("temporaryPickerActive").toBool(),
                 "releasing Option restores the brush cursor"))
        return false;
    QTest::keyPress(_window, Qt::Key_Meta);
    if (!require(!_window->property("temporaryMoveActive").toBool(),
                 "physical Control on Mac does not activate Command's temporary Move"))
        return false;
    QTest::keyRelease(_window, Qt::Key_Meta);
    _document.setTool("colorSampler");
    QTest::keyPress(_window, Qt::Key_Control);
    auto measureArea = _window->findChild<QQuickItem *>("measureToolArea");
    if (!require(_window->property("temporaryMoveActive").toBool() && measureArea &&
                     !measureArea->property("enabled").toBool(),
                 "Command/Ctrl temporarily moves without the measurement overlay stealing input"))
        return false;
    QTest::keyRelease(_window, Qt::Key_Control);
    _document.setTool("brush");
    _document.setBrushOpacity(1);
    _document.setBrushFlow(1);
    QTest::qWait(40);
    std::function<QQuickItem *(QQuickItem *)> findBrushGroup =
        [&](QQuickItem *_item) -> QQuickItem * {
        if (_item->objectName() == "toolGroup_brush")
            return _item;
        for (auto *child : _item->childItems())
            if (auto *result = findBrushGroup(child))
                return result;
        return nullptr;
    };
    auto brushGroup = findBrushGroup(_window->contentItem());
    if (!require(brushGroup, "brush group button exists"))
        return false;
    const QPoint brushGroupPoint =
        brushGroup->mapToScene(QPointF(brushGroup->width() / 2, brushGroup->height() / 2))
            .toPoint();
    QTest::mouseClick(_window, Qt::LeftButton, Qt::AltModifier, brushGroupPoint);
    if (!require(_document.tool() == "pencil", "Option-click cycles the toolbar group"))
        return false;
    QTest::keyClick(_window, Qt::Key_B, Qt::ShiftModifier);
    if (!require(_document.tool() == "colorReplacement",
                 "keyboard cycling follows the toolbar variant"))
        return false;
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "brush"),
                              Q_ARG(QVariant, "brush"));
    QTest::mouseMove(_window, start);
    _document.setBrushSize(20);
    _document.setBrushHardness(.5);
    const int  beforeHudRevision = _document.revision();
    const auto hudModifiers = Qt::AltModifier | (QGuiApplication::platformName() == "cocoa" ||
                                                         QOperatingSystemVersion::currentType() ==
                                                             QOperatingSystemVersion::MacOS
                                                     ? Qt::MetaModifier
                                                     : Qt::NoModifier);
    const auto hudButton = QOperatingSystemVersion::currentType() == QOperatingSystemVersion::MacOS
                               ? Qt::LeftButton
                               : Qt::RightButton;
    QTest::mousePress(_window, hudButton, hudModifiers, start);
    QTest::mouseMove(_window, start + QPoint(20, -10), 20);
    QTest::mouseRelease(_window, hudButton, hudModifiers, start + QPoint(20, -10));
    if (!require(qAbs(_document.brushSize() - 40) < .01 &&
                     qAbs(_document.brushHardness() - .6) < .01 &&
                     _document.revision() == beforeHudRevision,
                 "canvas brush adjustment changes size/hardness without painting or undo entries"))
        return false;
    QTest::keyClick(_window, Qt::Key_CapsLock);
    if (!require(area->property("cursorShape").toInt() == Qt::CrossCursor,
                 "Caps Lock switches the brush to a precise cursor"))
        return false;
    QTest::keyClick(_window, Qt::Key_CapsLock);
    _document.setTool("select");
    _document.deselect();
    const qreal gestureZoom = _window->property("zoom").toReal();
    const auto  selectionStart =
        area->mapToScene(QPointF(20 * gestureZoom, 20 * gestureZoom)).toPoint();
    const auto selectionEnd =
        area->mapToScene(QPointF(40 * gestureZoom, 30 * gestureZoom)).toPoint();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, selectionStart);
    QTest::mouseMove(_window, selectionEnd, 20);
    QTest::keyPress(_window, Qt::Key_Shift);
    if (!require(_document.selection().width() == _document.selection().height(),
                 "pressing Shift during marquee drawing immediately constrains the frame"))
        return false;
    QTest::keyRelease(_window, Qt::Key_Shift);
    const QSize beforeReposition = _document.selection().size();
    QTest::keyPress(_window, Qt::Key_Space);
    if (!require(!panArea->property("enabled").toBool(),
                 "Space while drawing a marquee does not steal the gesture for canvas panning"))
        return false;
    QTest::mouseMove(_window,
                     selectionEnd + QPoint(qRound(10 * gestureZoom), qRound(5 * gestureZoom)), 20);
    if (!require(_document.selection().size() == beforeReposition,
                 "Space-drag repositions a marquee without changing its size"))
        return false;
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier,
                        selectionEnd + QPoint(qRound(10 * gestureZoom), qRound(5 * gestureZoom)));
    QTest::keyRelease(_window, Qt::Key_Space);
    _document.deselect();
    auto toolMenu = _window->findChild<QObject *>("toolGroupMenu");
    if (!require(toolMenu, "grouped toolbar menu exists"))
        return false;
    QMetaObject::invokeMethod(_window, "openToolGroupMenu", Q_ARG(QVariant, "shape"),
                              Q_ARG(QVariant, QVariant()));
    QTest::qWait(100);
    if (!require(toolMenu->property("visible").toBool() && toolMenu->property("count").toInt() == 7,
                 "right click exposes implemented and NYI tools in a toolbar group"))
        return false;
    QMetaObject::invokeMethod(toolMenu, "close");
    QTest::keyClick(_window, Qt::Key_L);
    QTest::qWait(30);
    if (!require(_document.tool() == "lasso", "L activates the current lasso-group tool"))
        return false;
    _window->setProperty("spacePressed", true);
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "selection"),
                              Q_ARG(QVariant, "select"));
    QTest::mouseMove(_window,
                     area->mapToScene(QPointF(area->width() / 2, area->height() / 2)).toPoint());
    QTest::qWait(30);
    if (!require(
            !_window->property("spacePressed").toBool() && !panArea->property("enabled").toBool() &&
                panArea->property("cursorShape").toInt() == int(Qt::CrossCursor) &&
                area->property("cursorShape").toInt() == int(Qt::CrossCursor),
            "choosing a selection tool leaves hand navigation and restores one native crosshair"))
        return false;
    QMetaObject::invokeMethod(_window, "openToolGroupMenu", Q_ARG(QVariant, "lasso"),
                              Q_ARG(QVariant, QVariant()));
    QTest::qWait(100);
    if (!require(toolMenu->property("visible").toBool() && toolMenu->property("count").toInt() == 4,
                 "lasso menu lists its two tools and the two NYI entries"))
        return false;
    QMetaObject::invokeMethod(toolMenu, "close");
    QTest::keyClick(_window, Qt::Key_W);
    QTest::qWait(30);
    if (!require(_document.tool() == "quickSelection",
                 "W activates the current smart-selection tool"))
        return false;
    QMetaObject::invokeMethod(_window, "openToolGroupMenu", Q_ARG(QVariant, "smartSelection"),
                              Q_ARG(QVariant, QVariant()));
    QTest::qWait(100);
    if (!require(toolMenu->property("visible").toBool() && toolMenu->property("count").toInt() == 3,
                 "smart-selection menu keeps object selection visible as NYI"))
        return false;
    QMetaObject::invokeMethod(toolMenu, "close");
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "smartSelection"),
                              Q_ARG(QVariant, "magicWand"));
    QTest::qWait(60);
    auto imageCanvas              = _window->findChild<QObject *>("imageCanvas");
    auto selectionTolerance       = _window->findChild<QObject *>("selectionTolerance");
    auto selectionContiguous      = _window->findChild<QObject *>("selectionContiguous");
    auto selectionSampleAllLayers = _window->findChild<QObject *>("selectionSampleAllLayers");
    if (!require(imageCanvas && QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor &&
                     selectionTolerance && selectionContiguous && selectionSampleAllLayers &&
                     selectionTolerance->property("visible").toBool() &&
                     selectionContiguous->property("visible").toBool() &&
                     selectionSampleAllLayers->property("visible").toBool(),
                 "magic wand exposes tolerance, contiguous pixels and all-layer sampling"))
        return false;
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "eraser"),
                              Q_ARG(QVariant, "magicEraser"));
    QTest::qWait(60);
    auto magicTolerance  = _window->findChild<QObject *>("magicEraserTolerance");
    auto magicContiguous = _window->findChild<QObject *>("magicEraserContiguous");
    auto magicAntialias  = _window->findChild<QObject *>("magicEraserAntialias");
    auto magicSampleAll  = _window->findChild<QObject *>("magicEraserSampleAllLayers");
    if (!require(
            _document.tool() == "magicEraser" && magicTolerance && magicContiguous &&
                magicAntialias && magicSampleAll && magicTolerance->property("visible").toBool() &&
                magicContiguous->property("visible").toBool() &&
                magicAntialias->property("visible").toBool() &&
                magicSampleAll->property("visible").toBool() && QGuiApplication::overrideCursor() &&
                QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor,
            "Magic Eraser exposes Adobe options and a dedicated native cursor"))
        return false;
    QTest::keyClick(_window, Qt::Key_E, Qt::ShiftModifier);
    QTest::qWait(30);
    if (!require(_document.tool() == "eraser", "Shift+E cycles past the NYI background eraser"))
        return false;
    QTest::keyClick(_window, Qt::Key_E, Qt::ShiftModifier);
    QTest::qWait(30);
    if (!require(_document.tool() == "magicEraser", "Shift+E returns to the Magic Eraser"))
        return false;
    const int beforeMagicErase = _document.historyIndex();
    _document.setMagicEraserAntialias(false);
    _document.beginStroke(4, 4);
    if (!require(_document.magicEraserBusy(),
                 "Magic Eraser schedules work without blocking the UI"))
        return false;
    QTest::keyClick(_window, Qt::Key_Escape);
    if (!require(!_document.magicEraserBusy() && _document.historyIndex() == beforeMagicErase,
                 "Escape cancels Magic Eraser without creating history"))
        return false;
    QTest::keyClick(_window, Qt::Key_S);
    QTest::qWait(60);
    auto stampAligned    = _window->findChild<QObject *>("stampAligned");
    auto cloneSampleMode = _window->findChild<QObject *>("cloneSampleMode");
    if (!require(_document.tool() == "cloneStamp" && stampAligned && cloneSampleMode &&
                     stampAligned->property("visible").toBool() &&
                     cloneSampleMode->property("visible").toBool() &&
                     QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor,
                 "S activates clone stamp with alignment, sampling and a native brush cursor"))
        return false;
    const int    beforeCloneSource = _document.historyIndex();
    const QPoint sourceClick =
        area->mapToScene(QPointF(area->width() / 2, area->height() / 2)).toPoint();
    QTest::mouseClick(_window, Qt::LeftButton, Qt::AltModifier, sourceClick);
    QTest::qWait(40);
    if (!require(_document.cloneSourceValid() && _document.historyIndex() == beforeCloneSource,
                 "Option/Alt click picks the clone source without painting or adding history"))
        return false;
    QMetaObject::invokeMethod(_window, "openToolGroupMenu", Q_ARG(QVariant, "stamp"),
                              Q_ARG(QVariant, QVariant()));
    QTest::qWait(60);
    if (!require(toolMenu->property("count").toInt() == 2,
                 "stamp toolbar slot contains clone and pattern stamp"))
        return false;
    QMetaObject::invokeMethod(toolMenu, "close");
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "stamp"),
                              Q_ARG(QVariant, "patternStamp"));
    QTest::qWait(60);
    auto stampPattern       = _window->findChild<QObject *>("stampPattern");
    auto defineStampPattern = _window->findChild<QObject *>("defineStampPattern");
    if (!require(stampPattern && defineStampPattern && stampPattern->property("visible").toBool() &&
                     defineStampPattern->property("visible").toBool() &&
                     !cloneSampleMode->property("visible").toBool(),
                 "pattern stamp exposes pattern selection and definition"))
        return false;
    QMetaObject::invokeMethod(_window, "openToolGroupMenu", Q_ARG(QVariant, "blur"),
                              Q_ARG(QVariant, QVariant()));
    QTest::qWait(60);
    if (!require(toolMenu->property("count").toInt() == 3,
                 "retouch slot contains Blur, Sharpen and Smudge"))
        return false;
    QMetaObject::invokeMethod(toolMenu, "close");
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "blur"),
                              Q_ARG(QVariant, "smudge"));
    QTest::qWait(60);
    auto smudgeStrength  = _window->findChild<QObject *>("retouchStrength");
    auto smudgeSampleAll = _window->findChild<QObject *>("smudgeSampleAllLayers");
    auto smudgeFinger    = _window->findChild<QObject *>("smudgeFingerPainting");
    auto smudgeBlend     = _window->findChild<QObject *>("smudgeBlendMode");
    if (!require(_document.tool() == "smudge" && smudgeStrength && smudgeSampleAll &&
                     smudgeFinger && smudgeBlend && smudgeStrength->property("visible").toBool() &&
                     smudgeSampleAll->property("visible").toBool() &&
                     smudgeFinger->property("visible").toBool() &&
                     smudgeBlend->property("visible").toBool() &&
                     QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor,
                 "Smudge exposes strength, sampling, finger painting and one native brush cursor"))
        return false;
    _document.setColor(Qt::red);
    _document.setToolStrength(1);
    const int beforeSmudge = _document.historyIndex();
    QTest::mouseClick(_window, Qt::LeftButton, Qt::AltModifier, sourceClick);
    QTest::qWait(40);
    if (!require(_document.historyIndex() == beforeSmudge + 1 && !_document.smudgeFingerPainting(),
                 "Option/Alt temporarily enables finger painting without changing its setting"))
        return false;
    _document.undo();
    _document.setToolStrength(.5);
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "brush"),
                              Q_ARG(QVariant, "colorReplacement"));
    QTest::qWait(60);
    for (const char *name : {"replacementMode", "replacementSampling", "replacementLimits",
                             "replacementTolerance", "replacementAntialias"}) {
        auto control = _window->findChild<QObject *>(name);
        if (!require(control && control->property("visible").toBool(),
                     "Color Replacement exposes its specialised controls"))
            return false;
    }
    if (!require(_document.tool() == "colorReplacement" && QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor,
                 "Color Replacement is enabled in group B with a single native brush cursor"))
        return false;
    const int beforeReplacementPick = _document.historyIndex();
    QTest::mouseClick(_window, Qt::LeftButton, Qt::AltModifier, sourceClick);
    QTest::qWait(40);
    if (!require(_document.historyIndex() == beforeReplacementPick,
                 "Option/Alt samples colour without creating a paint stroke"))
        return false;
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "brush"),
                              Q_ARG(QVariant, "mixerBrush"));
    QTest::qWait(60);
    auto mixerOptions = _window->findChild<QObject *>("mixerBrushOptions");
    if (!require(_document.tool() == "mixerBrush" && mixerOptions &&
                     mixerOptions->property("visible").toBool(),
                 "Mixer Brush is available in group B"))
        return false;
    // Repeater delegates belong to the visual tree, which can differ from the
    // QObject ownership tree for dynamically created controls.
    const std::function<QQuickItem *(QQuickItem *, const QString &)> findVisualItem =
        [&](QQuickItem *_parent, const QString &_name) -> QQuickItem * {
        if (_parent->objectName() == _name)
            return _parent;
        for (auto *child : _parent->childItems())
            if (auto *found = findVisualItem(child, _name))
                return found;
        return nullptr;
    };
    for (const char *name : {"mixerWet", "mixerLoad", "mixerMix", "mixerFlow", "mixerLoadSwatch",
                             "mixerPreset", "mixerSampleAllLayers"}) {
        auto control = findVisualItem(_window->contentItem(), QString::fromLatin1(name));
        if (!require(control && control->property("visible").toBool(),
                     "Mixer Brush exposes its paint reservoir and mixing controls"))
            return false;
    }
    auto mixerMenu = _window->findChild<QObject *>("mixerLoadMenu");
    if (!require(mixerMenu, "Mixer reservoir menu exists"))
        return false;
    QMetaObject::invokeMethod(mixerMenu, "open");
    QTest::qWait(40);
    auto cleanAction = _window->findChild<QObject *>("mixerCleanAction");
    auto loadAction  = _window->findChild<QObject *>("mixerLoadAction");
    if (!require(mixerMenu && cleanAction && loadAction && mixerMenu->property("visible").toBool(),
                 "Mixer reservoir menu offers load and clean"))
        return false;
    QMetaObject::invokeMethod(cleanAction, "triggered");
    if (!require(!_document.mixerLoaded(), "Clean removes paint from the reservoir"))
        return false;
    QMetaObject::invokeMethod(loadAction, "triggered");
    QMetaObject::invokeMethod(mixerMenu, "close");
    QTest::qWait(40);
    const int beforeMixerPick = _document.historyIndex();
    QTest::keyPress(_window, Qt::Key_Alt);
    QTest::mouseClick(_window, Qt::LeftButton, Qt::AltModifier, sourceClick);
    QTest::keyRelease(_window, Qt::Key_Alt);
    QTest::qWait(40);
    if (!require(_document.historyIndex() == beforeMixerPick && _document.mixerLoaded(),
                 "Option/Alt loads Mixer Brush paint without drawing"))
        return false;
    QTest::keyClick(_window, Qt::Key_B);
    QTest::mouseMove(_window, sourceClick + QPoint(1, 1));
    QTest::qWait(40);
    if (!require(_document.tool() == "mixerBrush" && QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor,
                 "B restores Mixer Brush with one native brush cursor"))
        return false;
    QTest::keyClick(_window, Qt::Key_J);
    QTest::mouseMove(_window, sourceClick + QPoint(2, 2));
    QTest::qWait(60);
    auto redEyeOptions = _window->findChild<QObject *>("redEyeOptions");
    auto redEyePupil   = _window->findChild<QObject *>("redEyePupilSize");
    auto redEyeDarken  = _window->findChild<QObject *>("redEyeDarken");
    auto redEyeOverlay = _window->findChild<QObject *>("redEyeRegionOverlay");
    if (!require(_document.tool() == "redEye" && redEyeOptions && redEyePupil && redEyeDarken &&
                     redEyeOverlay && redEyeOptions->property("visible").toBool() &&
                     redEyePupil->property("value").toInt() == 50 &&
                     redEyeDarken->property("value").toInt() == 50 &&
                     QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::CrossCursor,
                 "J selects Red Eye with pupil size, darkening and one native crosshair"))
        return false;
    QMetaObject::invokeMethod(_window, "openToolGroupMenu", Q_ARG(QVariant, "healing"),
                              Q_ARG(QVariant, QVariant()));
    QTest::qWait(40);
    if (!require(toolMenu->property("count").toInt() == 6,
                 "correction group lists its six tools, including the remaining NYI tools"))
        return false;
    QMetaObject::invokeMethod(toolMenu, "close");
    _document.deselect();
    const qreal  savedSize = _document.brushSize(), savedHardness = _document.brushHardness(),
                 savedSmoothing = _document.brushSmoothing();
    const QColor savedColour    = _document.color();
    _document.setTool("brush");
    _document.setColor(QColor(220, 60, 40));
    _document.setBrushSize(16);
    _document.setBrushHardness(1);
    _document.setBrushSmoothing(0);
    _document.beginStroke(_document.width() / 2., _document.height() / 2.);
    _document.endStroke();
    _document.setTool("redEye");
    const int redEyeHistory = _document.historyIndex();
    QTest::qWait(60); // specialised options row changes the canvas position
    const QPoint eyeClick =
        area->mapToScene(QPointF(area->width() / 2, area->height() / 2)).toPoint();
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, eyeClick);
    QTest::qWait(40);
    const QColor correctedEye = _document.layerImage(_document.activeLayer())
                                    .pixelColor(_document.width() / 2, _document.height() / 2);
    if (!require(_document.historyIndex() == redEyeHistory + 1 &&
                     correctedEye.red() == correctedEye.green() &&
                     correctedEye.green() == correctedEye.blue(),
                 "canvas click corrects the red pupil and creates one undo step"))
        return false;
    _document.undo();
    const qreal  redEyeZoom = _window->property("zoom").toReal();
    const QPoint eyeFrom    = area->mapToScene(QPointF(area->width() / 2 - 20 * redEyeZoom,
                                                       area->height() / 2 - 20 * redEyeZoom))
                                  .toPoint();
    const QPoint eyeTo      = area->mapToScene(QPointF(area->width() / 2 + 20 * redEyeZoom,
                                                       area->height() / 2 + 20 * redEyeZoom))
                                  .toPoint();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, eyeFrom);
    QTest::mouseMove(_window, eyeTo);
    QTest::qWait(40);
    if (!require(redEyeOverlay->property("visible").toBool() &&
                     _document.historyIndex() == redEyeHistory,
                 "Red Eye drag shows its search box without editing before release"))
        return false;
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, eyeTo);
    QTest::qWait(40);
    if (!require(!redEyeOverlay->property("visible").toBool() &&
                     _document.historyIndex() == redEyeHistory + 1,
                 "Red Eye rectangle disappears after correction at release"))
        return false;
    _document.undo();
    _document.undo(); // restore the document before the synthetic red pupil
    _document.setBrushSize(savedSize);
    _document.setBrushHardness(savedHardness);
    _document.setBrushSmoothing(savedSmoothing);
    _document.setColor(savedColour);
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "healing"),
                              Q_ARG(QVariant, "healing"));
    QTest::qWait(60);
    auto healingOptions = _window->findChild<QObject *>("healingBrushOptions");
    for (const char *name : {"healingAligned", "healingSampleMode", "healingDiffusion",
                             "healingMode", "healingIgnoreAdjustments"}) {
        auto control = _window->findChild<QObject *>(name);
        if (!require(control && control->property("visible").toBool(),
                     "Healing Brush exposes alignment, sample layers, diffusion and modes"))
            return false;
    }
    if (!require(healingOptions && healingOptions->property("visible").toBool(),
                 "Healing Brush is enabled in the correction group"))
        return false;
    _document.setColor(QColor(160, 160, 160));
    _document.fillWithColor(true);
    _document.setTool("brush");
    _document.setBrushSize(6);
    _document.setBrushHardness(1);
    _document.setColor(Qt::red);
    _document.beginStroke(_document.width() / 2., _document.height() / 2.);
    _document.endStroke();
    _document.setTool("healing");
    _document.setBrushSize(24);
    _document.setBrushSmoothing(0);
    QTest::qWait(60);
    const qreal  healingZoom = _window->property("zoom").toReal();
    const QPoint healingSourcePoint =
        area->mapToScene(QPointF(area->width() / 2 - 40 * healingZoom, area->height() / 2))
            .toPoint();
    const int     beforeHealingSource      = _document.historyIndex();
    const QPointF cloneSourceBeforeHealing = _document.cloneSource();
    QTest::mouseMove(_window, healingSourcePoint);
    QTest::keyPress(_window, Qt::Key_Alt);
    QTest::qWait(30);
    if (!require(QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::CrossCursor,
                 "Option/Alt changes the Healing Brush cursor to a single source target"))
        return false;
    QTest::mouseClick(_window, Qt::LeftButton, Qt::AltModifier, healingSourcePoint);
    QTest::keyRelease(_window, Qt::Key_Alt);
    QTest::qWait(40);
    if (!require(_document.healingSourceValid() &&
                     _document.historyIndex() == beforeHealingSource &&
                     _document.cloneSource() == cloneSourceBeforeHealing,
                 "Healing source sampling creates no undo and does not change clone source"))
        return false;
    QTest::keyClick(_window, Qt::Key_J);
    const QPoint healingPoint =
        area->mapToScene(QPointF(area->width() / 2, area->height() / 2)).toPoint();
    QTest::mouseMove(_window, healingPoint);
    QTest::qWait(40);
    if (!require(_document.tool() == "healing" && QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor,
                 "J restores the chosen Healing Brush with one native brush cursor"))
        return false;
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, healingPoint);
    QTest::qWait(40);
    const QColor healedPixel = _document.layerImage(_document.activeLayer())
                                   .pixelColor(_document.width() / 2, _document.height() / 2);
    if (!require(_document.historyIndex() == beforeHealingSource + 1 &&
                     qAbs(healedPixel.red() - 160) <= 2 && qAbs(healedPixel.green() - 160) <= 2 &&
                     qAbs(healedPixel.blue() - 160) <= 2,
                 "Healing click removes a defect while adapting to surrounding grey"))
        return false;
    _document.undo();
    _document.undo();
    _document.undo();
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "healing"),
                              Q_ARG(QVariant, "spotHealing"));
    QTest::qWait(60);
    auto spotOptions = _window->findChild<QObject *>("spotHealingOptions");
    auto spotMode    = _window->findChild<QObject *>("spotHealingMode");
    auto spotLayers  = _window->findChild<QObject *>("spotHealingSampleAllLayers");
    if (!require(spotOptions && spotOptions->property("visible").toBool() && spotMode &&
                     spotMode->property("count").toInt() == 8 && spotLayers,
                 "Spot Healing Brush has its own options in the correction group"))
        return false;
    _document.setColor(QColor(160, 160, 160));
    _document.fillWithColor(true);
    _document.setTool("brush");
    _document.setBrushSize(6);
    _document.setColor(Qt::black);
    _document.beginStroke(_document.width() / 2, _document.height() / 2);
    _document.endStroke();
    _document.setTool("spotHealing");
    _document.setBrushSize(24);
    _document.setSpotHealingSampleAllLayers(true);
    QTest::qWait(30);
    if (!require(spotLayers->property("checked").toBool(),
                 "Spot Healing all-layer sampling is bound to its own setting"))
        return false;
    _document.setSpotHealingSampleAllLayers(false);
    const int     beforeSpot            = _document.historyIndex();
    const QPointF previousHealingOffset = _document.healingOffset();
    QTest::keyClick(_window, Qt::Key_J);
    QTest::mouseMove(_window, healingPoint);
    QTest::qWait(40);
    if (!require(_document.tool() == "spotHealing" && QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor,
                 "J recalls Spot Healing with a single native brush cursor"))
        return false;
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, healingPoint);
    QTest::qWait(40);
    const QColor spotPixel = _document.layerImage(_document.activeLayer())
                                 .pixelColor(_document.width() / 2, _document.height() / 2);
    if (!require(_document.historyIndex() == beforeSpot + 1 && qAbs(spotPixel.red() - 160) <= 2 &&
                     qAbs(spotPixel.green() - 160) <= 2 && qAbs(spotPixel.blue() - 160) <= 2 &&
                     _document.healingOffset() == previousHealingOffset,
                 "Spot Healing repairs a blemish without sampling or changing Healing source"))
        return false;
    _document.undo();
    _document.undo();
    _document.undo();
    _document.setColor(savedColour);
    _document.setBrushSize(savedSize);
    _document.setBrushHardness(savedHardness);
    _document.setBrushSmoothing(savedSmoothing);
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "healing"),
                              Q_ARG(QVariant, "patch"));
    QTest::qWait(60);
    auto patchOptions     = _window->findChild<QObject *>("patchOptions");
    auto patchSource      = _window->findChild<QObject *>("patchSource");
    auto patchDestination = _window->findChild<QObject *>("patchDestination");
    auto patchDiffusion   = _window->findChild<QObject *>("patchDiffusion");
    if (!require(_document.tool() == "patch" && patchOptions &&
                     patchOptions->property("visible").toBool() && patchSource &&
                     patchDestination && patchDiffusion &&
                     patchSource->property("checked").toBool(),
                 "Patch has Normal Source/Destination and Diffusion controls"))
        return false;
    _document.deselect();
    const qreal   patchZoom = _window->property("zoom").toReal();
    const QPointF patchCentre(_document.width() / 2., _document.height() / 2.);
    const auto    patchPoint = [&](qreal _dx, qreal _dy) {
        return area->mapToScene((patchCentre + QPointF(_dx, _dy)) * patchZoom).toPoint();
    };
    const int beforePatch = _document.historyIndex();
    QTest::keyClick(_window, Qt::Key_J);
    QTest::mouseMove(_window, patchPoint(-15, -15));
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(-15, -15));
    for (const QPoint offset : {QPoint(15, -15), QPoint(15, 15), QPoint(-15, 15), QPoint(-15, -15)})
        QTest::mouseMove(_window, patchPoint(offset.x(), offset.y()));
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(-15, -15));
    QTest::qWait(30);
    if (!require(_document.hasSelection() && _document.historyIndex() == beforePatch,
                 "Patch traces a selection without changing pixels or history"))
        return false;
    const QRegion patchSelection = _document.selectionRegion();
    QTest::mouseMove(_window, patchPoint(0, 0));
    QTest::qWait(20);
    if (!require(QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::SizeAllCursor,
                 "Patch uses one native move cursor inside its selection"))
        return false;
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(0, 0));
    QTest::mouseMove(_window, patchPoint(-50, 0));
    QTest::qWait(20);
    if (!require(_document.patchDragging() && !_document.patchPreviewImage().isNull() &&
                     _document.historyIndex() == beforePatch,
                 "Patch drag shows donor texture before committing pixels"))
        return false;
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(-50, 0));
    QTest::qWait(30);
    if (!require(!_document.patchDragging() && _document.patchPreviewImage().isNull() &&
                     _document.selectionRegion() == patchSelection,
                 "Patch release restores the repair selection and clears its preview"))
        return false;
    // This uniform donor can be a no-op; undo only when the repair changes pixels.
    if (_document.historyIndex() > beforePatch)
        _document.undo();
    auto destinationButton = qobject_cast<QQuickItem *>(patchDestination);
    QTest::mouseClick(
        _window, Qt::LeftButton, Qt::NoModifier,
        destinationButton
            ->mapToScene(QPointF(destinationButton->width() / 2, destinationButton->height() / 2))
            .toPoint());
    QTest::qWait(20);
    if (!require(_document.patchMode() == 1 && patchDestination->property("checked").toBool(),
                 "Patch destination option updates the document"))
        return false;
    _document.setPatchMode(0);
    _document.deselect();
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "healing"),
                              Q_ARG(QVariant, "contentAwareMove"));
    QTest::qWait(40);
    auto contentMoveOptions   = _window->findChild<QObject *>("contentAwareMoveOptions");
    auto contentMoveMode      = _window->findChild<QObject *>("contentMoveMode");
    auto contentMoveStructure = _window->findChild<QObject *>("contentMoveStructure");
    auto contentMoveColor     = _window->findChild<QObject *>("contentMoveColor");
    auto contentMoveLayers    = _window->findChild<QObject *>("contentMoveAllLayers");
    if (!require(_document.tool() == "contentAwareMove" && contentMoveOptions &&
                     contentMoveOptions->property("visible").toBool() && contentMoveMode &&
                     contentMoveMode->property("count").toInt() == 2 && contentMoveStructure &&
                     contentMoveColor && contentMoveLayers,
                 "Content-Aware Move has Move/Extend, Structure, Color and sampling controls"))
        return false;
    _document.setColor(QColor(165, 165, 165));
    _document.fillWithColor(true);
    _document.setTool("brush");
    _document.setBrushHardness(1);
    _document.setBrushSize(8);
    _document.setBrushSmoothing(0);
    _document.setColor(Qt::red);
    _document.beginStroke(patchCentre.x(), patchCentre.y());
    _document.endStroke();
    _document.setTool("contentAwareMove");
    QTest::qWait(20);
    QTest::keyClick(_window, Qt::Key_J);
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(-15, -15));
    for (const QPoint offset : {QPoint(15, -15), QPoint(15, 15), QPoint(-15, 15), QPoint(-15, -15)})
        QTest::mouseMove(_window, patchPoint(offset.x(), offset.y()));
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(-15, -15));
    const int beforeContentMove = _document.historyIndex();
    QTest::mouseMove(_window, patchPoint(0, 0));
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(0, 0));
    QTest::mouseMove(_window, patchPoint(-60, 0));
    if (!require(_document.patchDragging() && QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::SizeAllCursor,
                 "Content-Aware Move keeps one native move cursor and a live object preview"))
        return false;
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(-60, 0));
    QElapsedTimer contentMoveWait;
    contentMoveWait.start();
    while ((_document.contentMoveBusy() || _document.patchDragging()) &&
           contentMoveWait.elapsed() < 5000)
        QTest::qWait(10);
    const QColor oldObject =
        _document.layerImage(_document.activeLayer()).pixelColor(patchCentre.toPoint());
    const QColor movedObject = _document.layerImage(_document.activeLayer())
                                   .pixelColor((patchCentre + QPointF(-60, 0)).toPoint());
    if (!require(!_document.contentMoveBusy() &&
                     _document.historyIndex() == beforeContentMove + 1 &&
                     qAbs(oldObject.red() - 165) <= 3 && movedObject.red() > 240 &&
                     movedObject.green() < 10,
                 "Content-Aware Move repairs the hole asynchronously and commits one undo"))
        return false;
    _document.undo();
    const int     beforeCancelledMove = _document.historyIndex();
    const QRegion contentSelection    = _document.selectionRegion();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(0, 0));
    QTest::mouseMove(_window, patchPoint(-60, 0));
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(-60, 0));
    QTest::keyClick(_window, Qt::Key_Escape, Qt::NoModifier, 0);
    QTest::qWait(40);
    if (!require(!_document.contentMoveBusy() && _document.historyIndex() == beforeCancelledMove &&
                     _document.selectionRegion() == contentSelection,
                 "Escape cancels a pending content move and restores its selection without undo"))
        return false;
    _document.deselect();
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "healing"),
                              Q_ARG(QVariant, "remove"));
    QTest::qWait(40);
    auto removeOptions   = _window->findChild<QObject *>("removeOptions");
    auto removeAutomatic = _window->findChild<QObject *>("removeAutomatic");
    auto removeApply     = _window->findChild<QObject *>("removeApply");
    auto removeLayers    = _window->findChild<QObject *>("removeAllLayers");
    if (!require(
            _document.tool() == "remove" && removeOptions &&
                removeOptions->property("visible").toBool() && removeAutomatic && removeApply &&
                removeLayers,
            "Remove is available in the J group with automatic, manual and all-layer controls"))
        return false;
    _document.setRemoveAfterEachStroke(false);
    _document.setBrushSize(28);
    QTest::mouseMove(_window, patchPoint(0, 0));
    QTest::keyClick(_window, Qt::Key_J);
    const int beforeRemoval = _document.historyIndex();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(0, 0));
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(0, 0));
    if (!require(_document.removeHasPending() && !_document.removeBusy() &&
                     _document.removeRegion().contains(patchCentre.toPoint()) &&
                     _document.historyIndex() == beforeRemoval &&
                     QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor,
                 "Remove paints a pending overlay and uses one native brush cursor"))
        return false;
    QTest::keyClick(_window, Qt::Key_Return);
    QElapsedTimer removalWait;
    removalWait.start();
    while (_document.removeBusy() && removalWait.elapsed() < 5000)
        QTest::qWait(10);
    if (!require(!_document.removeBusy() && !_document.removeHasPending() &&
                     _document.historyIndex() == beforeRemoval + 1 &&
                     _document.layerImage(_document.activeLayer())
                             .pixelColor(patchCentre.toPoint())
                             .green() > 150,
                 "Return applies the pending removal asynchronously with one undo"))
        return false;
    _document.undo();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(0, 0));
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(0, 0));
    QTest::keyClick(_window, Qt::Key_Escape, Qt::NoModifier, 0);
    if (!require(!_document.removeHasPending() && !_document.removeBusy() &&
                     _document.historyIndex() == beforeRemoval,
                 "Escape discards Remove marks without changing pixels or history"))
        return false;
    _document.setRemoveAfterEachStroke(true);
    _document.undo();
    _document.undo();
    _document.deselect();
    _document.setColor(savedColour);
    _document.setBrushSize(savedSize);
    _document.setBrushHardness(savedHardness);
    _document.setBrushSmoothing(savedSmoothing);
    _document.setColor(Qt::red);
    _document.fillWithColor(true);
    const int historySourceRow = _document.historyIndex();
    _document.setColor(Qt::blue);
    _document.fillWithColor(true);
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "historyBrush"),
                              Q_ARG(QVariant, "historyBrush"));
    QTest::qWait(50);
    auto historyOptions      = _window->findChild<QObject *>("historyBrushOptions");
    auto historySourcePicker = _window->findChild<QObject *>("historyBrushSourcePicker");
    auto historyMode         = _window->findChild<QObject *>("historyBrushBlend");
    if (!require(_document.tool() == "historyBrush" && historyOptions &&
                     historyOptions->property("visible").toBool() && historySourcePicker &&
                     historyMode,
                 "History Brush is available in group Y with source and blend controls"))
        return false;
    auto historyStateList = _window->findChild<QObject *>("historyStateList");
    if (!require(historyStateList, "History state list exists"))
        return false;
    QMetaObject::invokeMethod(historyStateList, "forceLayout");
    QMetaObject::invokeMethod(historyStateList, "positionViewAtIndex", Q_ARG(int, historySourceRow),
                              Q_ARG(int, 4));
    QTest::qWait(40);
    auto historySourceButton = findVisualItem(
        _window->contentItem(), QString("historyBrushSource%1").arg(historySourceRow));
    if (!require(historySourceButton, "History panel exposes source buttons in the left column"))
        return false;
    const int beforeHistoryBrush = _document.historyIndex();
    QMetaObject::invokeMethod(historySourceButton, "clicked");
    if (!require(
            _document.historyBrushSourceIndex() == historySourceRow &&
                _document.historyIndex() == beforeHistoryBrush &&
                _document.layerImage(_document.activeLayer()).pixelColor(patchCentre.toPoint()) ==
                    QColor(Qt::blue),
            "Choosing a history source does not undo or change the image"))
        return false;
    _document.setBrushSize(24);
    _document.setBrushHardness(1);
    _document.setBrushFlow(1);
    _document.setBrushOpacity(1);
    _document.setBrushBlend(0);
    _document.setBrushSmoothing(0);
    QTest::mouseMove(_window, patchPoint(0, 0));
    QTest::keyClick(_window, Qt::Key_Y);
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(0, 0));
    QTest::mouseMove(_window, patchPoint(25, 0));
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(25, 0));
    if (!require(
            _document.tool() == "historyBrush" &&
                _document.layerImage(_document.activeLayer()).pixelColor(patchCentre.toPoint()) ==
                    QColor(Qt::red) &&
                _document.layerImage(_document.activeLayer())
                        .pixelColor((patchCentre + QPointF(-50, 0)).toPoint()) ==
                    QColor(Qt::blue) &&
                _document.historyIndex() == beforeHistoryBrush + 1 &&
                QGuiApplication::overrideCursor() &&
                QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor,
            "Y paints the chosen source locally with one native brush cursor and one undo"))
        return false;
    _document.undo();
    if (!require(
            _document.historyBrushSourceIndex() == historySourceRow &&
                _document.layerImage(_document.activeLayer()).pixelColor(patchCentre.toPoint()) ==
                    QColor(Qt::blue),
            "Undo restores the pixels without changing the history source"))
        return false;
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "historyBrush"),
                              Q_ARG(QVariant, "artHistoryBrush"));
    QTest::qWait(50);
    auto artOptions       = _window->findChild<QObject *>("artHistoryBrushOptions");
    auto artStyle         = _window->findChild<QObject *>("artHistoryStyle");
    auto artArea          = _window->findChild<QObject *>("artHistoryArea");
    auto artTolerance     = _window->findChild<QObject *>("artHistoryTolerance");
    auto artCurrentSource = _window->findChild<QObject *>("artHistoryCurrentSource");
    if (!require(
            _document.tool() == "artHistoryBrush" && artOptions &&
                artOptions->property("visible").toBool() && artStyle &&
                artStyle->property("count").toInt() == 10 && artArea && artTolerance &&
                artCurrentSource,
            "Art History Brush exposes ten styles, area, tolerance and current-source shortcut"))
        return false;
    QMetaObject::invokeMethod(artCurrentSource, "clicked");
    if (!require(_document.historyBrushSourceIndex() == _document.historyIndex(),
                 "Current source shortcut captures the image without undoing"))
        return false;
    _document.setHistoryBrushSource(historySourceRow);
    _document.setArtHistoryStyle(5);
    _document.setArtHistoryArea(1);
    _document.setArtHistoryTolerance(0);
    _document.setBrushSize(40);
    const int beforeArtHistory = _document.historyIndex();
    QTest::keyClick(_window, Qt::Key_Y);
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(0, 0));
    QTest::mouseMove(_window, patchPoint(25, 0));
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(25, 0));
    if (!require(
            _document.tool() == "artHistoryBrush" &&
                _document.historyIndex() == beforeArtHistory + 1 &&
                _document.layerImage(_document.activeLayer()).pixelColor(patchCentre.toPoint()) ==
                    QColor(Qt::red) &&
                QGuiApplication::overrideCursor() &&
                QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor,
            "Group Y remembers the artistic tool and mouse painting uses its source and native "
            "cursor"))
        return false;
    _document.undo();
    _document.setArtHistoryStyle(0);
    _document.setArtHistoryArea(50);
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "measure"),
                              Q_ARG(QVariant, "picker"));
    QTest::qWait(50);
    auto eyedropperOptions = _window->findChild<QObject *>("eyedropperOptions");
    auto eyedropperSize    = _window->findChild<QObject *>("eyedropperSampleSize");
    auto eyedropperSource  = _window->findChild<QObject *>("eyedropperSampleSource");
    auto eyedropperRing    = _window->findChild<QObject *>("eyedropperShowRing");
    auto samplingRing      = _window->findChild<QObject *>("samplingRing");
    if (!require(_document.tool() == "picker" && eyedropperOptions &&
                     eyedropperOptions->property("visible").toBool() && eyedropperSize &&
                     eyedropperSize->property("count").toInt() == 7 && eyedropperSource &&
                     eyedropperRing && samplingRing,
                 "Eyedropper has Adobe-style sample size, layer source and sampling ring controls"))
        return false;
    if (!require(
            informationPanel->property("panelShown").toBool() &&
                informationPanel->property("floating").toBool() &&
                !informationPanel->property("visible").toBool() &&
                informationContent->window() != _window,
            "Selecting Eyedropper automatically opens Info in a floating window outside the dock"))
        return false;
    if (!require(_window->isActive() && QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor &&
                     QGuiApplication::overrideCursor()->hotSpot() == QPoint(3, 29),
                 "First Eyedropper activation installs its cursor without moving the mouse or "
                 "clicking again"))
        return false;
    _document.setEyedropperSize(1);
    _document.setEyedropperSource(0);
    _document.setEyedropperRing(true);
    _document.setColor(Qt::green);
    const int beforeSampling = _document.historyIndex();
    QTest::mouseMove(_window, patchPoint(0, 0));
    QTest::qWait(30);
    if (!require(informationContent->property("pointerValid").toBool() &&
                     informationContent->property("pointerColor").value<QColor>() ==
                         QColor(Qt::blue),
                 "Info updates RGB values under the pointer without a click"))
        return false;
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(0, 0));
    if (!require(
            samplingRing->property("visible").toBool() && _document.color() == QColor(Qt::blue) &&
                QGuiApplication::overrideCursor() &&
                QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor &&
                QGuiApplication::overrideCursor()->hotSpot() == QPoint(3, 29),
            "Eyedropper uses a single native cursor with an exact tip and a colour sampling ring"))
        return false;
    _document.setColor(Qt::green);
    QTest::mouseMove(_window, patchPoint(25, 0));
    if (!require(_document.color() == QColor(Qt::blue),
                 "Dragging the Eyedropper updates the colour continuously"))
        return false;
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(25, 0));
    _document.setColor(Qt::green);
    QTest::mouseClick(_window, Qt::LeftButton, Qt::AltModifier, patchPoint(0, 0));
    if (!require(_document.secondaryColor() == QColor(Qt::blue) &&
                     _document.color() == QColor(Qt::green) &&
                     !samplingRing->property("visible").toBool() &&
                     _document.historyIndex() == beforeSampling,
                 "Option samples the background colour without changing pixels or history"))
        return false;
    _window->setProperty("colorSamples", QVariantList{});
    QTest::mouseClick(_window, Qt::LeftButton, Qt::ShiftModifier, patchPoint(0, 0));
    if (!require(_window->property("colorSamples").toList().size() == 1 &&
                     _document.color() == QColor(Qt::green),
                 "Shift-click with Eyedropper places a numbered sampler without changing the paint "
                 "colour"))
        return false;
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "measure"),
                              Q_ARG(QVariant, "colorSampler"));
    QTest::qWait(30);
    QTest::mouseMove(_window, patchPoint(0, 0));
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(0, 0));
    QTest::mouseMove(_window, patchPoint(25, 0));
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(25, 0));
    const QVariantList movedSamples = _window->property("colorSamples").toList();
    if (!require(informationContent->property("samples").toList().size() == 1 &&
                     eyedropperOptions->property("implicitHeight").toInt() == 44,
                 "Sampler readouts live in Info while its options bar remains compact"))
        return false;
    if (!require(movedSamples.size() == 1 &&
                     movedSamples[0].toMap().value("x").toReal() > patchCentre.x() + 20 &&
                     QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor,
                 "Colour Sampler moves an existing numbered target instead of adding another"))
        return false;
    _document.setColor(Qt::red);
    _document.fillWithColor(true);
    const auto sampleMatches = [&](const QColor &_color) {
        const auto samples = _window->property("colorSamples").toList();
        return samples.size() == 1 && samples[0].toMap().value("color").value<QColor>() == _color;
    };
    if (!require(QTest::qWaitFor([&] { return sampleMatches(Qt::red); }, 2000),
                 "Sampler readouts follow image changes"))
        return false;
    _document.undo();
    if (!require(QTest::qWaitFor([&] { return sampleMatches(Qt::blue); }, 2000),
                 "Undo updates a sampler without deleting its non-destructive marker"))
        return false;
    QTest::mouseClick(_window, Qt::LeftButton, Qt::AltModifier, patchPoint(25, 0));
    if (!require(_window->property("colorSamples").toList().isEmpty(),
                 "Option-click deletes the targeted sampler"))
        return false;
    for (int i = 0; i < 5; ++i)
        QMetaObject::invokeMethod(_window, "addColorSample",
                                  Q_ARG(QVariant, patchCentre.x() + i * 20),
                                  Q_ARG(QVariant, patchCentre.y()));
    if (!require(_window->property("colorSamples").toList().size() == 4,
                 "A fifth sample does not silently replace an existing one"))
        return false;
    auto clearSamplers = _window->findChild<QObject *>("clearColorSamplers");
    if (!require(clearSamplers, "Colour Sampler exposes Clear All"))
        return false;
    QMetaObject::invokeMethod(clearSamplers, "clicked");
    if (!require(_window->property("colorSamples").toList().isEmpty(),
                 "Clear All removes the sampler markers and readouts"))
        return false;
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "measure"),
                              Q_ARG(QVariant, "ruler"));
    QTest::qWait(30);
    QTest::mouseMove(_window, patchPoint(0, 0));
    auto rulerOptions = _window->findChild<QObject *>("rulerOptions");
    if (!require(
            rulerOptions && rulerOptions->property("visible").toBool() &&
                QGuiApplication::overrideCursor() &&
                QGuiApplication::overrideCursor()->hotSpot() == QPoint(8, 8),
            "Ruler has dedicated coordinate, angle and length controls and a measuring cursor"))
        return false;
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(0, 0));
    QTest::mouseMove(_window, patchPoint(30, 40));
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, patchPoint(30, 40));
    if (!require(informationContent->property("rulerVisible").toBool() &&
                     qAbs(informationContent->property("distance").toReal() - 50.0) < 1.5,
                 "Info displays live Ruler length from the actual measuring gesture"))
        return false;
    auto informationWindow = informationContent->window();
    informationWindow->close();
    QTest::qWait(20);
    if (!require(!informationPanel->property("panelShown").toBool() &&
                     _window->property("informationFloating").toBool(),
                 "Closing floating Info hides it without losing its floating placement"))
        return false;
    QMetaObject::invokeMethod(_window, "saveWorkspaceLayout");
    const QVariantMap hiddenInfoLayout = _workspaceSettings.layout();
    if (!require(!hiddenInfoLayout.value("visibility").toMap().value("information").toBool() &&
                     hiddenInfoLayout.value("panels")
                         .toMap()
                         .value("information")
                         .toMap()
                         .value("floating")
                         .toBool(),
                 "Hidden Info retains its floating preference in workspace settings"))
        return false;
    _window->setProperty("informationFloating", false);
    QMetaObject::invokeMethod(_window, "restoreWorkspaceLayout");
    QTest::qWait(30);
    QMetaObject::invokeMethod(_window, "showInformationPanel");
    QTest::qWait(20);
    if (!require(informationPanel->property("floating").toBool(),
                 "Restored Info reopens in its saved floating placement"))
        return false;
    QMetaObject::invokeMethod(informationPanel, "setPanelFloating", Q_ARG(QVariant, false));
    QMetaObject::invokeMethod(informationPanel, "floatingChangedByUser", Q_ARG(bool, false));
    QMetaObject::invokeMethod(_window, "groupDockPanels", Q_ARG(QVariant, "information"),
                              Q_ARG(QVariant, "histogram"));
    QMetaObject::invokeMethod(_window, "setDockPanelVisible", Q_ARG(QVariant, "information"),
                              Q_ARG(QVariant, false));
    QMetaObject::invokeMethod(_window, "showInformationPanel");
    QTest::qWait(20);
    if (!require(!informationPanel->property("floating").toBool() &&
                     informationPanel->property("visible").toBool() &&
                     informationContent->window() == _window &&
                     !histogramPanel->property("visible").toBool(),
                 "Auto-show selects the docked Info tab instead of creating a floating duplicate"))
        return false;
    QMetaObject::invokeMethod(_window, "ungroupDockPanel", Q_ARG(QVariant, "information"));
    QMetaObject::invokeMethod(_window, "setDockPanelVisible", Q_ARG(QVariant, "information"),
                              Q_ARG(QVariant, false));
    _window->setProperty("informationFloating", true);
    _window->requestActivate();
    QTest::qWait(20);
    _window->setProperty("rulerVisible", false);
    _document.setSecondaryColor(Qt::white);
    _document.undo();
    _document.undo();
    _document.setHistoryBrushSource(0);
    _document.setColor(savedColour);
    _document.setBrushSize(savedSize);
    _document.setBrushHardness(savedHardness);
    _document.setBrushSmoothing(savedSmoothing);
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "shape"),
                              Q_ARG(QVariant, "rectangle"));
    QTest::qWait(60);
    auto shapeFill        = _window->findChild<QObject *>("shapeFillEnabled");
    auto shapeStroke      = _window->findChild<QObject *>("shapeStrokeEnabled");
    auto shapeStrokeWidth = _window->findChild<QObject *>("shapeStrokeWidth");
    auto shapeStrokeStyle = _window->findChild<QObject *>("shapeStrokeStyle");
    auto shapeRadius      = _window->findChild<QObject *>("shapeCornerRadius");
    if (!require(shapeFill && shapeStroke && shapeStrokeWidth && shapeStrokeStyle && shapeRadius &&
                     shapeFill->property("visible").toBool() &&
                     shapeStroke->property("visible").toBool() &&
                     shapeRadius->property("visible").toBool(),
                 "rectangle exposes fill, stroke, stroke style and corner controls"))
        return false;
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "shape"),
                              Q_ARG(QVariant, "line"));
    QTest::qWait(60);
    if (!require(!shapeFill->property("visible").toBool() &&
                     shapeStroke->property("visible").toBool() &&
                     shapeStrokeWidth->property("visible").toBool() &&
                     shapeStrokeStyle->property("visible").toBool() &&
                     !shapeRadius->property("visible").toBool(),
                 "line exposes its stroke controls without rectangle-only controls"))
        return false;
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
    // The four C variants share one toolbar slot and operate through real pointer delivery.
    _document.newDocument(300, 200, true);
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "crop"),
                              Q_ARG(QVariant, "crop"));
    QTest::qWait(100);
    auto cropOptions = _window->findChild<QQuickItem *>("cropOptions");
    auto cropOverlay = _window->findChild<QQuickItem *>("cropOverlay");
    if (!require(cropOptions && cropOptions->isVisible() && cropOverlay,
                 "crop has a dedicated options bar and frame overlay"))
        return false;
    const auto cropScene = [&](qreal _x, qreal _y) {
        return area->mapToScene(QPointF(_x, _y) * _window->property("zoom").toReal()).toPoint();
    };
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, cropScene(20, 20));
    QTest::mouseMove(_window, cropScene(180, 140), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, cropScene(180, 140));
    if (!require(_document.hasCrop() && cropOverlay->isVisible() && !_document.hasSelection(),
                 "drawing a crop frame leaves the pixel selection separate"))
        return false;
    QTest::keyClick(_window, Qt::Key_Escape);
    if (!require(!_document.hasCrop() && _document.width() == 300,
                 "Escape discards a crop without touching image pixels"))
        return false;
    _document.resetCrop();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, cropScene(150, 100));
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, cropScene(150, 100));
    QTest::keyClick(_window, Qt::Key_Return);
    QElapsedTimer cropTimer;
    cropTimer.start();
    while (_document.cropBusy() && cropTimer.elapsed() < 5000)
        QTest::qWait(10);
    if (!require(!_document.cropBusy() && !_document.hasCrop() && _document.historyIndex() == 1,
                 "Return applies one asynchronous crop with a single undo step"))
        return false;
    _document.undo();
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "crop"),
                              Q_ARG(QVariant, "perspectiveCrop"));
    QTest::qWait(60);
    _document.beginCropGesture(20, 20, 0);
    _document.updateCropGesture(250, 170);
    _document.endCropGesture();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, cropScene(20, 20));
    QTest::mouseMove(_window, cropScene(40, 35), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, cropScene(40, 35));
    if (!require(_document.tool() == "perspectiveCrop" &&
                     qAbs(_document.cropPoints().first().toMap()["x"].toReal() - 40) < 1,
                 "perspective corner handles can be adjusted with the mouse"))
        return false;
    QTest::keyClick(_window, Qt::Key_Escape);
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "crop"),
                              Q_ARG(QVariant, "slice"));
    QTest::qWait(60);
    auto sliceOptions = _window->findChild<QQuickItem *>("sliceOptions");
    if (!require(sliceOptions && sliceOptions->isVisible(), "Slice has dedicated controls"))
        return false;
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, cropScene(30, 30));
    QTest::mouseMove(_window, cropScene(150, 120), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, cropScene(150, 120));
    if (!require(_document.slices().size() == 1, "dragging Slice creates a user slice"))
        return false;
    QMetaObject::invokeMethod(_window, "activateGroupTool", Q_ARG(QVariant, "crop"),
                              Q_ARG(QVariant, "sliceSelect"));
    QTest::qWait(60);
    QTest::keyClick(_window, Qt::Key_C);
    if (!require(_document.tool() == "sliceSelect", "C selects the group's last active variant"))
        return false;
    const auto sliceBeforeMove = _document.slices().first().toMap();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, cropScene(80, 70));
    QTest::mouseMove(_window, cropScene(90, 80), 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, cropScene(90, 80));
    if (!require(qAbs(_document.slices().first().toMap()["x"].toReal() -
                      sliceBeforeMove["x"].toReal() - 10) <= 1 &&
                     _document.width() == 300 && _document.height() == 200,
                 "Slice Select moves a slice without changing document dimensions"))
        return false;
    auto deleteSlice = _window->findChild<QObject *>("deleteSliceButton");
    QMetaObject::invokeMethod(deleteSlice, "clicked");
    if (!require(_document.slices().isEmpty(),
                 "slice deletion is wired to the dedicated options bar"))
        return false;
    _document.setTool("crop");
    _document.resetCrop();
    _window->setProperty("zoom", 8.0);
    QTest::qWait(40);
    if (!require(cropOverlay->width() <= viewport->width() + 2 &&
                     cropOverlay->height() <= viewport->height() + 2,
                 "crop overlay allocation stays limited to the visible viewport at high zoom"))
        return false;
    _window->setProperty("zoom", 1.0);
    _document.setTool("brush");
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
    auto layerProperties = _window->findChild<QObject *>("layerPropertiesDialog");
    if (!require(layerProperties, "layer properties dialog exists"))
        return false;
    auto      settingsLight              = _window->findChild<QObject *>("settingsLightMenuItem");
    const int layersBeforeSettingsAction = _document.layers().size();
    if (!require(settingsLight, "Settings menu exposes working adjustment actions"))
        return false;
    QMetaObject::invokeMethod(settingsLight, "triggered");
    QTest::qWait(120);
    if (!require(_document.layers().size() == layersBeforeSettingsAction + 1 &&
                     _document.layers().first().toMap().value("kind").toString() == "adjustment" &&
                     layerProperties->property("visible").toBool(),
                 "Settings menu creates an adjustment layer and opens its properties"))
        return false;
    layerProperties->setProperty("keepChanges", true);
    QMetaObject::invokeMethod(layerProperties, "accept");
    _document.removeLayer();
    _document.addAdjustmentLayer("brightnessContrast");
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
    auto propertiesActions = _window->findChild<QObject *>("layerPropertiesActions");
    auto resetButton       = _window->findChild<QObject *>("layerPropertiesResetButton");
    auto curveColorPicker  = _window->findChild<QObject *>("curveColorPicker");
    auto curveBlackPicker  = _window->findChild<QObject *>("curveBlackPicker");
    auto curveGrayPicker   = _window->findChild<QObject *>("curveGrayPicker");
    auto curveWhitePicker  = _window->findChild<QObject *>("curveWhitePicker");
    auto curvePickerArea   = _window->findChild<QObject *>("curvePickerArea");

    if (!require(propertiesActions && propertiesActions->property("visible").toBool() &&
                     resetButton && curveColorPicker && curveBlackPicker && curveGrayPicker &&
                     curveWhitePicker && curvePickerArea &&
                     curvePickerArea->property("cursorShape").toInt() == int(Qt::BlankCursor),
                 "adjustment properties expose shared actions and curve eyedroppers"))
        return false;
    _window->setProperty("curvePickerMode", "gray");
    QTest::qWait(20);
    auto         calibrationArea = qobject_cast<QQuickItem *>(curvePickerArea);
    const QPoint calibrationPoint =
        calibrationArea->mapToScene(QPointF(40, calibrationArea->height() / 2)).toPoint();
    QTest::mouseMove(_window, calibrationPoint);
    QTest::qWait(30);
    if (!require(QGuiApplication::overrideCursor() &&
                     QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor &&
                     QGuiApplication::overrideCursor()->hotSpot() == QPoint(3, 29) &&
                     !_window->findChild<QObject *>("curvePickerCursor"),
                 "Curve calibration shares the native eyedropper without a second canvas cursor"))
        return false;
    _window->setProperty("curvePickerMode", "");
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
    _document.addAdjustmentLayer("gradientMap");
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    auto gradientMapEditor  = _window->findChild<QObject *>("gradientMapEditor");
    auto gradientMapWheel   = _window->findChild<QObject *>("gradientMapWheel");
    auto gradientBrightness = _window->findChild<QObject *>("gradientMapBrightness");
    if (!require(gradientMapEditor && gradientMapEditor->property("visible").toBool() &&
                     gradientMapWheel && gradientBrightness,
                 "gradient map uses its color wheel and brightness editor"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "close");
    _document.addAdjustmentLayer("light");
    QTest::qWait(100);
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    auto lightEditor = _window->findChild<QObject *>("lightEditor");
    if (!require(lightEditor && lightEditor->property("visible").toBool() &&
                     lightEditor->property("controlCount").toInt() == 6,
                 "light adjustment exposes six dedicated sliders and numeric fields"))
        return false;
    auto exposureField = qvariant_cast<QObject *>(lightEditor->property("exposureField"));
    if (!require(exposureField, "light adjustment exposes its editable exposure field"))
        return false;
    QMetaObject::invokeMethod(exposureField, "forceActiveFocus");
    QTest::keyClick(_window, Qt::Key_Return);
    QTest::qWait(50);
    if (!require(layerProperties->property("opened").toBool(),
                 "enter validates a light value without closing layer properties"))
        return false;
    QTest::keyClick(_window, Qt::Key_Return);
    QTest::qWait(50);
    if (!require(!layerProperties->property("opened").toBool(),
                 "a second enter activates the default dialog action"))
        return false;
    _document.addAdjustmentLayer("solidColor");
    QTest::qWait(100);
    auto propertiesButton = _window->findChild<QObject *>("layerPropertiesButton");
    if (!require(propertiesButton && propertiesButton->property("enabled").toBool(),
                 "solid color fill enables layer properties"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    auto solidColorPicker = _window->findChild<QObject *>("solidColorPickerButton");
    if (!require(solidColorPicker && solidColorPicker->property("visible").toBool(),
                 "solid color fill exposes its color picker"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "close");
    _document.addAdjustmentLayer("blackWhite");
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    auto blackWhiteEditor = _window->findChild<QObject *>("blackWhiteEditor");
    auto blackWhitePreset = _window->findChild<QObject *>("blackWhitePreset");
    auto blackWhiteAuto   = _window->findChild<QObject *>("blackWhiteAutoButton");
    auto blackWhiteTint   = _window->findChild<QObject *>("blackWhiteTintCheck");
    if (!require(blackWhiteEditor && blackWhiteEditor->property("visible").toBool() &&
                     blackWhiteEditor->property("controlCount").toInt() == 6 && blackWhitePreset &&
                     blackWhiteAuto && blackWhiteTint,
                 "black-and-white adjustment exposes presets, auto, tint, and six families"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "close");
    _document.addAdjustmentLayer("photoFilter");
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    auto photoFilterEditor  = _window->findChild<QObject *>("photoFilterEditor");
    auto photoFilterPreset  = _window->findChild<QObject *>("photoFilterPreset");
    auto photoFilterColor   = _window->findChild<QObject *>("photoFilterColorRadio");
    auto photoFilterDensity = _window->findChild<QObject *>("photoFilterDensity");
    auto preserveLuminosity = _window->findChild<QObject *>("photoFilterPreserveLuminosity");
    if (!require(photoFilterEditor && photoFilterEditor->property("visible").toBool() &&
                     photoFilterPreset && photoFilterColor && photoFilterDensity &&
                     preserveLuminosity,
                 "photo filter exposes presets, custom color, density, and luminosity control"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "close");
    _document.addAdjustmentLayer("channelMixer");
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    auto channelMixerEditor = _window->findChild<QObject *>("channelMixerEditor");
    auto channelMixerPreset = _window->findChild<QObject *>("channelMixerPreset");
    auto channelMixerOutput = _window->findChild<QObject *>("channelMixerOutput");
    auto channelMixerTotal  = _window->findChild<QObject *>("channelMixerTotal");
    auto channelMixerMono   = _window->findChild<QObject *>("channelMixerMonochrome");
    if (!require(channelMixerEditor && channelMixerEditor->property("visible").toBool() &&
                     channelMixerPreset && channelMixerOutput && channelMixerTotal &&
                     channelMixerMono,
                 "channel mixer exposes presets, output channels, total, and monochrome mode"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "close");
    _document.addAdjustmentLayer("colorLookup");
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    auto colorLookupEditor   = _window->findChild<QObject *>("colorLookupEditor");
    auto colorLookupStrength = _window->findChild<QObject *>("colorLookupStrength");
    auto colorLookupPreserve = _window->findChild<QObject *>("colorLookupPreserveLuminosity");
    if (!require(colorLookupEditor && colorLookupEditor->property("visible").toBool() &&
                     colorLookupStrength && colorLookupPreserve,
                 "color lookup exposes named styles, strength, and luminosity control"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "close");
    _document.addAdjustmentLayer("selectiveColor");
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    auto selectiveColorEditor   = _window->findChild<QObject *>("selectiveColorEditor");
    auto selectiveColorFamilies = _window->findChild<QObject *>("selectiveColorFamilyGrid");
    auto selectiveColorRelative = _window->findChild<QObject *>("selectiveColorRelative");
    auto selectiveColorAbsolute = _window->findChild<QObject *>("selectiveColorAbsolute");
    if (!require(selectiveColorEditor && selectiveColorEditor->property("visible").toBool() &&
                     selectiveColorFamilies && selectiveColorRelative && selectiveColorAbsolute,
                 "selective color exposes nine families, four controls, and two methods"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "close");
    _document.addAdjustmentLayer("posterize");
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    auto posterizeEditor = _window->findChild<QObject *>("posterizeEditor");
    auto posterizeRamp   = _window->findChild<QObject *>("posterizeToneRamp");
    auto posterizeSlider = _window->findChild<QObject *>("posterizeLevelSlider");
    auto posterizeQuick  = _window->findChild<QObject *>("posterizeQuickLevels");
    if (!require(posterizeEditor && posterizeEditor->property("visible").toBool() &&
                     posterizeRamp && posterizeSlider && posterizeQuick,
                 "posterize exposes a tone ramp, level slider, and quick values"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "close");
    _document.addAdjustmentLayer("threshold");
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    auto thresholdEditor    = _window->findChild<QObject *>("thresholdEditor");
    auto thresholdHistogram = _window->findChild<QObject *>("thresholdHistogram");
    auto thresholdValue     = _window->findChild<QObject *>("thresholdValue");
    if (!require(thresholdEditor && thresholdEditor->property("visible").toBool() &&
                     thresholdHistogram && thresholdValue,
                 "threshold exposes an interactive histogram and numeric cutoff"))
        return false;
    QMetaObject::invokeMethod(layerProperties, "close");
    _document.newDocument(64, 64);
    for (int i = 0; i < 300; ++i)
        _document.addLayer();
    QTest::qWait(100);
    auto manyLayerList = _window->findChild<QObject *>("layerListView");
    if (!require(manyLayerList && manyLayerList->property("count").toInt() == 301 &&
                     _window->findChildren<QObject *>("layerDelegate").size() < 40,
                 "hundreds of layers instantiate only visible list delegates"))
        return false;
    QMetaObject::invokeMethod(manyLayerList, "positionViewAtEnd");
    QTest::qWait(100);
    if (!require(_window->findChildren<QObject *>("layerDelegate").size() < 40,
                 "layer delegates remain bounded after scrolling through a large stack"))
        return false;
    _document.newDocument(64, 64);
    _document.addFrequencySeparation(4);
    QMetaObject::invokeMethod(layerProperties, "open");
    QTest::qWait(100);
    auto frequencyEditor  = _window->findChild<QObject *>("frequencySeparationEditor");
    auto frequencyRetouch = _window->findChild<QObject *>("frequencyRetouchButton");
    if (!require(frequencyEditor && frequencyEditor->property("visible").toBool() &&
                     frequencyRetouch && frequencyRetouch->property("enabled").toBool(),
                 "frequency separation exposes radius and retouch preparation"))
        return false;
    QMetaObject::invokeMethod(frequencyRetouch, "clicked");
    if (!require(QTest::qWaitFor(
                     [&] {
                         return !_document.frequencyRetouchBusy() &&
                                !layerProperties->property("visible").toBool() &&
                                _document.layers().first().toMap()["kind"].toString() == "pixel";
                     },
                     10000),
                 "retouch button closes settings and prepares editable frequency pixels"))
        return false;
    for (const int layerIndex : {1, 2}) {
        _document.setActiveLayer(layerIndex);
        QTest::qWait(50);
        if (!require(propertiesButton->property("enabled").toBool(),
                     "prepared frequency pixels retain their properties button"))
            return false;
        QMetaObject::invokeMethod(propertiesButton, "clicked");
        QTest::qWait(100);
        auto frequencyRadius = _window->findChild<QObject *>("frequencyRadiusValue");
        if (!require(layerProperties->property("visible").toBool() &&
                         frequencyEditor->property("visible").toBool() &&
                         frequencyEditor->property("prepared").toBool() && frequencyRadius &&
                         !frequencyRadius->property("enabled").toBool() &&
                         frequencyRadius->property("value").toInt() == 4 &&
                         !frequencyRetouch->property("visible").toBool(),
                     "properties reopen on both frequency bands with a fixed radius"))
            return false;
        auto restoreRadius = _window->findChild<QObject *>("frequencyRestoreRadiusButton");
        auto radiusDialog  = _window->findChild<QObject *>("frequencyRadiusDialog");
        auto radiusCancel  = _window->findChild<QObject *>("frequencyRadiusCancelButton");
        auto radiusConfirm = _window->findChild<QObject *>("frequencyRadiusConfirmButton");
        if (!require(restoreRadius && restoreRadius->property("enabled").toBool() && radiusDialog &&
                         radiusCancel && radiusConfirm,
                     "prepared frequencies expose a return-to-radius action"))
            return false;
        QMetaObject::invokeMethod(restoreRadius, "clicked");
        QTest::qWait(50);
        if (!require(radiusDialog->property("visible").toBool(),
                     "returning to radius asks before discarding retouches"))
            return false;
        QTest::keyClick(_window, Qt::Key_Return);
        QTest::qWait(50);
        if (!require(!radiusDialog->property("visible").toBool() &&
                         layerProperties->property("visible").toBool() &&
                         _document.layers().first().toMap()["kind"].toString() == "pixel",
                     "canceling the confirmation keeps prepared pixels"))
            return false;
        QMetaObject::invokeMethod(restoreRadius, "clicked");
        QTest::qWait(50);
        QMetaObject::invokeMethod(radiusConfirm, "clicked");
        QTest::qWait(100);
        if (!require(layerProperties->property("visible").toBool() &&
                         !frequencyEditor->property("prepared").toBool() &&
                         frequencyRadius->property("enabled").toBool() &&
                         _document.layers().first().toMap()["kind"].toString() == "frequencyHigh",
                     "confirmed return keeps the panel open with an editable radius"))
            return false;
        QMetaObject::invokeMethod(layerIndex == 1 ? layerPropertiesCancel : layerPropertiesClose,
                                  "clicked");
        QTest::qWait(100);
        if (!require(!layerProperties->property("visible").toBool() &&
                         _document.layers().first().toMap()["kind"].toString() ==
                             (layerIndex == 1 ? "pixel" : "frequencyHigh"),
                     "Cancel restores prepared pixels while Close keeps the dynamic pair"))
            return false;
    }
    if (!runLayerContextMenuSmoke(_window, _document))
        return false;
    if (!runPenUiSmoke(_window, _document))
        return false;
    if (!runSelectionUiSmoke(_window, _document))
        return false;
    _document.createDemo();
    _document.setTool("brush");
    QTest::qWait(200);
    qInfo("UI interaction checks passed");
    return true;
}

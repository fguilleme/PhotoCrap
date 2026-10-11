#pragma once
#include "Document.h"
#include "RecentFilesManager.h"
#include "WorkspaceSettings.h"
#include <QFile>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>
#include <functional>

// Exercise pointer delivery through QML, keyboard focus and the unsaved guard.
// Compiled only with BUILD_TESTING; production builds need no Qt Test runtime.
inline bool runUiSmoke(QQuickWindow *_window, Document &_document, RecentFilesManager &_recentFiles,
                       WorkspaceSettings &_workspaceSettings)
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
    _document.newDocument(300, 200, true);
    QTest::qWait(100);
    auto histogramPanel  = _window->findChild<QObject *>("histogramDockPanel");
    auto navigatorPanel  = _window->findChild<QObject *>("navigatorDockPanel");
    auto histogramCanvas = _window->findChild<QQuickItem *>("colorHistogramCanvas");
    auto colorPanel      = _window->findChild<QObject *>("colorDockPanel");
    auto layersPanel     = _window->findChild<QObject *>("layersDockPanel");
    auto channelsPanel   = _window->findChild<QObject *>("channelsDockPanel");
    auto historyPanel    = _window->findChild<QObject *>("historyDockPanel");
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
                     reordered.last().toString() == "navigator",
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
                     savedWorkspace.value("order").toList().size() == 6,
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
    if (!require(shortcutList && shortcutList->property("count").toInt() == 41,
                 "keyboard settings list is populated"))
        return false;
    _window->setProperty("preferencesPage", 2);
    QTest::qWait(80);
    auto toolSettingsList = _window->findChild<QObject *>("toolSettingsList");
    if (!require(toolSettingsList && toolSettingsList->property("visible").toBool(),
                 "toolbar settings page is available"))
        return false;
    QMetaObject::invokeMethod(preferences, "close");
    QTest::qWait(80);
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
    auto curvePickerCursor = _window->findChild<QObject *>("curvePickerCursor");
    if (!require(propertiesActions && propertiesActions->property("visible").toBool() &&
                     resetButton && curveColorPicker && curveBlackPicker && curveGrayPicker &&
                     curveWhitePicker && curvePickerArea && curvePickerCursor &&
                     curvePickerArea->property("cursorShape").toInt() == int(Qt::BlankCursor),
                 "adjustment properties expose shared actions and curve eyedroppers"))
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
    _document.createDemo();
    _document.setTool("brush");
    QTest::qWait(200);
    qInfo("UI interaction checks passed");
    return true;
}

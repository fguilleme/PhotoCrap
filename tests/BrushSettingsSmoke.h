#pragma once
#include "Document.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>
#include <functional>

// Go through the real numeric editor, not valueModified or a C++ setter: the
// settings can look correct while Qt never commits a nested TextInput's text.
inline bool runBrushSettingsSmoke(QQuickWindow *_window, Document &_document)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Brush settings check failed: %s", _message);
        return _ok;
    };
    auto area     = _window->findChild<QQuickItem *>("drawingArea");
    auto hardness = _window->findChild<QQuickItem *>("brushHardnessTop");
    auto opacity  = _window->findChild<QQuickItem *>("brushOpacityTop");
    auto flow     = _window->findChild<QQuickItem *>("brushFlowTop");
    if (!require(area && hardness && opacity && flow, "brush settings controls exist"))
        return false;
    const auto type = [&](QQuickItem *_control, int _value, bool _return = true) {
        auto input = _control->property("contentItem").value<QQuickItem *>();
        if (!input)
            return false;
        QTest::mouseClick(
            _window, Qt::LeftButton, Qt::NoModifier,
            input->mapToScene(QPointF(input->width() / 2, input->height() / 2)).toPoint());
        QTest::keySequence(_window, QKeySequence::SelectAll);
        for (const auto c : QString::number(_value))
            QTest::keyClick(_window, c.toLatin1());
        if (_return)
            QTest::keyClick(_window, Qt::Key_Return);
        else
            area->forceActiveFocus();
        QCoreApplication::processEvents();
        return true;
    };
    for (int bits : {8, 16})
        for (const auto &tool : {QStringLiteral("brush"), QStringLiteral("pencil")}) {
            _document.newDocument(96, 96, true, bits);
            _document.setTool(tool);
            _document.setColor(Qt::black);
            _document.setBrushSize(32);
            _document.setBrushBlend(0);
            _document.setBrushHardness(1);
            _document.setBrushOpacity(1);
            _document.setBrushFlow(1);
            _document.setBrushRoundness(1);
            _document.setBrushSmoothing(0);
            _document.setPencilAutoErase(false);
            _window->setProperty("zoom", 1.);
            QTest::qWait(40);
            if (!require(type(opacity, 40) && qAbs(_document.brushOpacity() - .4) < .001,
                         "Return commits typed opacity to the painting engine"))
                return false;
            if (!require(type(flow, 20, false) && qAbs(_document.brushFlow() - .2) < .001,
                         "leaving the editor commits typed flow"))
                return false;
            const auto dab = [&] {
                area->forceActiveFocus();
                QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier,
                                  area->mapToScene(QPointF(48, 48)).toPoint());
                return _document.composite();
            };
            if (!require(type(flow, 100), "set full flow"))
                return false;
            const auto capped = dab().pixelColor(48, 48).alpha();
            if (!require(capped >= 98 && capped <= 105,
                         "painted opacity is capped at the typed 40 percent"))
                return false;
            _document.undo();
            if (!require(type(opacity, 100) && type(flow, 20), "set low flow"))
                return false;
            const auto thin = dab().pixelColor(48, 48).alpha();
            if (!require(thin >= 48 && thin <= 55, "typed flow changes the alpha of a single dab"))
                return false;
            _document.undo();
            if (!require(type(flow, 100), "restore full flow"))
                return false;
            if (tool == "brush") {
                if (!require(type(hardness, 0) && _document.brushHardness() == 0,
                             "typed zero hardness reaches the painting engine"))
                    return false;
                const auto soft = dab();
                if (!require(soft.pixelColor(60, 48).alpha() > 0 &&
                                 soft.pixelColor(60, 48).alpha() < 150,
                             "zero hardness produces a genuinely feathered brush edge"))
                    return false;
                _document.undo();
                if (!require(type(hardness, 100), "restore hard edge"))
                    return false;
                if (!require(dab().pixelColor(60, 48).alpha() > 245,
                             "100 percent hardness produces an opaque inner edge"))
                    return false;
            } else if (!require(!hardness->isVisible(),
                                "Pencil does not expose unsupported soft hardness"))
                return false;
            // A programmatic update (keyboard shortcut/preset) must still refresh
            // the editor after a user has typed into it.
            _document.setBrushOpacity(.73);
            if (!require(
                    opacity->property("displayText").toString() == "73" &&
                        opacity->property("contentItem")
                                .value<QQuickItem *>()
                                ->property("text")
                                .toString() == "73",
                    "external setting changes refresh the numeric editor without a percent sign"))
                return false;
            QTest::mouseClick(
                _window, Qt::LeftButton, Qt::NoModifier,
                opacity->mapToScene(QPointF(opacity->width() - 15, opacity->height() / 2))
                    .toPoint());
            if (!require(qAbs(_document.brushOpacity() - .74) < .001,
                         "the plus button still increments the typed setting"))
                return false;
            QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier,
                              opacity->mapToScene(QPointF(15, opacity->height() / 2)).toPoint());
            if (!require(qAbs(_document.brushOpacity() - .73) < .001,
                         "the minus button still decrements the typed setting"))
                return false;
            if (tool == "pencil") {
                _document.setBrushHardness(0);
                auto options = _window->findChild<QObject *>("brushOptionsButton");
                auto dialog  = _window->findChild<QObject *>("brushSettingsDialog");
                if (!require(options && dialog, "pencil settings dialog exists"))
                    return false;
                QMetaObject::invokeMethod(options, "clicked");
                QObject    *preview     = nullptr;
                QQuickItem *hardnessRow = nullptr;
                // Repeater delegates have visual parents without necessarily
                // belonging to the window's QObject ownership tree.
                const std::function<QQuickItem *(QQuickItem *)> findHardness =
                    [&](QQuickItem *item) -> QQuickItem * {
                    if (!item)
                        return nullptr;
                    if (item->objectName() == "brushSetting_brushHardness")
                        return item;
                    for (auto child : item->childItems())
                        if (auto found = findHardness(child))
                            return found;
                    return nullptr;
                };
                if (!require(QTest::qWaitFor(
                                 [&] {
                                     preview     = _window->findChild<QObject *>("brushTipPreview");
                                     hardnessRow = findHardness(
                                         dialog->property("contentItem").value<QQuickItem *>());
                                     return preview && hardnessRow &&
                                            dialog->property("opened").toBool();
                                 },
                                 2000),
                             "pencil settings content is ready"))
                    return false;
                if (!require(!hardnessRow->isVisible() &&
                                 preview->property("hardness").toDouble() == 1.,
                             "pencil settings hide inapplicable hardness and preview a hard tip"))
                    return false;
                QMetaObject::invokeMethod(dialog, "reject");
                if (!require(QTest::qWaitFor([&] { return !dialog->property("visible").toBool(); },
                                             1500) &&
                                 _document.brushHardness() == 0,
                             "viewing Pencil settings preserves the saved Brush hardness"))
                    return false;
            }
            area->forceActiveFocus();
        }
    _document.setBrushSize(24);
    _document.setBrushHardness(1);
    _document.setBrushOpacity(1);
    _document.setBrushFlow(1);
    _document.setTool("brush");
    return true;
}

#pragma once
#include "Document.h"
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtTest>

inline bool runPresetPreviewSmoke(QQuickWindow *_window, Document &_document)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Preset preview check failed: %s", _message);
        return _ok;
    };
    const auto parameters = [&] {
        for (const auto &entry : _document.layers()) {
            const auto layer = entry.toMap();
            if (layer.value("layerIndex").toInt() == _document.activeLayer())
                return layer.value("parameters").toMap();
        }
        return QVariantMap{};
    };
    const auto findVisual = [](auto &&_self, QQuickItem *_item,
                               const QString &_name) -> QQuickItem * {
        if (!_item)
            return nullptr;
        if (_item->objectName() == _name)
            return _item;
        for (auto *child : _item->childItems())
            if (auto *found = _self(_self, child, _name))
                return found;
        return nullptr;
    };
    auto *dialog = _window->findChild<QObject *>("layerPropertiesDialog");
    if (!require(dialog, "layer properties exist"))
        return false;
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        _window->raise();
        _window->requestActivate();
    }
    for (const QString &type : {QStringLiteral("blackWhite"), QStringLiteral("photoFilter"),
                                QStringLiteral("channelMixer")}) {
        if (!require(_document.newDocument(256, 128, false), "create a small opaque reference"))
            return false;
        _document.setColor(QColor(60, 120, 180));
        _document.fillWithColor(true);
        _document.addAdjustmentLayer(type);
        if (type == "photoFilter") {
            _document.beginLayerPreview();
            _document.previewLayerParameter("color", "#387d92");
            _document.previewLayerParameter("useCustomColor", true);
            _document.previewLayerParameter("density", 45);
            _document.finishLayerPreview();
        }
        const auto original = parameters();
        const auto image    = _document.composite();
        const int  history  = _document.historyIndex();
        QMetaObject::invokeMethod(dialog, "open");
        QTest::qWait(100);
        auto *combo = _window->findChild<QQuickItem *>(type + "Preset");
        auto *popup = combo ? combo->property("popup").value<QObject *>() : nullptr;
        if (!require(combo && popup && combo->isVisible(), "preset menu is visible"))
            return false;
        const auto open = [&] {
            combo->forceActiveFocus();
            QMetaObject::invokeMethod(popup, "open");
            return QTest::qWaitFor([&] { return popup->property("opened").toBool(); }, 2000);
        };
        const auto optionAt = [&](int _index) {
            auto *content = popup->property("contentItem").value<QQuickItem *>();
            return findVisual(findVisual, content, "presetOption" + QString::number(_index));
        };
        const auto hover = [&](int _index) {
            auto *option = optionAt(_index);
            if (!option)
                return false;
            const auto point =
                option->mapToScene({option->width() / 2, option->height() / 2}).toPoint();
            QTest::mouseMove(option->window(), point, 10);
            return QTest::qWaitFor(
                [&] { return combo->property("lastPreviewIndex").toInt() == _index; }, 2000);
        };
        const bool firstOpen  = open();
        const bool firstHover = firstOpen && hover(1);
        if (!firstHover) {
            auto *option = optionAt(1);
            qWarning() << "Preset hover diagnostic" << type << firstOpen << _window->isActive()
                       << popup->property("visible") << combo->property("lastPreviewIndex")
                       << (option ? option->property("hovered") : QVariant{})
                       << (option ? option->mapToScene(QPointF{}) : QPointF{});
        }
        if (!require(firstHover, "mouse hover previews the first candidate"))
            return false;
        if (!require(parameters() != original && _document.historyIndex() == history,
                     "hover changes parameters without creating history"))
            return false;
        if (QSGRendererInterface::isApiRhiBased(_window->rendererInterface()->graphicsApi()))
            if (!require(QTest::qWaitFor([&] { return _document.gpuTonePreviewActive(); }, 3000),
                         "preset hover uses the GPU preview on a hardware backend"))
                return false;
        if (!require(QTest::qWaitFor([&] { return _document.displayComposite() != image; }, 3000),
                     "the main image changes during hover"))
            return false;
        if (!require(hover(2) && _document.historyIndex() == history,
                     "another hover replaces the preview"))
            return false;
        QTest::keyClick(combo->window(), Qt::Key_Escape);
        if (!require(QTest::qWaitFor([&] { return !popup->property("visible").toBool(); }, 2000) &&
                         parameters() == original && _document.historyIndex() == history &&
                         dialog->property("visible").toBool(),
                     "Escape restores the original adjustment and leaves properties open"))
            return false;

        if (!require(open() && hover(1), "reopen the menu for an outside dismissal"))
            return false;
        QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, QPoint(20, 80));
        if (!require(QTest::qWaitFor([&] { return !popup->property("visible").toBool(); }, 2000) &&
                         parameters() == original && _document.historyIndex() == history,
                     "clicking outside cancels the preview"))
            return false;

        auto *previewCheck = _window->findChild<QObject *>("layerPreviewCheck");
        if (!require(previewCheck, "the preview toggle exists"))
            return false;
        previewCheck->setProperty("checked", false);
        if (!require(open() && hover(2) && parameters() == original,
                     "disabled preview leaves parameters unchanged during hover"))
            return false;
        QTest::keyClick(combo->window(), Qt::Key_Escape);
        if (!require(QTest::qWaitFor([&] { return !popup->property("visible").toBool(); }, 2000),
                     "close the disabled preview menu"))
            return false;
        previewCheck->setProperty("checked", true);

        if (!require(open() && hover(1), "reopen the menu for keyboard activation"))
            return false;
        QTest::keyClick(combo->window(), Qt::Key_Down);
        QTest::qWait(50);
        const auto selected = parameters();
        QTest::keyClick(combo->window(), Qt::Key_Return);
        if (!require(QTest::qWaitFor([&] { return !popup->property("visible").toBool(); }, 2000) &&
                         dialog->property("visible").toBool() && parameters() == selected &&
                         _document.historyIndex() == history + 1,
                     "Return commits exactly one history entry without closing properties"))
            return false;
        dialog->setProperty("keepChanges", true);
        QMetaObject::invokeMethod(dialog, "accept");
        if (!require(QTest::qWaitFor([&] { return !dialog->property("visible").toBool(); }, 2000),
                     "properties close after accepting the preset"))
            return false;
        _document.undo();
        if (!require(parameters() == original && _document.composite() == image,
                     "Undo restores the state before all menu hovers"))
            return false;
        QMetaObject::invokeMethod(dialog, "open");
        if (!require(QTest::qWaitFor([&] { return dialog->property("opened").toBool(); }, 2000),
                     "properties reopen after undo"))
            return false;
        const int  index   = combo->property("currentIndex").toInt();
        const bool opened  = open();
        const bool hovered = opened && hover(index);
        if (!hovered || parameters() == original)
            qWarning() << "Preset current-row diagnostic" << type << index << opened << hovered
                       << combo->property("lastPreviewIndex") << parameters() << original;
        if (!require(hovered && parameters() != original,
                     "hovering the already highlighted row also previews over custom values"))
            return false;
        const auto clicked = parameters();
        auto      *option  = optionAt(index);
        QTest::mouseClick(
            option->window(), Qt::LeftButton, Qt::NoModifier,
            option->mapToScene({option->width() / 2, option->height() / 2}).toPoint());
        if (!require(QTest::qWaitFor([&] { return !popup->property("visible").toBool(); }, 2000) &&
                         parameters() == clicked && _document.historyIndex() == history + 1,
                     "a mouse click commits one history entry"))
            return false;
        QMetaObject::invokeMethod(dialog, "reject");
        if (!require(QTest::qWaitFor([&] { return !dialog->property("visible").toBool(); }, 2000),
                     "properties close after rejecting the preset"))
            return false;
        if (!require(parameters() == original && _document.historyIndex() == history,
                     "properties cancellation also cancels a committed preset"))
            return false;
    }
    qInfo("Preset preview checks passed");
    return true;
}

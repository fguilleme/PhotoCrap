#pragma once
#include "Document.h"
#include "WorkspaceSettings.h"
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <memory>

// Each scenario runs in its own process: a hidden tool window must not veto
// the real Quit event, and cancellation must leave the workspace intact.
inline void startShutdownSmoke(QQuickWindow *_window, Document &_document,
                               WorkspaceSettings &_settings, bool _save, bool _clean)
{
    auto require = [](bool _ok, const char *_message) {
        if (!_ok) {
            qWarning("Shutdown check failed: %s", _message);
            QCoreApplication::exit(3);
        }
        return _ok;
    };
    auto histogram = _window ? _window->findChild<QObject *>("histogramDockPanel") : nullptr;
    auto color     = _window ? _window->findChild<QObject *>("colorDockPanel") : nullptr;
    auto guard     = _window ? _window->findChild<QObject *>("unsavedDialog") : nullptr;
    auto cancel    = _window ? _window->findChild<QObject *>("unsavedCancelButton") : nullptr;
    auto decision  = _window ? _window->findChild<QObject *>(_save ? "unsavedSaveButton"
                                                                   : "unsavedDiscardButton")
                             : nullptr;
    if (!require(histogram && color && guard && cancel && decision, "shutdown UI exists"))
        return;
    auto       directory = std::make_shared<QTemporaryDir>();
    const auto path      = directory->filePath("shutdown.pcr");
    _document.newDocument(64, 64);
    if (_save && !require(_document.saveProject(QUrl::fromLocalFile(path)), "save initial project"))
        return;
    _document.addLayer();
    QMetaObject::invokeMethod(color, "setPanelFloating", Q_ARG(QVariant, true));
    QTest::qWait(50);
    auto hiddenTool = _window->findChild<QQuickWindow *>("colorFloatingWindow");
    if (!require(hiddenTool && hiddenTool->isVisible(), "tool window opened"))
        return;
    const bool closed = hiddenTool->close();
    if (!require(!closed && !hiddenTool->isVisible() && hiddenTool->handle() &&
                     !color->property("floating").toBool(),
                 "normal tool closure redocks but retains a hidden platform window"))
        return;
    QMetaObject::invokeMethod(histogram, "setPanelFloating", Q_ARG(QVariant, true));
    QTest::qWait(50);
    auto visibleTool = _window->findChild<QQuickWindow *>("histogramFloatingWindow");
    if (!require(visibleTool && visibleTool->isVisible(), "a second tool remains floating"))
        return;
    auto approved = std::make_shared<bool>(false);
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                     QCoreApplication::instance(), [=, &_document, &_settings] {
                         const auto panels = _settings.layout().value("panels").toMap();
                         bool passed = *approved && !_window->isVisible() &&
                                       !visibleTool->isVisible() && !hiddenTool->isVisible() &&
                                       histogram->property("floating").toBool() &&
                                       panels.value("histogram").toMap().value("floating").toBool();
                         if (_save || _clean) {
                             QFile file(directory->filePath("shutdown.pcr"));
                             passed =
                                 passed && !_document.modified() && file.open(QIODevice::ReadOnly);
                             if (passed)
                                 passed = QJsonDocument::fromJson(file.readAll())
                                              .object()
                                              .value("layers")
                                              .toArray()
                                              .size() == 2;
                         }
                         QCoreApplication::instance()->setProperty("shutdownChecksPassed", passed);
                         if (passed)
                             qInfo("Shutdown checks passed: %s, visible and hidden tool windows",
                                   _clean  ? "clean native quit"
                                   : _save ? "save"
                                           : "discard");
                         else
                             qWarning("Shutdown lost confirmation, saved data, or floating layout");
                     });
    QTimer::singleShot(5000, QCoreApplication::instance(), [] {
        qWarning("Shutdown timed out: application remained alive after closing its document");
        QCoreApplication::exit(3);
    });
    for (int attempt = 0; attempt < 2; ++attempt) {
        QEvent quit(QEvent::Quit);
        QCoreApplication::sendEvent(QCoreApplication::instance(), &quit);
        if (!require(!quit.isAccepted() && _window->isVisible() && visibleTool->isVisible() &&
                         histogram->property("floating").toBool() &&
                         guard->property("visible").toBool() && _document.modified(),
                     "native Quit protects unsaved edits before closing any tool window"))
            return;
        if (attempt == 0) {
            QMetaObject::invokeMethod(cancel, "clicked");
            QTest::qWait(100);
            if (!require(!guard->property("visible").toBool() && _window->isVisible() &&
                             visibleTool->isVisible() && _document.modified(),
                         "Cancel keeps the document and floating panel open"))
                return;
        }
    }
    *approved = true;
    if (_clean) {
        QMetaObject::invokeMethod(cancel, "clicked");
        if (!require(_document.saveProject(QUrl::fromLocalFile(path)), "save before native Quit"))
            return;
        QCoreApplication::postEvent(QCoreApplication::instance(), new QEvent(QEvent::Quit));
    } else
        QMetaObject::invokeMethod(decision, "clicked");
}

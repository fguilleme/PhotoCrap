#pragma once

#include "Document.h"
#include <QDir>
#include <QFileInfo>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

inline bool captureWorkspaceReview(QQuickWindow *window, Document &document, const QString &photo,
                                   const QString &destination)
{
    QDir output(destination);
    if (!window || !QFileInfo(photo).isFile() || !output.exists() ||
        !document.openFile(QUrl::fromLocalFile(photo)))
        return false;
    document.setBackgroundEditThreshold(-1);
    document.addAdjustmentLayer("light");
    document.addLayer();
    document.renameLayer(document.activeLayer(), QObject::tr("Retouche locale"));
    document.setTool("brush");
    document.setBrushSize(32);
    const auto project = output.filePath(QFileInfo(photo).completeBaseName() + ".pcr");
    if (QFileInfo::exists(project) || !document.saveProject(QUrl::fromLocalFile(project)))
        return false;
    for (const QSize size : {QSize(1480, 900), QSize(1000, 680)}) {
        window->showNormal();
        window->setMinimumSize(size);
        window->setMaximumSize(size);
        window->setGeometry(QRect(QPoint(32, 60), size));
        QTest::qWait(250);
        window->setProperty("zoom", size.width() > 1000 ? .78 : .46);
        QMetaObject::invokeMethod(window, "centerCanvas");
        QTest::qWait(400);
        const auto frame = window->grabWindow();
        qInfo() << "Workspace" << window->size() << frame.size() << window->devicePixelRatio()
                << window->property("zoom") << document.layers().size();
        if (window->size() != size || frame.isNull() ||
            !frame.save(output.filePath(QString::number(size.width()) + ".png")))
            return false;
    }
    auto *actualSize = window->findChild<QQuickItem *>("actualSizeButton");
    auto *exportButton = window->findChild<QQuickItem *>("workspaceExport");
    auto *dialog = window->findChild<QObject *>("exportDialog");
    if (!actualSize || !exportButton || !dialog) return false;
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      actualSize->mapToScene(actualSize->boundingRect().center()).toPoint());
    if (!qFuzzyCompare(window->property("zoom").toReal(), 1.0)) return false;
    const QString exported = output.absoluteFilePath("export.png");
    dialog->setProperty("currentFolder", QUrl::fromLocalFile(output.absolutePath()));
    dialog->setProperty("selectedFile", QUrl::fromLocalFile(exported));
    window->requestActivate();
    if (!QTest::qWaitForWindowActive(window, 3000)) return false;
    exportButton->forceActiveFocus(Qt::TabFocusReason);
    if (!QTest::qWaitFor([&] { return window->property("keyboardCommandFocused").toBool(); }, 1000)) {
        qWarning() << "Export did not receive keyboard focus";
        return false;
    }
    QTest::keyClick(window, Qt::Key_Space);
    if (!QTest::qWaitFor([&] { return dialog->property("visible").toBool(); }, 3000)) {
        qWarning() << "Export button did not open the file dialog";
        return false;
    }
    if (!QMetaObject::invokeMethod(dialog, "accept") ||
        !QTest::qWaitFor([&] { return QFileInfo::exists(exported); }, 5000)) {
        qWarning() << "Export was not written" << exported << dialog->property("selectedFile");
        return false;
    }
    return QImage(exported).size() == document.composite().size();
}

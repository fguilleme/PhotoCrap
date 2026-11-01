#include "Application.h"
#include "CanvasItem.h"
#include "CropShadeItem.h"
#include "Document.h"
#include "LanguageManager.h"
#include "RecentFilesManager.h"
#include "ShortcutManager.h"
#include "ToolInput.h"
#include "WorkspaceSettings.h"
#include "render/CanvasFonts.h"
#include "render/CurvesGeometry.h"
#include "render/HistogramGeometry.h"
#include "text/TextEditorController.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlFileSelector>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#ifdef PHOTOCRAP_TESTING
#include "../tests/BrushCursorSmoke.h"
#include "../tests/CanvasRenderSmoke.h"
#include "../tests/CropOverlaySmoke.h"
#include "../tests/CurvesGraphSmoke.h"
#include "../tests/FileDialogSmoke.h"
#include "../tests/HistogramRenderSmoke.h"
#include "../tests/LassoGuideSmoke.h"
#include "../tests/LevelsGraphSmoke.h"
#include "../tests/MeasurementOverlaySmoke.h"
#include "../tests/PenOverlaySmoke.h"
#include "../tests/PresetPreviewSmoke.h"
#include "../tests/ShutdownSmoke.h"
#include "../tests/SliceOverlaySmoke.h"
#include "../tests/ThresholdGraphSmoke.h"
#include "../tests/ToneRenderSmoke.h"
#include "../tests/UiSmoke.h"
#include "../tests/ViewportRulerSmoke.h"
#ifdef Q_OS_MACOS
#include "../tests/MacApplicationMenuTest.h"
#include "../tests/MacToolCursorTest.h"
#endif
#endif

class ThumbnailProvider : public QQuickImageProvider
{
public:
    explicit ThumbnailProvider(Document *_document)
        : QQuickImageProvider(Image), m_document(_document)
    {
    }
    QImage requestImage(const QString &_id, QSize *_size, const QSize &_requested) override
    {
        QImage image;
        if (_id.startsWith("colorSelection/")) {
            const QRect region(_id.section('/', 3, 3).toInt(), _id.section('/', 4, 4).toInt(),
                               _id.section('/', 5, 5).toInt(), _id.section('/', 6, 6).toInt());
            image = m_document->colorSelectionPreview(_id.section('/', 1, 1) == "mask", region,
                                                      _requested);
        } else if (_id.startsWith("composite"))
            image = m_document->displayComposite();
        else if (_id.startsWith("mask/"))
            image = m_document->maskImage(_id.section('/', 1, 1).toInt(),
                                          _requested.isValid() ? _requested : QSize(180, 120));
        else
            image = m_document->layerImage(_id.section('/', 0, 0).toInt(),
                                           _requested.isValid() ? _requested : QSize(180, 120));
        // A removed delegate can finish requesting its thumbnail while the
        // layer list is being reset. Serve a transparent placeholder for it.
        if (image.isNull()) {
            image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
        }
        if (_size)
            *_size = image.size();
        return image.scaled(_requested.isValid() ? _requested : QSize(180, 120),
                            Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

private:
    Document *m_document;
};

int main(int argc, char *argv[])
{
    Application app(argc, argv);
    app.setApplicationName("PhotoCrap");
    app.setOrganizationName("PhotoCrap");
    app.setApplicationVersion(PHOTOCRAP_VERSION);
    QQuickStyle::setStyle("Basic");
    const auto args  = app.arguments();
    const bool smoke = args.contains("--smoke-test");
#ifdef PHOTOCRAP_TESTING
    if (args.contains("--file-dialog-test"))
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
#endif
    qmlRegisterType<ToolInput>("PhotoCrap.Native", 1, 0, "ToolInput");
    qmlRegisterType<CanvasItem>("PhotoCrap.Native", 1, 0, "ImageCanvas");
    qmlRegisterType<CropShadeItem>("PhotoCrap.Native", 1, 0, "CropShade");
    qmlRegisterSingletonType<CanvasFonts>(
        "PhotoCrap.Native", 1, 0, "CanvasFonts",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new CanvasFonts; });
    qmlRegisterSingletonType<CurvesGeometry>(
        "PhotoCrap.Native", 1, 0, "CurvesGeometry",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new CurvesGeometry; });
    qmlRegisterSingletonType<HistogramGeometry>(
        "PhotoCrap.Native", 1, 0, "HistogramGeometry",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new HistogramGeometry; });
    qmlRegisterType<TextEditorController>("PhotoCrap.Native", 1, 0, "TextFormatting");
    qmlRegisterUncreatableType<Document>("PhotoCrap.Native", 1, 0, "ImageDocument",
                                         "Provided by the application");
    LanguageManager    languageManager(nullptr, !smoke);
    RecentFilesManager recentFilesManager(nullptr, !smoke);
    Document           document;
    ShortcutManager    shortcutManager(nullptr, !smoke);
    WorkspaceSettings  workspaceSettings(nullptr, !smoke);
    const auto         configureScratch = [&] {
        document.configureScratchCache(
            workspaceSettings.scratchDirectory(),
            smoke ? 0 : qint64(workspaceSettings.scratchDiskGiB()) * 1024 * 1024 * 1024);
    };
    configureScratch();
    QObject::connect(&workspaceSettings, &WorkspaceSettings::scratchCacheChanged, &document,
                     configureScratch);
    int                   qmlWarnings = 0;
    QQmlApplicationEngine engine;
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    // Qt 6.8's Basic fallback breadcrumb has a fixed white background. Select
    // Qt's desktop dialog skins without changing the application's Basic controls.
    QQmlFileSelector dialogSelector(&engine);
    dialogSelector.setExtraSelectors({QStringLiteral("Fusion")});
#endif
    QObject::connect(
        &engine, &QQmlApplicationEngine::warnings, &app,
        [&](const QList<QQmlError> &_waraings) { qmlWarnings += int(_waraings.size()); });
    engine.addImageProvider("layers", new ThumbnailProvider(&document));
    engine.rootContext()->setContextProperty("editor", &document);
    engine.rootContext()->setContextProperty("languages", &languageManager);
    engine.rootContext()->setContextProperty("recentFiles", &recentFilesManager);
    engine.rootContext()->setContextProperty("shortcuts", &shortcutManager);
    engine.rootContext()->setContextProperty("workspaceSettings", &workspaceSettings);
    QObject::connect(&document, &Document::rawFileOpened, &recentFilesManager,
                     &RecentFilesManager::add);
    QObject::connect(&languageManager, &LanguageManager::languageChanged, &engine, [&] {
        engine.retranslate();
        emit shortcutManager.shortcutsChanged();
    });
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("PhotoCrap", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    app.setMainWindow(qobject_cast<QWindow *>(engine.rootObjects().first()));
    if (args.contains("--demo") || smoke)
        document.createDemo();
    else if (args.size() > 1 && !args[1].startsWith("--")) {
        const QUrl url = QUrl::fromLocalFile(args[1]);
        if (document.isRawFile(url))
            document.openRawFile(url);
        else if (document.openFile(url))
            recentFilesManager.add(url);
    }
    if (smoke) {
        QTimer::singleShot(1200, &app, [&] {
            auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
#ifdef PHOTOCRAP_TESTING
            bool passed = false;
            if (args.contains("--preset-preview-test")) {
                passed = window && runPresetPreviewSmoke(window, document);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--tone-render-test")) {
                passed = window && runToneRenderSmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--canvas-render-test")) {
                passed = window && runCanvasRenderSmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--curves-graph-test")) {
                passed = window && runCurvesGraphSmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--levels-graph-test")) {
                passed = window && runLevelsGraphSmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--threshold-graph-test")) {
                passed = window && runThresholdGraphSmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--viewport-ruler-test")) {
                passed = window && runViewportRulerSmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--histogram-render-test")) {
                passed = window && runHistogramRenderSmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--measurement-overlay-test")) {
                passed = window && runMeasurementOverlaySmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--brush-cursor-test")) {
                passed = window && runBrushCursorSmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--pen-overlay-test")) {
                passed = window && runPenOverlaySmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--lasso-guide-test")) {
                passed = window && runLassoGuideSmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--slice-overlay-test")) {
                passed = window && runSliceOverlaySmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--tool-layer-test")) {
                passed = window && runToolLayerSmoke(window, document, workspaceSettings);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--crop-overlay-test")) {
                passed = window && runCropOverlaySmoke(window);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--file-dialog-test")) {
                passed = window && runFileDialogSmoke(window, document);
                app.exit(passed && !qmlWarnings ? 0 : 3);
                return;
            }
            if (args.contains("--shutdown-test")) {
                if (qmlWarnings) {
                    app.exit(2);
                    return;
                }
                startShutdownSmoke(window, document, workspaceSettings,
                                   args.contains("--shutdown-save"),
                                   args.contains("--shutdown-clean"));
                return;
            }
#ifdef Q_OS_MACOS
            if (window && args.contains("--native-menu-test"))
                passed = runMacApplicationMenuTest(window);
            else if (window && args.contains("--native-cursor-test"))
                passed = runMacToolCursorTest(window);
            else if (window && args.contains("--native-pen-test"))
                passed = runMacPenInputTest(window, document);
            else
#endif
                passed = window && runUiSmoke(window, document, recentFilesManager,
                                              workspaceSettings, shortcutManager);
            if (!passed) {
                app.exit(3);
                return;
            }
#endif
            const auto frame = window ? window->grabWindow() : QImage{};
            const int  index = args.indexOf("--screenshot");
            bool       saved = true;
            if (index >= 0 && index + 1 < args.size())
                saved = frame.save(args[index + 1]);
            if (frame.isNull() || !saved || qmlWarnings) {
                qWarning("QML smoke test failed (%d warnings)", qmlWarnings);
                app.exit(2);
            } else {
                qInfo("QML smoke test passed: %dx%d", frame.width(), frame.height());
                app.exit(0);
            }
        });
    }
    const int result = app.exec();
#ifdef PHOTOCRAP_TESTING
    if (args.contains("--shutdown-test") && !app.property("shutdownChecksPassed").toBool())
        return 3;
#endif
    return result;
}

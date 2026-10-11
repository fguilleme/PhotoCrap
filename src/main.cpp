#include "CanvasItem.h"
#include "Document.h"
#include "LanguageManager.h"
#include "RecentFilesManager.h"
#include "ShortcutManager.h"
#include "WorkspaceSettings.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#ifdef PHOTOCRAP_TESTING
#include "../tests/UiSmoke.h"
#ifdef Q_OS_MACOS
#include "../tests/MacApplicationMenuTest.h"
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
        if (_id.startsWith("composite"))
            image = m_document->composite();
        else if (_id.startsWith("mask/"))
            image = m_document->maskImage(_id.section('/', 1, 1).toInt());
        else
            image = m_document->layerImage(_id.section('/', 0, 0).toInt());
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
    QGuiApplication app(argc, argv);
    app.setApplicationName("PhotoCrap");
    app.setOrganizationName("PhotoCrap");
    app.setApplicationVersion("0.1.0");
    QQuickStyle::setStyle("Basic");
    const auto args  = app.arguments();
    const bool smoke = args.contains("--smoke-test");
    qmlRegisterType<CanvasItem>("PhotoCrap.Native", 1, 0, "ImageCanvas");
    qmlRegisterUncreatableType<Document>("PhotoCrap.Native", 1, 0, "ImageDocument",
                                         "Provided by the application");
    LanguageManager       languageManager(nullptr, !smoke);
    RecentFilesManager    recentFilesManager(nullptr, !smoke);
    Document              document;
    ShortcutManager       shortcutManager;
    WorkspaceSettings     workspaceSettings(nullptr, !smoke);
    int                   qmlWarnings = 0;
    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::warnings, &app,
        [&](const QList<QQmlError> &_waraings) { qmlWarnings += int(_waraings.size()); });
    engine.addImageProvider("layers", new ThumbnailProvider(&document));
    engine.rootContext()->setContextProperty("editor", &document);
    engine.rootContext()->setContextProperty("languages", &languageManager);
    engine.rootContext()->setContextProperty("recentFiles", &recentFilesManager);
    engine.rootContext()->setContextProperty("shortcuts", &shortcutManager);
    engine.rootContext()->setContextProperty("workspaceSettings", &workspaceSettings);
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
    if (args.contains("--demo") || smoke)
        document.createDemo();
    else if (args.size() > 1 && !args[1].startsWith("--")) {
        const QUrl url = QUrl::fromLocalFile(args[1]);
        if (document.openFile(url))
            recentFilesManager.add(url);
    }
    if (smoke) {
        QTimer::singleShot(1200, &app, [&] {
            auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
#ifdef PHOTOCRAP_TESTING
            bool passed = false;
#ifdef Q_OS_MACOS
            if (window && args.contains("--native-menu-test"))
                passed = runMacApplicationMenuTest(window);
            else
#endif
                passed =
                    window && runUiSmoke(window, document, recentFilesManager, workspaceSettings);
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
    return app.exec();
}

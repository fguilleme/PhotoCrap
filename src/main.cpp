#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include "CanvasItem.h"
#include "Document.h"
#include "LanguageManager.h"
#include "ShortcutManager.h"
#ifdef PHOTOCRAP_TESTING
#include "../tests/UiSmoke.h"
#endif

class ThumbnailProvider : public QQuickImageProvider {
public:
    explicit ThumbnailProvider(Document *document):QQuickImageProvider(Image),m_document(document) {}
    QImage requestImage(const QString &id,QSize *size,const QSize &requested) override {
        QImage image;
        if(id.startsWith("composite")) image=m_document->composite();
        else if(id.startsWith("mask/")) image=m_document->maskImage(id.section('/',1,1).toInt());
        else image=m_document->layerImage(id.section('/',0,0).toInt());
        // A removed delegate can finish requesting its thumbnail while the
        // layer list is being reset. Serve a transparent placeholder for it.
        if(image.isNull()) {image=QImage(1,1,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);}
        if(size) *size=image.size();
        return image.scaled(requested.isValid()?requested:QSize(180,120),Qt::KeepAspectRatio,Qt::SmoothTransformation);
    }
private:
    Document *m_document;
};

int main(int argc,char *argv[]) {
    QGuiApplication app(argc,argv);
    app.setApplicationName("PhotoCrap"); app.setOrganizationName("PhotoCrap"); app.setApplicationVersion("0.1.0");
    QQuickStyle::setStyle("Basic");
    const auto args=app.arguments();
    const bool smoke=args.contains("--smoke-test");
    qmlRegisterType<CanvasItem>("PhotoCrap.Native",1,0,"ImageCanvas");
    qmlRegisterUncreatableType<Document>("PhotoCrap.Native",1,0,"ImageDocument","Provided by the application");
    LanguageManager languageManager(nullptr,!smoke);
    Document document;
    ShortcutManager shortcutManager;
    int qmlWarnings=0;
    QQmlApplicationEngine engine;
    QObject::connect(&engine,&QQmlApplicationEngine::warnings,&app,[&](const QList<QQmlError> &warnings){qmlWarnings+=int(warnings.size());});
    engine.addImageProvider("layers",new ThumbnailProvider(&document));
    engine.rootContext()->setContextProperty("editor",&document);
    engine.rootContext()->setContextProperty("languages",&languageManager);
    engine.rootContext()->setContextProperty("shortcuts",&shortcutManager);
    QObject::connect(&languageManager,&LanguageManager::languageChanged,&engine,[&]{engine.retranslate();emit shortcutManager.shortcutsChanged();});
    QObject::connect(&engine,&QQmlApplicationEngine::objectCreationFailed,&app,[]{QCoreApplication::exit(1);},Qt::QueuedConnection);
    engine.loadFromModule("PhotoCrap","Main");
    if(engine.rootObjects().isEmpty()) return 1;
    if(args.contains("--demo") || smoke) document.createDemo();
    else if(args.size()>1 && !args[1].startsWith("--")) document.openFile(QUrl::fromLocalFile(args[1]));
    if(smoke) {
        QTimer::singleShot(1200,&app,[&]{
            auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
#ifdef PHOTOCRAP_TESTING
            if(!window || !runUiSmoke(window,document)) { app.exit(3); return; }
#endif
            const auto frame=window?window->grabWindow():QImage{};
            const int index=args.indexOf("--screenshot");
            bool saved=true;
            if(index>=0 && index+1<args.size()) saved=frame.save(args[index+1]);
            if(frame.isNull() || !saved || qmlWarnings) {qWarning("QML smoke test failed (%d warnings)",qmlWarnings); app.exit(2);}
            else {qInfo("QML smoke test passed: %dx%d",frame.width(),frame.height()); app.exit(0);}
        });
    }
    return app.exec();
}

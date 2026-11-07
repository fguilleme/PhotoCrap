#include "Document.h"
#include "ImageUtils.h"
#include "ai/AiImageController.h"
#include "ai/AiImageTypes.h"
#include "ai/AiSettings.h"
#include "storage/ProjectPng.h"
#include "storage/ProjectWriteDevice.h"
#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSettings>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

namespace
{
QByteArray png(const QColor &_color = Qt::green)
{
    QImage image(32, 24, QImage::Format_ARGB32_Premultiplied);
    image.fill(_color);
    QByteArray bytes;
    QBuffer    buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}
struct Request {
    QByteArray method, target, headers, body;
};
struct Response {
    int        code = 200;
    QByteArray body;
    QByteArray type = "application/json";
    QByteArray extra;
    int        delay = 0;
};
class Server : public QTcpServer
{
public:
    QList<Request>                           requests;
    std::function<Response(const Request &)> handler;
    Server()
    {
        listen(QHostAddress::LocalHost);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection();
                auto  buffer = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                    buffer->append(socket->readAll());
                    const auto at = buffer->indexOf("\r\n\r\n");
                    if (at < 0)
                        return;
                    const auto headers = buffer->left(at);
                    qint64     length  = 0;
                    for (const auto &line : headers.split('\n'))
                        if (line.toLower().startsWith("content-length:"))
                            length = line.mid(15).trimmed().toLongLong();
                    if (buffer->size() < at + 4 + length)
                        return;
                    const auto first = headers.left(headers.indexOf("\r\n")).split(' ');
                    Request    request{first.value(0), first.value(1), headers,
                                    buffer->mid(at + 4, length)};
                    requests.append(request);
                    buffer->clear();
                    const auto response = handler ? handler(request) : Response{500, "{}"};
                    QPointer<QTcpSocket> target(socket);
                    QTimer::singleShot(response.delay, this, [target, response] {
                        if (!target || target->state() != QAbstractSocket::ConnectedState)
                            return;
                        target->write(
                            "HTTP/1.1 " + QByteArray::number(response.code) +
                            " OK\r\nContent-Type: " + response.type +
                            "\r\nContent-Length: " + QByteArray::number(response.body.size()) +
                            "\r\nConnection: close\r\n" + response.extra + "\r\n" + response.body);
                        target->disconnectFromHost();
                    });
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }
    QString url(const QString &_path = {}) const
    {
        return "http://127.0.0.1:" + QString::number(serverPort()) + _path;
    }
};
QByteArray json(const QJsonObject &_object)
{
    return QJsonDocument(_object).toJson(QJsonDocument::Compact);
}
PhotoCrap::Ai::Input input()
{
    PhotoCrap::Ai::Input value;
    value.image    = png(Qt::red);
    value.mask     = png(Qt::transparent);
    value.canvas   = {32, 24};
    value.bounds   = {0, 0, 32, 24};
    value.sentSize = {32, 24};
    return value;
}
QVariantMap provider(AiSettings &_settings, Server &_server, const QString &_kind = "compatible")
{
    auto value     = _settings.newProvider(_kind);
    value["url"]   = _server.url();
    value["model"] = _kind == "fal" ? "fal-ai/flux-2/edit" : "test-model";
    return value;
}
void fixture(Document &_document, int _bits = 16)
{
    _document.setBackgroundEditThreshold(-1);
    QVERIFY(_document.newDocument(120, 90, true, _bits));
    _document.setColor(Qt::red);
    _document.fillWithColor(true);
}
} // namespace
class AiImageTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_settings;
private slots:
    void initTestCase()
    {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
        QCoreApplication::setOrganizationName("PhotoCrapAiTest");
        QCoreApplication::setApplicationName("AiImageTest");
    }
    void settingsPersistProvidersAndPromptsButNeverSessionKeys()
    {
        QSettings().clear();
        const auto credentials = memoryAiCredentialStore();
        QString    saved;
        {
            AiSettings settings(nullptr, true, credentials);
            QCOMPARE(settings.providers().size(), 3);
            QVERIFY(settings.prompts().size() >= 3);
            auto p      = settings.newProvider("compatible");
            p["model"]  = "image-editor";
            p["apiKey"] = "should-not-save";
            saved       = settings.saveProvider(p);
            QVERIFY(!saved.isEmpty());
            settings.setSessionKey(saved, "session-secret");
            QVERIFY(settings.hasKey(saved));
            QVERIFY(!settings
                         .savePrompt(
                             {{"name", "Custom"}, {"text", "Conserve le visage, change le décor."}})
                         .isEmpty());
            QSettings file;
            file.sync();
            QFile raw(file.fileName());
            QVERIFY(raw.open(QIODevice::ReadOnly));
            const auto bytes = raw.readAll();
            QVERIFY(!bytes.contains("session-secret"));
            QVERIFY(!bytes.contains("should-not-save"));
        }
        AiSettings reopened(nullptr, true, credentials);
        QCOMPARE(reopened.provider(saved).value("model").toString(), QString("image-editor"));
        QVERIFY(!reopened.hasKey(saved));
        QCOMPARE(reopened.prompts().last().toMap().value("name").toString(), QString("Custom"));
        QVERIFY(reopened.removeProvider(saved));
        QVERIFY(reopened.provider(saved).isEmpty());
        AiSettings profiles(nullptr, false);
        auto       changed = profiles.newProvider("compatible");
        changed["model"]   = "editor";
        auto id            = profiles.saveProvider(changed);
        profiles.setSessionKey(id, "endpoint-secret");
        changed        = profiles.provider(id);
        changed["url"] = "http://127.0.0.1:9000/v1";
        QVERIFY(!profiles.saveProvider(changed).isEmpty());
        QVERIFY(!profiles.hasKey(id));
        for (const auto &entry : reopened.providers())
            reopened.removeProvider(entry.toMap().value("id").toString());
        for (const auto &entry : reopened.prompts())
            reopened.removePrompt(entry.toMap().value("id").toString());
        AiSettings empty(nullptr, true, credentials);
        QVERIFY(empty.providers().isEmpty());
        QVERIFY(empty.prompts().isEmpty());
    }
    void apiKeysPersistInTheCredentialBackendAndStayOutOfSettings()
    {
        QSettings().clear();
        const auto credentials = memoryAiCredentialStore();
        QString    id;
        {
            AiSettings settings(nullptr, true, credentials);
            QTRY_VERIFY(!settings.credentialsBusy());
            auto profile     = settings.newProvider("compatible");
            profile["model"] = "editor";
            id               = settings.saveProvider(profile);
            QVERIFY(!id.isEmpty());
            settings.saveKey(id, "persistent-test-secret");
            QTRY_VERIFY(!settings.credentialsBusy());
            QCOMPARE(settings.key(id), QString("persistent-test-secret"));
            QCOMPARE(settings.keySource(id), QString("stored"));
            QSettings file;
            file.sync();
            QFile raw(file.fileName());
            QVERIFY(raw.open(QIODevice::ReadOnly));
            QVERIFY(!raw.readAll().contains("persistent-test-secret"));
        }
        {
            AiSettings reopened(nullptr, true, credentials);
            QTRY_VERIFY(!reopened.credentialsBusy());
            QCOMPARE(reopened.key(id), QString("persistent-test-secret"));
            auto profile   = reopened.provider(id);
            profile["url"] = "http://127.0.0.1:9999/v1";
            QVERIFY(!reopened.saveProvider(profile).isEmpty());
            QTRY_VERIFY(!reopened.credentialsBusy());
            QVERIFY(!reopened.hasKey(id));
            reopened.saveKey(id, "replacement-secret");
            QTRY_VERIFY(!reopened.credentialsBusy());
            QCOMPARE(reopened.key(id), QString("replacement-secret"));
            reopened.removeKey(id);
            QTRY_VERIFY(!reopened.credentialsBusy());
            QVERIFY(!reopened.hasKey(id));
        }
        AiSettings removed(nullptr, true, credentials);
        QTRY_VERIFY(!removed.credentialsBusy());
        QVERIFY(!removed.hasKey(id));
    }
    void nativeCredentialStoreRoundTrip()
    {
        if (!qEnvironmentVariableIsSet("PHOTOCRAP_TEST_NATIVE_KEYCHAIN"))
            QSKIP("Native keychain test is opt-in and only uses a disposable dummy entry");
        const auto store = nativeAiCredentialStore();
        const auto id    = "PhotoCrap-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        bool       done  = false;
        QString    writeError, readError, removeError, value;
        store->write(id, "disposable-keychain-test", this,
                     [&](const QString &, const QString &error) {
                         writeError = error;
                         done       = true;
                     });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
        done = false;
        store->read(id, this, [&](const QString &data, const QString &error) {
            value     = data;
            readError = error;
            done      = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
        done = false;
        store->remove(id, this, [&](const QString &, const QString &error) {
            removeError = error;
            done        = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
        QVERIFY2(writeError.isEmpty(), qPrintable(writeError));
        QVERIFY2(readError.isEmpty(), qPrintable(readError));
        QVERIFY2(removeError.isEmpty(), qPrintable(removeError));
        QCOMPARE(value, QString("disposable-keychain-test"));
        done = false;
        store->read(id, this, [&](const QString &data, const QString &error) {
            value     = data;
            readError = error;
            done      = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
        QVERIFY(readError.isEmpty());
        QVERIFY(value.isEmpty());
    }
    void invalidProviderAndWorkflowAreRejected()
    {
        AiSettings settings(nullptr, false);
        auto       value = settings.newProvider("openai");
        value["url"]     = "file:///tmp/image";
        QVERIFY(settings.saveProvider(value).isEmpty());
        value["url"] = "https://user:secret@example.com/v1";
        QVERIFY(settings.saveProvider(value).isEmpty());
        value             = settings.newProvider("comfy");
        value["workflow"] = "{\"nodes\": []}";
        QVERIFY(settings.saveProvider(value).isEmpty());
        value          = settings.newProvider("fal");
        value["model"] = "http://elsewhere";
        QVERIFY(settings.saveProvider(value).isEmpty());
        QVERIFY(settings.savePrompt({{"name", "Empty"}, {"text", " "}}).isEmpty());
    }
    void sourceAndResultPreserveSelectionGeometryAndUndo()
    {
        for (int bits : {8, 16}) {
            Document d;
            fixture(d, bits);
            d.setTool("ellipticalSelect");
            d.beginStroke(20, 20);
            d.continueStroke(80, 70);
            d.endStroke();
            d.modifySelection("feather", 2);
            QTRY_VERIFY(!d.selectionBusy());
            const auto coverage = d.selectionMask();
            const auto original = d.layerImage(0);
            auto       cancel   = std::make_shared<std::atomic_bool>(false);
            auto       future   = d.prepareAiInput(true, 40, cancel);
            QTRY_VERIFY(future.isFinished());
            const auto source = future.result();
            QVERIFY(source.error.isEmpty());
            QVERIFY(source.sentSize.width() <= 40);
            QVERIFY(source.sentSize.height() <= 40);
            const auto sent = QImage::fromData(source.image), mask = QImage::fromData(source.mask);
            QCOMPARE(sent.size(), source.sentSize);
            QCOMPARE(mask.size(), source.sentSize);
            QVERIFY(sent.pixelColor(0, 0).alpha() < 50);
            QVERIFY(mask.pixelColor(0, 0).alpha() > 200);
            QVERIFY(mask.pixelColor(mask.rect().center()).alpha() < 20);
            d.deselect();
            const auto history = d.historyIndex();
            const auto result  = PhotoCrap::Ai::prepareResult(source, png(), bits, cancel);
            QVERIFY(result.error.isEmpty());
            QString error;
            QVERIFY2(
                d.insertAiResult(source, result, {{"provider", "test"}, {"prompt", "edit"}}, error),
                qPrintable(error));
            QCOMPARE(d.layers().size(), 2);
            QCOMPARE(d.layerImage(0), original);
            QCOMPARE(d.maskImage(1), coverage);
            QVERIFY(!d.hasSelection());
            QCOMPARE(d.historyIndex(), history + 1);
            QCOMPARE(d.layerImage(1).pixelColor(source.bounds.center()), QColor(Qt::green));
            QCOMPARE(d.layerImage(1).pixelColor(0, 0).alpha(), 0);
            QCOMPARE(d.layerImage(1).format(), d.workingFormat());
            d.undo();
            QCOMPARE(d.layers().size(), 1);
            QCOMPARE(d.layerImage(0), original);
            d.redo();
            QCOMPARE(d.maskImage(1), coverage);
        }
    }
    void insertingTheSameVariantTwiceDoesNotDuplicateTheLayer()
    {
        for (int bits : {8, 16}) {
            Document d;
            fixture(d, bits);
            auto cancel  = std::make_shared<std::atomic_bool>(false);
            auto capture = d.prepareAiInput(false, 120, cancel);
            QTRY_VERIFY(capture.isFinished());
            const auto source = capture.result();
            const auto result = PhotoCrap::Ai::prepareResult(source, png(), bits, cancel);
            QVERIFY(result.error.isEmpty());
            QVariantMap metadata{{"batch", "one-launch"}, {"variant", 1}, {"variantCount", 2}};
            QString     error;
            const int   history = d.historyIndex();
            QVERIFY(d.insertAiResult(source, result, metadata, error));
            QVERIFY(d.insertAiResult(source, result, metadata, error));
            QCOMPARE(d.layers().size(), 2);
            QCOMPARE(d.historyIndex(), history + 1);
            // Identical pixels from an explicitly requested second variant are
            // separate results. Do not deduplicate unrelated generations.
            metadata["variant"] = 2;
            QVERIFY(d.insertAiResult(source, result, metadata, error));
            QCOMPARE(d.layers().size(), 3);
            QCOMPARE(d.historyIndex(), history + 2);
            d.undo();
            d.undo();
            QCOMPARE(d.layers().size(), 1);
        }
    }
    void bandedResultMatchesOnePassScaling()
    {
        QImage image(79, 67, QImage::Format_RGBA8888);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                image.setPixelColor(x, y, QColor(x * 3, y * 3, (x + y) % 256, (x * y) % 256));
        QByteArray bytes;
        QBuffer    buffer(&bytes);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "PNG"));
        image = QImage::fromData(bytes, "PNG");
        QVERIFY(!image.isNull());
        auto source   = input();
        source.canvas = {700, 650};
        source.bounds = {31, 17, 601, 590};
        auto cancel   = std::make_shared<std::atomic_bool>(false);
        for (int bits : {8, 16}) {
            auto result = PhotoCrap::Ai::prepareResult(source, bytes, bits, cancel);
            QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
            auto expected =
                ImageUtils::blank(source.canvas, bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                                            : QImage::Format_ARGB32_Premultiplied);
            QPainter painter(&expected);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.drawImage(source.bounds, image);
            painter.end();
            QCOMPARE(QImage(result.image), expected);
        }
    }
    void staleDocumentAndInvalidImageCannotInsert()
    {
        Document d;
        fixture(d);
        auto cancel = std::make_shared<std::atomic_bool>(false);
        auto future = d.prepareAiInput(false, 256, cancel);
        QTRY_VERIFY(future.isFinished());
        auto       source  = future.result();
        const auto invalid = PhotoCrap::Ai::prepareResult(source, "not an image", 16, cancel);
        QVERIFY(!invalid.error.isEmpty());
        const auto result = PhotoCrap::Ai::prepareResult(source, png(), 16, cancel);
        d.newDocument(120, 90, true, 16);
        const int history = d.historyIndex();
        QString   error;
        QVERIFY(!d.insertAiResult(source, result, {}, error));
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.historyIndex(), history);
    }
    void aiSourceProjectRoundTripPreservesNativePixelsAndEditedFallback()
    {
        for (int bits : {8, 16}) {
            Document d;
            fixture(d, bits);
            d.setTool("ellipticalSelect");
            d.beginStroke(20, 20);
            d.continueStroke(100, 75);
            d.endStroke();
            d.modifySelection("feather", 2);
            QTRY_VERIFY(!d.selectionBusy());
            auto cancel  = std::make_shared<std::atomic_bool>(false);
            auto capture = d.prepareAiInput(true, 256, cancel);
            QTRY_VERIFY(capture.isFinished());
            QImage model(21, 17, QImage::Format_RGBA8888);
            for (int y = 0; y < model.height(); ++y)
                for (int x = 0; x < model.width(); ++x)
                    model.setPixelColor(x, y,
                                        QColor(x * 11, y * 13, (x + y) * 5, 80 + (x * y) % 176));
            QByteArray bytes;
            QBuffer    buffer(&bytes);
            QVERIFY(buffer.open(QIODevice::WriteOnly));
            QVERIFY(model.save(&buffer, "PNG"));
            const auto result = PhotoCrap::Ai::prepareResult(capture.result(), bytes, bits, cancel);
            QVERIFY(result.error.isEmpty());
            QVERIFY(result.original && result.original->matches(result.image, {120, 90}, bits));
            QString           error;
            const QVariantMap ai{{"provider", "test"},
                                 {"model", "image-model"},
                                 {"variant", 1},
                                 {"variantCount", 1},
                                 {"prompt", "Retouch the selected subject"},
                                 {"date", "2026-11-08T12:00:00Z"}};
            QVERIFY(d.insertAiResult(capture.result(), result, ai, error));
            const auto layerName = d.layers().first().toMap().value("name").toString();
            QVERIFY(layerName.contains("test"));
            QVERIFY(layerName.contains("image-model"));
            QVERIFY(layerName.contains("1/1"));
            const auto    image = d.layerImage(1), mask = d.maskImage(1), composite = d.composite();
            QTemporaryDir directory;
            const auto    url = QUrl::fromLocalFile(directory.filePath("source.pcr"));
            QVERIFY(d.saveProject(url));
            const auto read = [&] {
                QFile file(url.toLocalFile());
                if (!file.open(QIODevice::ReadOnly))
                    return QJsonObject{};
                return QJsonDocument::fromJson(file.readAll()).object();
            };
            auto root = read();
            QCOMPARE(root["version"].toInt(), 13);
            auto saved = root["layers"].toArray()[1].toObject();
            QVERIFY(saved.contains("aiRaster"));
            QCOMPARE(QByteArray::fromBase64(saved["png"].toString().toLatin1()), bytes);
            Document reopened;
            reopened.setBackgroundEditThreshold(-1);
            QVERIFY(reopened.openFile(url));
            QCOMPARE(reopened.channelBits(), bits);
            QCOMPARE(reopened.layerImage(1), image);
            QCOMPARE(reopened.maskImage(1), mask);
            QCOMPARE(reopened.composite(), composite);
            QCOMPARE(reopened.layers().first().toMap().value("name").toString(), layerName);
            QCOMPARE(
                reopened.layers().first().toMap().value("parameters").toMap().value("_ai").toMap(),
                ai);
            // Only a pixel edit invalidates the compact image source.
            reopened.setEditingMask(true);
            reopened.setColor(Qt::black);
            reopened.fillWithColor(true);
            const auto editedMask = reopened.maskImage(1);
            QVERIFY(reopened.saveProject(url));
            QVERIFY(read()["layers"].toArray()[1].toObject().contains("aiRaster"));
            reopened.setEditingMask(false);
            reopened.setColor(Qt::blue);
            reopened.fillWithColor(true);
            const auto edited = reopened.layerImage(1);
            QVERIFY(reopened.saveProject(url));
            saved = read()["layers"].toArray()[1].toObject();
            QVERIFY(!saved.contains("aiRaster"));
            const auto png =
                QImage::fromData(QByteArray::fromBase64(saved["png"].toString().toLatin1()), "PNG");
            QCOMPARE(png.size(), QSize(120, 90));
            Document raster;
            QVERIFY(raster.openFile(url));
            QCOMPARE(raster.layerImage(1), edited);
            QCOMPARE(raster.maskImage(1), editedMask);
            reopened.undo();
            QVERIFY(reopened.saveProject(url));
            QVERIFY(read()["layers"].toArray()[1].toObject().contains("aiRaster"));
            // Invalid recipes never replace an already open document.
            root              = read();
            auto layers       = root["layers"].toArray();
            auto layer        = layers[1].toObject();
            auto recipe       = layer["aiRaster"].toObject();
            recipe["x"]       = -1;
            layer["aiRaster"] = recipe;
            layers[1]         = layer;
            root["layers"]    = layers;
            QFile corrupt(directory.filePath("corrupt.pcr"));
            QVERIFY(corrupt.open(QIODevice::WriteOnly));
            corrupt.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
            corrupt.close();
            const auto before = raster.composite();
            QVERIFY(!raster.openFile(QUrl::fromLocalFile(corrupt.fileName())));
            QCOMPARE(raster.composite(), before);
        }
    }
    void threeAiLayersStoreOriginalSizeInsteadOfEnlarged16BitRasters()
    {
        class Counter final : public QIODevice
        {
        public:
            qint64 bytes = 0;
            Counter()
            {
                open(QIODevice::WriteOnly | QIODevice::Unbuffered);
            }
            qint64 readData(char *, qint64) override
            {
                return -1;
            }
            qint64 writeData(const char *, qint64 count) override
            {
                bytes += count;
                return count;
            }
        } legacy;
        Document d;
        d.setBackgroundEditThreshold(-1);
        QVERIFY(d.newDocument(4096, 1024, true, 16));
        auto cancel  = std::make_shared<std::atomic_bool>(false);
        auto capture = d.prepareAiInput(false, 1536, cancel);
        QTRY_VERIFY(capture.isFinished());
        qint64  originalBytes = 0;
        quint32 rng           = 0x932765ab;
        for (int variant = 0; variant < 3; ++variant) {
            QImage model(512, 128, QImage::Format_RGB32);
            for (int y = 0; y < model.height(); ++y) {
                auto *row = reinterpret_cast<QRgb *>(model.scanLine(y));
                for (int x = 0; x < model.width(); ++x) {
                    rng ^= rng << 13;
                    rng ^= rng >> 17;
                    rng ^= rng << 5;
                    row[x] = qRgb(rng & 255, (rng >> 8) & 255, (rng >> 16) & 255);
                }
            }
            QByteArray bytes;
            QBuffer    buffer(&bytes);
            QVERIFY(buffer.open(QIODevice::WriteOnly));
            QVERIFY(model.save(&buffer, "PNG"));
            originalBytes += bytes.size();
            const auto prepared = PhotoCrap::Ai::prepareResult(capture.result(), bytes, 16, cancel);
            QVERIFY(prepared.error.isEmpty());
            QString error;
            QVERIFY(d.insertAiResult(capture.result(), prepared,
                                     {{"provider", "probe"},
                                      {"variant", variant + 1},
                                      {"variantCount", 3},
                                      {"batch", "probe"}},
                                     error));
            Base64WriteDevice encoder(legacy);
            QVERIFY(PhotoCrap::Storage::writeProjectPng(encoder, prepared.image, {}));
            QVERIFY(encoder.finish());
        }
        QTemporaryDir directory;
        const auto    url = QUrl::fromLocalFile(directory.filePath("three-ai.pcr"));
        QVERIFY(d.saveProject(url));
        const auto stored = QFileInfo(url.toLocalFile()).size();
        qInfo()
            << "AI storage probe: 4096x1024, three 512x128 results, 16-bit document; legacy base64"
            << legacy.bytes << "bytes, compact project" << stored << "bytes, original model images"
            << originalBytes << "bytes";
        QVERIFY2(stored < legacy.bytes / 5,
                 "AI originals should be far smaller than enlarged native rasters");
        QVERIFY(stored < originalBytes * 4 / 3 + 64 * 1024);
        Document reopened;
        QVERIFY(reopened.openFile(url));
        QCOMPARE(reopened.layers().size(), 4);
        QCOMPARE(reopened.channelBits(), 16);
        for (int i = 0; i < 4; ++i)
            QCOMPARE(reopened.layerImage(i), d.layerImage(i));
        QCOMPARE(reopened.composite(), d.composite());
    }
    void openAiUsesMultipartAndPreservesMaskConvention()
    {
        Server server;
        QVERIFY2(server.isListening(), qPrintable(server.errorString()));
        AiSettings settings(nullptr, false);
        const auto bytes = png();
        server.handler   = [&](const Request &) {
            return Response{
                200, json({{"data", QJsonArray{QJsonObject{
                                        {"b64_json", QString::fromLatin1(bytes.toBase64())}}}}})};
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        auto       source = input();
        source.selected   = true;
        job.start(provider(settings, server, "openai"), "test-key", "Un décor \"bleu\"\npréservé",
                  source);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QCOMPARE(finished.first().at(0).toByteArray(), bytes);
        QVERIFY(finished.first().at(1).toString().isEmpty());
        QCOMPARE(server.requests.size(), 1);
        const auto request = server.requests.first();
        QCOMPARE(request.target, QByteArray("/images/edits"));
        QVERIFY(request.headers.contains("Bearer test-key"));
        QVERIFY(request.body.contains("name=\"mask\""));
        QVERIFY(request.body.contains(source.image));
        QVERIFY(request.body.contains(source.mask));
        QVERIFY(request.body.contains(QStringLiteral("préservé").toUtf8()));
    }
    void falQueueDownloadsResultWithoutForwardingCredentials()
    {
        Server server;
        QVERIFY(server.isListening());
        AiSettings settings(nullptr, false);
        const auto bytes = png();
        server.handler   = [&](const Request &_request) {
            if (_request.method == "POST")
                return Response{200, json({{"status_url", server.url("/status")},
                                           {"response_url", server.url("/result")},
                                           {"cancel_url", server.url("/cancel")}})};
            if (_request.target == "/status")
                return Response{200, "{\"status\":\"COMPLETED\"}"};
            if (_request.target == "/result")
                return Response{
                    200,
                    json({{"images", QJsonArray{QJsonObject{{"url", server.url("/image")}}}}})};
            if (_request.target == "/image")
                return Response{302, {}, "image/png", "Location: /final\r\n"};
            return Response{200, bytes, "image/png"};
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        job.start(provider(settings, server, "fal"), "fal-key", "Edit", input());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QVERIFY2(finished.first().at(1).toString().isEmpty(),
                 qPrintable(finished.first().at(1).toString()));
        QCOMPARE(finished.first().at(0).toByteArray(), bytes);
        QCOMPARE(server.requests.size(), 5);
        QVERIFY(server.requests.first().headers.contains("Key fal-key"));
        const auto data = QJsonDocument::fromJson(server.requests.first().body).object();
        QVERIFY(data.value("image_urls")
                    .toArray()
                    .first()
                    .toString()
                    .startsWith("data:image/png;base64,"));
        QVERIFY(!server.requests[3].headers.toLower().contains("authorization:"));
        QVERIFY(!server.requests[4].headers.toLower().contains("authorization:"));
    }
    void comfyUploadsSubstitutesAndReadsOwnHistory()
    {
        Server server;
        QVERIFY(server.isListening());
        AiSettings  settings(nullptr, false);
        const auto  bytes   = png();
        int         uploads = 0;
        QJsonObject workflow;
        server.handler = [&](const Request &_request) {
            if (_request.target == "/upload/image")
                return Response{200,
                                json({{"name", ++uploads % 2 == 1 ? "source.png" : "mask.png"}})};
            if (_request.target == "/prompt") {
                workflow =
                    QJsonDocument::fromJson(_request.body).object().value("prompt").toObject();
                return Response{200, "{\"prompt_id\":\"own-job\"}"};
            }
            if (_request.target == "/history/own-job")
                return Response{
                    200,
                    json({{"own-job",
                           QJsonObject{
                               {"status",
                                QJsonObject{{"completed", true}, {"status_str", "success"}}},
                               {"outputs",
                                QJsonObject{
                                    {"8", QJsonObject{{"images", QJsonArray{QJsonObject{
                                                                     {"filename", "result.png"},
                                                                     {"subfolder", ""},
                                                                     {"type", "output"}}}}}}}}}}})};
            return Response{200, bytes, "image/png"};
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        job.start(provider(settings, server, "comfy"), {}, "Texte \"exact\"\navec accents",
                  input());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QVERIFY2(finished.first().at(1).toString().isEmpty(),
                 qPrintable(finished.first().at(1).toString()));
        QCOMPARE(finished.first().at(0).toByteArray(), bytes);
        QCOMPARE(uploads, 2);
        QCOMPARE(workflow["1"].toObject()["inputs"].toObject()["image"].toString(),
                 QString("source.png"));
        QCOMPARE(workflow["3"].toObject()["inputs"].toObject()["text"].toString(),
                 QString("Texte \"exact\"\navec accents"));
        QVERIFY(workflow["6"].toObject()["inputs"].toObject()["seed"].isDouble());
        QVERIFY(server.requests.last().target.startsWith("/view?"));
        QVERIFY(server.requests.last().target.contains("result.png"));
        const auto firstSeed = workflow["6"].toObject()["inputs"].toObject()["seed"].toDouble();
        finished.clear();
        job.start(provider(settings, server, "comfy"), {}, "Second variant", input());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QVERIFY(finished.first().at(1).toString().isEmpty());
        QCOMPARE(uploads, 4);
        QCOMPARE(finished.first().at(0).toByteArray(), bytes);
        QVERIFY(workflow["6"].toObject()["inputs"].toObject()["seed"].toDouble() != firstSeed);
    }
    void errorsAreRedactedAndForeignQueueUrlsRejected()
    {
        Server server;
        QVERIFY(server.isListening());
        AiSettings settings(nullptr, false);
        server.handler = [](const Request &) {
            return Response{401, "{\"error\":{\"message\":\"invalid secret-value\"}}"};
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        job.start(provider(settings, server, "openai"), "secret-value", "Edit", input());
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!finished.first().at(1).toString().contains("secret-value"));
        QVERIFY(finished.first().at(1).toString().contains("401"));
        finished.clear();
        server.requests.clear();
        server.handler = [](const Request &) {
            return Response{200, "{\"status_url\":\"https://outside.invalid/"
                                 "status\",\"response_url\":\"https://outside.invalid/result\"}"};
        };
        job.start(provider(settings, server, "fal"), "key", "Edit", input());
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!finished.first().at(1).toString().isEmpty());
        QCOMPARE(server.requests.size(), 1);
    }
    void controllerAddsLayerWithoutBlockingOrOverwritingConcurrentEdits()
    {
        Server server;
        QVERIFY(server.isListening());
        AiSettings settings(nullptr, false);
        Document   d;
        fixture(d);
        const auto bytes = png();
        server.handler   = [&](const Request &) {
            return Response{
                200,
                json({{"data", QJsonArray{QJsonObject{
                                   {"b64_json", QString::fromLatin1(bytes.toBase64())}}}}}),
                "application/json",
                {},
                150};
        };
        const auto id = settings.saveProvider(provider(settings, server));
        QVERIFY(!id.isEmpty());
        AiImageController controller(&d, &settings);
        QSignalSpy        added(&controller, &AiImageController::layerAdded);
        QVERIFY(controller.generate(id, "Edit", false));
        QVERIFY(controller.busy());
        QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 5000);
        d.setColor(Qt::blue);
        d.fillWithColor(true);
        const int history = d.historyIndex();
        QTRY_COMPARE_WITH_TIMEOUT(added.count(), 1, 5000);
        QVERIFY(!controller.busy());
        QVERIFY(controller.error().isEmpty());
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.layerImage(0).pixelColor(0, 0), QColor(Qt::blue));
        QCOMPARE(d.layerImage(1).pixelColor(0, 0), QColor(Qt::green));
        QCOMPARE(d.historyIndex(), history + 1);
        QVERIFY(d.layers().first().toMap().value("parameters").toMap().contains("_ai"));
        d.undo();
        QCOMPARE(d.composite().pixelColor(0, 0), QColor(Qt::blue));
    }
    void repeatedCompletionCreatesOnlyOneLayerPerRequestedVariant()
    {
        Server server;
        QVERIFY(server.isListening());
        AiSettings settings(nullptr, false);
        Document   d;
        fixture(d);
        server.handler = [](const Request &) {
            return Response{
                200, json({{"data", QJsonArray{QJsonObject{
                                        {"b64_json", QString::fromLatin1(png().toBase64())}}}}})};
        };
        const auto        id = settings.saveProvider(provider(settings, server));
        AiImageController controller(&d, &settings);
        auto *            job = controller.findChild<AiImageJob *>();
        QVERIFY(job);
        // Reproduce a second delivery while the first result is decoding.
        bool replaying = false;
        connect(job, &AiImageJob::finished, &controller,
                [&](const QByteArray &bytes, const QString &error) {
                    if (replaying)
                        return;
                    replaying = true;
                    emit job->finished(bytes, error);
                    replaying = false;
                });
        QSignalSpy added(&controller, &AiImageController::layerAdded);
        const int  history = d.historyIndex();
        QVERIFY(controller.generate(id, "One result", false));
        QVERIFY(!controller.generate(id, "Double click", false));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QTest::qWait(100);
        QCOMPARE(server.requests.size(), 1);
        QCOMPARE(added.count(), 1);
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.historyIndex(), history + 1);
        controller.retryInsertion();
        QCOMPARE(d.layers().size(), 2);
        QVERIFY(controller.error().isEmpty());
        QVERIFY(controller.generate(id, "Two intentional variants", false, 2));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QTest::qWait(100);
        QCOMPARE(server.requests.size(), 3);
        QCOMPARE(added.count(), 3);
        QCOMPARE(d.layers().size(), 4);
        QCOMPARE(controller.completedVariants(), 2);
        QCOMPARE(d.historyIndex(), history + 3);
    }
    void variantsReuseTheOriginalSelectionAndHaveIndependentUndo()
    {
        Server server;
        QVERIFY(server.isListening());
        AiSettings settings(nullptr, false);
        Document   d;
        fixture(d);
        d.setTool("ellipticalSelect");
        d.beginStroke(20, 20);
        d.continueStroke(80, 70);
        d.endStroke();
        d.modifySelection("feather", 2);
        QTRY_VERIFY(!d.selectionBusy());
        const auto          mask     = d.selectionMask();
        const auto          original = d.layerImage(0);
        int                 history  = d.historyIndex();
        const QList<QColor> colors{Qt::green, Qt::blue, Qt::yellow};
        QList<QByteArray>   sources;
        server.handler = [&](const Request &request) {
            const auto field = request.body.indexOf("name=\"image\"");
            const auto start = request.body.indexOf("\r\n\r\n", field) + 4;
            const auto end   = request.body.indexOf("\r\n--", start);
            sources.append(request.body.mid(start, end - start));
            const auto bytes = png(colors[sources.size() - 1]);
            return Response{
                200,
                json({{"data", QJsonArray{QJsonObject{
                                   {"b64_json", QString::fromLatin1(bytes.toBase64())}}}}}),
                "application/json",
                {},
                100};
        };
        const auto        id = settings.saveProvider(provider(settings, server));
        AiImageController controller(&d, &settings);
        QSignalSpy        added(&controller, &AiImageController::layerAdded);
        QVERIFY(controller.generate(id, "Three alternatives", true, 3));
        QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 5000);
        d.deselect();
        history = d.historyIndex();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        QCOMPARE(added.count(), 3);
        QVERIFY(controller.error().isEmpty());
        QCOMPARE(server.requests.size(), 3);
        QVERIFY(!sources.first().isEmpty());
        QCOMPARE(sources[0], sources[1]);
        QCOMPARE(sources[0], sources[2]);
        QCOMPARE(d.layers().size(), 4);
        QCOMPARE(d.layerImage(0), original);
        QString batch;
        for (int i = 1; i <= 3; ++i) {
            QCOMPARE(d.layerImage(i).pixelColor(50, 45), colors[i - 1]);
            QCOMPARE(d.maskImage(i), mask);
            bool found = false;
            for (const auto &row : d.layers()) {
                const auto layer = row.toMap();
                const auto ai    = layer.value("parameters").toMap().value("_ai").toMap();
                if (ai.value("variant").toInt() == i) {
                    found = true;
                    if (batch.isEmpty())
                        batch = ai.value("batch").toString();
                    QCOMPARE(ai.value("batch").toString(), batch);
                    QCOMPARE(ai.value("variantCount").toInt(), 3);
                    QCOMPARE(layer.value("visible").toBool(), i == 3);
                    QVERIFY(layer.value("name").toString().contains(QString::number(i)));
                    QVERIFY(layer.value("name").toString().contains(QString::number(i) + "/3"));
                }
            }
            QVERIFY(found);
        }
        QVERIFY(!batch.isEmpty());
        QCOMPARE(d.historyIndex(), history + 3);
        d.undo();
        QCOMPARE(d.layers().size(), 3);
        QVERIFY(d.layers().first().toMap().value("visible").toBool());
        d.undo();
        QCOMPARE(d.layers().size(), 2);
        QVERIFY(d.layers().first().toMap().value("visible").toBool());
        d.undo();
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(server.requests.size(), 3);
    }
    void cancelingOrFailingABatchPreservesCompletedVariants()
    {
        for (bool cancel : {true, false}) {
            Server server;
            QVERIFY(server.isListening());
            AiSettings settings(nullptr, false);
            Document   d;
            fixture(d);
            int requests   = 0;
            server.handler = [&](const Request &) {
                if (++requests > 1)
                    return Response{400, "{\"error\":\"model unavailable\"}"};
                return Response{
                    200,
                    json({{"data", QJsonArray{QJsonObject{
                                       {"b64_json", QString::fromLatin1(png().toBase64())}}}}})};
            };
            const auto        id = settings.saveProvider(provider(settings, server));
            AiImageController controller(&d, &settings);
            QSignalSpy        added(&controller, &AiImageController::layerAdded);
            if (cancel)
                connect(&controller, &AiImageController::layerAdded, &controller,
                        &AiImageController::cancel);
            QVERIFY(controller.generate(id, "Three alternatives", false, 3));
            QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
            QCOMPARE(added.count(), 1);
            QCOMPARE(d.layers().size(), 2);
            QCOMPARE(server.requests.size(), cancel ? 1 : 2);
            QCOMPARE(controller.error().isEmpty(), cancel);
            QVERIFY(d.layers().first().toMap().value("visible").toBool());
            d.undo();
            QCOMPARE(d.layers().size(), 1);
        }
    }
    void progressExposesRunDetailsAndElapsedTime()
    {
        Server server;
        QVERIFY(server.isListening());
        AiSettings settings(nullptr, false);
        Document   d;
        fixture(d);
        server.handler = [](const Request &) {
            return Response{
                200,
                json({{"data", QJsonArray{QJsonObject{
                                   {"b64_json", QString::fromLatin1(png().toBase64())}}}}}),
                "application/json",
                {},
                1400};
        };
        auto profile         = provider(settings, server);
        profile["name"]      = "Progress test";
        const auto        id = settings.saveProvider(profile);
        AiImageController controller(&d, &settings);
        QVERIFY(controller.generate(id, "Edit", false, 1));
        QCOMPARE(controller.providerName(), QString("Progress test"));
        QCOMPARE(controller.modelName(), QString("test-model"));
        QCOMPARE(controller.variantCount(), 1);
        QCOMPARE(controller.completedVariants(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(controller.elapsedSeconds() >= 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QCOMPARE(controller.completedVariants(), 1);
        QVERIFY(controller.elapsedSeconds() >= 1);
        QCOMPARE(controller.providerName(), QString("Progress test"));
        const int elapsed = controller.elapsedSeconds();
        QTest::qWait(30);
        QCOMPARE(controller.elapsedSeconds(), elapsed);
    }
    void completedResultWaitsForTheCurrentBrushGesture()
    {
        Server server;
        QVERIFY(server.isListening());
        AiSettings settings(nullptr, false);
        Document   d;
        fixture(d);
        const auto bytes = png();
        server.handler   = [&](const Request &) {
            return Response{
                200,
                json({{"data", QJsonArray{QJsonObject{
                                   {"b64_json", QString::fromLatin1(bytes.toBase64())}}}}}),
                "application/json",
                {},
                100};
        };
        const auto        id = settings.saveProvider(provider(settings, server));
        AiImageController controller(&d, &settings);
        QSignalSpy        added(&controller, &AiImageController::layerAdded);
        QVERIFY(controller.generate(id, "Edit", false));
        QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 5000);
        d.setTool("brush");
        d.setColor(Qt::blue);
        d.beginStroke(40, 40);
        QTRY_VERIFY_WITH_TIMEOUT(controller.pendingResult(), 5000);
        QTest::qWait(150);
        QVERIFY(controller.busy());
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.activeLayer(), 0);
        d.continueStroke(50, 40);
        d.endStroke();
        QTRY_COMPARE_WITH_TIMEOUT(added.count(), 1, 5000);
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.layerImage(0).pixelColor(40, 40), QColor(Qt::blue));
    }
    void aLayerLimitPausesVariantsWithoutRepeatingRequests()
    {
        Server server;
        QVERIFY(server.isListening());
        AiSettings settings(nullptr, false);
        Document   d;
        fixture(d);
        const auto bytes = png();
        server.handler   = [&](const Request &) {
            return Response{
                200,
                json({{"data", QJsonArray{QJsonObject{
                                   {"b64_json", QString::fromLatin1(bytes.toBase64())}}}}}),
                "application/json",
                {},
                100};
        };
        const auto        id = settings.saveProvider(provider(settings, server));
        AiImageController controller(&d, &settings);
        QSignalSpy        added(&controller, &AiImageController::layerAdded);
        QVERIFY(controller.generate(id, "Edit", false, 2));
        QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 5000);
        for (int i = 1; i < 512; ++i)
            d.addLayer();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
        QVERIFY(controller.pendingResult());
        QVERIFY(!controller.error().isEmpty());
        QCOMPARE(added.count(), 0);
        QCOMPARE(server.requests.size(), 1);
        d.removeLayer();
        d.removeLayer();
        controller.retryInsertion();
        QTRY_COMPARE_WITH_TIMEOUT(added.count(), 2, 5000);
        QCOMPARE(d.layers().size(), 512);
        QCOMPARE(server.requests.size(), 2);
        QVERIFY(!controller.pendingResult());
    }
    void cancellationTargetsOnlyItsQueuedTask()
    {
        for (const QString kind : {"fal", "comfy"}) {
            Server server;
            QVERIFY(server.isListening());
            AiSettings settings(nullptr, false);
            server.handler = [&](const Request &request) {
                if (request.target == "/upload/image")
                    return Response{200, "{\"name\":\"input.png\"}"};
                if (request.target == "/prompt")
                    return Response{200, "{\"prompt_id\":\"own-queued-job\"}"};
                if (request.target.startsWith("/history/"))
                    return Response{200, "{}"};
                if (request.target == "/status")
                    return Response{200, "{\"status\":\"IN_QUEUE\"}"};
                if (request.method == "POST" && kind == "fal")
                    return Response{200, json({{"status_url", server.url("/status")},
                                               {"response_url", server.url("/result")},
                                               {"cancel_url", server.url("/cancel-own")}})};
                return Response{200, "{}"};
            };
            AiImageJob job;
            QSignalSpy finished(&job, &AiImageJob::finished);
            job.start(provider(settings, server, kind), "key", "Edit", input());
            const int requests = kind == "fal" ? 2 : 4;
            QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), requests, 5000);
            QTest::qWait(30);
            job.cancel();
            QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), requests + 1, 5000);
            QCOMPARE(finished.count(), 0);
            const auto last = server.requests.last();
            if (kind == "fal") {
                QCOMPARE(last.method, QByteArray("PUT"));
                QCOMPARE(last.target, QByteArray("/cancel-own"));
            } else {
                QCOMPARE(last.method, QByteArray("POST"));
                QCOMPARE(last.target, QByteArray("/queue"));
                QCOMPARE(QJsonDocument::fromJson(last.body).object().value("delete").toArray(),
                         QJsonArray{"own-queued-job"});
            }
            for (const auto &request : server.requests)
                QVERIFY(request.target != "/interrupt");
        }
    }
    void cancellationAndDocumentSwitchDropLateResults()
    {
        for (bool change : {false, true}) {
            Server server;
            QVERIFY(server.isListening());
            AiSettings settings(nullptr, false);
            Document   d;
            fixture(d);
            const auto bytes = png();
            server.handler   = [&](const Request &) {
                return Response{
                    200,
                    json({{"data", QJsonArray{QJsonObject{
                                       {"b64_json", QString::fromLatin1(bytes.toBase64())}}}}}),
                    "application/json",
                    {},
                    200};
            };
            const auto        id = settings.saveProvider(provider(settings, server));
            AiImageController controller(&d, &settings);
            QSignalSpy        added(&controller, &AiImageController::layerAdded);
            QVERIFY(controller.generate(id, "Edit", false));
            QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, 5000);
            if (change)
                d.newDocument(120, 90, true, 16);
            else
                controller.cancel();
            QTRY_VERIFY(!controller.busy());
            QTest::qWait(300);
            QCOMPARE(added.count(), 0);
            QCOMPARE(d.layers().size(), 1);
        }
    }
};
QTEST_MAIN(AiImageTest)
#include "AiImageTest.moc"

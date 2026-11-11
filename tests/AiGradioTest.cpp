#include "Document.h"
#include "DocumentManager.h"
#include "ai/AiImageJob.h"
#include "ai/AiSettings.h"
#include <QBuffer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
QByteArray json(const QJsonObject &value)
{
    return QJsonDocument(value).toJson(QJsonDocument::Compact);
}
QByteArray png()
{
    QImage image(24, 18, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::green);
    QByteArray bytes;
    QBuffer    buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}
QString imageUri()
{
    return "data:image/png;base64," + QString::fromLatin1(png().toBase64());
}
struct Request {
    QByteArray method, path, headers, body;
    int        connection = 0;

    QByteArray header(const QByteArray &_name) const
    {
        for (const auto &line : headers.split('\n')) {
            const auto colon = line.indexOf(':');
            if (colon > 0 && line.left(colon).compare(_name, Qt::CaseInsensitive) == 0)
                return line.mid(colon + 1).trimmed();
        }
        return {};
    }
};
struct Response {
    int               code = 200;
    QByteArray        body = "{}", type = "application/json", extra;
    QList<QByteArray> chunks;
    bool              keepOpen = false;
    bool              silent = false, drop = false;
};
class Server : public QTcpServer
{
public:
    QList<Request>                           requests;
    int                                      connections = 0;
    std::function<Response(const Request &)> handler;
    Server()
    {
        listen(QHostAddress::LocalHost);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto      socket     = nextPendingConnection();
                const int connection = ++connections;
                auto      bytes      = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, bytes, connection] {
                    *bytes += socket->readAll();
                    const auto at = bytes->indexOf("\r\n\r\n");
                    if (at < 0)
                        return;
                    const auto headers = bytes->left(at);
                    qint64     length  = 0;
                    for (const auto &line : headers.split('\n'))
                        if (line.toLower().startsWith("content-length:"))
                            length = line.mid(15).trimmed().toLongLong();
                    if (bytes->size() < at + 4 + length)
                        return;
                    const auto    first = headers.left(headers.indexOf("\r\n")).split(' ');
                    const Request request{first.value(0), first.value(1), headers,
                                          bytes->mid(at + 4, length), connection};
                    requests.append(request);
                    bytes->clear();
                    const auto response = handler ? handler(request) : Response{};
                    if (response.silent)
                        return;
                    if (response.drop) {
                        socket->disconnectFromHost();
                        return;
                    }
                    const auto streaming = !response.chunks.isEmpty();
                    socket->write(
                        "HTTP/1.1 " + QByteArray::number(response.code) +
                        " Result\r\nContent-Type: " + response.type + "\r\n" + response.extra +
                        (streaming ? QByteArray("Connection: keep-alive\r\n\r\n")
                                   : "Content-Length: " + QByteArray::number(response.body.size()) +
                                         (response.keepOpen ? "\r\nConnection: keep-alive\r\n\r\n"
                                                            : "\r\nConnection: close\r\n\r\n")));
                    if (!streaming) {
                        socket->write(response.body);
                        if (!response.keepOpen)
                            socket->disconnectFromHost();
                        return;
                    }
                    for (int i = 0; i < response.chunks.size(); ++i) {
                        QPointer<QTcpSocket> target(socket);
                        QTimer::singleShot(i * 5, this, [target, response, i] {
                            if (!target || target->state() != QAbstractSocket::ConnectedState)
                                return;
                            target->write(response.chunks[i]);
                            if (i + 1 == response.chunks.size() && !response.keepOpen)
                                target->disconnectFromHost();
                        });
                    }
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }
    QString url() const
    {
        return "http://127.0.0.1:" + QString::number(serverPort()) + "/mounted";
    }
};
QVariantMap profile(AiSettings &settings, const Server &server)
{
    auto p        = settings.newProvider("gradio");
    p["url"]      = server.url();
    p["username"] = "photo-user";
    p["options"] =
        R"({"lora_adapter":"Photo-to-Anime","seed":42,"randomize_seed":false,"guidance_scale":1.0,"steps":4})";
    return p;
}
PhotoCrap::Ai::Input input()
{
    PhotoCrap::Ai::Input i;
    i.image    = png();
    i.canvas   = {24, 18};
    i.bounds   = {0, 0, 24, 18};
    i.sentSize = i.canvas;
    return i;
}
Response stream(const QByteArray &data, bool open = true)
{
    Response response;
    response.type          = "text/event-stream";
    response.keepOpen      = open;
    const QByteArray bytes = ": comment\r\nevent: heartbeat\r\ndata: null\r\n\r\nevent: "
                             "generating\r\ndata: []\r\n\r\nevent: complete\r\ndata: " +
                             data + "\r\n\r\n";
    for (qsizetype at = 0; at < bytes.size(); at += 23)
        response.chunks.append(bytes.mid(at, 23));
    return response;
}
} // namespace

class AiGradioTest : public QObject
{
    Q_OBJECT
private slots:
    void basicPasswordSurvivesRestartWithoutPlaintextSettings()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QCoreApplication::setOrganizationName("PhotoCrapTests");
        QCoreApplication::setApplicationName("QwenCredentialRestart");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
        const auto store = memoryAiCredentialStore();
        QString    id;
        Server     server;
        {
            AiSettings settings(nullptr, true, store);
            id = settings.saveProvider(profile(settings, server));
            QVERIFY(!id.isEmpty());
            settings.saveKey(id, " exact password ");
            QTRY_VERIFY(!settings.credentialsBusy());
        }
        AiSettings restarted(nullptr, true, store);
        QCOMPARE(restarted.provider(id).value("username").toString(), QString("photo-user"));
        QString password;
        restarted.resolveKey(id, &restarted, [&](const QString &value, const QString &error) {
            QVERIFY(error.isEmpty());
            password = value;
        });
        QTRY_COMPARE(password, QString(" exact password "));
        QFile settingsFile(QSettings().fileName());
        QVERIFY(settingsFile.open(QIODevice::ReadOnly));
        QVERIFY(!settingsFile.readAll().contains("exact password"));
    }
    void selectionAndGenerationParametersReachTheResult_data()
    {
        QTest::addColumn<bool>("newDocument");
        QTest::newRow("new-layer") << false;
        QTest::newRow("native-new-document") << true;
    }
    void selectionAndGenerationParametersReachTheResult()
    {
        QFETCH(bool, newDocument);
        AiSettings settings(nullptr, false);
        Server     server;
        server.handler = [&](const Request &request) {
            return request.method == "POST" ? Response{200, "{\"event_id\":\"selection\"}"}
                                            : stream(json({{"image", imageUri()}, {"seed", 987}}));
        };
        const auto id = settings.saveProvider(profile(settings, server));
        settings.setSessionKey(id, "password");
        DocumentManager documents(&settings);
        QVERIFY(documents.newDocument(96, 80, true, 16));
        auto *origin = documents.activeDocument();
        origin->setTool("ellipticalSelect");
        origin->beginStroke(10, 12);
        origin->continueStroke(60, 60);
        origin->endStroke();
        const auto history    = origin->historyIndex();
        const int  count      = documents.count();
        auto *     controller = documents.activeAi();
        QVERIFY(controller->generate(id, "Adapt only the selection", true, 1, false,
                                     {{"lora_adapter", "Upscaler"},
                                      {"seed", 91},
                                      {"randomize_seed", false},
                                      {"steps", 8},
                                      {"guidance_scale", .75}},
                                     newDocument));
        QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), 5000);
        QVERIFY2(controller->error().isEmpty(), qPrintable(controller->error()));
        Document *target = origin;
        if (newDocument) {
            QCOMPARE(documents.count(), count + 1);
            target =
                documents.document(documents.documents().last().toMap().value("id").toString());
            QVERIFY(target);
            QVERIFY(target != origin);
            QCOMPARE(QSize(target->width(), target->height()), QSize(24, 18));
            QCOMPARE(origin->historyIndex(), history);
        }
        QCOMPARE(target->channelBits(), 16);
        const auto metadata =
            target->layers().first().toMap().value("parameters").toMap().value("_ai").toMap();
        QCOMPARE(metadata.value("seed").toInt(), 987);
        const auto parameters = metadata.value("qwenParameters").toMap();
        QCOMPARE(parameters.value("lora_adapter").toString(), QString("Upscaler"));
        QCOMPARE(parameters.value("steps").toInt(), 8);
        QCOMPARE(parameters.value("guidance_scale").toDouble(), .75);
        const auto body = QJsonDocument::fromJson(server.requests.first().body).object();
        QCOMPARE(body.value("seed").toInt(), 91);
        QCOMPARE(body.value("lora_adapter").toString(), QString("Upscaler"));
        const auto mask = target->maskImage(target->activeLayer());
        QVERIFY(!mask.isNull());
        QCOMPARE(mask.size(), QSize(target->width(), target->height()));
        QVERIFY(qGray(mask.pixel(0, 0)) == 0);
        const auto center = newDocument ? QPoint(12, 9) : QPoint(35, 35);
        QVERIFY(qGray(mask.pixel(center)) > 0);
        if (!newDocument) {
            origin->undo();
            QCOMPARE(origin->historyIndex(), history);
        }
    }
    void settingsValidateAndIsolateBasicPasswords()
    {
        AiSettings settings(nullptr, false);
        Server     server;
        auto       p  = profile(settings, server);
        const auto id = settings.saveProvider(p);
        QVERIFY(!id.isEmpty());
        QCOMPARE(settings.provider(id).value("username").toString(), QString("photo-user"));
        settings.saveKey(id, " password with spaces ");
        QTRY_VERIFY(!settings.credentialsBusy());
        QCOMPARE(settings.key(id), QString(" password with spaces "));
        QVERIFY(!QJsonDocument::fromVariant(settings.providers())
                     .toJson()
                     .contains("password with spaces"));
        p             = settings.provider(id);
        p["username"] = "other-user";
        QVERIFY(!settings.saveProvider(p).isEmpty());
        QTRY_VERIFY(!settings.credentialsBusy());
        QVERIFY(settings.key(id).isEmpty());
        p["options"] = R"({"seed":1.5})";
        QVERIFY(settings.saveProvider(p).isEmpty());
        p["options"] = R"({"randomize_seed":"false"})";
        QVERIFY(settings.saveProvider(p).isEmpty());
        p["options"] = "{}";
        p["model"]   = "../../other";
        QVERIFY(settings.saveProvider(p).isEmpty());
    }
    void namedJsonAndFragmentedEvents_data()
    {
        QTest::addColumn<int>("format");
        QTest::newRow("object") << 0;
        QTest::newRow("gradio-output-array") << 1;
        QTest::newRow("json-component-string") << 2;
        QTest::newRow("image-and-seed") << 3;
    }
    void namedJsonAndFragmentedEvents()
    {
        QFETCH(int, format);
        AiSettings settings(nullptr, false);
        Server     server;
        const auto object = QJsonObject{{"image", imageUri()}, {"seed", 123}};
        QByteArray result = json(object);
        if (format == 1)
            result = QJsonDocument(QJsonArray{object}).toJson(QJsonDocument::Compact);
        if (format == 2)
            result =
                QJsonDocument(QJsonArray{QString::fromUtf8(result)}).toJson(QJsonDocument::Compact);
        if (format == 3)
            result = QJsonDocument(QJsonArray{imageUri(), 123}).toJson(QJsonDocument::Compact);
        server.handler = [&](const Request &r) {
            return r.method == "POST" ? Response{200, "{\"event_id\":\"job_42\"}"} : stream(result);
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        job.start(profile(settings, server), " pw ", "Transforme \"cette\" photo\nen dessin animé",
                  input());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 4000);
        QVERIFY2(finished[0][1].toString().isEmpty(), qPrintable(finished[0][1].toString()));
        QCOMPARE(finished[0][0].toByteArray(), png());
        QCOMPARE(job.resultMetadata().value("seed").toInt(), 123);
        QCOMPARE(server.requests.size(), 2);
        QCOMPARE(server.requests[0].path, QByteArray("/mounted/gradio_api/call/v2/edit_image"));
        QCOMPARE(server.requests[1].path, QByteArray("/mounted/gradio_api/call/edit_image/job_42"));
        const auto authorization = "Basic " + QByteArray("photo-user: pw ").toBase64();
        for (const auto &r : server.requests)
            QCOMPARE(r.header("Authorization"), authorization);
        QCOMPARE(server.requests[1].header("Accept"), QByteArray("text/event-stream"));
        const auto sent = QJsonDocument::fromJson(server.requests[0].body).object();
        QVERIFY(sent.value("images_b64_json").isString());
        QCOMPARE(QJsonDocument::fromJson(sent.value("images_b64_json").toString().toUtf8()).array(),
                 QJsonArray{imageUri()});
        QCOMPARE(sent.value("prompt").toString(),
                 QString("Transforme \"cette\" photo\nen dessin animé"));
        QCOMPARE(sent.value("lora_adapter").toString(), QString("Photo-to-Anime"));
        QCOMPARE(sent.value("seed").toInt(), 42);
        QCOMPARE(sent.value("randomize_seed").toBool(), false);
        QCOMPARE(sent.value("guidance_scale").toDouble(), 1.);
        QCOMPARE(sent.value("steps").toInt(), 4);
        QVERIFY(!sent.contains("data"));
        QVERIFY(!sent.contains("password"));
        QTest::qWait(30);
        QCOMPARE(finished.size(), 1);
    }
    void failures_data()
    {
        QTest::addColumn<QByteArray>("post");
        QTest::addColumn<QByteArray>("events");
        QTest::newRow("invalid-id") << QByteArray("{\"event_id\":\"../other\"}") << QByteArray{};
        QTest::newRow("missing-id") << QByteArray("{}") << QByteArray{};
        QTest::newRow("server-error")
            << QByteArray("{\"event_id\":\"job\"}")
            << QByteArray("event: error\ndata: [\"Generation failed\"]\n\n");
        QTest::newRow("incomplete") << QByteArray("{\"event_id\":\"job\"}")
                                    << QByteArray("event: heartbeat\ndata: null\n\n");
        QTest::newRow("bad-image")
            << QByteArray("{\"event_id\":\"job\"}")
            << QByteArray(
                   "event: complete\ndata: [{\"image\":\"data:image/png;base64,%%%%\"}]\n\n");
        QTest::newRow("missing-image") << QByteArray("{\"event_id\":\"job\"}")
                                       << QByteArray("event: complete\ndata: [{}]\n\n");
    }
    void failures()
    {
        QFETCH(QByteArray, post);
        QFETCH(QByteArray, events);
        AiSettings settings(nullptr, false);
        Server     server;
        server.handler = [&](const Request &r) {
            if (r.method == "POST")
                return Response{200, post};
            Response s;
            s.type   = "text/event-stream";
            s.chunks = {events};
            return s;
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        job.start(profile(settings, server), "private", "edit", input());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 6000);
        QVERIFY(finished[0][0].toByteArray().isEmpty());
        QVERIFY(!finished[0][1].toString().isEmpty());
        QVERIFY(!finished[0][1].toString().contains("private"));
    }
    void cancellationDoesNotCallComfyOrInsertLateResult()
    {
        AiSettings settings(nullptr, false);
        Server     server;
        bool       completing = false;
        server.handler        = [&](const Request &r) {
            if (r.method == "POST")
                return Response{200, "{\"event_id\":\"job\"}"};
            if (completing)
                return stream(json({{"image", imageUri()}, {"seed", 17}}));
            Response s;
            s.type     = "text/event-stream";
            s.chunks   = {"event: heartbeat\ndata: null\n\n"};
            s.keepOpen = true;
            return s;
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        job.start(profile(settings, server), "pass", "edit", input());
        QTRY_COMPARE(server.requests.size(), 2);
        job.cancel();
        QTest::qWait(40);
        QCOMPARE(server.requests.size(), 2);
        QCOMPARE(finished.size(), 0);
        completing = true;
        job.start(profile(settings, server), "pass", "second", input());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
        QCOMPARE(finished[0][0].toByteArray(), png());
        QCOMPARE(server.requests.size(), 4);
        for (const auto &r : server.requests)
            QVERIFY(!r.path.endsWith("/queue"));
    }
    void unauthorizedAndRedirectNeverLeakCredentials()
    {
        AiSettings       settings(nullptr, false);
        Server           server, foreign;
        const QByteArray secret = "never-log-this";
        const auto       token  = (QByteArray("photo-user:") + secret).toBase64();
        server.handler          = [&](const Request &) {
            return Response{401, json({{"detail", "Rejected Basic " + QString::fromLatin1(token) +
                                                      " " + QString::fromLatin1(secret)}})};
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        job.start(profile(settings, server), QString::fromLatin1(secret), "edit", input());
        QTRY_COMPARE(finished.size(), 1);
        const auto error = finished[0][1].toString();
        QVERIFY(!error.contains(QString::fromLatin1(secret)));
        QVERIFY(!error.contains(QString::fromLatin1(token)));
        server.handler = [&](const Request &) {
            return Response{302, "{}", "application/json",
                            "Location: " + foreign.url().toUtf8() + "\r\n"};
        };
        job.start(profile(settings, server), QString::fromLatin1(secret), "edit", input());
        QTRY_COMPARE(finished.size(), 2);
        QCOMPARE(foreign.requests.size(), 0);
    }
    void unauthenticatedServersDoNotSendCredentials()
    {
        AiSettings settings(nullptr, false);
        Server     server;
        server.handler = [&](const Request &r) {
            return r.method == "POST" ? Response{200, "{\"event_id\":\"anonymous\"}"}
                                      : stream(json({{"image", imageUri()}, {"seed", 5}}));
        };
        auto p        = profile(settings, server);
        p["authMode"] = "none";
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        job.start(p, "must-not-be-sent", "edit", input());
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished[0][0].toByteArray(), png());
        for (const auto &r : server.requests)
            QVERIFY(!r.headers.toLower().contains("authorization:"));
    }
    void credentialFreeErrorsRemainReadable()
    {
        AiSettings settings(nullptr, false);
        Server     server;
        server.handler = [](const Request &) {
            return Response{400, json({{"detail", "Readable error"}})};
        };
        auto p     = settings.newProvider("compatible");
        p["url"]   = server.url();
        p["model"] = "local-model";
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        job.start(p, {}, "edit", input());
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(finished[0][1].toString().contains("Readable error"));
        QVERIFY(!finished[0][1].toString().contains("identifiants masqués"));
    }
    void successiveGenerationsAvoidAStalePersistentSocket()
    {
        AiSettings settings(nullptr, false);
        Server     server;
        QSet<int>  uploads;
        int        posts = 0;
        server.handler   = [&](const Request &r) {
            if (r.method == "POST") {
                ++posts;
                // Reusing a completed request's socket reproduces the server
                // accepting TCP but leaving the next upload unanswered.
                if (uploads.contains(r.connection)) {
                    Response s;
                    s.silent = true;
                    return s;
                }
                uploads.insert(r.connection);
                Response s{200, json({{"event_id", QString::number(posts)}})};
                s.keepOpen = true;
                return s;
            }
            return stream(json({{"image", imageUri()}, {"seed", posts}}));
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        for (int i = 1; i <= 3; ++i) {
            job.start(profile(settings, server), "password", "edit", input());
            QTRY_COMPARE_WITH_TIMEOUT(finished.size(), i, 2000);
            QCOMPARE(finished[i - 1][0].toByteArray(), png());
            QVERIFY(finished[i - 1][1].toString().isEmpty());
            QTest::qWait(20);
        }
        QCOMPARE(posts, 3);
        QCOMPARE(uploads.size(), 3);
        QCOMPARE(server.requests.size(), 6);
        for (const auto &r : server.requests)
            QVERIFY(r.headers.toLower().contains("connection: close"));
    }
    void interruptedReadsResumeTheSameTask_data()
    {
        QTest::addColumn<int>("failure");
        QTest::newRow("truncated-event") << 0;
        QTest::newRow("temporarily-unavailable") << 1;
        QTest::newRow("closed-socket") << 2;
    }
    void interruptedReadsResumeTheSameTask()
    {
        QFETCH(int, failure);
        AiSettings settings(nullptr, false);
        Server     server;
        int        posts = 0, gets = 0;
        server.handler = [&](const Request &r) {
            if (r.method == "POST") {
                ++posts;
                return Response{200, "{\"event_id\":\"original_task\"}"};
            }
            ++gets;
            if (gets == 1) {
                if (failure == 1)
                    return Response{503, "{}"};
                Response s;
                if (failure == 2)
                    s.drop = true;
                else {
                    s.type   = "text/event-stream";
                    s.chunks = {"event: heartbeat\ndata: null\n\nevent: complete\ndata: "
                                "{\"image\":\"unfinished"};
                }
                return s;
            }
            return stream(json({{"image", imageUri()}, {"seed", 84}}));
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        job.start(profile(settings, server), "password", "edit", input());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
        QCOMPARE(finished[0][0].toByteArray(), png());
        QVERIFY2(finished[0][1].toString().isEmpty(), qPrintable(finished[0][1].toString()));
        QCOMPARE(posts, 1);
        QVERIFY(gets >= 2);
        for (const auto &r : server.requests)
            if (r.method == "GET")
                QCOMPARE(r.path, QByteArray("/mounted/gradio_api/call/edit_image/original_task"));
        QCOMPARE(job.resultMetadata().value("seed").toInt(), 84);
    }
    void cancellingDuringReconnectDoesNotRestartTheTask()
    {
        AiSettings settings(nullptr, false);
        Server     server;
        server.handler = [](const Request &r) {
            if (r.method == "POST")
                return Response{200, "{\"event_id\":\"cancelled_task\"}"};
            Response s;
            s.type   = "text/event-stream";
            s.chunks = {"event: heartbeat\ndata: null\n\n"};
            return s;
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished), status(&job, &AiImageJob::status);
        job.start(profile(settings, server), "password", "edit", input());
        QTRY_VERIFY(status.size() >= 3);
        job.cancel();
        QTest::qWait(700);
        QCOMPARE(finished.size(), 0);
        QCOMPARE(server.requests.size(), 2);
    }
    void authenticationFailuresDuringReadingAreNotRetried()
    {
        AiSettings settings(nullptr, false);
        Server     server;
        server.handler = [](const Request &r) {
            return r.method == "POST" ? Response{200, "{\"event_id\":\"private_task\"}"}
                                      : Response{401, "{\"detail\":\"Unauthorized\"}"};
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        job.start(profile(settings, server), "password", "edit", input());
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(finished[0][1].toString().contains("401"));
        QTest::qWait(700);
        QCOMPARE(server.requests.size(), 2);
    }
    void anUnconfirmedSubmissionIsNotPostedAgain()
    {
        AiSettings settings(nullptr, false);
        Server     server;
        server.handler = [](const Request &) {
            Response s;
            s.drop = true;
            return s;
        };
        AiImageJob job;
        QSignalSpy finished(&job, &AiImageJob::finished);
        job.start(profile(settings, server), "password", "edit", input());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
        QVERIFY(finished[0][0].toByteArray().isEmpty());
        QVERIFY(finished[0][1].toString().contains("non confirmé"));
        QTest::qWait(700);
        QCOMPARE(server.requests.size(), 1);
        QCOMPARE(server.requests.first().method, QByteArray("POST"));
    }
};
QTEST_MAIN(AiGradioTest)
#include "AiGradioTest.moc"

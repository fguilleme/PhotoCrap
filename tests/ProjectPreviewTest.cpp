#include "storage/ProjectPreview.h"
#include "Document.h"
#include "storage/ProjectPreviewScan.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include <fstream>
#include <sstream>

class ProjectPreviewTest : public QObject
{
    Q_OBJECT
private slots:
    void prefixScanDoesNotReadPixelPayloads()
    {
        std::string header =
            R"({"format":"PhotoCrap","preview":{"version":1,"image":"small"},"layers":[)";
        std::istringstream file(header + std::string(2 * 1024 * 1024, 'x'));
        const auto         entry = PhotoCrap::Preview::Scan(file).read();
        QVERIFY(!entry.json.empty());
        QVERIFY(entry.bytesRead <= 16384);
        QCOMPARE(QJsonDocument::fromJson(QByteArray::fromStdString(entry.json))
                     .object()["image"]
                     .toString(),
                 QString("small"));
        std::istringstream old(R"({"format":"PhotoCrap","layers":[{"png":"YWJj","mask":""}]})");
        const auto         source = PhotoCrap::Preview::Scan(old).read(true);
        QCOMPARE(source.length, std::uint64_t(4));
        old.clear();
        old.seekg(std::streamoff(source.offset));
        std::string encoded(4, ' ');
        old.read(encoded.data(), 4);
        QCOMPARE(encoded, std::string("YWJj"));
    }
    void malformedAndOversizedMetadataAreBounded()
    {
        for (const std::string text :
             {std::string("not a project"), std::string(R"({"format":"Other","preview":{}})"),
              std::string(R"({"format":"PhotoCrap","preview":{"image":")") +
                  std::string(1024 * 1024 + 1, 'a') + "\"}}",
              std::string(R"({"format":"PhotoCrap","preview":)") + std::string(100, '[') + "0"}) {
            std::istringstream file(text);
            const auto         entry = PhotoCrap::Preview::Scan(file).read();
            QVERIFY(entry.json.empty());
        }
    }
    void projectPreviewKeepsCompositionAndNativeData()
    {
        QTemporaryDir directory;
        for (int configuration = 0; configuration < 4; ++configuration) {
            const int  bits   = configuration < 2 ? 8 : 16;
            const bool cached = configuration % 2;
            Document   document;
            QVERIFY(document.newDocument(120, 90, false, bits));
            document.setColor(QColor(40, 80, 120));
            document.fillWithColor(true);
            document.addAdjustmentLayer("invert");
            QCOMPARE(document.layers().size(), 2);
            const auto reference = cached ? document.composite() : QImage{};
            const auto history   = document.historyIndex();
            const auto path = directory.filePath(QString("prévisualisation-%1.pcr").arg(bits));
            QVERIFY(document.saveProject(QUrl::fromLocalFile(path)));
            const auto expectedImage = cached ? reference : document.composite();
            const auto image         = PhotoCrap::Preview::read(path, {60, 60});
            QCOMPARE(image.size(), QSize(60, 45));
            const auto actual   = image.pixelColor(30, 20),
                       expected = expectedImage.pixelColor(60, 40);
            QVERIFY(qAbs(actual.red() - expected.red()) <= 3);
            QVERIFY(qAbs(actual.green() - expected.green()) <= 3);
            QVERIFY(qAbs(actual.blue() - expected.blue()) <= 3);
            QCOMPARE(document.historyIndex(), history);
            Document restored;
            QVERIFY(restored.openFile(QUrl::fromLocalFile(path)));
            QCOMPARE(restored.channelBits(), bits);
            QCOMPARE(restored.composite(), expectedImage);
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto root = QJsonDocument::fromJson(file.readAll()).object();
            QCOMPARE(root["version"].toInt(), 9);
            QCOMPARE(root["preview"].toObject()["channelBits"].toInt(), bits);
        }
    }
    void largeUniformProjectStaysCompact()
    {
        QTemporaryDir directory;
        Document      document;
        QVERIFY(document.newDocument(6000, 4000, false, 16));
        for (int i = 0; i < 64; ++i)
            document.addLayer();
        const auto path = directory.filePath("large.pcr");
        QVERIFY(document.saveProject(QUrl::fromLocalFile(path)));
        QVERIFY2(QFileInfo(path).size() < 512 * 1024,
                 qPrintable(QString("Project bytes: %1").arg(QFileInfo(path).size())));
        const auto image = PhotoCrap::Preview::read(path, {400, 400});
        QCOMPARE(image.size(), QSize(400, 266));
        QCOMPARE(image.pixelColor(200, 130).rgb(), QColor(Qt::white).rgb());
    }
};
QTEST_MAIN(ProjectPreviewTest)
#include "ProjectPreviewTest.moc"

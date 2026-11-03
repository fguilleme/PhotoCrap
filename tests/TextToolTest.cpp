#include "Document.h"
#include "ImageUtils.h"
#include "text/TextLayer.h"
#include <QFontDatabase>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QtTest>

class TextToolTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY2(!QFontDatabase::families().isEmpty(),
                 "Text rendering tests need real fonts; set QT_QPA_FONTDIR for Windows offscreen.");
    }
    void regionalTextMatchesFullCanvasRendering()
    {
        const QSize canvas(523, 531);
        for (const auto format :
             {QImage::Format_ARGB32_Premultiplied, QImage::Format_RGBA64_Premultiplied})
            for (bool paragraph : {false, true})
                for (int geometry = 0; geometry < 6; ++geometry) {
                    auto p = PhotoCrap::Text::create(246.25, 244.5, 24, QColor(170, 40, 90, 200));
                    p["paragraph"] = paragraph;
                    p["width"]     = 120.25;
                    p["height"]    = 75.75;
                    p["html"]      = QString::fromUtf8(
                        "<p><span style='font-size:42px;font-style:italic;color:#66aa44'>Été "
                        "gfj</span> "
                        "<span style='font-size:18px'>中文 👋</span></p>"
                        "<p dir='rtl'>العربية</p><p>More wrapped text</p>");
                    QTransform geometryTransform;
                    if (geometry == 1)
                        geometryTransform.rotate(19);
                    else if (geometry == 2)
                        geometryTransform.shear(.23, -.13);
                    else if (geometry == 3)
                        geometryTransform.scale(1.25, .8);
                    else if (geometry == 4)
                        geometryTransform.translate(700, 800);
                    else if (geometry == 5)
                        geometryTransform.translate(-266, -258);
                    PhotoCrap::Text::transform(p, geometryTransform);
                    QVERIFY(PhotoCrap::Text::normalize(p));
                    const auto reference = ImageUtils::expandedPlane(
                        PhotoCrap::Text::render(p, canvas, format), canvas);
                    const auto regional = PhotoCrap::Text::renderPlane(p, canvas, format);
                    QVERIFY(!regional.isNull());
                    QCOMPARE(regional.format(), format);
                    const auto actual = regional.region(QRect(QPoint{}, canvas));
                    QCOMPARE(actual, reference);
                }
    }
    void smallTextStaysCompactOnTwentyFourMegapixelDocuments()
    {
        for (int bits : {8, 16}) {
            Document d;
            QVERIFY(d.newDocument(6000, 4000, true, bits));
            d.setTextSize(24);
            d.setTool("text");
            d.beginStroke(100, 100);
            d.endStroke();
            d.setText("Small");
            d.finishTextEdit();
            const qint64 tileBytes = 256LL * 256 * bits / 2;
            const auto   before    = d.memoryUsage()["layerBytes"].toLongLong();
            QVERIFY(before <= 2 * tileBytes);
            const int history = d.historyIndex();
            d.setTool("move");
            d.beginStroke(110, 110);
            d.continueStroke(5110, 3110);
            QVERIFY(d.memoryUsage()["layerBytes"].toLongLong() <= 4 * tileBytes);
            QCOMPARE(PhotoCrap::Text::transform(d.textEditState()).dx(), 5100.);
            d.continueStroke(110, 110);
            d.endStroke();
            QCOMPARE(d.historyIndex(), history);
            QCOMPARE(d.memoryUsage()["layerBytes"].toLongLong(), before);
            d.beginStroke(110, 110, true);
            d.continueStroke(5110, 3110);
            QCOMPARE(d.layers().size(), 3);
            QVERIFY(d.memoryUsage()["layerBytes"].toLongLong() <= 6 * tileBytes);
            d.cancelStroke();
            QCOMPARE(d.layers().size(), 2);
            QCOMPARE(d.historyIndex(), history);
            QCOMPARE(d.memoryUsage()["layerBytes"].toLongLong(), before);
            d.beginStroke(110, 110);
            d.continueStroke(5110, 3110);
            d.endStroke();
            QCOMPARE(d.historyIndex(), history + 1);
            d.undo();
            QCOMPARE(PhotoCrap::Text::transform(d.textEditState()).dx(), 100.);
            d.redo();
            QCOMPARE(PhotoCrap::Text::transform(d.textEditState()).dx(), 5100.);
        }
    }
    void movingTextBackToAnchorRestoresTheOriginalWithoutHistory()
    {
        for (int bits : {8, 16}) {
            Document d;
            QVERIFY(d.newDocument(523, 531, true, bits));
            d.setTextSize(24);
            d.setTool("text");
            d.beginStroke(70, 60);
            d.endStroke();
            d.setText("Before move");
            d.finishTextEdit();
            const auto before  = d.composite();
            const int  history = d.historyIndex();
            d.setTool("move");
            d.beginStroke(85, 75);
            d.continueStroke(400, 420);
            QVERIFY(d.composite() != before);
            QCOMPARE(d.composite(), d.layerImage(1));
            d.continueStroke(85, 75);
            QCOMPARE(d.composite(), before);
            d.endStroke();
            QCOMPARE(d.historyIndex(), history);
            QCOMPARE(d.layerImage(1), before);
        }
    }
    void editableTextPersistsAndUndoes_data()
    {
        QTest::addColumn<int>("bits");
        QTest::newRow("8bit") << 8;
        QTest::newRow("16bit") << 16;
    }
    void editableTextPersistsAndUndoes()
    {
        QFETCH(int, bits);
        Document d;
        QVERIFY(d.newDocument(240, 120, true, bits));
        const auto original = d.layerImage(0);
        d.setTool("text");
        d.setTextSize(24);
        d.beginStroke(12, 10);
        d.endStroke();
        QVERIFY(d.textEditing());
        QCOMPARE(d.text(), QString{});
        QCOMPARE(d.historyIndex(), 0);
        d.setText("Hello\nWorld");
        d.finishTextEdit();
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.historyIndex(), 1);
        QCOMPARE(d.layers().first().toMap()["kind"].toString(), QString("text"));
        QCOMPARE(d.layerImage(0), original);
        QCOMPARE(ImageUtils::is16Bit(d.layerImage(1)), bits == 16);
        const auto first = d.layerImage(1);
        d.renameLayer(1, "Title");
        QCOMPARE(d.layerImage(1), first);
        d.editTextLayer(1);
        d.setText("Editable");
        d.finishTextEdit();
        QCOMPARE(d.layers().first().toMap()["name"].toString(), QString("Title"));
        const auto edited = d.layerImage(1);
        QVERIFY(edited != first);
        QCOMPARE(d.layers().size(), 2);
        d.undo();
        QCOMPARE(d.layerImage(1), first);
        d.redo();
        QCOMPARE(d.layerImage(1), edited);
        QTemporaryDir dir;
        const auto    url = QUrl::fromLocalFile(dir.filePath("text.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.layerImage(1), edited);
        loaded.editTextLayer(1);
        QCOMPARE(loaded.text(), QString("Editable"));
        loaded.setText("Changed");
        loaded.finishTextEdit(true);
        QCOMPARE(loaded.layerImage(1), edited);
        loaded.editTextLayer(1);
        loaded.setText("Saved during edit");
        QVERIFY(loaded.saveProject(url));
        QVERIFY(!loaded.textEditing());
        QVERIFY(d.openFile(url));
        d.editTextLayer(1);
        QCOMPARE(d.text(), QString("Saved during edit"));
    }
    void emptyClickAndParagraphDrag()
    {
        Document d;
        QVERIFY(d.newDocument(320, 200, true));
        d.setTool("text");
        d.setTextSize(18);
        d.beginStroke(20, 20);
        d.endStroke();
        d.finishTextEdit();
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.historyIndex(), 0);
        d.beginStroke(20, 20);
        d.continueStroke(150, 110);
        d.endStroke();
        QVERIFY(d.textEditState()["paragraph"].toBool());
        QCOMPARE(d.textEditState()["width"].toDouble(), 130.);
        d.setText("A paragraph that wraps over several lines.");
        const auto a = d.layerImage(1);
        d.setTextBox(true, 80, 100);
        QVERIFY(d.layerImage(1) != a);
        d.finishTextEdit();
        d.editTextLayer(1);
        QVERIFY(d.textEditState()["paragraph"].toBool());
        d.finishTextEdit(true);
        d.undo();
        QCOMPARE(d.layers().size(), 1);
    }
    void mixedFormattingKeepsNativeProperties()
    {
        auto                      p = PhotoCrap::Text::create(3, 4, 24, Qt::red);
        PhotoCrap::Text::Document doc;
        PhotoCrap::Text::configure(doc, p);
        doc.setPlainText("Mixed text");
        QTextCursor c(&doc);
        c.setPosition(0);
        c.setPosition(5, QTextCursor::KeepAnchor);
        QTextCharFormat f;
        f.setProperty(QTextFormat::FontPixelSize, 32);
        f.setFontWeight(QFont::Bold);
        f.setFontStretch(145);
        f.setBaselineOffset(20);
        f.setFontLetterSpacingType(QFont::AbsoluteSpacing);
        f.setFontLetterSpacing(2.5);
        f.setForeground(Qt::blue);
        c.mergeCharFormat(f);
        QTextBlockFormat b;
        b.setLineHeight(40, QTextBlockFormat::FixedHeight);
        b.setTopMargin(6);
        c.mergeBlockFormat(b);
        p["html"]             = doc.toHtml();
        const auto formatting = PhotoCrap::Text::formatting(doc);
        for (auto it = formatting.begin(); it != formatting.end(); ++it)
            p[it.key()] = it.value();
        QVERIFY(PhotoCrap::Text::normalize(p));
        PhotoCrap::Text::Document restored;
        PhotoCrap::Text::configure(restored, p);
        QTextCursor r(&restored);
        r.setPosition(2);
        QCOMPARE(r.charFormat().font().pixelSize(), 32);
        QCOMPARE(r.charFormat().fontStretch(), 145);
        QCOMPARE(r.charFormat().baselineOffset(), 20.);
        QCOMPARE(r.charFormat().fontLetterSpacing(), 2.5);
        QCOMPARE(r.charFormat().foreground().color(), QColor(Qt::blue));
        r.setPosition(8);
        QVERIFY(r.charFormat().fontWeight() != QFont::Bold);
        QCOMPARE(r.blockFormat().lineHeight(), 40.);
        Document d;
        QVERIFY(d.newDocument(320, 120, true, 16));
        d.setTool("text");
        d.beginStroke(3, 4);
        d.endStroke();
        d.updateTextContent(p["html"].toString(), formatting);
        d.finishTextEdit();
        const auto    pixels = d.layerImage(1);
        QTemporaryDir dir;
        const auto    url = QUrl::fromLocalFile(dir.filePath("rich.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        loaded.editTextLayer(1);
        PhotoCrap::Text::Document roundTrip;
        PhotoCrap::Text::configure(roundTrip, loaded.textEditState());
        QTextCursor rc(&roundTrip);
        rc.setPosition(2);
        QCOMPARE(rc.charFormat().font().pixelSize(), 32);
        QCOMPARE(rc.charFormat().baselineOffset(), 20.);
        loaded.finishTextEdit();
        QCOMPARE(loaded.layerImage(1), pixels);
    }
    void transformsAndRasterizationPreserveEditability()
    {
        Document d;
        QVERIFY(d.newDocument(160, 100, true));
        d.setTool("text");
        d.setTextSize(18);
        d.beginStroke(10, 10);
        d.endStroke();
        d.setText("Move");
        d.finishTextEdit();
        d.setTool("move");
        d.beginStroke(20, 20);
        d.continueStroke(40, 30);
        d.endStroke();
        d.editTextLayer(1);
        auto t = PhotoCrap::Text::transform(d.textEditState());
        QCOMPARE(t.dx(), 30.);
        QCOMPARE(t.dy(), 20.);
        d.finishTextEdit();
        d.resizeImage(320, 200);
        d.transformImage("rotateRight");
        d.editTextLayer(1);
        t = PhotoCrap::Text::transform(d.textEditState());
        QVERIFY(t.isInvertible());
        QVERIFY(qAbs(t.m12()) > 1.);
        d.setText("Still editable");
        d.finishTextEdit();
        QCOMPARE(d.layers().first().toMap()["kind"].toString(), QString("text"));
        d.rasterizeTextLayer();
        QCOMPARE(d.layers().first().toMap()["kind"].toString(), QString("pixel"));
        d.undo();
        QCOMPARE(d.layers().first().toMap()["kind"].toString(), QString("text"));
    }
    void clickExistingTextKeepsEditingAfterRelease()
    {
        Document d;
        QVERIFY(d.newDocument(320, 200, true, 16));
        d.setTool("text");
        d.setTextSize(24);
        d.beginStroke(20, 30);
        d.endStroke();
        d.setText("Click to edit");
        d.finishTextEdit();
        const auto original = d.layerImage(1);
        d.setActiveLayer(0);
        d.beginStroke(30, 40);
        QVERIFY(d.textEditing());
        QVERIFY(d.textGesture());
        d.continueStroke(40, 50);
        d.endStroke();
        QVERIFY(d.textEditing());
        QVERIFY(!d.textGesture());
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.activeLayer(), 1);
        QVERIFY(!d.textEditState().value("paragraph").toBool());
        d.setText("Edited by a canvas click");
        QVERIFY(d.layerImage(1) != original);
        d.finishTextEdit(true);
        QCOMPARE(d.layerImage(1), original);
    }
    void movingWhileTypingKeepsOneSession()
    {
        Document d;
        QVERIFY(d.newDocument(320, 200, true, 16));
        d.setTool("text");
        d.setTextSize(24);
        d.beginStroke(20, 30);
        d.endStroke();
        d.setText("Editable");
        d.finishTextEdit();
        const auto original = d.layerImage(1);
        d.editTextLayer(1);
        d.moveTextBy(12, 8);
        QCOMPARE(PhotoCrap::Text::transform(d.textEditState()).dx(), 32.);
        QCOMPARE(PhotoCrap::Text::transform(d.textEditState()).dy(), 38.);
        QCOMPARE(d.text(), QString("Editable"));
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.composite(), d.layerImage(1));
        d.finishTextEdit(true);
        QCOMPARE(d.layerImage(1), original);
        d.editTextLayer(1);
        d.moveTextBy(-10, 5);
        d.setText("Moved and edited");
        d.finishTextEdit();
        d.undo();
        QCOMPARE(d.layerImage(1), original);
        QCOMPARE(PhotoCrap::Text::transform(d.textEditState()).dx(), 20.);
        QCOMPARE(PhotoCrap::Text::transform(d.textEditState()).dy(), 30.);
    }
    void typingRecomposesOldAndNewBounds()
    {
        Document d;
        QVERIFY(d.newDocument(320, 200, true, 16));
        d.setTool("text");
        d.setTextSize(24);
        d.beginStroke(10, 10);
        d.endStroke();
        for (const auto &text : {QString("A long first line"), QString("Short"),
                                 QString::fromUtf8("Été — العربية 👋"), QString("Two\nlines")}) {
            d.setText(text);
            QCOMPARE(d.composite(), d.layerImage(1));
        }
        d.finishTextEdit();
        d.editTextLayer(1);
        d.setText("Changed later");
        QCOMPARE(d.composite(), d.layerImage(1));
        d.finishTextEdit(true);
        QCOMPARE(d.composite(), d.layerImage(1));
    }
    void cancellingEmptyTextDoesNotRetargetLayerCommands()
    {
        Document d;
        QVERIFY(d.newDocument(80, 80, true));
        d.addLayer();
        d.renameLayer(1, "middle");
        d.addLayer();
        d.renameLayer(2, "top");
        d.setActiveLayer(0);
        d.setTool("text");
        d.beginStroke(10, 10);
        d.endStroke();
        QCOMPARE(d.layers().size(), 4);
        d.setActiveLayer(3);
        QCOMPARE(d.layers().size(), 3);
        QCOMPARE(d.activeLayer(), 2);
        QCOMPARE(d.layers().first().toMap().value("name").toString(), QString("top"));
        d.beginStroke(10, 10);
        d.endStroke();
        d.removeLayer();
        QCOMPARE(d.layers().size(), 3);
        d.beginStroke(10, 10);
        d.endStroke();
        d.addLayerMask();
        QCOMPARE(d.layers().size(), 3);
        QVERIFY(d.maskImage(2).isNull());
        d.beginStroke(10, 10);
        d.endStroke();
        d.duplicateLayer();
        QCOMPARE(d.layers().size(), 3);
    }
    void untrustedTextDoesNotLoadExternalObjects()
    {
        auto p    = PhotoCrap::Text::create(0, 0, 20, Qt::black);
        p["html"] = "<img src='file:///etc/passwd'>";
        QVERIFY(!PhotoCrap::Text::normalize(p));
        p["html"] = "<span style='font-size:100000000px'>large</span>";
        QVERIFY(!PhotoCrap::Text::normalize(p));
        p["html"] = "<table><tr><td colspan='2000000000'>large</td></tr></table>";
        QVERIFY(!PhotoCrap::Text::normalize(p));
        p["html"] = "safe";
        p["runs"] = QVariantList{
            QVariantMap{{"start", 100}, {"length", 10}, {"properties", QVariantMap{}}}};
        QVERIFY(!PhotoCrap::Text::normalize(p));
    }
};
QTEST_MAIN(TextToolTest)
#include "TextToolTest.moc"

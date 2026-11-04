#pragma once
#include "Document.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

inline bool runMarqueeUiSmoke(QQuickWindow *_window, Document &_document)
{
    const auto require = [](bool ok, const char *message) {
        if (!ok)
            qWarning("Marquee options check failed: %s", message);
        return ok;
    };
    const auto item = [&](const char *name) { return _window->findChild<QQuickItem *>(name); };
    auto options = item("selectionOptions"), area = item("drawingArea"),
         style = item("marqueeStyle"), width = item("marqueeWidth"), height = item("marqueeHeight"),
         feather = item("selectionFeather"), aa = item("selectionAntialias"),
         swap = item("swapMarqueeDimensions");
    if (!require(options && area && style && width && height && feather && aa && swap,
                 "selection options exist"))
        return false;
    const auto click = [&](QQuickItem *control) {
        QTest::mouseClick(
            _window, Qt::LeftButton, Qt::NoModifier,
            control->mapToScene(QPointF(control->width() / 2, control->height() / 2)).toPoint());
    };
    const auto type = [&](QQuickItem *control, const char *value) {
        auto input = control->property("contentItem").value<QQuickItem *>();
        if (!input)
            return false;
        click(input);
        QTest::keySequence(_window, QKeySequence::SelectAll);
        for (auto c : QByteArray(value))
            QTest::keyClick(_window, c);
        QTest::keyClick(_window, Qt::Key_Return);
        area->forceActiveFocus();
        return true;
    };
    _document.newDocument(320, 240, true, 16);
    _document.setTool("select");
    _document.setMarqueeStyle(0);
    _document.setSelectionFeather(0);
    _document.setSelectionMode(0);
    _window->setProperty("zoom", 1.);
    QTest::qWait(80);
    if (!require(options->isVisible() && !width->isEnabled() && !height->isEnabled() &&
                     !aa->isEnabled(),
                 "Normal rectangle has disabled dimensions and antialiasing"))
        return false;
    style->setProperty("currentIndex", 2);
    QMetaObject::invokeMethod(style, "activated", Q_ARG(int, 2));
    QTest::qWait(40); // Let the newly enabled fields and their px labels finish layout.
    if (!require(type(width, "60") && type(height, "40") && _document.marqueeWidth() == 60 &&
                     _document.marqueeHeight() == 40,
                 "fixed pixel dimensions accept keyboard input")) {
        qWarning() << "Marquee dimensions" << _document.marqueeWidth() << _document.marqueeHeight()
                   << width->property("displayText") << height->property("displayText")
                   << _window->activeFocusItem();
        return false;
    }
    click(swap);
    if (!require(_document.marqueeWidth() == 40 && _document.marqueeHeight() == 60,
                 "swap exchanges the two dimensions"))
        return false;
    area->forceActiveFocus();
    QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier,
                      area->mapToScene(QPointF(25, 25)).toPoint());
    if (!require(_document.selection().size() == QSize(40, 60) &&
                     qAbs(_document.selection().x() - 25) <= 1 &&
                     qAbs(_document.selection().y() - 25) <= 1,
                 "a single canvas click creates an exact fixed-size selection")) {
        qWarning() << "Fixed selection" << _document.selection() << _window->property("zoom")
                   << _document.marqueeStyle();
        return false;
    }
    style->setProperty("currentIndex", 1);
    QMetaObject::invokeMethod(style, "activated", Q_ARG(int, 1));
    QTest::qWait(40);
    const auto ratio = QLocale().toString(2.5, 'f', 2).toLatin1();
    if (!require(type(width, ratio.constData()) && type(height, "1") &&
                     _document.marqueeWidth() == 2.5 && _document.marqueeHeight() == 1,
                 "fixed ratio accepts localized decimal proportions"))
        return false;
    for (int mode : {1, 2, 3, 0}) {
        auto button = item(qPrintable(QString("selectionMode_%1").arg(mode)));
        if (!require(button, "combination button exists"))
            return false;
        click(button);
        if (!require(_document.selectionMode() == mode,
                     "combination buttons update the active mode"))
            return false;
    }
    _document.deselect();
    _document.setTool("ellipticalSelect");
    _document.setSelectionAntialias(true);
    _document.setMarqueeStyle(0);
    QTest::qWait(40);
    if (!require(aa->isEnabled() && aa->property("checked").toBool(),
                 "Ellipse enables the checked antialias control"))
        return false;
    if (!require(type(feather, "4") && _document.selectionFeather() == 4,
                 "Feather commits a typed radius"))
        return false;
    const auto before = _document.historyIndex();
    area->forceActiveFocus();
    const auto start = area->mapToScene(QPointF(30, 30)).toPoint(),
               end   = area->mapToScene(QPointF(150, 120)).toPoint();
    QTest::mousePress(_window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(_window, end, 20);
    QTest::mouseRelease(_window, Qt::LeftButton, Qt::NoModifier, end);
    if (!require(QTest::qWaitFor([&] { return !_document.selectionBusy(); }, 4000) &&
                     _document.hasSelection() && _document.historyIndex() == before + 1,
                 "feathered ellipse commits exactly one selection history step"))
        return false;
    int partial = 0;
    for (int y = 20; y < 130; ++y)
        for (int x = 20; x < 160; ++x) {
            const auto v = _document.selectionCoverage(x, y);
            partial += v > 0 && v < 255;
        }
    if (!require(partial > 100, "the actual selection contains feathered pixels"))
        return false;
    _document.undo();
    if (!require(!_document.hasSelection(), "Undo removes the whole feathered selection"))
        return false;
    _document.redo();
    if (!require(_document.hasSelection(), "Redo restores it"))
        return false;
    _document.setTool("singleRowSelect");
    QTest::qWait(20);
    if (!require(!style->isVisible() && !aa->isEnabled(),
                 "single-pixel marquees hide Style and disable antialiasing"))
        return false;
    _document.setTool("lasso");
    QTest::qWait(20);
    if (!require(!style->isVisible() && aa->isEnabled(),
                 "lasso exposes Feather and antialiasing without rectangle constraints"))
        return false;
    _document.setTool("ellipticalSelect");
    _document.setSelectionFeather(0);
    _document.setMarqueeStyle(0);
    _document.setSelectionMode(0);
    area->forceActiveFocus();
    return true;
}

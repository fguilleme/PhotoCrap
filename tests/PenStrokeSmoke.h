#pragma once
#include "Document.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

inline bool runPenStrokeVisibilitySmoke(QQuickWindow *_window, Document &_document)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("Pen stroke check failed: %s", _message);
        return _ok;
    };
    auto overlay    = _window->findChild<QQuickItem *>("penOverlay");
    auto handles    = overlay ? overlay->findChild<QQuickItem *>("penHandlesOverlay") : nullptr;
    auto board      = _window->findChild<QQuickItem *>("artboard");
    auto makeShape  = _window->findChild<QQuickItem *>("penMakeShapeButton");
    auto widthField = _window->findChild<QQuickItem *>("penShapeStrokeWidth");
    auto swatch     = _window->findChild<QQuickItem *>("penShapeStrokeColor");
    auto paintMenu  = _window->findChild<QObject *>("penPaintMenu");
    auto foreground = _window->findChild<QQuickItem *>("penPaintForeground");
    if (!require(overlay && handles && board && makeShape && widthField && swatch && paintMenu &&
                     foreground,
                 "Path conversion and shape paint controls exist"))
        return false;
    _document.newDocument(4000, 3000, true, 16);
    _document.setTool("pen");
    _document.setPenMode("path");
    _document.setPenShapeStyle("fillColor", "#ffff9900");
    _document.setPenShapeStyle("strokeEnabled", false);
    for (const auto p :
         {QPointF(1000, 800), QPointF(2500, 800), QPointF(2500, 2200), QPointF(1000, 2200)}) {
        _document.beginPenGesture(p.x(), p.y(), 2);
        _document.endPenGesture();
    }
    _document.closePenPath();
    _window->setProperty("zoom", .13);
    QTest::qWait(80);
    const auto click = [](QQuickItem *_item) {
        QTest::mouseClick(
            _item->window(), Qt::LeftButton, Qt::NoModifier,
            _item->mapToScene(QPointF(_item->width() / 2, _item->height() / 2)).toPoint());
    };
    const auto history = _document.historyIndex();
    click(makeShape);
    if (!require(_document.penMode() == "shape" && _document.penPaths().size() == 2 &&
                     _document.layers().size() == 2 && _document.historyIndex() == history + 1,
                 "Make Shape creates an editable layer and preserves its source path"))
        return false;
    _document.setColor(QColor("#ff209f"));
    QTest::qWait(50);
    click(swatch);
    if (!require(QTest::qWaitFor([&] { return paintMenu->property("visible").toBool(); }, 1000),
                 "clicking Stroke opens None/solid-colour choices"))
        return false;
    QTest::qWait(40);
    click(foreground);
    if (!require(_document.penShapeStyle()["strokeEnabled"].toBool() &&
                     QColor(_document.penShapeStyle()["strokeColor"].toString()) ==
                         QColor("#ff209f"),
                 "choosing a colour turns None into a painted stroke on the existing shape"))
        return false;
    widthField->forceActiveFocus();
    QTest::keySequence(_window, QKeySequence::SelectAll);
    const auto locale = QLocale{};
    const auto value  = locale.toString(45.62, 'f', 2);
    for (const auto c : value)
        QTest::keyClick(_window, c.toLatin1());
    QTest::keyClick(_window, Qt::Key_Return);
    if (!require(qAbs(_document.penShapeStyle()["strokeWidth"].toDouble() - 45.62) < .001,
                 "typing and validating a decimal stroke width really updates the shape")) {
        qWarning() << "Width text" << widthField->property("text") << _document.penShapeStyle();
        return false;
    }
    if (!require(overlay->property("curveVisible").toBool() && handles->isVisible(),
                 "the shape keeps its editable curve and control points"))
        return false;
    const auto pinkCount = [&]() {
        auto frame = _window->grabWindow();
        if (frame.isNull())
            return 0;
        const auto  scene  = board->mapRectToScene(QRectF(0, 0, board->width(), board->height()));
        const qreal ratio  = qreal(frame.width()) / _window->width();
        const auto  rect   = QRectF(scene.x() * ratio, scene.y() * ratio, scene.width() * ratio,
                                    scene.height() * ratio)
                                 .toAlignedRect()
                                 .intersected(frame.rect());
        const auto  pixels = frame.copy(rect).convertToFormat(QImage::Format_RGB32);
        int         count  = 0;
        for (int y = 0; y < pixels.height(); ++y) {
            const auto line = reinterpret_cast<const QRgb *>(pixels.constScanLine(y));
            for (int x = 0; x < pixels.width(); ++x)
                count +=
                    qRed(line[x]) > qGreen(line[x]) + 60 && qBlue(line[x]) > qGreen(line[x]) + 8;
        }
        return count;
    };
    if (!require(QTest::qWaitFor([&] { return pinkCount() > 200; }, 3000),
                 "the painted magenta stroke is visibly distinct from the guide at 13 percent"))
        return false;
    const auto pixels         = _document.gpuLayerStack().layers.last().image.cacheKey();
    const auto beforeDeselect = _document.historyIndex();
    _document.setActivePenPath(-1);
    if (!require(
            QTest::qWaitFor([&] { return pinkCount() > 200; }, 3000) &&
                _document.gpuLayerStack().layers.last().image.cacheKey() == pixels &&
                _document.historyIndex() == beforeDeselect,
            "deselecting hides editing controls but leaves the real painted outline unchanged"))
        return false;
    _document.editPenShape();
    _document.setTool("directSelection");
    _document.setPenShapeStyle("strokeWidth", 5.1);
    _document.setPenShapeStyle("strokeStyle", 2);
    if (!require(QTest::qWaitFor([&] { return pinkCount() > 30; }, 3000),
                 "the fine editing guide also leaves a thin dotted stroke visible"))
        return false;
    _document.setActivePenPath(-1);
    if (!require(QTest::qWaitFor([&] { return pinkCount() > 30; }, 3000),
                 "a thin dotted stroke is present in the actual canvas after deselecting"))
        return false;
    _document.editPenShape();
    _document.setPenShapeStyle("strokeWidth", 45.62);
    _document.setPenShapeStyle("strokeStyle", 0);
    _window->setProperty("zoom", 1.);
    _document.setPenMode("path");
    return true;
}

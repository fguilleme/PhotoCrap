#include "render/SelectionMesh.h"
#include <QtTest>
#include <cmath>

class SelectionMeshTest : public QObject
{
    Q_OBJECT
private slots:
    void clipsAndPreservesDashPhase()
    {
        QPainterPath path;
        path.moveTo(-100, 20);
        path.lineTo(200, 20);
        const QRectF first(0, 0, 100, 50), second(31, 0, 100, 50);
        const auto   a = PhotoCrap::Render::selectionMesh(path, {1, 1}, first, 2);
        const auto   b = PhotoCrap::Render::selectionMesh(path, {1, 1}, second, 2);
        QCOMPARE(a.vertices.size(), 6);
        QCOMPARE(b.vertices.size(), 6);
        QVERIFY(!a.fallback && !b.fallback);
        const auto phase = [](const auto &_mesh, const QRectF &_view) {
            const auto &v = _mesh.vertices.first();
            return std::fmod(v.distance + (50 - (v.x + _view.x())) * 2, 15.);
        };
        QVERIFY(qAbs(phase(a, first) - phase(b, second)) < .001);
        for (const auto &v : a.vertices) {
            QVERIFY(v.x >= -6 && v.x <= 106);
            QVERIFY(v.distance >= 0 && v.distance < 240);
            QCOMPARE(qAbs(v.side), 1.25f);
        }
    }
    void closedPathsHolesAndScale()
    {
        QPainterPath path;
        path.addRect(10, 10, 80, 60);
        path.addRect(30, 30, 20, 20);
        const auto mesh = PhotoCrap::Render::selectionMesh(path, {2, 3}, {0, 0, 200, 240}, 1.5);
        QCOMPARE(mesh.vertices.size(), 48);
        QVERIFY(!mesh.fallback);
        for (const auto &v : mesh.vertices) {
            QVERIFY(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.distance));
            QVERIFY(v.x >= 0 && v.x <= 200 && v.y >= 0 && v.y <= 240);
        }
    }
    void ellipsesStayCurvedAtDifferentScales()
    {
        QPainterPath path;
        path.addEllipse(QRectF(100, 80, 160, 90));
        for (const auto scale : {QSizeF(.25, .25), QSizeF(1, 1), QSizeF(3.5, 2.1)}) {
            const auto mesh = PhotoCrap::Render::selectionMesh(
                path, scale, {0, 0, 400 * scale.width(), 300 * scale.height()}, 2);
            QVERIFY(!mesh.fallback);
            QVERIFY(mesh.vertices.size() > 24);
            for (qsizetype i = 0; i < mesh.vertices.size(); i += 6) {
                const auto a = QPointF(mesh.vertices[i].x + mesh.vertices[i + 1].x,
                                       mesh.vertices[i].y + mesh.vertices[i + 1].y) /
                               2;
                const auto b = QPointF(mesh.vertices[i + 2].x + mesh.vertices[i + 5].x,
                                       mesh.vertices[i + 2].y + mesh.vertices[i + 5].y) /
                               2;
                for (const auto point : {a, b}) {
                    const qreal x = (point.x() / scale.width() - 180) / 80;
                    const qreal y = (point.y() / scale.height() - 125) / 45;
                    QVERIFY(qAbs(x * x + y * y - 1) < .025);
                }
            }
        }
    }
    void longSinglePixelBandsStayBounded()
    {
        for (bool row : {true, false})
            for (qreal scale : {.25, 1., 8.}) {
                QPainterPath path;
                path.addRect(row ? QRectF(0, 40, 16384, 1) : QRectF(80, 0, 1, 16384));
                const QRectF viewport = row ? QRectF(500 * scale, 35 * scale, 300, 50)
                                            : QRectF(75 * scale, 500 * scale, 50, 300);
                const auto   mesh =
                    PhotoCrap::Render::selectionMesh(path, {scale, scale}, viewport, 2);
                QVERIFY(!mesh.fallback);
                QVERIFY(!mesh.vertices.isEmpty());
                QVERIFY(mesh.vertices.size() <= 24);
                for (const auto &v : mesh.vertices) {
                    QVERIFY(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.distance));
                    QVERIFY(v.x >= -4 && v.x <= viewport.width() + 4);
                    QVERIFY(v.y >= -4 && v.y <= viewport.height() + 4);
                }
            }
    }
    void boundedComplexityAndLargeCoordinates()
    {
        QPainterPath path;
        path.addRect(0, 0, 100, 100);
        const auto limited = PhotoCrap::Render::selectionMesh(path, {1, 1}, {0, 0, 200, 200}, 1, 6);
        QVERIFY(limited.fallback);
        QVERIFY(limited.vertices.isEmpty());
        QVERIFY(PhotoCrap::Render::selectionMesh(path, {1, 1}, {500, 500, 100, 100}, 1)
                    .vertices.isEmpty());
        QVERIFY(PhotoCrap::Render::selectionMesh(path, {1, 1}, {}, 1).vertices.isEmpty());
        path = QPainterPath{};
        path.moveTo(1e9, 20);
        path.lineTo(1e9 + 1000, 20);
        const auto local =
            PhotoCrap::Render::selectionMesh(path, {1, 1}, {1e9 + 300, 0, 100, 50}, 2);
        QCOMPARE(local.vertices.size(), 6);
        for (const auto &v : local.vertices) {
            QVERIFY(qAbs(v.x) < 110);
            QVERIFY(v.distance < 240);
        }
    }
};
QTEST_GUILESS_MAIN(SelectionMeshTest)
#include "SelectionMeshTest.moc"

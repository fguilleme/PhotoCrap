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

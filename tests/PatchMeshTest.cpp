#include "render/PatchMesh.h"
#include <QtTest>
#include <cmath>

class PatchMeshTest : public QObject
{
    Q_OBJECT
private slots:
    void clippedUvsAndHoles()
    {
        const QRect   target(100, 200, 80, 60);
        const QRegion region = QRegion(target) - QRegion(QRect(120, 220, 20, 20));
        const QRectF  view(230, 650, 70, 50);
        const auto    mesh = PhotoCrap::Render::patchMesh(region, target, {2, 3}, view);
        QVERIFY(!mesh.fallback);
        double area = 0;
        for (qsizetype i = 0; i < mesh.vertices.size(); i += 3) {
            const auto a = mesh.vertices[i], b = mesh.vertices[i + 1], c = mesh.vertices[i + 2];
            area += std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) * .5;
        }
        QCOMPARE(area, 1900.);
        for (const auto &v : mesh.vertices) {
            QVERIFY(v.x >= 0 && v.x <= 70 && v.y >= 0 && v.y <= 50);
            QVERIFY(v.u >= 0 && v.u <= 1 && v.v >= 0 && v.v <= 1);
            QVERIFY(qAbs(v.u - (v.x + view.x() - 200) / 160) < 1e-6);
            QVERIFY(qAbs(v.v - (v.y + view.y() - 600) / 180) < 1e-6);
        }
    }
    void limitsTargetAndLargeCoordinates()
    {
        const QRect target(1000000000, 1000000000, 1000, 1000);
        const auto  mesh = PhotoCrap::Render::patchMesh(QRegion(target), target, {1, 1},
                                                        {1000000100, 1000000100, 200, 200});
        QCOMPARE(mesh.vertices.size(), 6);
        for (const auto &v : mesh.vertices) {
            QVERIFY(v.x >= 0 && v.x <= 200 && v.y >= 0 && v.y <= 200);
            QVERIFY(v.u >= .09999 && v.u <= .30001 && v.v >= .09999 && v.v <= .30001);
        }
        auto limited = PhotoCrap::Render::patchMesh(QRegion(target), target, {1, 1},
                                                    {1000000100, 1000000100, 200, 200}, 5);
        QVERIFY(limited.fallback && limited.vertices.isEmpty());
        // A malformed region cannot sample beyond its donor target rectangle.
        auto clipped = PhotoCrap::Render::patchMesh(
            QRegion(QRect(0, 0, 100, 100)), QRect(20, 20, 10, 10), {1, 1}, {0, 0, 100, 100});
        QCOMPARE(clipped.vertices.size(), 6);
        for (const auto &v : clipped.vertices)
            QVERIFY(v.u >= 0 && v.u <= 1 && v.v >= 0 && v.v <= 1);
        QVERIFY(
            PhotoCrap::Render::patchMesh({}, target, {1, 1}, {0, 0, 10, 10}).vertices.isEmpty());
        QVERIFY(PhotoCrap::Render::patchMesh(QRegion(target), {}, {1, 1}, {0, 0, 10, 10})
                    .vertices.isEmpty());
    }
};
QTEST_GUILESS_MAIN(PatchMeshTest)
#include "PatchMeshTest.moc"

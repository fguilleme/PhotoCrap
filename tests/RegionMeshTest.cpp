#include "render/RegionMesh.h"
#include <QtTest>
#include <cmath>

class RegionMeshTest : public QObject
{
    Q_OBJECT
    static double area(const PhotoCrap::Render::RegionMesh &_mesh)
    {
        double result = 0;
        for (qsizetype i = 0; i < _mesh.vertices.size(); i += 3) {
            const auto a = _mesh.vertices[i], b = _mesh.vertices[i + 1], c = _mesh.vertices[i + 2];
            result += std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) * .5;
        }
        return result;
    }
private slots:
    void holesAndNonUniformScale()
    {
        const QRegion ring = QRegion(QRect(0, 0, 20, 20)) - QRegion(QRect(6, 6, 8, 8));
        const auto    mesh = PhotoCrap::Render::regionMesh(ring, {1.5, 2}, {0, 0, 40, 50});
        QVERIFY(!mesh.fallback);
        QCOMPARE(mesh.vertices.size(), ring.rectCount() * 6);
        QCOMPARE(area(mesh), (400. - 64) * 3);
    }
    void clippingAndLargeCoordinates()
    {
        const auto mesh = PhotoCrap::Render::regionMesh(QRegion(QRect(100, 200, 80, 60)), {2, 3},
                                                        {230, 650, 70, 50});
        QCOMPARE(mesh.vertices.size(), 6);
        QCOMPARE(area(mesh), 3500.);
        for (const auto &v : mesh.vertices)
            QVERIFY(v.x >= 0 && v.x <= 70 && v.y >= 0 && v.y <= 50);
        const auto local =
            PhotoCrap::Render::regionMesh(QRegion(QRect(1000000000, 1000000000, 1000, 1000)),
                                          {1, 1}, {1000000100, 1000000100, 200, 200});
        QCOMPARE(area(local), 40000.);
        for (const auto &v : local.vertices)
            QVERIFY(v.x >= 0 && v.x <= 200 && v.y >= 0 && v.y <= 200);
    }
    void boundedComplexityAndEmptyViews()
    {
        const QRegion region(QRect(0, 0, 20, 20));
        auto          mesh = PhotoCrap::Render::regionMesh(region, {1, 1}, {0, 0, 20, 20}, 5);
        QVERIFY(mesh.fallback && mesh.vertices.isEmpty());
        mesh = PhotoCrap::Render::regionMesh(region, {1, 1}, {100, 100, 20, 20});
        QVERIFY(!mesh.fallback && mesh.vertices.isEmpty());
        QVERIFY(PhotoCrap::Render::regionMesh(region, {1, 1}, {}).vertices.isEmpty());
        QVERIFY(PhotoCrap::Render::regionMesh({}, {1, 1}, {0, 0, 20, 20}).vertices.isEmpty());
    }
    void fragmentedRegionsAtViewEdges()
    {
        QRegion region;
        for (int i = 0; i < 2000; ++i)
            region += QRect(1000 + i * 2, 1000 + (i % 2) * 3, 1, 1);
        const auto outside = PhotoCrap::Render::regionMesh(region, {1, 1}, {0, 0, 100, 100}, 1);
        QVERIFY(outside.vertices.isEmpty() && !outside.fallback);
        const auto touching = PhotoCrap::Render::regionMesh(region, {1, 1}, {900, 1000, 100, 1});
        QVERIFY(touching.vertices.isEmpty());
        const auto edge = PhotoCrap::Render::regionMesh(region, {1, 1}, {1000.5, 1000, 1, 1});
        QCOMPARE(edge.vertices.size(), 6);
        QCOMPARE(area(edge), .5);
    }
};
QTEST_GUILESS_MAIN(RegionMeshTest)
#include "RegionMeshTest.moc"

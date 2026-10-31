#include "render/CropShade.h"
#include <QRectF>
#include <QtTest>
#include <cmath>
#include <limits>

class CropShadeTest : public QObject
{
    Q_OBJECT
private slots:
    void planesMatchConvexQuadsAtDifferentScales()
    {
        const QList<QPolygonF> quads{
            QPolygonF{{10, 20}, {200, 20}, {200, 160}, {10, 160}},
            QPolygonF{{20, 30}, {260, 60}, {220, 230}, {50, 190}},
            QPolygonF{{-500000, -499980}, {500000, 500020}, {499960, 500060}, {-500040, -499940}}};
        for (const auto &quad : quads)
            for (qreal zoom : {.05, 1., 32.})
                for (const auto origin :
                     {QPointF{}, QPointF(11.25, -70.5), QPointF(31000, 27000)}) {
                    const auto parameters =
                        PhotoCrap::Render::cropShadeParameters(quad, zoom, origin);
                    QCOMPARE(parameters.valid, 1.f);
                    QPolygonF projected;
                    for (const auto &p : quad)
                        projected.append(p * zoom - origin);
                    for (int y = -50; y < 260; y += 11)
                        for (int x = -40; x < 300; x += 7) {
                            const QPointF point(x + .25, y + .5);
                            bool          inside = true, nearEdge = false;
                            for (const auto &edge : parameters.edges) {
                                QVERIFY(std::isfinite(edge.x()) && std::isfinite(edge.y()) &&
                                        std::isfinite(edge.z()));
                                QVERIFY(qAbs(std::hypot(edge.x(), edge.y()) - 1) < .00001);
                                const qreal distance =
                                    edge.x() * point.x() + edge.y() * point.y() + edge.z();
                                inside &= distance <= 0;
                                nearEdge |= qAbs(distance) < .05;
                            }
                            if (!nearEdge)
                                QCOMPARE(inside, projected.containsPoint(point, Qt::OddEvenFill));
                        }
                }
    }
    void invalidQuadsProduceFullShadeWithoutInvalidNumbers()
    {
        const qreal            nan = std::numeric_limits<qreal>::quiet_NaN();
        const QList<QPolygonF> quads{{},
                                     {{0, 0}, {10, 0}, {10, 0}, {0, 0}},
                                     {{0, 0}, {10, 10}, {0, 10}, {10, 0}},
                                     {{0, 0}, {0, 10}, {10, 10}, {10, 0}},
                                     {{0, 0}, {10, 0}, {10, 10}, {nan, 10}}};
        for (const auto &quad : quads) {
            const auto p = PhotoCrap::Render::cropShadeParameters(quad, 1, {});
            QCOMPARE(p.valid, 0.f);
            for (const auto &edge : p.edges)
                QVERIFY(edge.isNull());
        }
        const QPolygonF quad{{10, 10}, {90, 10}, {90, 70}, {10, 70}};
        for (qreal zoom : {qreal(0), qreal(-1), nan})
            QCOMPARE(PhotoCrap::Render::cropShadeParameters(quad, zoom, {}).valid, 0.f);
        QCOMPARE(PhotoCrap::Render::cropShadeParameters(quad, 1, {nan, 0}).valid, 0.f);
    }
    void viewportOriginRetainsTheSameEdgeDistance()
    {
        const QPolygonF quad{{100000, 100020}, {200000, 200020}, {199960, 200060}, {99960, 100060}};
        const QPointF   firstOrigin(150000, 150000), secondOrigin(150031, 150017);
        const auto      a = PhotoCrap::Render::cropShadeParameters(quad, 1, firstOrigin);
        const auto      b = PhotoCrap::Render::cropShadeParameters(quad, 1, secondOrigin);
        QCOMPARE(a.valid, 1.f);
        QCOMPARE(b.valid, 1.f);
        for (int i = 0; i < 4; ++i) {
            const auto p      = QPointF(150100, 150120);
            const auto localA = p - firstOrigin, localB = p - secondOrigin;
            const auto ea = a.edges[i], eb = b.edges[i];
            const auto da = ea.x() * localA.x() + ea.y() * localA.y() + ea.z();
            const auto db = eb.x() * localB.x() + eb.y() * localB.y() + eb.z();
            QVERIFY(qAbs(da - db) < .005);
        }
    }
};
QTEST_GUILESS_MAIN(CropShadeTest)
#include "CropShadeTest.moc"

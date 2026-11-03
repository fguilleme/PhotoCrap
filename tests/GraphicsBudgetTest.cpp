#include "render/GraphicsBudget.h"
#include <QtTest>
using PhotoCrap::Render::GraphicsBudget;
class GraphicsBudgetTest : public QObject
{
    Q_OBJECT
private slots:
    void sharesSpareSpaceAndPinsVisibleResources()
    {
        auto budget = std::make_shared<GraphicsBudget>();
        auto a = GraphicsBudget::attach(budget, 100), b = GraphicsBudget::attach(budget, 100);
        a->pin(20);
        b->pin(40);
        QCOMPARE(a->allowance(), 40);
        QCOMPARE(b->allowance(), 60);
        a->report(GraphicsBudget::Photo, 30);
        b->report(GraphicsBudget::Mask, 50);
        QCOMPARE(budget->usage().resident, 80);
        QCOMPARE(budget->usage().peak, 80);
        QCOMPARE(a->available(), 60);
        b->pin(120);
        QCOMPARE(a->allowance(), 20);
        QCOMPARE(b->allowance(), 120);
        QCOMPARE(a->available(), 0);
        b.reset();
        QCOMPARE(a->allowance(), 100);
        QCOMPARE(budget->usage().resident, 30);
        a.reset();
        QCOMPARE(budget->usage().resident, 0);
        QCOMPARE(budget->usage().target, 0);
    }
    void invalidationDoesNotHoldTheAccountingLock()
    {
        auto budget = std::make_shared<GraphicsBudget>();
        auto a = GraphicsBudget::attach(budget, 100), b = GraphicsBudget::attach(budget, 50);
        int  notices = 0;
        a->invalidateWith([&] {
            ++notices;
            (void)budget->usage();
        });
        b->pin(20);
        QCOMPARE(notices, 1);
        b->pin(20);
        QCOMPARE(notices, 1);
        b->configure(25);
        QCOMPARE(notices, 2);
        b->pin(10);
        QCOMPARE(b->allowance(), 25);
        b.reset();
        QCOMPARE(notices, 4);
    }
    void zeroTargetKeepsOnlyPins()
    {
        auto budget = std::make_shared<GraphicsBudget>();
        auto a      = GraphicsBudget::attach(budget, 0);
        a->pin(50);
        a->report(GraphicsBudget::Stack, 50);
        QCOMPARE(a->allowance(), 50);
        a->report(GraphicsBudget::Stack, 0);
        a->pin(0);
        QCOMPARE(budget->usage().resident, 0);
    }
};
QTEST_GUILESS_MAIN(GraphicsBudgetTest)
#include "GraphicsBudgetTest.moc"

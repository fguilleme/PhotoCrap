#pragma once

#include "Document.h"
#include "text/TextEditorController.h"
#include <QFontDatabase>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

// Exercise the real delegates: hovering must never change the editable runs or their history.
inline bool checkFontMenu(QQuickWindow *window, Document &doc, QObject *panel, QQuickItem *input,
                          TextEditorController *formatter)
{
    auto *combo    = panel->findChild<QQuickItem *>("textFontFamily");
    auto *advanced = panel->findChild<QQuickItem *>("textAdvancedOptions");
    auto *toggle   = panel->findChild<QQuickItem *>("textAdvancedToggle");
    if (!combo || !toggle || !advanced || advanced->isVisible())
        return false;
    const auto center = [](QQuickItem *item) {
        return item->mapToScene(item->boundingRect().center()).toPoint();
    };
    auto *button = window->findChild<QQuickItem *>("textTypographyButton");
    auto *body = panel->property("contentItem").value<QQuickItem *>();
    auto *alignment = panel->findChild<QQuickItem *>("textAlignmentOptions");
    if (!button || !body || !alignment || alignment->isVisible())
        return false;
    const auto panelBounds = [&] {
        return body->mapRectToScene(body->boundingRect()).adjusted(
            -panel->property("leftPadding").toReal(), -panel->property("topPadding").toReal(),
            panel->property("rightPadding").toReal(), panel->property("bottomPadding").toReal());
    };
    const auto anchored = [&] {
        const QRectF bounds = panelBounds();
        const QRectF anchor = button->mapRectToScene(button->boundingRect());
        const qreal gap = anchor.top() - bounds.bottom();
        return gap >= 4 && gap <= 8 && bounds.top() >= 0 &&
               qAbs(anchor.center().x() - bounds.center().x()) < 2;
    };
    if (!QTest::qWaitFor(anchored, 1000))
        return false;
    const qreal foldedTop = panelBounds().top();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(toggle));
    if (!QTest::qWaitFor([&] {
            return advanced->isVisible() && panelBounds().top() < foldedTop - 30 && anchored();
        }, 1000))
        return false;
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(toggle));
    if (!QTest::qWaitFor([&] {
            return !advanced->isVisible() && qAbs(panelBounds().top() - foldedTop) < 2 && anchored();
        }, 1000))
        return false;

    input->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Return);
    if (!QTest::qWaitFor([&] { return alignment->isVisible(); }, 1000))
        return false;
    QTest::keyClick(window, Qt::Key_Backspace);
    if (!QTest::qWaitFor([&] { return !alignment->isVisible(); }, 1000))
        return false;
    const auto state = doc.textEditState();
    const auto w = state.value("width", 320).toReal(), h = state.value("height", 180).toReal();
    doc.setTextBox(true, w, h);
    if (!alignment->isVisible())
        return false;
    doc.setTextBox(false, w, h);
    if (alignment->isVisible() || !QTest::qWaitFor(anchored, 1000))
        return false;

    QMetaObject::invokeMethod(input, "select", Q_ARG(int, 0), Q_ARG(int, 3));
    const QImage original = doc.composite();
    const auto   before   = doc.textEditState();
    const auto   history  = doc.history();
    const auto   family   = formatter->format(1).value("family").toString();
    const auto   families = formatter->fontFamilies();
    QString      fixed    = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    if (!families.contains(fixed) || fixed == family) {
        for (const QString &name : families)
            if (name != family && QFontDatabase::isFixedPitch(name) &&
                QFontDatabase::writingSystems(name).contains(QFontDatabase::Latin)) {
                fixed = name;
                break;
            }
    }
    const int index = families.indexOf(fixed);
    if (index < 0 || family == fixed)
        return false;
    auto *indicator = combo->property("indicator").value<QQuickItem *>();
    if (!indicator)
        return false;
    auto openCandidate = [&]() -> QQuickItem * {
        // Selecting a run can change the visible style row and move this anchored panel.
        QTest::qWait(50);
        const auto arrow = center(indicator);
        QTest::mouseMove(window, arrow);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, arrow);
        auto *menu = combo->property("popup").value<QObject *>();
        auto *list = menu ? menu->property("contentItem").value<QQuickItem *>() : nullptr;
        if (!list || !QTest::qWaitFor(
                         [&] {
                             return menu->property("opened").toBool() &&
                                    list->property("count").toInt() > index && list->height() > 30;
                         },
                         1500)) {
            qWarning() << "Font picker did not open at" << arrow << combo->mapToScene(QPointF{})
                       << list << (menu && menu->property("opened").toBool());
            return nullptr;
        }
        QMetaObject::invokeMethod(list, "positionViewAtIndex", Q_ARG(int, index), Q_ARG(int, 1));
        QQuickItem *row = nullptr;
        if (!QTest::qWaitFor(
                [&] {
                    QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, row),
                                                    Q_ARG(int, index));
                    return row != nullptr;
                },
                1000))
            return nullptr;
        return row;
    };
    auto *row = openCandidate();
    if (!row || row->property("font").value<QFont>().family() != fixed) {
        qWarning() << "No font delegate for" << fixed << row;
        return false;
    }
    QTest::mouseMove(window, center(row));
    if (!QTest::qWaitFor([&] { return doc.displayComposite() != original; }, 3000) ||
        doc.textEditState() != before || doc.composite() != original || doc.history() != history ||
        formatter->format(1).value("family") != family) {
        qWarning() << "Preview did not appear or changed the original" << fixed
                   << panel->property("candidate");
        return false;
    }
    const QImage preview = doc.displayComposite();
    if (QCoreApplication::arguments().contains("--capture-text"))
        window->grabWindow().save("/tmp/photocrap-font-menu.png");
    QTest::keyClick(window, Qt::Key_Escape);
    if (!QTest::qWaitFor([&] { return doc.displayComposite() == original; }, 1000) ||
        !panel->property("visible").toBool() || input->property("selectionEnd").toInt() != 3)
        return false;
    row = openCandidate();
    if (!row)
        return false;
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(row));
    return QTest::qWaitFor([&] { return formatter->format(1).value("family") == fixed; }, 1000) &&
           formatter->format(4).value("family") == family && doc.history() == history &&
           doc.composite() == preview;
}

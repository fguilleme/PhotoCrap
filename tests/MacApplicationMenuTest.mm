#include "MacApplicationMenuTest.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QtTest>

#import <AppKit/AppKit.h>

namespace
{
NSMenuItem *preferencesItem()
{
    NSMenu *menu = [[[NSApp mainMenu] itemAtIndex:0] submenu];
    for (NSMenuItem *item in [menu itemArray]) {
        if ([[item keyEquivalent] isEqualToString:@","])
            return item;
    }
    return nil;
}

bool checkDialog(QObject *_preferences, QObject *_brush, const char *_stage)
{
    QTest::qWait(100);
    const bool ok =
        _preferences->property("visible").toBool() && !_brush->property("visible").toBool();
    if (!ok)
        qWarning() << "Native Preferences must open application settings, never brush properties:"
                   << _stage << "preferences" << _preferences->property("visible") << "brush"
                   << _brush->property("visible");
    QMetaObject::invokeMethod(_preferences, "close");
    QMetaObject::invokeMethod(_brush, "reject");
    QTest::qWait(100);
    return ok;
}
} // namespace

bool runMacApplicationMenuTest(QQuickWindow *_window)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
        qWarning("Native menu regression test requires the cocoa platform");
        return false;
    }
    auto preferences   = _window->findChild<QObject *>("preferencesDialog");
    auto brush         = _window->findChild<QObject *>("brushSettingsDialog");
    auto brushMenuItem = _window->findChild<QObject *>("brushOptionsMenuItem");
    if (!preferences || !brush || !brushMenuItem)
        return false;

    // Exercise the actual Cocoa menu after startup and repeated retranslations,
    // not just the QML function that the native action ought to call.
    _window->requestActivate();
    [NSApp activateIgnoringOtherApps:YES];
    QTest::qWait(5100);
    for (const QString &language :
         {QStringLiteral("fr"), QStringLiteral("en"), QStringLiteral("fr")}) {
        QMetaObject::invokeMethod(_window, "setInterfaceLanguage", Q_ARG(QVariant, language));
        QTest::qWait(100);

        // Opening/closing a modal changes native action enablement and forces
        // another synchronization, which used to undo the startup target patch.
        NSMenuItem *nativeBrush = nil;
        for (NSMenuItem *top in [[NSApp mainMenu] itemArray]) {
            if (top == [[NSApp mainMenu] itemAtIndex:0])
                continue;
            for (NSMenuItem *entry in [[top submenu] itemArray]) {
                if (QString::fromNSString([entry title]) ==
                    brushMenuItem->property("text").toString())
                    nativeBrush = entry;
            }
        }
        if (!nativeBrush) {
            qWarning("Brush properties must remain in their own native menu item");
            return false;
        }
        [[nativeBrush menu]
            performActionForItemAtIndex:[[nativeBrush menu] indexOfItem:nativeBrush]];
        QTest::qWait(100);
        if (!brush->property("visible").toBool() || preferences->property("visible").toBool())
            return false;
        QMetaObject::invokeMethod(brush, "reject");
        QTest::qWait(100);

        NSMenuItem *item = preferencesItem();
        [[item menu] update];
        if (!item || [item isHidden] || ![item isEnabled]) {
            qWarning("Application menu must contain enabled Preferences with Command-comma");
            for (NSMenuItem *top in [[NSApp mainMenu] itemArray]) {
                qWarning() << QString::fromNSString([top title]);
                for (NSMenuItem *entry in [[top submenu] itemArray])
                    qWarning() << QString::fromNSString([entry title])
                               << QString::fromNSString([entry keyEquivalent]) << [entry isEnabled]
                               << [entry isHidden];
            }
            return false;
        }
        [[item menu] performActionForItemAtIndex:[[item menu] indexOfItem:item]];
        if (!checkDialog(preferences, brush, "native menu"))
            return false;

        NSEvent *key = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                        location:NSZeroPoint
                                   modifierFlags:NSEventModifierFlagCommand
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                      characters:@","
                     charactersIgnoringModifiers:@","
                                       isARepeat:NO
                                         keyCode:43];
        if (![[NSApp mainMenu] performKeyEquivalent:key] ||
            !checkDialog(preferences, brush, "Cocoa Command-comma"))
            return false;

        QTest::keyClick(_window, Qt::Key_Comma, Qt::ControlModifier);
        if (!checkDialog(preferences, brush, "Qt Command-comma"))
            return false;
    }
    return true;
}

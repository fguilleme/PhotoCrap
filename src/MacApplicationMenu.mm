#include "MacApplicationMenu.h"

#include <QMetaObject>
#include <QObject>
#include <QTimer>

#import <AppKit/AppKit.h>

@interface                            PhotoCrapPreferencesTarget : NSObject
@property(nonatomic, assign) QObject *rootObject;
- (void)openPreferences:(id)sender;
@end

@implementation PhotoCrapPreferencesTarget
- (void)openPreferences:(id)sender
{
    Q_UNUSED(sender);
    if (self.rootObject)
        QMetaObject::invokeMethod(self.rootObject, "openPreferences", Qt::QueuedConnection);
}
@end

namespace
{
PhotoCrapPreferencesTarget *preferencesTarget()
{
    static PhotoCrapPreferencesTarget *target = [[PhotoCrapPreferencesTarget alloc] init];
    return target;
}

bool isPreferencesItem(NSMenuItem *_item)
{
    if ([[_item keyEquivalent] isEqualToString:@","])
        return true;
    const NSString *title = [[_item title] lowercaseString];
    return [title containsString:@"preferences"] || [title containsString:@"settings"] ||
           [title containsString:@"préférences"] || [title containsString:@"réglages"];
}

bool installPreferencesHandler(QObject *_rootObject)
{
    NSMenu *mainMenu = [NSApp mainMenu];
    if (!mainMenu || [mainMenu numberOfItems] < 1)
        return false;
    NSMenu *applicationMenu = [[mainMenu itemAtIndex:0] submenu];
    if (!applicationMenu)
        return false;
    for (NSMenuItem *item in [applicationMenu itemArray]) {
        if (!isPreferencesItem(item))
            continue;
        PhotoCrapPreferencesTarget *target = preferencesTarget();
        target.rootObject                  = _rootObject;
        [item setTarget:target];
        [item setAction:@selector(openPreferences:)];
        return true;
    }
    return false;
}
} // namespace

void scheduleMacPreferencesHandler(QObject *_rootObject)
{
    // Qt builds and may rebuild the native application menu asynchronously while
    // the QML scene is loading. Re-apply the target across that startup window so
    // its text-based role inference cannot reconnect Preferences to another item.
    static constexpr int delays[]{0, 100, 300, 750, 1500, 3000, 5000};
    for (const int delay : delays) {
        QTimer::singleShot(delay, _rootObject,
                           [_rootObject] { installPreferencesHandler(_rootObject); });
    }
}

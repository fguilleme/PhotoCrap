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

void installPreferencesHandler(QObject *_rootObject)
{
    NSMenu *mainMenu = [NSApp mainMenu];
    if (!mainMenu || [mainMenu numberOfItems] < 1)
        return;
    NSMenu *applicationMenu = [[mainMenu itemAtIndex:0] submenu];
    if (!applicationMenu)
        return;
    for (NSMenuItem *item in [applicationMenu itemArray]) {
        if (!isPreferencesItem(item))
            continue;
        PhotoCrapPreferencesTarget *target = preferencesTarget();
        target.rootObject                  = _rootObject;
        [item setTarget:target];
        [item setAction:@selector(openPreferences:)];
        return;
    }
}
} // namespace

void scheduleMacPreferencesHandler(QObject *_rootObject)
{
    QTimer::singleShot(100, _rootObject, [_rootObject] { installPreferencesHandler(_rootObject); });
}

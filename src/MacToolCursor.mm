#include "MacToolCursor.h"
#include <QBuffer>
#include <QCursor>
#include <QGuiApplication>
#include <QPixmap>

#import <AppKit/AppKit.h>

void reapplyMacToolCursor(const QCursor &_cursor)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa") || !NSApp.active)
        return;
    const QPixmap pixmap = _cursor.pixmap();
    if (pixmap.isNull())
        return;
    // Qt's Cocoa backend may keep its cached cursor while AppKit has reset the
    // actual cursor during a floating window transition. Reapply the bitmap.
    static qint64    previousKey = 0;
    static QPoint    previousHotspot;
    static NSCursor *nativeCursor = nil;
    if (previousKey != pixmap.cacheKey() || previousHotspot != _cursor.hotSpot()) {
        QByteArray bytes;
        QBuffer    buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!pixmap.save(&buffer, "PNG"))
            return;
        NSData  *data  = [NSData dataWithBytes:bytes.constData() length:bytes.size()];
        NSImage *image = [[NSImage alloc] initWithData:data];
        if (!image)
            return;
        image.size            = NSMakeSize(pixmap.width() / pixmap.devicePixelRatio(),
                                           pixmap.height() / pixmap.devicePixelRatio());
        NSCursor *replacement = [[NSCursor alloc]
            initWithImage:image
                  hotSpot:NSMakePoint(_cursor.hotSpot().x(), _cursor.hotSpot().y())];
        [image release];
        [nativeCursor release];
        nativeCursor    = replacement;
        previousKey     = pixmap.cacheKey();
        previousHotspot = _cursor.hotSpot();
    }
    [nativeCursor set];
}

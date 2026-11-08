#import "PCRPreviewReader.h"
#import <QuickLookUI/QuickLookUI.h>

@interface PCRPreviewProvider : QLPreviewProvider <QLPreviewingController>
@end
@implementation PCRPreviewProvider
- (void)providePreviewForFileRequest:(QLFilePreviewRequest *)request
                   completionHandler:(void (^)(QLPreviewReply *, NSError *))handler
{
    NSURL *url = request.fileURL;
    dispatch_async(PCRPreviewQueue(), ^{
      @autoreleasepool {
          NSError *        error   = nil;
          PCRPreviewImage *preview = PCRReadPreview(url, 1280, &error);
          if (!preview) {
              handler(nil, error);
              return;
          }
          id              image = preview.image;
          QLPreviewReply *reply = [[QLPreviewReply alloc]
              initWithContextSize:preview.size
                         isBitmap:YES
                     drawingBlock:^BOOL(CGContextRef context, QLPreviewReply *, NSError **) {
                       CGContextDrawImage(context,
                                          CGRectMake(0, 0, preview.size.width, preview.size.height),
                                          (__bridge CGImageRef)image);
                       return YES;
                     }];
          if (preview.sourceOnly)
              reply.title = [url.lastPathComponent
                  stringByAppendingString:
                      @" — Source layer preview · Save in PhotoCrap for the final image"];
          handler(reply, nil);
      }
    });
}
@end

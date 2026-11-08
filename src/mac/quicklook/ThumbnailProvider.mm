#import "PCRPreviewReader.h"
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>
#include <algorithm>
#include <cmath>

@interface PCRThumbnailProvider : QLThumbnailProvider
@end
@implementation PCRThumbnailProvider
- (void)provideThumbnailForFileRequest:(QLFileThumbnailRequest *)request
                     completionHandler:(void (^)(QLThumbnailReply *, NSError *))handler
{
    NSURL *       url     = request.fileURL;
    const CGSize  maximum = request.maximumSize;
    const CGFloat scale   = request.scale;
    dispatch_async(PCRPreviewQueue(), ^{
      @autoreleasepool {
          NSError *        error   = nil;
          PCRPreviewImage *preview = PCRReadPreview(
              url, NSUInteger(std::ceil(std::max(maximum.width, maximum.height) * scale)), &error);
          if (!preview) {
              handler(nil, error);
              return;
          }
          const CGFloat fit =
              std::min(maximum.width / preview.size.width, maximum.height / preview.size.height);
          const CGSize      size  = CGSizeMake(std::max(CGFloat(1), preview.size.width * fit),
                                         std::max(CGFloat(1), preview.size.height * fit));
          id                image = preview.image;
          QLThumbnailReply *reply = [QLThumbnailReply
              replyWithContextSize:size
                      drawingBlock:^BOOL(CGContextRef context) {
                        CGContextDrawImage(context, CGRectMake(0, 0, size.width, size.height),
                                           (__bridge CGImageRef)image);
                        return YES;
                      }];
          reply.extensionBadge    = preview.sourceOnly ? @"PCR*" : @"PCR";
          handler(reply, nil);
      }
    });
}
@end

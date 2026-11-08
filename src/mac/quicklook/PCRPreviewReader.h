#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN
@interface                      PCRPreviewImage : NSObject
@property(nonatomic, strong) id image;
@property(nonatomic) CGSize     size;
@property(nonatomic) BOOL       sourceOnly;
@property(nonatomic) NSUInteger bytesRead;
@end
PCRPreviewImage *_Nullable PCRReadPreview(NSURL *url, NSUInteger maximumPixelSize, NSError **error);
dispatch_queue_t PCRPreviewQueue(void);
NS_ASSUME_NONNULL_END

#import "PCRPreviewReader.h"
#include "storage/ProjectPreviewScan.h"
#import <ImageIO/ImageIO.h>
#include <algorithm>
#include <fstream>
#include <memory>
#include <unistd.h>

@implementation PCRPreviewImage
@end

dispatch_queue_t PCRPreviewQueue(void)
{
    static dispatch_queue_t queue;
    static dispatch_once_t  once;
    dispatch_once(&once, ^{
      queue = dispatch_queue_create("org.photocrap.quicklook.reader", DISPATCH_QUEUE_SERIAL);
    });
    return queue;
}
namespace
{
int digit(int c)
{
    return c >= 'A' && c <= 'Z'   ? c - 'A'
           : c >= 'a' && c <= 'z' ? c - 'a' + 26
           : c >= '0' && c <= '9' ? c - '0' + 52
           : c == '+'             ? 62
           : c == '/'             ? 63
                                  : -1;
}
struct File {
    int fd = -1;
    explicit File(int descriptor) : fd(descriptor)
    {
    }
    ~File()
    {
        if (fd >= 0)
            close(fd);
    }
};
size_t readImage(void *info, void *buffer, off_t position, size_t count)
{
    const auto n = pread(static_cast<File *>(info)->fd, buffer, count, position);
    return n > 0 ? size_t(n) : 0;
}
void closeImage(void *info)
{
    delete static_cast<File *>(info);
}

// Spool only one legacy raster to an unlinked file. ImageIO can seek it without
// retaining a document-sized Base64 string or an encoded image buffer in RAM.
CGImageSourceRef legacySource(std::ifstream &input, const PhotoCrap::Preview::Entry &entry)
{
    if (!entry.length || entry.length > 256ULL * 1024 * 1024)
        return nullptr;
    NSString *pattern =
        [NSTemporaryDirectory() stringByAppendingPathComponent:@"pcr-quicklook-XXXXXX"];
    std::string name(pattern.fileSystemRepresentation);
    int         descriptor = mkstemp(name.data());
    if (descriptor < 0)
        return nullptr;
    unlink(name.c_str());
    auto file = std::make_unique<File>(descriptor);
    input.clear();
    input.seekg(std::streamoff(entry.offset));
    std::array<char, 16384>          raw{};
    std::array<unsigned char, 12288> decoded{};
    std::uint64_t                    remaining = entry.length, written = 0;
    unsigned                         value = 0;
    int                              phase = 0, padding = 0, unicode = -1, hexCount = 0;
    bool                             escape = false, finished = false;
    auto                             started = std::chrono::steady_clock::now();
    while (remaining) {
        if (std::chrono::steady_clock::now() - started > std::chrono::seconds(5))
            return nullptr;
        const auto count = std::min<std::uint64_t>(remaining, raw.size());
        input.read(raw.data(), std::streamsize(count));
        if (std::uint64_t(input.gcount()) != count)
            return nullptr;
        remaining -= count;
        size_t output = 0;
        for (std::size_t i = 0; i < count; ++i) {
            int c = static_cast<unsigned char>(raw[i]);
            if (unicode >= 0) {
                int h = c >= '0' && c <= '9'   ? c - '0'
                        : c >= 'a' && c <= 'f' ? c - 'a' + 10
                        : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                               : -1;
                if (h < 0)
                    return nullptr;
                unicode = unicode * 16 + h;
                if (++hexCount < 4)
                    continue;
                c       = unicode;
                unicode = -1;
            } else if (escape) {
                escape = false;
                if (c == 'u') {
                    unicode  = 0;
                    hexCount = 0;
                    continue;
                }
                if (c != '/')
                    return nullptr;
            } else if (c == '\\') {
                escape = true;
                continue;
            }
            if (finished)
                return nullptr;
            if (c == '=') {
                if (phase < 2 || ++padding > 2)
                    return nullptr;
                value <<= 6;
            } else {
                const int d = digit(c);
                if (d < 0 || padding)
                    return nullptr;
                value = (value << 6) | unsigned(d);
            }
            if (++phase == 4) {
                if ((padding == 2 && (value & 0xffff)) || (padding == 1 && (value & 0xff)))
                    return nullptr;
                decoded[output++] = (value >> 16) & 255;
                if (padding < 2)
                    decoded[output++] = (value >> 8) & 255;
                if (!padding)
                    decoded[output++] = value & 255;
                finished = padding != 0;
                value    = 0;
                phase    = 0;
            }
        }
        if (written + output > 192ULL * 1024 * 1024)
            return nullptr;
        size_t offset = 0;
        while (offset < output) {
            const auto n = write(descriptor, decoded.data() + offset, output - offset);
            if (n <= 0)
                return nullptr;
            offset += size_t(n);
        }
        written += output;
    }
    if (phase || escape || unicode >= 0 || !written)
        return nullptr;
    const CGDataProviderDirectCallbacks callbacks{0, nullptr, nullptr, readImage, closeImage};
    CGDataProviderRef provider = CGDataProviderCreateDirect(file.get(), off_t(written), &callbacks);
    if (!provider)
        return nullptr;
    file.release();
    CGImageSourceRef source = CGImageSourceCreateWithDataProvider(
        provider, (__bridge CFDictionaryRef)
                      @{(__bridge NSString *)kCGImageSourceShouldCache : @NO});
    CGDataProviderRelease(provider);
    return source;
}
}

PCRPreviewImage *PCRReadPreview(NSURL *url, NSUInteger maximumPixelSize, NSError **error)
{
    const auto fail = [&]() -> PCRPreviewImage * {
        if (error)
            *error = [NSError
                errorWithDomain:@"org.photocrap.quicklook"
                           code:1
                       userInfo:@{
                           NSLocalizedDescriptionKey : @"No readable PhotoCrap preview. Open and "
                                                       @"save the project in PhotoCrap to add one."
                       }];
        return nil;
    };
    if (!url.isFileURL)
        return fail();
    const BOOL scoped = [url startAccessingSecurityScopedResource];
    @try {
        std::ifstream input(url.fileSystemRepresentation, std::ios::binary);
        if (!input)
            return fail();
        const auto       entry  = PhotoCrap::Preview::Scan(input).read(true);
        CGImageSourceRef source = nullptr;
        bool             legacy = entry.json.empty();
        if (!legacy) {
            NSData *json   = [NSData dataWithBytes:entry.json.data() length:entry.json.size()];
            id      object = [NSJSONSerialization JSONObjectWithData:json options:0 error:nil];
            if (![object isKindOfClass:NSDictionary.class] || ![object[@"version"] isEqual:@1] ||
                ![object[@"encoding"] isEqual:@"jpeg"] ||
                ![object[@"image"] isKindOfClass:NSString.class])
                return fail();
            NSData *image = [[NSData alloc] initWithBase64EncodedString:object[@"image"] options:0];
            if (!image.length || image.length > 512 * 1024)
                return fail();
            source = CGImageSourceCreateWithData(
                (__bridge CFDataRef)image,
                (__bridge CFDictionaryRef)
                    @{(__bridge NSString *)kCGImageSourceShouldCache : @NO});
        } else
            source = legacySource(input, entry);
        if (!source)
            return fail();
        NSDictionary *properties =
            CFBridgingRelease(CGImageSourceCopyPropertiesAtIndex(source, 0, nullptr));
        const double width =
            [properties[(__bridge NSString *)kCGImagePropertyPixelWidth] doubleValue];
        const double height =
            [properties[(__bridge NSString *)kCGImagePropertyPixelHeight] doubleValue];
        const bool valid = width > 0 && height > 0 && width <= 16384 && height <= 16384 &&
                           width * height <= 32. * 1024 * 1024 &&
                           (legacy || (width <= 1280 && height <= 1280));
        CGImageRef image =
            valid
                ? CGImageSourceCreateThumbnailAtIndex(
                      source, 0, (__bridge CFDictionaryRef) @{
                          (__bridge NSString *)kCGImageSourceCreateThumbnailFromImageAlways : @YES,
                          (__bridge NSString *)kCGImageSourceCreateThumbnailWithTransform : @YES,
                          (__bridge NSString *)kCGImageSourceShouldCacheImmediately : @YES,
                          (__bridge NSString *)kCGImageSourceThumbnailMaxPixelSize :
                              @(std::clamp<NSUInteger>(maximumPixelSize, 32, 1280))
                      })
                : nullptr;
        CFRelease(source);
        if (!image)
            return fail();
        // Detach the display bitmap so the preview holds no project/scratch fd.
        CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGContextRef    bitmap =
            CGBitmapContextCreate(nullptr, CGImageGetWidth(image), CGImageGetHeight(image), 8, 0,
                                  colorSpace, kCGImageAlphaPremultipliedLast);
        CGColorSpaceRelease(colorSpace);
        if (!bitmap) {
            CGImageRelease(image);
            return fail();
        }
        CGContextDrawImage(
            bitmap, CGRectMake(0, 0, CGImageGetWidth(image), CGImageGetHeight(image)), image);
        CGImageRelease(image);
        image = CGBitmapContextCreateImage(bitmap);
        CGContextRelease(bitmap);
        if (!image)
            return fail();
        PCRPreviewImage *result = [PCRPreviewImage new];
        result.size             = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));
        result.image            = CFBridgingRelease(image);
        result.sourceOnly       = legacy;
        result.bytesRead        = entry.bytesRead;
        return result;
    } @finally {
        if (scoped)
            [url stopAccessingSecurityScopedResource];
    }
}

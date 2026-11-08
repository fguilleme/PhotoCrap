#import "PCRPreviewReader.h"
#import <ImageIO/ImageIO.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <unistd.h>

namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
NSData *fixture(BOOL jpeg)
{
    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef    context =
        CGBitmapContextCreate(nullptr, 160, 120, 8, 0, space, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(space);
    CGContextSetRGBFillColor(context, .2, .5, .8, 1);
    CGContextFillRect(context, CGRectMake(0, 0, 160, 120));
    CGImageRef image = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    NSMutableData *       data        = [NSMutableData data];
    CGImageDestinationRef destination = CGImageDestinationCreateWithData(
        (__bridge CFMutableDataRef)data,
        (__bridge CFStringRef)(jpeg ? UTTypeJPEG.identifier : UTTypePNG.identifier), 1, nullptr);
    CGImageDestinationAddImage(destination, image, nullptr);
    check(CGImageDestinationFinalize(destination), "encode test image");
    CFRelease(destination);
    CGImageRelease(image);
    return data;
}
}
int main(int argc, const char **argv)
{
    @autoreleasepool {
        try {
            if (argc > 2 && std::string(argv[1]) == "--fixture") {
                NSData *      jpeg = fixture(YES), *png = fixture(NO);
                NSDictionary *preview = @{
                    @"version" : @1,
                    @"encoding" : @"jpeg",
                    @"image" : [jpeg base64EncodedStringWithOptions:0]
                };
                NSString *json =
                    [[NSString alloc] initWithData:[NSJSONSerialization dataWithJSONObject:preview
                                                                                   options:0
                                                                                     error:nil]
                                          encoding:NSUTF8StringEncoding];
                NSString *project = [NSString
                    stringWithFormat:
                        @"{\"format\":\"PhotoCrap\",\"preview\":%@,\"version\":9,\"width\":160,"
                        @"\"height\":120,\"channelBits\":8,\"layers\":[{\"name\":\"Quick Look "
                        @"test\",\"kind\":\"pixel\",\"visible\":true,\"opacity\":1,\"png\":\"%@\"}]"
                        @"}",
                        json, [png base64EncodedStringWithOptions:0]];
                check([project writeToFile:@(argv[2])
                                atomically:YES
                                  encoding:NSUTF8StringEncoding
                                     error:nil],
                      "write Finder fixture");
                return 0;
            }
            if (argc > 2 && std::string(argv[1]) == "--finder") {
                __block BOOL                  done = NO, success = NO;
                QLThumbnailGenerationRequest *request = [[QLThumbnailGenerationRequest alloc]
                      initWithFileAtURL:[NSURL fileURLWithPath:@(argv[2])]
                                   size:CGSizeMake(256, 256)
                                  scale:1
                    representationTypes:QLThumbnailGenerationRequestRepresentationTypeThumbnail];
                [[QLThumbnailGenerator sharedGenerator]
                    generateBestRepresentationForRequest:request
                                       completionHandler:^(QLThumbnailRepresentation *result,
                                                           NSError *                  error) {
                                         success =
                                             result != nil &&
                                             result.type == QLThumbnailRepresentationTypeThumbnail;
                                         if (success)
                                             printf("Finder thumbnail: %zu x %zu\n",
                                                    CGImageGetWidth(result.CGImage),
                                                    CGImageGetHeight(result.CGImage));
                                         else
                                             fprintf(stderr, "Finder thumbnail: %s\n",
                                                     error.description.UTF8String);
                                         done = YES;
                                       }];
                NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:25];
                while (!done && deadline.timeIntervalSinceNow > 0)
                    [[NSRunLoop currentRunLoop]
                        runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.05]];
                if (!done)
                    [[QLThumbnailGenerator sharedGenerator] cancelRequest:request];
                check(done && success, "system Quick Look thumbnail request");
                return 0;
            }
            if (argc > 1) {
                NSError *error   = nil;
                auto *   preview = PCRReadPreview([NSURL fileURLWithPath:@(argv[1])], 1280, &error);
                check(preview != nil, error.localizedDescription.UTF8String ?: "read project");
                printf("{\"width\":%.0f,\"height\":%.0f,\"sourceOnly\":%s,\"bytesScanned\":%lu}\n",
                       preview.size.width, preview.size.height,
                       preview.sourceOnly ? "true" : "false", (unsigned long)preview.bytesRead);
                return 0;
            }
            NSString *folder =
                [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
            check([[NSFileManager defaultManager] createDirectoryAtPath:folder
                                            withIntermediateDirectories:YES
                                                             attributes:nil
                                                                  error:nil],
                  "create test directory");
            NSURL *url =
                [NSURL fileURLWithPath:[folder stringByAppendingPathComponent:@"test.pcr"]];
            NSData *      image   = fixture(YES);
            NSDictionary *preview = @{
                @"version" : @1,
                @"encoding" : @"jpeg",
                @"image" : [image base64EncodedStringWithOptions:0]
            };
            NSData *json = [NSJSONSerialization dataWithJSONObject:preview options:0 error:nil];
            NSMutableData *prefix = [[@"{\"format\":\"PhotoCrap\",\"preview\":"
                dataUsingEncoding:NSUTF8StringEncoding] mutableCopy];
            [prefix appendData:json];
            [prefix appendData:[@",\"layers\":[" dataUsingEncoding:NSUTF8StringEncoding]];
            check([prefix writeToURL:url atomically:YES], "write prefix");
            int fd = open(url.fileSystemRepresentation, O_WRONLY);
            check(fd >= 0 && ftruncate(fd, 200LL * 1024 * 1024) == 0, "make large sparse project");
            close(fd);
            NSError *        error  = nil;
            PCRPreviewImage *result = PCRReadPreview(url, 80, &error);
            check(result && !result.sourceOnly && result.size.width == 80 &&
                      result.size.height == 60,
                  "read embedded preview");
            check(result.bytesRead <= 16384, "large project reads only its prefix");
            NSData *  png    = fixture(NO);
            NSString *base64 = [png base64EncodedStringWithOptions:0];
            for (NSString *payload in @[
                     base64, [base64 stringByReplacingOccurrencesOfString:@"/"
                                                               withString:@"\\u002f"]
                 ]) {
                NSString *legacy = [NSString
                    stringWithFormat:@"{\"format\":\"PhotoCrap\",\"layers\":[{\"png\":\"%@\"}]}",
                                     payload];
                check([legacy writeToURL:url
                              atomically:YES
                                encoding:NSUTF8StringEncoding
                                   error:nil],
                      "write legacy fixture");
                result = PCRReadPreview(url, 80, &error);
                check(result && result.sourceOnly && result.size.width == 80,
                      "read legacy source raster");
            }
            for (NSString *invalid in @[
                     @"not a project", @"{\"format\":\"Other\",\"preview\":{}}",
                     @"{\"format\":\"PhotoCrap\",\"layers\":[{\"png\":\"YWJj!\"}]}"
                 ]) {
                [invalid writeToURL:url atomically:YES encoding:NSUTF8StringEncoding error:nil];
                error = nil;
                check(PCRReadPreview(url, 80, &error) == nil && error != nil,
                      "reject malformed input");
            }
            [[NSFileManager defaultManager] removeItemAtPath:folder error:nil];
            puts("Native Quick Look reader: all checks passed");
        } catch (const std::exception &e) {
            fprintf(stderr, "Quick Look test failed: %s\n", e.what());
            return 1;
        }
    }
    return 0;
}

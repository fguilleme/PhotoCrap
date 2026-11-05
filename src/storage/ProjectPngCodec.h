#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ProjectPngImage {
    unsigned width, height;
    int bits, grayscale, littleEndian, srgb;
    unsigned dotsX, dotsY;
};

int projectPngWrite(const struct ProjectPngImage *image, void *context,
                    const unsigned char *(*row)(void *, unsigned),
                    int (*write)(void *, const unsigned char *, size_t));

struct ProjectPngReader;

// Return 1 for native rows, 0 for a decoding failure, and -1 for formats
// that require the legacy decoder (for example an interlaced PNG).
int projectPngOpen(struct ProjectPngReader **reader, struct ProjectPngImage *image,
                   void *context, int (*read)(void *, unsigned char *, size_t));
int projectPngRows(struct ProjectPngReader *reader, unsigned char *pixels,
                   size_t stride, unsigned count);
int projectPngEnd(struct ProjectPngReader *reader);
void projectPngClose(struct ProjectPngReader *reader);

#ifdef __cplusplus
}
#endif

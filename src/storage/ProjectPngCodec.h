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

#ifdef __cplusplus
}
#endif

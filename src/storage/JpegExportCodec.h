#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int exportJpegRows(unsigned width, unsigned height, const unsigned char *profile,
                   unsigned profileSize, void *context,
                   const unsigned char *(*row)(void *, unsigned),
                   int (*write)(void *, const unsigned char *, size_t));

#ifdef __cplusplus
}
#endif

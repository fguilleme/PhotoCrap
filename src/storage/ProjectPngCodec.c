#include "ProjectPngCodec.h"

#include <png.h>
#include <stdlib.h>

struct ProjectPngOutput {
    void *context;
    int (*write)(void *, const unsigned char *, size_t);
};

static void pngFailure(png_structp png, png_const_charp message)
{
    (void)message;
    png_longjmp(png, 1);
}

static void pngWarning(png_structp png, png_const_charp message)
{
    (void)png;
    (void)message;
}

static void writeBytes(png_structp png, png_bytep bytes, png_size_t count)
{
    struct ProjectPngOutput *output = png_get_io_ptr(png);
    if (!output->write(output->context, bytes, count))
        png_error(png, "Write failed");
}

int projectPngWrite(const struct ProjectPngImage *image, void *context,
                    const unsigned char *(*row)(void *, unsigned),
                    int (*write)(void *, const unsigned char *, size_t))
{
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, pngFailure, pngWarning);
    if (!png)
        return 0;
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        return 0;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return 0;
    }
    struct ProjectPngOutput output = {context, write};
    png_set_write_fn(png, &output, writeBytes, NULL);
    png_set_IHDR(png, info, image->width, image->height, image->bits,
                 image->grayscale ? PNG_COLOR_TYPE_GRAY : PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    if (image->srgb)
        png_set_sRGB(png, info, PNG_sRGB_INTENT_PERCEPTUAL);
    if (image->dotsX && image->dotsY)
        png_set_pHYs(png, info, image->dotsX, image->dotsY, PNG_RESOLUTION_METER);
    png_write_info(png, info);
    if (image->bits == 16 && image->littleEndian)
        png_set_swap(png);
    for (unsigned y = 0; y < image->height; ++y) {
        const unsigned char *pixels = row(context, y);
        if (!pixels) {
            png_destroy_write_struct(&png, &info);
            return 0;
        }
        png_write_row(png, pixels);
    }
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    return 1;
}

struct ProjectPngReader {
    png_structp png;
    png_infop info;
    void *context;
    int (*read)(void *, unsigned char *, size_t);
};

static void readBytes(png_structp png, png_bytep bytes, png_size_t count)
{
    struct ProjectPngReader *reader = png_get_io_ptr(png);
    if (!reader->read(reader->context, bytes, count))
        png_error(png, "Truncated input");
}

int projectPngOpen(struct ProjectPngReader **result, struct ProjectPngImage *image,
                   void *context, int (*read)(void *, unsigned char *, size_t))
{
    struct ProjectPngReader *reader = calloc(1, sizeof(*reader));
    *result = reader;
    if (!reader)
        return 0;
    reader->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, pngFailure, pngWarning);
    if (!reader->png)
        return 0;
    reader->info = png_create_info_struct(reader->png);
    if (!reader->info || setjmp(png_jmpbuf(reader->png)))
        return 0;
    reader->context = context;
    reader->read = read;
    png_set_read_fn(reader->png, reader, readBytes);
    png_set_user_limits(reader->png, image->width, image->height);
    png_set_chunk_malloc_max(reader->png, 8 * 1024 * 1024);
    png_read_info(reader->png, reader->info);
    png_uint_32 width, height;
    int bits, color, interlace, compression, filter;
    if (!png_get_IHDR(reader->png, reader->info, &width, &height, &bits, &color, &interlace,
                      &compression, &filter) || width != image->width || height != image->height)
        return 0;
    const int transparent = png_get_valid(reader->png, reader->info, PNG_INFO_tRNS);
    if (interlace != PNG_INTERLACE_NONE || bits != image->bits ||
        (image->grayscale ? color != PNG_COLOR_TYPE_GRAY || transparent
                          : color != PNG_COLOR_TYPE_RGB && color != PNG_COLOR_TYPE_RGBA))
        return -1;
    if (!image->grayscale && color == PNG_COLOR_TYPE_RGB) {
        if (transparent)
            png_set_tRNS_to_alpha(reader->png);
        else
            png_set_filler(reader->png, bits == 16 ? 65535 : 255, PNG_FILLER_AFTER);
    }
    if (bits == 16 && image->littleEndian)
        png_set_swap(reader->png);
    png_uint_32 dotsX, dotsY;
    int unit;
    if (png_get_pHYs(reader->png, reader->info, &dotsX, &dotsY, &unit) &&
        unit == PNG_RESOLUTION_METER) {
        image->dotsX = dotsX;
        image->dotsY = dotsY;
    }
    png_read_update_info(reader->png, reader->info);
    return png_get_rowbytes(reader->png, reader->info) ==
                   width * (image->grayscale ? 1U : bits == 16 ? 8U : 4U)
               ? 1 : 0;
}

int projectPngRows(struct ProjectPngReader *reader, unsigned char *pixels,
                   size_t stride, unsigned count)
{
    if (setjmp(png_jmpbuf(reader->png)))
        return 0;
    for (unsigned y = 0; y < count; ++y)
        png_read_row(reader->png, pixels + y * stride, NULL);
    return 1;
}

int projectPngEnd(struct ProjectPngReader *reader)
{
    if (setjmp(png_jmpbuf(reader->png)))
        return 0;
    png_read_end(reader->png, NULL);
    return 1;
}

void projectPngClose(struct ProjectPngReader *reader)
{
    if (reader) {
        png_destroy_read_struct(&reader->png, &reader->info, NULL);
        free(reader);
    }
}

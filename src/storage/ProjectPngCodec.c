#include "ProjectPngCodec.h"

#include <png.h>

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

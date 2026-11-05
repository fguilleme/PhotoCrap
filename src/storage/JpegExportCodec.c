#include "JpegExportCodec.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include <jerror.h>
#include <jpeglib.h>

struct JpegErrors {
    struct jpeg_error_mgr base;
    jmp_buf               jump;
};

struct JpegOutput {
    struct jpeg_destination_mgr base;
    void                       *context;
    int (*write)(void *, const unsigned char *, size_t);
    JOCTET bytes[32768];
};

static void failJpeg(j_common_ptr compressor)
{
    struct JpegErrors *error = (struct JpegErrors *)compressor->err;
    longjmp(error->jump, 1);
}

static void startOutput(j_compress_ptr compressor)
{
    struct JpegOutput *output     = (struct JpegOutput *)compressor->dest;
    output->base.next_output_byte = output->bytes;
    output->base.free_in_buffer   = sizeof(output->bytes);
}

static boolean flushOutput(j_compress_ptr compressor)
{
    struct JpegOutput *output = (struct JpegOutput *)compressor->dest;
    const size_t       count  = sizeof(output->bytes) - output->base.free_in_buffer;
    if (count && !output->write(output->context, output->bytes, count))
        ERREXIT(compressor, JERR_FILE_WRITE);
    startOutput(compressor);
    return TRUE;
}

static void endOutput(j_compress_ptr compressor)
{
    flushOutput(compressor);
}

int exportJpegRows(unsigned width, unsigned height, const unsigned char *profile,
                   unsigned profileSize, void *context,
                   const unsigned char *(*row)(void *, unsigned),
                   int (*write)(void *, const unsigned char *, size_t))
{
    j_compress_ptr compressor = calloc(1, sizeof(*compressor));
    if (!compressor)
        return 0;
    struct JpegErrors error;
    compressor->err       = jpeg_std_error(&error.base);
    error.base.error_exit = failJpeg;
    if (setjmp(error.jump)) {
        jpeg_destroy_compress(compressor);
        free(compressor);
        return 0;
    }
    jpeg_create_compress(compressor);
    struct JpegOutput output = {
        {NULL, 0, startOutput, flushOutput, endOutput}, context, write, {0}};
    compressor->dest             = &output.base;
    compressor->image_width      = width;
    compressor->image_height     = height;
    compressor->input_components = 3;
    compressor->in_color_space   = JCS_RGB;
    jpeg_set_defaults(compressor);
    jpeg_set_quality(compressor, 95, TRUE);
    jpeg_start_compress(compressor, TRUE);
    if (profileSize)
        jpeg_write_icc_profile(compressor, profile, profileSize);
    for (unsigned y = 0; y < height; ++y) {
        const unsigned char *pixels = row(context, y);
        if (!pixels) {
            jpeg_destroy_compress(compressor);
            free(compressor);
            return 0;
        }
        JSAMPROW scanline = (JSAMPROW)pixels;
        jpeg_write_scanlines(compressor, &scanline, 1);
    }
    jpeg_finish_compress(compressor);
    jpeg_destroy_compress(compressor);
    free(compressor);
    return 1;
}

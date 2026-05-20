#include "ana/ana_gfx.h"

#include "ana_internal.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANA_FRAMEBUFFER_PIXELS (ANA_DEFAULT_WIDTH * ANA_DEFAULT_HEIGHT)
#define ANA_FRAMEBUFFER_COUNT 2
#define ANA_IMAGE_HEADER_SIZE 20
#define ANA_IMAGE_FLAG_MASKED 0x01u
#define ANA_IMAGE_MAX_WIDTH ANA_DEFAULT_WIDTH
#define ANA_IMAGE_MAX_HEIGHT ANA_DEFAULT_HEIGHT
#define ANA_IMAGE_MAX_FRAMES 256

static unsigned char ana_framebuffers[ANA_FRAMEBUFFER_COUNT][ANA_FRAMEBUFFER_PIXELS];
static ANA_Color ana_palette[ANA_DEFAULT_COLORS];
static int ana_gfx_opened = 0;
static int ana_draw_buffer = 0;
static int ana_front_buffer = 1;
static int ana_presented_frames = 0;

struct ANA_ImageData {
    int width;
    int height;
    int frame_count;
    int bitplanes;
    unsigned int flags;
    int row_bytes;
    long mask_size;
    long plane_size;
    long planes_size;
    long frame_size;
    long data_size;
    unsigned char* data;
};

static int ana_image_magic_is_valid(const unsigned char* header)
{
    return header[0] == 'A' &&
        header[1] == 'N' &&
        header[2] == 'A' &&
        header[3] == 'I' &&
        header[4] == 'M' &&
        header[5] == 'G' &&
        header[6] == '0' &&
        header[7] == '1';
}

static int ana_read_u16_le(const unsigned char* bytes)
{
    return (int)bytes[0] | ((int)bytes[1] << 8);
}

static int ana_image_has_mask(ANA_Image image)
{
    return (image->flags & ANA_IMAGE_FLAG_MASKED) != 0u;
}

static const unsigned char* ana_image_frame_base(ANA_Image image, int frame)
{
    return image->data + ((long)frame * image->frame_size);
}

static const unsigned char* ana_image_mask_base(ANA_Image image, int frame)
{
    if (!ana_image_has_mask(image)) {
        return NULL;
    }

    return ana_image_frame_base(image, frame);
}

static const unsigned char* ana_image_planes_base(ANA_Image image, int frame)
{
    const unsigned char* base;

    base = ana_image_frame_base(image, frame);
    if (ana_image_has_mask(image)) {
        base += image->mask_size;
    }

    return base;
}

static int ana_image_bit_at(const unsigned char* bytes, int row_bytes, int x, int y)
{
    int offset;
    int shift;

    offset = (y * row_bytes) + (x >> 3);
    shift = 7 - (x & 7);

    return ((bytes[offset] >> shift) & 1u) != 0u;
}

static unsigned char ana_image_pixel_at(ANA_Image image, int frame, int x, int y)
{
    const unsigned char* planes;
    unsigned char color;
    int plane;

    planes = ana_image_planes_base(image, frame);
    color = 0u;

    for (plane = 0; plane < image->bitplanes; plane++) {
        if (ana_image_bit_at(
                planes + ((long)plane * image->plane_size),
                image->row_bytes,
                x,
                y)) {
            color = (unsigned char)(color | (1u << plane));
        }
    }

    return (unsigned char)(color & 0x0f);
}

static int ana_image_validate_header(
    const unsigned char* header,
    int* width,
    int* height,
    int* frame_count,
    int* bitplanes,
    unsigned int* flags)
{
    if (!ana_image_magic_is_valid(header)) {
        return 0;
    }

    *width = ana_read_u16_le(header + 8);
    *height = ana_read_u16_le(header + 10);
    *frame_count = ana_read_u16_le(header + 12);
    *bitplanes = (int)header[14];
    *flags = (unsigned int)header[15];

    if (*width <= 0 || *width > ANA_IMAGE_MAX_WIDTH) {
        return 0;
    }

    if (*height <= 0 || *height > ANA_IMAGE_MAX_HEIGHT) {
        return 0;
    }

    if (*frame_count <= 0 || *frame_count > ANA_IMAGE_MAX_FRAMES) {
        return 0;
    }

    if (*bitplanes <= 0 || *bitplanes > ANA_DEFAULT_BITPLANES) {
        return 0;
    }

    if ((*flags & ~ANA_IMAGE_FLAG_MASKED) != 0u) {
        return 0;
    }

    if (header[16] != 0u || header[17] != 0u ||
            header[18] != 0u || header[19] != 0u) {
        return 0;
    }

    return 1;
}

static int ana_image_compute_sizes(ANA_Image image)
{
    image->row_bytes = (image->width + 7) / 8;
    image->plane_size = (long)image->row_bytes * image->height;
    image->mask_size = ana_image_has_mask(image) ? image->plane_size : 0L;
    image->planes_size = image->plane_size * image->bitplanes;
    image->frame_size = image->mask_size + image->planes_size;
    image->data_size = image->frame_size * image->frame_count;

    return image->row_bytes > 0 &&
        image->plane_size > 0L &&
        image->planes_size > 0L &&
        image->frame_size > 0L &&
        image->data_size > 0L;
}

ANA_Result ana_gfx_open(const ANA_Profile* profile)
{
    ANA_Result result;

    result = ana_validate_profile(profile);
    if (result != ANA_OK) {
        return result;
    }

    memset(ana_framebuffers, 0, sizeof(ana_framebuffers));
    memset(ana_palette, 0, sizeof(ana_palette));

    ana_gfx_opened = 1;
    ana_draw_buffer = 0;
    ana_front_buffer = 1;
    ana_presented_frames = 0;

    return ANA_OK;
}

ANA_Image ana_load_image(const char* path)
{
    FILE* file;
    unsigned char header[ANA_IMAGE_HEADER_SIZE];
    struct ANA_ImageData* image;
    size_t bytes_read;
    int width;
    int height;
    int frame_count;
    int bitplanes;
    unsigned int flags;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    bytes_read = fread(header, 1u, ANA_IMAGE_HEADER_SIZE, file);
    if (bytes_read != ANA_IMAGE_HEADER_SIZE) {
        fclose(file);
        return NULL;
    }

    if (!ana_image_validate_header(
            header,
            &width,
            &height,
            &frame_count,
            &bitplanes,
            &flags)) {
        fclose(file);
        return NULL;
    }

    image = (struct ANA_ImageData*)malloc(sizeof(struct ANA_ImageData));
    if (image == NULL) {
        fclose(file);
        return NULL;
    }

    image->width = width;
    image->height = height;
    image->frame_count = frame_count;
    image->bitplanes = bitplanes;
    image->flags = flags;
    image->data = NULL;

    if (!ana_image_compute_sizes(image)) {
        free(image);
        fclose(file);
        return NULL;
    }

    image->data = (unsigned char*)malloc((size_t)image->data_size);
    if (image->data == NULL) {
        free(image);
        fclose(file);
        return NULL;
    }

    bytes_read = fread(image->data, 1u, (size_t)image->data_size, file);
    fclose(file);

    if (bytes_read != (size_t)image->data_size) {
        ana_free_image(image);
        return NULL;
    }

    return image;
}

void ana_free_image(ANA_Image image)
{
    if (image == NULL) {
        return;
    }

    free(image->data);
    image->data = NULL;
    free(image);
}

void ana_draw_image(ANA_Image image, int x, int y)
{
    ana_draw_image_frame(image, 0, x, y);
}

void ana_draw_image_frame(ANA_Image image, int frame, int x, int y)
{
    int start_x;
    int start_y;
    int end_x;
    int end_y;
    int dest_x;
    int dest_y;
    int src_x;
    int src_y;
    const unsigned char* mask;

    if (!ana_gfx_opened || image == NULL || image->data == NULL) {
        return;
    }

    if (frame < 0 || frame >= image->frame_count) {
        return;
    }

    if (x >= ANA_DEFAULT_WIDTH || y >= ANA_DEFAULT_HEIGHT ||
            x <= -image->width || y <= -image->height) {
        return;
    }

    start_x = x < 0 ? 0 : x;
    start_y = y < 0 ? 0 : y;
    end_x = x + image->width;
    end_y = y + image->height;

    if (end_x > ANA_DEFAULT_WIDTH) {
        end_x = ANA_DEFAULT_WIDTH;
    }

    if (end_y > ANA_DEFAULT_HEIGHT) {
        end_y = ANA_DEFAULT_HEIGHT;
    }

    if (start_x >= end_x || start_y >= end_y) {
        return;
    }

    mask = ana_image_mask_base(image, frame);

    for (dest_y = start_y; dest_y < end_y; dest_y++) {
        src_y = dest_y - y;

        for (dest_x = start_x; dest_x < end_x; dest_x++) {
            src_x = dest_x - x;

            if (mask == NULL ||
                    ana_image_bit_at(mask, image->row_bytes, src_x, src_y)) {
                ana_framebuffers[ana_draw_buffer]
                    [(dest_y * ANA_DEFAULT_WIDTH) + dest_x] =
                    ana_image_pixel_at(image, frame, src_x, src_y);
            }
        }
    }
}

int ana_image_width(ANA_Image image)
{
    if (image == NULL) {
        return 0;
    }

    return image->width;
}

int ana_image_height(ANA_Image image)
{
    if (image == NULL) {
        return 0;
    }

    return image->height;
}

int ana_image_frame_count(ANA_Image image)
{
    if (image == NULL) {
        return 0;
    }

    return image->frame_count;
}

void ana_gfx_close(void)
{
    ana_gfx_opened = 0;
    ana_presented_frames = 0;
}

int ana_gfx_is_open(void)
{
    return ana_gfx_opened;
}

void ana_set_palette(const ANA_Color* colors, int count)
{
    int i;

    if (!ana_gfx_opened || colors == NULL || count <= 0) {
        return;
    }

    if (count > ANA_DEFAULT_COLORS) {
        count = ANA_DEFAULT_COLORS;
    }

    for (i = 0; i < count; i++) {
        ana_palette[i] = colors[i];
    }
}

void ana_clear(unsigned char color_index)
{
    if (!ana_gfx_opened) {
        return;
    }

    color_index = (unsigned char)(color_index & 0x0f);
    memset(ana_framebuffers[ana_draw_buffer], color_index, ANA_FRAMEBUFFER_PIXELS);
}

void ana_present(void)
{
    int previous_front;

    if (!ana_gfx_opened) {
        return;
    }

    previous_front = ana_front_buffer;
    ana_front_buffer = ana_draw_buffer;
    ana_draw_buffer = previous_front;
    ana_presented_frames++;
}

int ana_gfx_present_count(void)
{
    return ana_presented_frames;
}

unsigned char ana_gfx_front_pixel(int x, int y)
{
    if (!ana_gfx_opened) {
        return 0;
    }

    if (x < 0 || x >= ANA_DEFAULT_WIDTH || y < 0 || y >= ANA_DEFAULT_HEIGHT) {
        return 0;
    }

    return ana_framebuffers[ana_front_buffer][(y * ANA_DEFAULT_WIDTH) + x];
}

unsigned char ana_gfx_draw_pixel(int x, int y)
{
    if (!ana_gfx_opened) {
        return 0;
    }

    if (x < 0 || x >= ANA_DEFAULT_WIDTH || y < 0 || y >= ANA_DEFAULT_HEIGHT) {
        return 0;
    }

    return ana_framebuffers[ana_draw_buffer][(y * ANA_DEFAULT_WIDTH) + x];
}

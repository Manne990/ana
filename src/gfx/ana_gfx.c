#include "ana/ana_gfx.h"

#include "ana_internal.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ANA_TARGET_AMIGA
#include <exec/ports.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#endif

#define ANA_FRAMEBUFFER_PIXELS (ANA_DEFAULT_WIDTH * ANA_DEFAULT_HEIGHT)
#define ANA_FRAMEBUFFER_COUNT 2
#define ANA_IMAGE_HEADER_SIZE 20
#define ANA_IMAGE_FLAG_MASKED 0x01u
#define ANA_IMAGE_MAX_WIDTH ANA_DEFAULT_WIDTH
#define ANA_IMAGE_MAX_HEIGHT ANA_DEFAULT_HEIGHT
#define ANA_IMAGE_MAX_FRAMES 256
#define ANA_FONT_HEADER_SIZE 16
#define ANA_AMIGA_MAX_DIRTY_RECTS 64

#ifdef ANA_TARGET_AMIGA
#ifdef BORDERLESS
#define ANA_AMIGA_WINDOW_BORDERLESS BORDERLESS
#else
#define ANA_AMIGA_WINDOW_BORDERLESS WFLG_BORDERLESS
#endif

#ifdef ACTIVATE
#define ANA_AMIGA_WINDOW_ACTIVATE ACTIVATE
#else
#define ANA_AMIGA_WINDOW_ACTIVATE WFLG_ACTIVATE
#endif

#ifdef RMBTRAP
#define ANA_AMIGA_WINDOW_RMBTRAP RMBTRAP
#else
#define ANA_AMIGA_WINDOW_RMBTRAP WFLG_RMBTRAP
#endif

#ifdef SIMPLE_REFRESH
#define ANA_AMIGA_WINDOW_REFRESH SIMPLE_REFRESH
#else
#define ANA_AMIGA_WINDOW_REFRESH WFLG_SIMPLE_REFRESH
#endif

#ifdef ANA_AMIGA_DIRECT_PRESENT
#define ANA_AMIGA_BLACK_FILL_USES_PLANAR_CLEAR 0
#else
#define ANA_AMIGA_BLACK_FILL_USES_PLANAR_CLEAR 1
#endif
#endif

static unsigned char ana_framebuffers[ANA_FRAMEBUFFER_COUNT][ANA_FRAMEBUFFER_PIXELS];
static ANA_Color ana_palette[ANA_DEFAULT_COLORS];
static int ana_gfx_opened = 0;
static int ana_draw_buffer = 0;
static int ana_front_buffer = 1;
static int ana_presented_frames = 0;
static ANA_RenderStats ana_gfx_stats;

static const ANA_Color ana_default_palette[ANA_DEFAULT_COLORS] = {
    { 0, 0, 0 },
    { 255, 255, 255 },
    { 255, 0, 0 },
    { 0, 255, 0 },
    { 0, 0, 255 },
    { 255, 255, 0 },
    { 0, 255, 255 },
    { 255, 0, 255 },
    { 136, 136, 136 },
    { 68, 68, 68 },
    { 255, 136, 0 },
    { 136, 255, 0 },
    { 0, 136, 255 },
    { 136, 0, 255 },
    { 255, 136, 136 },
    { 136, 255, 255 }
};

#ifdef ANA_TARGET_AMIGA
struct GfxBase* GfxBase = NULL;
struct IntuitionBase* IntuitionBase = NULL;

static struct Screen* ana_amiga_screen = NULL;
static struct Window* ana_amiga_window = NULL;
static struct BitMap ana_amiga_hidden_bitmap;
static struct BitMap* ana_amiga_original_bitmap = NULL;
static struct BitMap* ana_amiga_visible_bitmap = NULL;
static struct BitMap* ana_amiga_draw_bitmap = NULL;
static struct ScreenBuffer* ana_amiga_screen_buffers[2];
static struct MsgPort* ana_amiga_safe_port = NULL;
static unsigned short ana_amiga_rgb4[ANA_DEFAULT_COLORS];
static int ana_amiga_hidden_bitmap_ready = 0;
static int ana_amiga_screen_buffers_ready = 0;
static int ana_amiga_clear_requested = 0;
static unsigned char ana_amiga_clear_color = 0;
#endif

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
    long pixels_size;
    unsigned char* data;
    unsigned char* pixels;
};

struct ANA_FontData {
    ANA_Image image;
    int char_width;
    int char_height;
    int first_char;
    int char_count;
    unsigned char color_index;
};

#ifdef ANA_TARGET_AMIGA
struct ANA_AmigaDirtyRect {
    int min_x;
    int min_y;
    int max_x;
    int max_y;
};

static struct ANA_AmigaDirtyRect
    ana_amiga_dirty_rects[ANA_AMIGA_MAX_DIRTY_RECTS];
static int ana_amiga_dirty_count = 0;
static struct ANA_AmigaDirtyRect
    ana_amiga_planar_clear_rects[ANA_AMIGA_MAX_DIRTY_RECTS];
static int ana_amiga_planar_clear_count = 0;

struct ANA_AmigaBitmapState {
    struct BitMap* bitmap;
    struct ANA_AmigaDirtyRect dirty_rects[ANA_AMIGA_MAX_DIRTY_RECTS];
    int dirty_count;
    int clear_color_valid;
    unsigned char clear_color;
};

struct ANA_AmigaFramebufferState {
    struct ANA_AmigaDirtyRect dirty_rects[ANA_AMIGA_MAX_DIRTY_RECTS];
    int dirty_count;
    int clear_color_valid;
    unsigned char clear_color;
};

static struct ANA_AmigaBitmapState ana_amiga_bitmap_states[2];
static struct ANA_AmigaFramebufferState
    ana_amiga_framebuffer_states[ANA_FRAMEBUFFER_COUNT];
static unsigned long ana_amiga_planar_pair_lut[4][256];
static int ana_amiga_planar_pair_lut_ready = 0;
#endif

static void ana_draw_image_frame_internal(
    ANA_Image image,
    int frame,
    int x,
    int y,
    int mark_dirty);

static int ana_draw_image_frame_fast(
    ANA_Image image,
    int frame,
    int x,
    int y);

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

static int ana_font_magic_is_valid(const unsigned char* header)
{
    return header[0] == 'A' &&
        header[1] == 'N' &&
        header[2] == 'A' &&
        header[3] == 'F' &&
        header[4] == 'N' &&
        header[5] == 'T' &&
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

static const unsigned char* ana_image_pixels_base(ANA_Image image, int frame)
{
    if (image->pixels == NULL) {
        return NULL;
    }

    return image->pixels +
        ((long)frame * image->width * image->height);
}

static int ana_image_bit_at(const unsigned char* bytes, int row_bytes, int x, int y)
{
    int offset;
    int shift;

    offset = (y * row_bytes) + (x >> 3);
    shift = 7 - (x & 7);

    return ((bytes[offset] >> shift) & 1u) != 0u;
}

static unsigned char ana_image_plane_pixel_at(
    ANA_Image image,
    int frame,
    int x,
    int y)
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

static unsigned char ana_image_pixel_at(ANA_Image image, int frame, int x, int y)
{
    const unsigned char* pixels;

    pixels = ana_image_pixels_base(image, frame);
    if (pixels != NULL) {
        return pixels[(y * image->width) + x];
    }

    return ana_image_plane_pixel_at(image, frame, x, y);
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

static int ana_font_validate_header(
    const unsigned char* header,
    int* char_width,
    int* char_height,
    int* first_char,
    int* char_count)
{
    if (!ana_font_magic_is_valid(header)) {
        return 0;
    }

    *char_width = ana_read_u16_le(header + 8);
    *char_height = ana_read_u16_le(header + 10);
    *first_char = (int)header[12];
    *char_count = (int)header[13];

    if (*char_width <= 0 || *char_width > ANA_IMAGE_MAX_WIDTH) {
        return 0;
    }

    if (*char_height <= 0 || *char_height > ANA_IMAGE_MAX_HEIGHT) {
        return 0;
    }

    if (*char_count <= 0 || *char_count > ANA_IMAGE_MAX_FRAMES) {
        return 0;
    }

    if (*first_char < 0 || *first_char + *char_count > 128) {
        return 0;
    }

    if (header[14] != 0u || header[15] != 0u) {
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
    image->pixels_size =
        (long)image->width * image->height * image->frame_count;

    return image->row_bytes > 0 &&
        image->plane_size > 0L &&
        image->planes_size > 0L &&
        image->frame_size > 0L &&
        image->data_size > 0L &&
        image->pixels_size > 0L;
}

static int ana_image_decode_pixels(ANA_Image image)
{
    int frame;
    int x;
    int y;
    unsigned char* out;

    if (image == NULL || image->data == NULL) {
        return 0;
    }

    if (image->pixels != NULL) {
        free(image->pixels);
        image->pixels = NULL;
    }

    image->pixels = (unsigned char*)malloc((size_t)image->pixels_size);
    if (image->pixels == NULL) {
        return 0;
    }

    out = image->pixels;
    for (frame = 0; frame < image->frame_count; frame++) {
        for (y = 0; y < image->height; y++) {
            for (x = 0; x < image->width; x++) {
                *out++ = ana_image_plane_pixel_at(image, frame, x, y);
            }
        }
    }

    return 1;
}

static ANA_Image ana_image_create_from_payload(
    int width,
    int height,
    int frame_count,
    int bitplanes,
    unsigned int flags,
    const unsigned char* payload,
    long payload_size)
{
    struct ANA_ImageData* image;

    image = (struct ANA_ImageData*)malloc(sizeof(struct ANA_ImageData));
    if (image == NULL) {
        return NULL;
    }

    image->width = width;
    image->height = height;
    image->frame_count = frame_count;
    image->bitplanes = bitplanes;
    image->flags = flags;
    image->data = NULL;
    image->pixels = NULL;

    if (!ana_image_compute_sizes(image) ||
            (payload != NULL && payload_size < image->data_size)) {
        free(image);
        return NULL;
    }

    image->data = (unsigned char*)malloc((size_t)image->data_size);
    if (image->data == NULL) {
        free(image);
        return NULL;
    }

    if (payload != NULL) {
        memcpy(image->data, payload, (size_t)image->data_size);
    }

    return image;
}

static void ana_gfx_load_default_palette(void)
{
    int i;

    for (i = 0; i < ANA_DEFAULT_COLORS; i++) {
        ana_palette[i] = ana_default_palette[i];
    }
}

static void ana_gfx_reset_stats(void)
{
    memset(&ana_gfx_stats, 0, sizeof(ana_gfx_stats));
    ana_gfx_stats.perf_ticks_per_second =
        (long)ana_platform_perf_ticks_per_second();
}

#ifdef ANA_TARGET_AMIGA
static void ana_gfx_record_perf_ticks(
    long* total,
    unsigned long start_ticks,
    unsigned long end_ticks)
{
    unsigned long elapsed;

    if (total == NULL) {
        return;
    }

    elapsed = end_ticks - start_ticks;
    *total += (long)elapsed;
}

static long ana_gfx_dirty_rect_area(const struct ANA_AmigaDirtyRect* rect)
{
    if (rect == NULL || rect->min_x >= rect->max_x ||
            rect->min_y >= rect->max_y) {
        return 0L;
    }

    return (long)(rect->max_x - rect->min_x) *
        (long)(rect->max_y - rect->min_y);
}

static long ana_gfx_dirty_rects_area(
    const struct ANA_AmigaDirtyRect* rects,
    int rect_count)
{
    long area;
    int i;

    area = 0L;
    for (i = 0; i < rect_count; i++) {
        area += ana_gfx_dirty_rect_area(&rects[i]);
    }

    return area;
}

static void ana_gfx_record_chunky_clear_rects(
    const struct ANA_AmigaDirtyRect* rects,
    int rect_count)
{
    long pixels;

    if (rect_count <= 0) {
        return;
    }

    pixels = ana_gfx_dirty_rects_area(rects, rect_count);
    ana_gfx_stats.chunky_clear_rects += (long)rect_count;
    ana_gfx_stats.chunky_clear_pixels += pixels;

    if (pixels > ana_gfx_stats.max_chunky_clear_pixels) {
        ana_gfx_stats.max_chunky_clear_pixels = pixels;
    }
}

static void ana_gfx_record_chunky_clear_fullscreen(void)
{
    ana_gfx_stats.chunky_clear_rects++;
    ana_gfx_stats.chunky_clear_pixels += ANA_FRAMEBUFFER_PIXELS;

    if (ANA_FRAMEBUFFER_PIXELS > ana_gfx_stats.max_chunky_clear_pixels) {
        ana_gfx_stats.max_chunky_clear_pixels = ANA_FRAMEBUFFER_PIXELS;
    }
}

static void ana_gfx_record_planar_clear_rects(
    const struct ANA_AmigaDirtyRect* rects,
    int rect_count)
{
    long pixels;

    if (rect_count <= 0) {
        return;
    }

    pixels = ana_gfx_dirty_rects_area(rects, rect_count);
    ana_gfx_stats.planar_clear_rects += (long)rect_count;
    ana_gfx_stats.planar_clear_pixels += pixels;

    if (pixels > ana_gfx_stats.max_planar_clear_pixels) {
        ana_gfx_stats.max_planar_clear_pixels = pixels;
    }
}

static void ana_gfx_record_planar_clear_fullscreen(void)
{
    ana_gfx_stats.planar_clear_rects++;
    ana_gfx_stats.planar_clear_pixels += ANA_FRAMEBUFFER_PIXELS;

    if (ANA_FRAMEBUFFER_PIXELS > ana_gfx_stats.max_planar_clear_pixels) {
        ana_gfx_stats.max_planar_clear_pixels = ANA_FRAMEBUFFER_PIXELS;
    }
}

static void ana_gfx_record_present_dirty_rects(
    const struct ANA_AmigaDirtyRect* rects,
    int rect_count)
{
    long pixels;

    if (rect_count <= 0) {
        return;
    }

    pixels = ana_gfx_dirty_rects_area(rects, rect_count);
    ana_gfx_stats.dirty_rects += (long)rect_count;
    ana_gfx_stats.converted_pixels += pixels;

    if ((long)rect_count > ana_gfx_stats.max_dirty_rects) {
        ana_gfx_stats.max_dirty_rects = (long)rect_count;
    }

    if (pixels > ana_gfx_stats.max_converted_pixels) {
        ana_gfx_stats.max_converted_pixels = pixels;
    }
}
static int ana_amiga_rect_contains(
    const struct ANA_AmigaDirtyRect* outer,
    const struct ANA_AmigaDirtyRect* inner)
{
    return outer->min_x <= inner->min_x &&
        outer->min_y <= inner->min_y &&
        outer->max_x >= inner->max_x &&
        outer->max_y >= inner->max_y;
}

static int ana_amiga_rects_overlap(
    const struct ANA_AmigaDirtyRect* a,
    const struct ANA_AmigaDirtyRect* b)
{
    return a->min_x < b->max_x &&
        b->min_x < a->max_x &&
        a->min_y < b->max_y &&
        b->min_y < a->max_y;
}

static int ana_amiga_rect_contained_by_any(
    const struct ANA_AmigaDirtyRect* rect,
    const struct ANA_AmigaDirtyRect* rects,
    int rect_count)
{
    int i;

    for (i = 0; i < rect_count; i++) {
        if (ana_amiga_rect_contains(&rects[i], rect)) {
            return 1;
        }
    }

    return 0;
}

static int ana_amiga_rects_should_merge(
    const struct ANA_AmigaDirtyRect* a,
    const struct ANA_AmigaDirtyRect* b)
{
    struct ANA_AmigaDirtyRect merged;
    long merged_area;
    long separate_area;

    merged = *a;

    if (b->min_x < merged.min_x) {
        merged.min_x = b->min_x;
    }

    if (b->min_y < merged.min_y) {
        merged.min_y = b->min_y;
    }

    if (b->max_x > merged.max_x) {
        merged.max_x = b->max_x;
    }

    if (b->max_y > merged.max_y) {
        merged.max_y = b->max_y;
    }

    merged_area = ana_gfx_dirty_rect_area(&merged);
    separate_area =
        ana_gfx_dirty_rect_area(a) + ana_gfx_dirty_rect_area(b);

    return merged_area <= separate_area + 64L;
}

static void ana_amiga_expand_dirty_rect(
    struct ANA_AmigaDirtyRect* target,
    const struct ANA_AmigaDirtyRect* source)
{
    if (source->min_x < target->min_x) {
        target->min_x = source->min_x;
    }

    if (source->min_y < target->min_y) {
        target->min_y = source->min_y;
    }

    if (source->max_x > target->max_x) {
        target->max_x = source->max_x;
    }

    if (source->max_y > target->max_y) {
        target->max_y = source->max_y;
    }
}

static void ana_amiga_remove_rect(
    struct ANA_AmigaDirtyRect* rects,
    int* rect_count,
    int index)
{
    int i;

    if (rects == NULL || rect_count == NULL ||
            index < 0 || index >= *rect_count) {
        return;
    }

    for (i = index; i < *rect_count - 1; i++) {
        rects[i] = rects[i + 1];
    }

    (*rect_count)--;
}

static void ana_amiga_coalesce_rect(
    struct ANA_AmigaDirtyRect* rects,
    int* rect_count,
    int index)
{
    int i;

    if (rects == NULL || rect_count == NULL ||
            index < 0 || index >= *rect_count) {
        return;
    }

    i = 0;
    while (i < *rect_count) {
        if (i == index) {
            i++;
            continue;
        }

        if (ana_amiga_rect_contains(
                &rects[index],
                &rects[i]) ||
                ana_amiga_rects_overlap(
                    &rects[index],
                    &rects[i]) ||
                ana_amiga_rects_should_merge(
                    &rects[index],
                    &rects[i])) {
            ana_amiga_expand_dirty_rect(
                &rects[index],
                &rects[i]);
            ana_amiga_remove_rect(rects, rect_count, i);
            if (i < index) {
                index--;
            }
            i = 0;
            continue;
        }

        i++;
    }
}

static void ana_amiga_reset_frame_state(void)
{
    ana_amiga_dirty_count = 0;
    ana_amiga_planar_clear_count = 0;
    ana_amiga_clear_requested = 0;
    ana_amiga_clear_color = 0;
}

static void ana_amiga_reset_bitmap_states(void)
{
    memset(ana_amiga_bitmap_states, 0, sizeof(ana_amiga_bitmap_states));
}

static void ana_amiga_reset_framebuffer_states(void)
{
    int i;

    for (i = 0; i < ANA_FRAMEBUFFER_COUNT; i++) {
        ana_amiga_framebuffer_states[i].dirty_count = 0;
        ana_amiga_framebuffer_states[i].clear_color_valid = 1;
        ana_amiga_framebuffer_states[i].clear_color = 0u;
    }
}

static void ana_amiga_init_planar_pair_lut(void)
{
    int slot;
    int index;
    int plane;
    int color0;
    int color1;
    int shift;
    unsigned char plane_values[ANA_DEFAULT_BITPLANES];

    if (ana_amiga_planar_pair_lut_ready) {
        return;
    }

    for (slot = 0; slot < 4; slot++) {
        shift = 6 - (slot * 2);

        for (index = 0; index < 256; index++) {
            color0 = (index >> 4) & 0x0f;
            color1 = index & 0x0f;

            for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
                plane_values[plane] = 0u;

                if ((color0 & (1 << plane)) != 0) {
                    plane_values[plane] = (unsigned char)(
                        plane_values[plane] | (1u << (shift + 1)));
                }

                if ((color1 & (1 << plane)) != 0) {
                    plane_values[plane] = (unsigned char)(
                        plane_values[plane] | (1u << shift));
                }
            }

            ana_amiga_planar_pair_lut[slot][index] =
                (unsigned long)plane_values[0] |
                ((unsigned long)plane_values[1] << 8) |
                ((unsigned long)plane_values[2] << 16) |
                ((unsigned long)plane_values[3] << 24);
        }
    }

    ana_amiga_planar_pair_lut_ready = 1;
}

static struct ANA_AmigaBitmapState* ana_amiga_bitmap_state_for(
    struct BitMap* bitmap)
{
    int i;

    if (bitmap == NULL) {
        return NULL;
    }

    for (i = 0; i < 2; i++) {
        if (ana_amiga_bitmap_states[i].bitmap == bitmap) {
            return &ana_amiga_bitmap_states[i];
        }
    }

    return NULL;
}

static void ana_amiga_store_bitmap_dirty_state(
    struct ANA_AmigaBitmapState* state)
{
    int i;

    if (state == NULL) {
        return;
    }

    state->dirty_count = ana_amiga_dirty_count;
    for (i = 0; i < ana_amiga_dirty_count; i++) {
        state->dirty_rects[i] = ana_amiga_dirty_rects[i];
    }
}

static void ana_amiga_store_framebuffer_dirty_state(int buffer_index)
{
    struct ANA_AmigaFramebufferState* state;
    int i;

    if (buffer_index < 0 || buffer_index >= ANA_FRAMEBUFFER_COUNT) {
        return;
    }

    state = &ana_amiga_framebuffer_states[buffer_index];
    state->dirty_count = ana_amiga_dirty_count;
    for (i = 0; i < ana_amiga_dirty_count; i++) {
        state->dirty_rects[i] = ana_amiga_dirty_rects[i];
    }
}

static void ana_amiga_fill_chunky_rect(
    int buffer_index,
    unsigned char color,
    const struct ANA_AmigaDirtyRect* rect)
{
    int y;
    int width;
    unsigned char* pixels;

    if (buffer_index < 0 || buffer_index >= ANA_FRAMEBUFFER_COUNT ||
            rect == NULL) {
        return;
    }

    if (rect->min_x >= rect->max_x || rect->min_y >= rect->max_y) {
        return;
    }

    width = rect->max_x - rect->min_x;
    pixels = ana_framebuffers[buffer_index];

    for (y = rect->min_y; y < rect->max_y; y++) {
        memset(
            pixels + (y * ANA_DEFAULT_WIDTH) + rect->min_x,
            color,
            (size_t)width);
    }
}

static void ana_amiga_clear_chunky_rects(
    int buffer_index,
    unsigned char color,
    const struct ANA_AmigaDirtyRect* rects,
    int rect_count)
{
    int i;

    for (i = 0; i < rect_count; i++) {
        ana_amiga_fill_chunky_rect(buffer_index, color, &rects[i]);
    }
}

static void ana_amiga_clear_draw_framebuffer(unsigned char color)
{
    struct ANA_AmigaFramebufferState* state;

    state = &ana_amiga_framebuffer_states[ana_draw_buffer];
    ana_amiga_planar_clear_count = 0;

    if (!state->clear_color_valid || state->clear_color != color) {
        memset(
            ana_framebuffers[ana_draw_buffer],
            color,
            ANA_FRAMEBUFFER_PIXELS);
        ana_gfx_record_chunky_clear_fullscreen();
        state->dirty_count = 0;
        state->clear_color_valid = 1;
        state->clear_color = color;
        ana_amiga_dirty_count = 0;
        return;
    }

    ana_gfx_record_chunky_clear_rects(
        state->dirty_rects,
        state->dirty_count);
    ana_amiga_clear_chunky_rects(
        ana_draw_buffer,
        color,
        state->dirty_rects,
        state->dirty_count);
    ana_gfx_record_chunky_clear_rects(
        ana_amiga_dirty_rects,
        ana_amiga_dirty_count);
    ana_amiga_clear_chunky_rects(
        ana_draw_buffer,
        color,
        ana_amiga_dirty_rects,
        ana_amiga_dirty_count);
    ana_amiga_dirty_count = 0;
}

static void ana_amiga_mark_rect(
    struct ANA_AmigaDirtyRect* rects,
    int* rect_count,
    int min_x,
    int min_y,
    int max_x,
    int max_y)
{
    struct ANA_AmigaDirtyRect rect;
    int i;

    if (min_x < 0) {
        min_x = 0;
    }

    if (min_y < 0) {
        min_y = 0;
    }

    if (max_x < 0) {
        max_x = 0;
    }

    if (max_x > ANA_DEFAULT_WIDTH) {
        max_x = ANA_DEFAULT_WIDTH;
    }

    if (max_y > ANA_DEFAULT_HEIGHT) {
        max_y = ANA_DEFAULT_HEIGHT;
    }

    min_x &= ~7;
    max_x = (max_x + 7) & ~7;

    if (max_x > ANA_DEFAULT_WIDTH) {
        max_x = ANA_DEFAULT_WIDTH;
    }

    if (min_x >= max_x || min_y >= max_y) {
        return;
    }

    rect.min_x = min_x;
    rect.min_y = min_y;
    rect.max_x = max_x;
    rect.max_y = max_y;

    for (i = 0; i < *rect_count; i++) {
        if (ana_amiga_rect_contains(&rects[i], &rect)) {
            return;
        }

        if (ana_amiga_rects_overlap(&rects[i], &rect)) {
            ana_amiga_expand_dirty_rect(&rects[i], &rect);
            ana_amiga_coalesce_rect(rects, rect_count, i);
            return;
        }

        if (ana_amiga_rects_should_merge(&rects[i], &rect)) {
            ana_amiga_expand_dirty_rect(&rects[i], &rect);
            ana_amiga_coalesce_rect(rects, rect_count, i);
            return;
        }
    }

    if (*rect_count < ANA_AMIGA_MAX_DIRTY_RECTS) {
        rects[*rect_count] = rect;
        (*rect_count)++;
        return;
    }

    ana_amiga_expand_dirty_rect(&rects[0], &rect);
    ana_amiga_coalesce_rect(rects, rect_count, 0);
}

static void ana_amiga_mark_dirty_rect(int min_x, int min_y, int max_x, int max_y)
{
    ana_amiga_mark_rect(
        ana_amiga_dirty_rects,
        &ana_amiga_dirty_count,
        min_x,
        min_y,
        max_x,
        max_y);
}

static void ana_amiga_mark_planar_clear_rect(
    int min_x,
    int min_y,
    int max_x,
    int max_y)
{
    ana_amiga_mark_rect(
        ana_amiga_planar_clear_rects,
        &ana_amiga_planar_clear_count,
        min_x,
        min_y,
        max_x,
        max_y);
}

static unsigned short ana_amiga_color_to_rgb4(ANA_Color color)
{
    unsigned short r;
    unsigned short g;
    unsigned short b;

    r = (unsigned short)((color.r >> 4) & 0x0f);
    g = (unsigned short)((color.g >> 4) & 0x0f);
    b = (unsigned short)((color.b >> 4) & 0x0f);

    return (unsigned short)((r << 8) | (g << 4) | b);
}

static void ana_amiga_apply_palette(void)
{
    int i;

    if (ana_amiga_screen == NULL) {
        return;
    }

    for (i = 0; i < ANA_DEFAULT_COLORS; i++) {
        ana_amiga_rgb4[i] = ana_amiga_color_to_rgb4(ana_palette[i]);
    }

    LoadRGB4(&ana_amiga_screen->ViewPort, ana_amiga_rgb4, ANA_DEFAULT_COLORS);
}

static int ana_amiga_open_libraries(void)
{
    if (GfxBase == NULL) {
        GfxBase = (struct GfxBase*)OpenLibrary(
            (const unsigned char*)"graphics.library",
            0L);
    }

    if (IntuitionBase == NULL) {
        IntuitionBase =
            (struct IntuitionBase*)OpenLibrary(
                (const unsigned char*)"intuition.library",
                0L);
    }

    return GfxBase != NULL && IntuitionBase != NULL;
}

static void ana_amiga_close_libraries(void)
{
    if (IntuitionBase != NULL) {
        CloseLibrary((struct Library*)IntuitionBase);
        IntuitionBase = NULL;
    }

    if (GfxBase != NULL) {
        CloseLibrary((struct Library*)GfxBase);
        GfxBase = NULL;
    }
}

static void ana_amiga_clear_bitmap(struct BitMap* bitmap)
{
    int plane;
    unsigned long plane_size;

    if (bitmap == NULL) {
        return;
    }

    plane_size = (unsigned long)bitmap->BytesPerRow * ANA_DEFAULT_HEIGHT;

    for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
        if (bitmap->Planes[plane] != NULL) {
            memset(
                (unsigned char*)bitmap->Planes[plane],
                0,
                (size_t)plane_size);
        }
    }
}

static void ana_amiga_fill_bitmap(struct BitMap* bitmap, unsigned char color)
{
    int plane;
    unsigned long plane_size;
    unsigned char fill;

    if (bitmap == NULL) {
        return;
    }

    plane_size = (unsigned long)bitmap->BytesPerRow * ANA_DEFAULT_HEIGHT;

    for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
        if (bitmap->Planes[plane] != NULL) {
            fill = (color & (1u << plane)) != 0u ? 0xffu : 0x00u;
            memset(
                (unsigned char*)bitmap->Planes[plane],
                fill,
                (size_t)plane_size);
        }
    }
}

static void ana_amiga_fill_bitmap_rect(
    struct BitMap* bitmap,
    unsigned char color,
    const struct ANA_AmigaDirtyRect* rect)
{
    int y;
    int plane;
    int start_byte_x;
    int end_byte_x;
    int byte_width;
    unsigned char fill;
    unsigned long row_offset;

    if (bitmap == NULL || rect == NULL) {
        return;
    }

    if (rect->min_x >= rect->max_x || rect->min_y >= rect->max_y) {
        return;
    }

    start_byte_x = rect->min_x / 8;
    end_byte_x = (rect->max_x + 7) / 8;
    byte_width = end_byte_x - start_byte_x;

    if (byte_width <= 0) {
        return;
    }

    for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
        if (bitmap->Planes[plane] == NULL) {
            continue;
        }

        fill = (color & (1u << plane)) != 0u ? 0xffu : 0x00u;

        for (y = rect->min_y; y < rect->max_y; y++) {
            row_offset = (unsigned long)y * bitmap->BytesPerRow;
            memset(
                ((unsigned char*)bitmap->Planes[plane]) +
                    row_offset + (unsigned long)start_byte_x,
                fill,
                (size_t)byte_width);
        }
    }
}

static void ana_amiga_free_hidden_bitmap(void)
{
    int plane;

    if (!ana_amiga_hidden_bitmap_ready) {
        return;
    }

    for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
        if (ana_amiga_hidden_bitmap.Planes[plane] != NULL) {
            FreeRaster(
                ana_amiga_hidden_bitmap.Planes[plane],
                ANA_DEFAULT_WIDTH,
                ANA_DEFAULT_HEIGHT);
            ana_amiga_hidden_bitmap.Planes[plane] = NULL;
        }
    }

    ana_amiga_hidden_bitmap_ready = 0;
}

static void ana_amiga_reset_screen_buffers(void)
{
    ana_amiga_screen_buffers[0] = NULL;
    ana_amiga_screen_buffers[1] = NULL;
    ana_amiga_screen_buffers_ready = 0;
}

static void ana_amiga_drain_safe_port(void)
{
    if (ana_amiga_safe_port == NULL) {
        return;
    }

    while (GetMsg(ana_amiga_safe_port) != NULL) {
    }
}

static void ana_amiga_free_safe_port(void)
{
    if (ana_amiga_safe_port == NULL) {
        return;
    }

    ana_amiga_drain_safe_port();
    DeleteMsgPort(ana_amiga_safe_port);
    ana_amiga_safe_port = NULL;
}

static int ana_amiga_supports_screen_buffers(void)
{
    return IntuitionBase != NULL &&
        IntuitionBase->LibNode.lib_Version >= 39u;
}

static void ana_amiga_free_screen_buffers(void)
{
    int i;

    if (ana_amiga_screen == NULL) {
        ana_amiga_free_safe_port();
        ana_amiga_reset_screen_buffers();
        return;
    }

    for (i = 0; i < 2; i++) {
        if (ana_amiga_screen_buffers[i] != NULL &&
                ana_amiga_screen_buffers[i]->sb_DBufInfo != NULL) {
            ana_amiga_screen_buffers[i]->sb_DBufInfo->
                dbi_SafeMessage.mn_ReplyPort = NULL;
        }
    }

    ana_amiga_free_safe_port();

    for (i = 1; i >= 0; i--) {
        if (ana_amiga_screen_buffers[i] != NULL) {
            FreeScreenBuffer(ana_amiga_screen, ana_amiga_screen_buffers[i]);
            ana_amiga_screen_buffers[i] = NULL;
        }
    }

    ana_amiga_screen_buffers_ready = 0;
}

static int ana_amiga_prepare_screen_buffer_messages(void)
{
    int i;

    ana_amiga_free_safe_port();
    ana_amiga_safe_port = CreateMsgPort();
    if (ana_amiga_safe_port == NULL) {
        return 0;
    }

    for (i = 0; i < 2; i++) {
        if (ana_amiga_screen_buffers[i] == NULL ||
                ana_amiga_screen_buffers[i]->sb_DBufInfo == NULL) {
            ana_amiga_free_safe_port();
            return 0;
        }

        ana_amiga_screen_buffers[i]->sb_DBufInfo->
            dbi_SafeMessage.mn_ReplyPort = ana_amiga_safe_port;
    }

    return 1;
}

static int ana_amiga_alloc_screen_buffers(void)
{
    if (ana_amiga_screen == NULL ||
            ana_amiga_original_bitmap == NULL ||
            !ana_amiga_supports_screen_buffers()) {
        return 0;
    }

    ana_amiga_screen_buffers[0] =
        AllocScreenBuffer(ana_amiga_screen, NULL, SB_SCREEN_BITMAP);
    if (ana_amiga_screen_buffers[0] == NULL) {
        ana_amiga_reset_screen_buffers();
        return 0;
    }

    ana_amiga_screen_buffers[1] =
        AllocScreenBuffer(ana_amiga_screen, NULL, SB_COPY_BITMAP);
    if (ana_amiga_screen_buffers[1] == NULL ||
            ana_amiga_screen_buffers[0]->sb_BitMap == NULL ||
            ana_amiga_screen_buffers[1]->sb_BitMap == NULL) {
        ana_amiga_free_screen_buffers();
        return 0;
    }

    if (!ana_amiga_prepare_screen_buffer_messages()) {
        ana_amiga_free_screen_buffers();
        return 0;
    }

    ana_amiga_screen_buffers_ready = 1;
    return 1;
}

static struct ScreenBuffer* ana_amiga_screen_buffer_for(
    struct BitMap* bitmap)
{
    int i;

    if (!ana_amiga_screen_buffers_ready || bitmap == NULL) {
        return NULL;
    }

    for (i = 0; i < 2; i++) {
        if (ana_amiga_screen_buffers[i] != NULL &&
                ana_amiga_screen_buffers[i]->sb_BitMap == bitmap) {
            return ana_amiga_screen_buffers[i];
        }
    }

    return NULL;
}

static void ana_amiga_wait_screen_buffer_safe(struct ScreenBuffer* screen_buffer)
{
    struct Message* expected;
    struct Message* message;

    if (ana_amiga_safe_port == NULL ||
            screen_buffer == NULL ||
            screen_buffer->sb_DBufInfo == NULL) {
        return;
    }

    expected = &screen_buffer->sb_DBufInfo->dbi_SafeMessage;

    for (;;) {
        while ((message = GetMsg(ana_amiga_safe_port)) != NULL) {
            if (message == expected) {
                return;
            }
        }

        WaitPort(ana_amiga_safe_port);
    }
}

static int ana_amiga_alloc_hidden_bitmap(void)
{
    int plane;

    memset(&ana_amiga_hidden_bitmap, 0, sizeof(ana_amiga_hidden_bitmap));
    InitBitMap(
        &ana_amiga_hidden_bitmap,
        ANA_DEFAULT_BITPLANES,
        ANA_DEFAULT_WIDTH,
        ANA_DEFAULT_HEIGHT);

    for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
        ana_amiga_hidden_bitmap.Planes[plane] =
            AllocRaster(ANA_DEFAULT_WIDTH, ANA_DEFAULT_HEIGHT);

        if (ana_amiga_hidden_bitmap.Planes[plane] == NULL) {
            ana_amiga_hidden_bitmap_ready = 1;
            ana_amiga_free_hidden_bitmap();
            return 0;
        }
    }

    ana_amiga_hidden_bitmap_ready = 1;
    ana_amiga_clear_bitmap(&ana_amiga_hidden_bitmap);

    return 1;
}

static void ana_amiga_set_screen_bitmap_direct(struct BitMap* bitmap)
{
    if (ana_amiga_screen == NULL || bitmap == NULL) {
        return;
    }

    ana_amiga_screen->RastPort.BitMap = bitmap;
    if (ana_amiga_screen->ViewPort.RasInfo != NULL) {
        ana_amiga_screen->ViewPort.RasInfo->BitMap = bitmap;
        ScrollVPort(&ana_amiga_screen->ViewPort);
        return;
    }

    MakeScreen(ana_amiga_screen);
    RethinkDisplay();
}

static int ana_amiga_set_screen_bitmap(struct BitMap* bitmap)
{
    struct ScreenBuffer* screen_buffer;

    if (ana_amiga_screen == NULL || bitmap == NULL) {
        return 0;
    }

    screen_buffer = ana_amiga_screen_buffer_for(bitmap);
    if (screen_buffer != NULL &&
            ChangeScreenBuffer(ana_amiga_screen, screen_buffer)) {
        ana_amiga_wait_screen_buffer_safe(screen_buffer);
        return 1;
    }

    ana_amiga_set_screen_bitmap_direct(bitmap);
    return 0;
}

static int ana_amiga_open_window(void)
{
    struct NewWindow window;

    memset(&window, 0, sizeof(window));

    window.LeftEdge = 0;
    window.TopEdge = 0;
    window.Width = ANA_DEFAULT_WIDTH;
    window.Height = ANA_DEFAULT_HEIGHT;
    window.DetailPen = 0;
    window.BlockPen = 1;
    window.IDCMPFlags = IDCMP_RAWKEY;
    window.Flags = ANA_AMIGA_WINDOW_BORDERLESS |
        ANA_AMIGA_WINDOW_ACTIVATE |
        ANA_AMIGA_WINDOW_RMBTRAP |
        ANA_AMIGA_WINDOW_REFRESH;
    window.Screen = ana_amiga_screen;
    window.Type = CUSTOMSCREEN;

    ana_amiga_window = OpenWindow(&window);
    if (ana_amiga_window == NULL) {
        return 0;
    }

    ActivateWindow(ana_amiga_window);
    return 1;
}

static int ana_amiga_open_display(void)
{
    struct NewScreen screen;

    ana_amiga_reset_screen_buffers();

    if (!ana_amiga_open_libraries()) {
        ana_amiga_close_libraries();
        return 0;
    }

    memset(&screen, 0, sizeof(screen));

    screen.LeftEdge = 0;
    screen.TopEdge = 0;
    screen.Width = ANA_DEFAULT_WIDTH;
    screen.Height = ANA_DEFAULT_HEIGHT;
    screen.Depth = ANA_DEFAULT_BITPLANES;
    screen.DetailPen = 0;
    screen.BlockPen = 1;
    screen.ViewModes = 0;
    screen.Type = CUSTOMSCREEN;
    screen.DefaultTitle = (unsigned char*)"ANA";

    ana_amiga_screen = OpenScreen(&screen);
    if (ana_amiga_screen == NULL) {
        ana_amiga_close_libraries();
        return 0;
    }

    ana_amiga_original_bitmap = ana_amiga_screen->RastPort.BitMap;
    ana_amiga_visible_bitmap = ana_amiga_original_bitmap;
    ana_amiga_draw_bitmap = &ana_amiga_hidden_bitmap;
    ana_amiga_reset_bitmap_states();

    if (!ana_amiga_alloc_hidden_bitmap()) {
        CloseScreen(ana_amiga_screen);
        ana_amiga_screen = NULL;
        ana_amiga_original_bitmap = NULL;
        ana_amiga_visible_bitmap = NULL;
        ana_amiga_draw_bitmap = NULL;
        ana_amiga_close_libraries();
        return 0;
    }

    ana_amiga_clear_bitmap(ana_amiga_original_bitmap);
    ana_amiga_apply_palette();
    ana_amiga_alloc_screen_buffers();
    if (ana_amiga_screen_buffers_ready) {
        ana_amiga_original_bitmap = ana_amiga_screen_buffers[0]->sb_BitMap;
        ana_amiga_draw_bitmap = ana_amiga_screen_buffers[1]->sb_BitMap;
        ana_amiga_visible_bitmap = ana_amiga_original_bitmap;
    }

    ana_amiga_bitmap_states[0].bitmap = ana_amiga_original_bitmap;
    ana_amiga_bitmap_states[0].clear_color_valid = 1;
    ana_amiga_bitmap_states[0].clear_color = 0u;
    ana_amiga_bitmap_states[1].bitmap = ana_amiga_draw_bitmap;
    ana_amiga_bitmap_states[1].clear_color_valid = 1;
    ana_amiga_bitmap_states[1].clear_color = 0u;

    if (!ana_amiga_open_window()) {
        ana_amiga_free_screen_buffers();
        ana_amiga_free_hidden_bitmap();
        CloseScreen(ana_amiga_screen);
        ana_amiga_screen = NULL;
        ana_amiga_original_bitmap = NULL;
        ana_amiga_visible_bitmap = NULL;
        ana_amiga_draw_bitmap = NULL;
        ana_amiga_close_libraries();
        return 0;
    }

    ScreenToFront(ana_amiga_screen);
    return 1;
}

static void ana_amiga_close_display(void)
{
    if (ana_amiga_screen != NULL && ana_amiga_original_bitmap != NULL) {
        ana_amiga_set_screen_bitmap(ana_amiga_original_bitmap);
        WaitTOF();
    }

    if (ana_amiga_window != NULL) {
        CloseWindow(ana_amiga_window);
        ana_amiga_window = NULL;
    }

    ana_amiga_free_screen_buffers();

    if (ana_amiga_screen != NULL) {
        CloseScreen(ana_amiga_screen);
        ana_amiga_screen = NULL;
    }

    ana_amiga_free_hidden_bitmap();
    ana_amiga_original_bitmap = NULL;
    ana_amiga_visible_bitmap = NULL;
    ana_amiga_draw_bitmap = NULL;
    ana_amiga_reset_frame_state();
    ana_amiga_reset_bitmap_states();

    ana_amiga_close_libraries();
}

static void ana_amiga_copy_chunky_rect_to_bitplanes(
    struct BitMap* bitmap,
    const unsigned char* chunky,
    int min_x,
    int min_y,
    int max_x,
    int max_y)
{
    int y;
    int byte_x;
    int start_byte_x;
    int end_byte_x;
    int byte_count;
    int index0;
    int index1;
    int index2;
    int index3;
    unsigned long packed;
    const unsigned long* lut0;
    const unsigned long* lut1;
    const unsigned long* lut2;
    const unsigned long* lut3;
    unsigned char* plane0;
    unsigned char* plane1;
    unsigned char* plane2;
    unsigned char* plane3;
    unsigned char* out0;
    unsigned char* out1;
    unsigned char* out2;
    unsigned char* out3;
    const unsigned char* in;
    unsigned long row_offset;

    if (bitmap == NULL || chunky == NULL) {
        return;
    }

    if (min_x < 0) {
        min_x = 0;
    }

    if (min_y < 0) {
        min_y = 0;
    }

    if (max_x > ANA_DEFAULT_WIDTH) {
        max_x = ANA_DEFAULT_WIDTH;
    }

    if (max_y > ANA_DEFAULT_HEIGHT) {
        max_y = ANA_DEFAULT_HEIGHT;
    }

    if (min_x >= max_x || min_y >= max_y) {
        return;
    }

    start_byte_x = min_x / 8;
    end_byte_x = (max_x + 7) / 8;
    byte_count = end_byte_x - start_byte_x;

    plane0 = (unsigned char*)bitmap->Planes[0];
    plane1 = (unsigned char*)bitmap->Planes[1];
    plane2 = (unsigned char*)bitmap->Planes[2];
    plane3 = (unsigned char*)bitmap->Planes[3];

    if (plane0 == NULL || plane1 == NULL || plane2 == NULL ||
            plane3 == NULL) {
        return;
    }

    ana_amiga_init_planar_pair_lut();
    lut0 = ana_amiga_planar_pair_lut[0];
    lut1 = ana_amiga_planar_pair_lut[1];
    lut2 = ana_amiga_planar_pair_lut[2];
    lut3 = ana_amiga_planar_pair_lut[3];

    for (y = min_y; y < max_y; y++) {
        row_offset = (unsigned long)y * bitmap->BytesPerRow;
        in = chunky + ((unsigned long)y * ANA_DEFAULT_WIDTH) +
            (start_byte_x * 8);
        out0 = plane0 + row_offset + start_byte_x;
        out1 = plane1 + row_offset + start_byte_x;
        out2 = plane2 + row_offset + start_byte_x;
        out3 = plane3 + row_offset + start_byte_x;

        for (byte_x = 0; byte_x < byte_count; byte_x++) {
            index0 = ((in[0] & 0x0f) << 4) | (in[1] & 0x0f);
            index1 = ((in[2] & 0x0f) << 4) | (in[3] & 0x0f);
            index2 = ((in[4] & 0x0f) << 4) | (in[5] & 0x0f);
            index3 = ((in[6] & 0x0f) << 4) | (in[7] & 0x0f);
            packed =
                lut0[index0] |
                lut1[index1] |
                lut2[index2] |
                lut3[index3];

            out0[byte_x] = (unsigned char)(packed & 0xffu);
            out1[byte_x] = (unsigned char)((packed >> 8) & 0xffu);
            out2[byte_x] = (unsigned char)((packed >> 16) & 0xffu);
            out3[byte_x] = (unsigned char)((packed >> 24) & 0xffu);

            in += 8;
        }
    }
}

static void ana_amiga_present_buffer(const unsigned char* chunky)
{
    struct BitMap* next_visible;
    struct BitMap* previous_visible;
    struct ANA_AmigaBitmapState* next_state;
    unsigned long total_start;
    unsigned long stage_start;
    int i;

    if (ana_amiga_screen == NULL) {
        return;
    }

    total_start = ana_platform_perf_ticks();

#ifdef ANA_AMIGA_DIRECT_PRESENT_SYNC
    stage_start = ana_platform_perf_ticks();
    WaitTOF();
    ana_gfx_record_perf_ticks(
        &ana_gfx_stats.present_flip_perf_ticks,
        stage_start,
        ana_platform_perf_ticks());
#endif

#ifdef ANA_AMIGA_DIRECT_PRESENT
    next_visible = ana_amiga_visible_bitmap;
    previous_visible = ana_amiga_visible_bitmap;
#else
    next_visible = ana_amiga_draw_bitmap;
    previous_visible = ana_amiga_visible_bitmap;
#endif

    if (next_visible == NULL || previous_visible == NULL) {
        return;
    }

    next_state = ana_amiga_bitmap_state_for(next_visible);

    stage_start = ana_platform_perf_ticks();
    if (ana_amiga_clear_requested) {
        if (next_state == NULL ||
                !next_state->clear_color_valid ||
                next_state->clear_color != ana_amiga_clear_color) {
            ana_amiga_fill_bitmap(next_visible, ana_amiga_clear_color);
            ana_gfx_record_planar_clear_fullscreen();
            if (next_state != NULL) {
                next_state->dirty_count = 0;
                next_state->clear_color_valid = 1;
                next_state->clear_color = ana_amiga_clear_color;
            }
        } else {
            for (i = 0; i < next_state->dirty_count; i++) {
                if (ana_amiga_rect_contained_by_any(
                        &next_state->dirty_rects[i],
                        ana_amiga_dirty_rects,
                        ana_amiga_dirty_count)) {
                    continue;
                }

                ana_gfx_record_planar_clear_rects(
                    &next_state->dirty_rects[i],
                    1);
                ana_amiga_fill_bitmap_rect(
                    next_visible,
                    ana_amiga_clear_color,
                    &next_state->dirty_rects[i]);
            }
        }
    }

    for (i = 0; i < ana_amiga_planar_clear_count; i++) {
        if (ana_amiga_rect_contained_by_any(
                &ana_amiga_planar_clear_rects[i],
                ana_amiga_dirty_rects,
                ana_amiga_dirty_count)) {
            continue;
        }

        ana_gfx_record_planar_clear_rects(
            &ana_amiga_planar_clear_rects[i],
            1);
        ana_amiga_fill_bitmap_rect(
            next_visible,
            0u,
            &ana_amiga_planar_clear_rects[i]);
    }
    ana_gfx_record_perf_ticks(
        &ana_gfx_stats.present_clear_perf_ticks,
        stage_start,
        ana_platform_perf_ticks());

    stage_start = ana_platform_perf_ticks();
    ana_gfx_record_present_dirty_rects(
        ana_amiga_dirty_rects,
        ana_amiga_dirty_count);

    for (i = 0; i < ana_amiga_dirty_count; i++) {
        ana_amiga_copy_chunky_rect_to_bitplanes(
            next_visible,
            chunky,
            ana_amiga_dirty_rects[i].min_x,
            ana_amiga_dirty_rects[i].min_y,
            ana_amiga_dirty_rects[i].max_x,
            ana_amiga_dirty_rects[i].max_y);
    }
    ana_gfx_record_perf_ticks(
        &ana_gfx_stats.present_convert_perf_ticks,
        stage_start,
        ana_platform_perf_ticks());

    ana_amiga_store_bitmap_dirty_state(next_state);

    stage_start = ana_platform_perf_ticks();
#ifdef ANA_AMIGA_DIRECT_PRESENT
#ifndef ANA_AMIGA_DIRECT_PRESENT_SYNC
    ana_gfx_stats.direct_flips++;
#endif
#else
    if (!ana_amiga_set_screen_bitmap(next_visible)) {
        ana_gfx_stats.direct_flips++;
        WaitTOF();
    } else {
        ana_gfx_stats.screen_buffer_flips++;
    }
#endif
#ifndef ANA_AMIGA_DIRECT_PRESENT_SYNC
    ana_gfx_record_perf_ticks(
        &ana_gfx_stats.present_flip_perf_ticks,
        stage_start,
        ana_platform_perf_ticks());
#endif
#ifdef ANA_AMIGA_DIRECT_PRESENT_SYNC
    ana_gfx_stats.direct_flips++;
#endif

#ifndef ANA_AMIGA_DIRECT_PRESENT
    ana_amiga_visible_bitmap = next_visible;
    ana_amiga_draw_bitmap = previous_visible;
#endif
    ana_amiga_reset_frame_state();
    ana_gfx_record_perf_ticks(
        &ana_gfx_stats.present_total_perf_ticks,
        total_start,
        ana_platform_perf_ticks());
}
#endif

ANA_Result ana_gfx_open(const ANA_Profile* profile)
{
    ANA_Result result;

    result = ana_validate_profile(profile);
    if (result != ANA_OK) {
        return result;
    }

    memset(ana_framebuffers, 0, sizeof(ana_framebuffers));
    ana_gfx_load_default_palette();
    ana_gfx_reset_stats();

#ifdef ANA_TARGET_AMIGA
    ana_amiga_reset_framebuffer_states();
    ana_amiga_init_planar_pair_lut();
#endif

    ana_gfx_opened = 1;
    ana_draw_buffer = 0;
    ana_front_buffer = 1;
    ana_presented_frames = 0;

#ifdef ANA_TARGET_AMIGA
    ana_amiga_reset_frame_state();

    if (!ana_amiga_open_display()) {
        ana_gfx_opened = 0;
        return ANA_ERROR_NOT_IMPLEMENTED;
    }
#endif

    return ANA_OK;
}

ANA_Image ana_load_image(const char* path)
{
    FILE* file;
    unsigned char header[ANA_IMAGE_HEADER_SIZE];
    ANA_Image image;
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

    image = ana_image_create_from_payload(
        width,
        height,
        frame_count,
        bitplanes,
        flags,
        NULL,
        0L);

    if (image == NULL) {
        fclose(file);
        return NULL;
    }

    bytes_read = fread(image->data, 1u, (size_t)image->data_size, file);
    fclose(file);

    if (bytes_read != (size_t)image->data_size) {
        ana_free_image(image);
        return NULL;
    }

    if (!ana_image_decode_pixels(image)) {
        ana_free_image(image);
        return NULL;
    }

    return image;
}

ANA_Image ana_load_image_data(const unsigned char* bytes, long size)
{
    ANA_Image image;
    int width;
    int height;
    int frame_count;
    int bitplanes;
    unsigned int flags;

    if (bytes == NULL || size < ANA_IMAGE_HEADER_SIZE) {
        return NULL;
    }

    if (!ana_image_validate_header(
            bytes,
            &width,
            &height,
            &frame_count,
            &bitplanes,
            &flags)) {
        return NULL;
    }

    image = ana_image_create_from_payload(
        width,
        height,
        frame_count,
        bitplanes,
        flags,
        bytes + ANA_IMAGE_HEADER_SIZE,
        size - ANA_IMAGE_HEADER_SIZE);

    if (image == NULL) {
        return NULL;
    }

    if (!ana_image_decode_pixels(image)) {
        ana_free_image(image);
        return NULL;
    }

    return image;
}

ANA_Font ana_load_font(const char* path)
{
    FILE* file;
    unsigned char* bytes;
    long size;
    size_t bytes_read;
    ANA_Font font;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    size = ftell(file);
    if (size <= 0L) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    bytes = (unsigned char*)malloc((size_t)size);
    if (bytes == NULL) {
        fclose(file);
        return NULL;
    }

    bytes_read = fread(bytes, 1u, (size_t)size, file);
    fclose(file);

    if (bytes_read != (size_t)size) {
        free(bytes);
        return NULL;
    }

    font = ana_load_font_data(bytes, size);
    free(bytes);

    return font;
}

ANA_Font ana_load_font_data(const unsigned char* bytes, long size)
{
    ANA_Font font;
    ANA_Image image;
    int char_width;
    int char_height;
    int first_char;
    int char_count;

    if (bytes == NULL || size < ANA_FONT_HEADER_SIZE + ANA_IMAGE_HEADER_SIZE) {
        return NULL;
    }

    if (!ana_font_validate_header(
            bytes,
            &char_width,
            &char_height,
            &first_char,
            &char_count)) {
        return NULL;
    }

    image = ana_load_image_data(
        bytes + ANA_FONT_HEADER_SIZE,
        size - ANA_FONT_HEADER_SIZE);
    if (image == NULL) {
        return NULL;
    }

    if (ana_image_width(image) != char_width ||
            ana_image_height(image) != char_height ||
            ana_image_frame_count(image) < char_count) {
        ana_free_image(image);
        return NULL;
    }

    font = (ANA_Font)malloc(sizeof(struct ANA_FontData));
    if (font == NULL) {
        ana_free_image(image);
        return NULL;
    }

    font->image = image;
    font->char_width = char_width;
    font->char_height = char_height;
    font->first_char = first_char;
    font->char_count = char_count;
    font->color_index = 1u;

    return font;
}

void ana_free_font(ANA_Font font)
{
    if (font == NULL) {
        return;
    }

    ana_free_image(font->image);
    font->image = NULL;
    free(font);
}

void ana_set_font_color(ANA_Font font, unsigned char color_index)
{
    if (font == NULL) {
        return;
    }

    font->color_index = (unsigned char)(color_index & 0x0f);
}

static void ana_draw_font_glyph(ANA_Font font, int frame, int x, int y)
{
    ANA_Image image;
    int start_x;
    int start_y;
    int end_x;
    int end_y;
    int dest_x;
    int dest_y;
    int src_x;
    int src_y;
    int draw_pixel;
    const unsigned char* mask;
    const unsigned char* mask_row;
    const unsigned char* pixels;
    const unsigned char* source_row;
    unsigned char* dest_row;

    if (!ana_gfx_opened || font == NULL || font->image == NULL) {
        return;
    }

    image = font->image;

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
    pixels = ana_image_pixels_base(image, frame);

    for (dest_y = start_y; dest_y < end_y; dest_y++) {
        src_y = dest_y - y;
        mask_row = mask != NULL ?
            mask + ((long)src_y * image->row_bytes) :
            NULL;
        source_row = pixels != NULL ?
            pixels + ((long)src_y * image->width) :
            NULL;
        dest_row =
            ana_framebuffers[ana_draw_buffer] +
            ((long)dest_y * ANA_DEFAULT_WIDTH);

        for (dest_x = start_x; dest_x < end_x; dest_x++) {
            src_x = dest_x - x;

            if (mask_row != NULL) {
                draw_pixel = (
                    mask_row[src_x >> 3] &
                        (0x80u >> (src_x & 7))) != 0u;
            } else {
                draw_pixel = source_row != NULL ?
                    source_row[src_x] != 0u :
                    ana_image_pixel_at(image, frame, src_x, src_y) != 0u;
            }

            if (draw_pixel) {
                dest_row[dest_x] = font->color_index;
            }
        }
    }
}

static int ana_draw_text_fast_mask8(ANA_Font font, int x, int y, const char* text)
{
    ANA_Image image;
    const unsigned char* mask;
    unsigned char* dest_row;
    unsigned char bits;
    int width;
    int pen_x;
    int frame;
    int row;
    unsigned char ch;

    image = font->image;
    width = ana_text_width(font, text);

    if (image == NULL ||
            !ana_image_has_mask(image) ||
            image->row_bytes != 1 ||
            image->width > 8 ||
            x < 0 ||
            y < 0 ||
            x + width > ANA_DEFAULT_WIDTH ||
            y + image->height > ANA_DEFAULT_HEIGHT) {
        return 0;
    }

    pen_x = x;
    while (*text != '\0') {
        ch = (unsigned char)*text;
        frame = (int)ch - font->first_char;

        if (frame >= 0 && frame < font->char_count) {
            mask = ana_image_mask_base(image, frame);
            for (row = 0; row < image->height; row++) {
                bits = mask[row];
                if (bits != 0u) {
                    dest_row =
                        ana_framebuffers[ana_draw_buffer] +
                        ((long)(y + row) * ANA_DEFAULT_WIDTH) +
                        pen_x;

                    if (image->width == 6) {
                        if ((bits & 0x80u) != 0u) {
                            dest_row[0] = font->color_index;
                        }
                        if ((bits & 0x40u) != 0u) {
                            dest_row[1] = font->color_index;
                        }
                        if ((bits & 0x20u) != 0u) {
                            dest_row[2] = font->color_index;
                        }
                        if ((bits & 0x10u) != 0u) {
                            dest_row[3] = font->color_index;
                        }
                        if ((bits & 0x08u) != 0u) {
                            dest_row[4] = font->color_index;
                        }
                        if ((bits & 0x04u) != 0u) {
                            dest_row[5] = font->color_index;
                        }
                    } else {
                        if ((bits & 0x80u) != 0u) {
                            dest_row[0] = font->color_index;
                        }
                        if (image->width > 1 && (bits & 0x40u) != 0u) {
                            dest_row[1] = font->color_index;
                        }
                        if (image->width > 2 && (bits & 0x20u) != 0u) {
                            dest_row[2] = font->color_index;
                        }
                        if (image->width > 3 && (bits & 0x10u) != 0u) {
                            dest_row[3] = font->color_index;
                        }
                        if (image->width > 4 && (bits & 0x08u) != 0u) {
                            dest_row[4] = font->color_index;
                        }
                        if (image->width > 5 && (bits & 0x04u) != 0u) {
                            dest_row[5] = font->color_index;
                        }
                        if (image->width > 6 && (bits & 0x02u) != 0u) {
                            dest_row[6] = font->color_index;
                        }
                        if (image->width > 7 && (bits & 0x01u) != 0u) {
                            dest_row[7] = font->color_index;
                        }
                    }
                }
            }
        }

        pen_x += font->char_width;
        text++;
    }

    return 1;
}

void ana_draw_text(ANA_Font font, int x, int y, const char* text)
{
    int pen_x;
    int frame;
    int width;
    unsigned char ch;

    if (font == NULL || font->image == NULL || text == NULL) {
        return;
    }

#ifdef ANA_TARGET_AMIGA
    width = ana_text_width(font, text);
    ana_amiga_mark_dirty_rect(x, y, x + width, y + font->char_height);
#else
    width = 0;
    (void)width;
#endif

    if (ana_draw_text_fast_mask8(font, x, y, text)) {
        return;
    }

    pen_x = x;

    while (*text != '\0') {
        ch = (unsigned char)*text;
        frame = (int)ch - font->first_char;

        if (frame >= 0 && frame < font->char_count) {
            ana_draw_font_glyph(font, frame, pen_x, y);
        }

        pen_x += font->char_width;
        text++;
    }
}

void ana_draw_int(ANA_Font font, int x, int y, int value)
{
    char text[16];
    char reversed[16];
    unsigned long magnitude;
    int count;
    int i;
    int out;

    if (value < 0) {
        magnitude = (unsigned long)(-(value + 1)) + 1u;
    } else {
        magnitude = (unsigned long)value;
    }

    count = 0;
    do {
        reversed[count++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    } while (magnitude != 0u && count < (int)sizeof(reversed));

    out = 0;
    if (value < 0 && out < (int)sizeof(text) - 1) {
        text[out++] = '-';
    }

    for (i = count - 1; i >= 0 && out < (int)sizeof(text) - 1; i--) {
        text[out++] = reversed[i];
    }

    text[out] = '\0';
    ana_draw_text(font, x, y, text);
}

int ana_text_width(ANA_Font font, const char* text)
{
    int count;

    if (font == NULL || text == NULL) {
        return 0;
    }

    count = 0;
    while (*text != '\0') {
        count++;
        text++;
    }

    return count * font->char_width;
}

void ana_free_image(ANA_Image image)
{
    if (image == NULL) {
        return;
    }

    free(image->data);
    image->data = NULL;
    free(image->pixels);
    image->pixels = NULL;
    free(image);
}

void ana_draw_image(ANA_Image image, int x, int y)
{
    ana_draw_image_frame(image, 0, x, y);
}

void ana_draw_image_frame(ANA_Image image, int frame, int x, int y)
{
    ana_draw_image_frame_internal(image, frame, x, y, 1);
}

static void ana_draw_image_frame_internal(
    ANA_Image image,
    int frame,
    int x,
    int y,
    int mark_dirty)
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
    const unsigned char* pixels;
    const unsigned char* source_row;
    unsigned char* dest_row;

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

#ifdef ANA_TARGET_AMIGA
    if (mark_dirty) {
        ana_amiga_mark_dirty_rect(start_x, start_y, end_x, end_y);
    }
#else
    (void)mark_dirty;
#endif

    if (ana_draw_image_frame_fast(image, frame, x, y)) {
        return;
    }

    mask = ana_image_mask_base(image, frame);
    pixels = ana_image_pixels_base(image, frame);

    for (dest_y = start_y; dest_y < end_y; dest_y++) {
        src_y = dest_y - y;
        source_row = pixels != NULL ?
            pixels + ((long)src_y * image->width) :
            NULL;
        dest_row =
            ana_framebuffers[ana_draw_buffer] +
            ((long)dest_y * ANA_DEFAULT_WIDTH);

        for (dest_x = start_x; dest_x < end_x; dest_x++) {
            src_x = dest_x - x;

            if (mask == NULL ||
                    ana_image_bit_at(mask, image->row_bytes, src_x, src_y)) {
                dest_row[dest_x] = source_row != NULL ?
                    source_row[src_x] :
                    ana_image_pixel_at(image, frame, src_x, src_y);
            }
        }
    }
}

static int ana_draw_image_frame_fast(
    ANA_Image image,
    int frame,
    int x,
    int y)
{
    const unsigned char* mask;
    const unsigned char* pixels;
    const unsigned char* source_row;
    unsigned char* dest_row;
    unsigned char bits;
    int row;

    if (image == NULL ||
            frame < 0 ||
            frame >= image->frame_count ||
            x < 0 ||
            y < 0 ||
            x + image->width > ANA_DEFAULT_WIDTH ||
            y + image->height > ANA_DEFAULT_HEIGHT) {
        return 0;
    }

    pixels = ana_image_pixels_base(image, frame);
    if (pixels == NULL) {
        return 0;
    }

    mask = ana_image_mask_base(image, frame);
    if (mask == NULL) {
        for (row = 0; row < image->height; row++) {
            memcpy(
                ana_framebuffers[ana_draw_buffer] +
                    ((long)(y + row) * ANA_DEFAULT_WIDTH) +
                    x,
                pixels + ((long)row * image->width),
                (size_t)image->width);
        }

        return 1;
    }

    if (image->row_bytes == 1 && image->width == 2) {
        for (row = 0; row < image->height; row++) {
            bits = mask[row];
            if (bits != 0u) {
                source_row = pixels + ((long)row * image->width);
                dest_row =
                    ana_framebuffers[ana_draw_buffer] +
                    ((long)(y + row) * ANA_DEFAULT_WIDTH) +
                    x;

                if ((bits & 0x80u) != 0u) {
                    dest_row[0] = source_row[0];
                }
                if ((bits & 0x40u) != 0u) {
                    dest_row[1] = source_row[1];
                }
            }
        }

        return 1;
    }

    if (image->row_bytes == 1 && image->width <= 8) {
        for (row = 0; row < image->height; row++) {
            bits = mask[row];
            if (bits != 0u) {
                source_row = pixels + ((long)row * image->width);
                dest_row =
                    ana_framebuffers[ana_draw_buffer] +
                    ((long)(y + row) * ANA_DEFAULT_WIDTH) +
                    x;

                if ((bits & 0x80u) != 0u) {
                    dest_row[0] = source_row[0];
                }
                if (image->width > 1 && (bits & 0x40u) != 0u) {
                    dest_row[1] = source_row[1];
                }
                if (image->width > 2 && (bits & 0x20u) != 0u) {
                    dest_row[2] = source_row[2];
                }
                if (image->width > 3 && (bits & 0x10u) != 0u) {
                    dest_row[3] = source_row[3];
                }
                if (image->width > 4 && (bits & 0x08u) != 0u) {
                    dest_row[4] = source_row[4];
                }
                if (image->width > 5 && (bits & 0x04u) != 0u) {
                    dest_row[5] = source_row[5];
                }
                if (image->width > 6 && (bits & 0x02u) != 0u) {
                    dest_row[6] = source_row[6];
                }
                if (image->width > 7 && (bits & 0x01u) != 0u) {
                    dest_row[7] = source_row[7];
                }
            }
        }

        return 1;
    }

    if (image->row_bytes == 2 && image->width == 16) {
        for (row = 0; row < image->height; row++) {
            source_row = pixels + ((long)row * image->width);
            dest_row =
                ana_framebuffers[ana_draw_buffer] +
                ((long)(y + row) * ANA_DEFAULT_WIDTH) +
                x;

            bits = mask[(row * 2)];
            if ((bits & 0x80u) != 0u) {
                dest_row[0] = source_row[0];
            }
            if ((bits & 0x40u) != 0u) {
                dest_row[1] = source_row[1];
            }
            if ((bits & 0x20u) != 0u) {
                dest_row[2] = source_row[2];
            }
            if ((bits & 0x10u) != 0u) {
                dest_row[3] = source_row[3];
            }
            if ((bits & 0x08u) != 0u) {
                dest_row[4] = source_row[4];
            }
            if ((bits & 0x04u) != 0u) {
                dest_row[5] = source_row[5];
            }
            if ((bits & 0x02u) != 0u) {
                dest_row[6] = source_row[6];
            }
            if ((bits & 0x01u) != 0u) {
                dest_row[7] = source_row[7];
            }

            bits = mask[(row * 2) + 1];
            if ((bits & 0x80u) != 0u) {
                dest_row[8] = source_row[8];
            }
            if ((bits & 0x40u) != 0u) {
                dest_row[9] = source_row[9];
            }
            if ((bits & 0x20u) != 0u) {
                dest_row[10] = source_row[10];
            }
            if ((bits & 0x10u) != 0u) {
                dest_row[11] = source_row[11];
            }
            if ((bits & 0x08u) != 0u) {
                dest_row[12] = source_row[12];
            }
            if ((bits & 0x04u) != 0u) {
                dest_row[13] = source_row[13];
            }
            if ((bits & 0x02u) != 0u) {
                dest_row[14] = source_row[14];
            }
            if ((bits & 0x01u) != 0u) {
                dest_row[15] = source_row[15];
            }
        }

        return 1;
    }

    return 0;
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
#ifdef ANA_TARGET_AMIGA
    ana_amiga_close_display();
#endif

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

#ifdef ANA_TARGET_AMIGA
    ana_amiga_apply_palette();
#endif
}

void ana_clear(unsigned char color_index)
{
    if (!ana_gfx_opened) {
        return;
    }

    color_index = (unsigned char)(color_index & 0x0f);

#ifdef ANA_TARGET_AMIGA
    ana_amiga_clear_draw_framebuffer(color_index);
    ana_amiga_clear_requested = 1;
    ana_amiga_clear_color = color_index;
#else
    memset(ana_framebuffers[ana_draw_buffer], color_index, ANA_FRAMEBUFFER_PIXELS);
#endif
}

void ana_fill_rect(unsigned char color_index, int x, int y, int width, int height)
{
    int start_x;
    int start_y;
    int end_x;
    int end_y;
    int row;
    unsigned char* pixels;

    if (!ana_gfx_opened || width <= 0 || height <= 0) {
        return;
    }

    if (x >= ANA_DEFAULT_WIDTH || y >= ANA_DEFAULT_HEIGHT ||
            x + width <= 0 || y + height <= 0) {
        return;
    }

    start_x = x < 0 ? 0 : x;
    start_y = y < 0 ? 0 : y;
    end_x = x + width;
    end_y = y + height;

    if (end_x > ANA_DEFAULT_WIDTH) {
        end_x = ANA_DEFAULT_WIDTH;
    }

    if (end_y > ANA_DEFAULT_HEIGHT) {
        end_y = ANA_DEFAULT_HEIGHT;
    }

    if (start_x >= end_x || start_y >= end_y) {
        return;
    }

    color_index = (unsigned char)(color_index & 0x0f);

#ifdef ANA_TARGET_AMIGA
    if (color_index == 0u && ANA_AMIGA_BLACK_FILL_USES_PLANAR_CLEAR) {
        ana_amiga_mark_planar_clear_rect(start_x, start_y, end_x, end_y);
    } else {
        ana_amiga_mark_dirty_rect(start_x, start_y, end_x, end_y);
    }
#endif

    pixels = ana_framebuffers[ana_draw_buffer];
    for (row = start_y; row < end_y; row++) {
        memset(
            pixels + ((long)row * ANA_DEFAULT_WIDTH) + start_x,
            color_index,
            (size_t)(end_x - start_x));
    }
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
    ana_gfx_stats.frames = (long)ana_presented_frames;

#ifdef ANA_TARGET_AMIGA
    ana_amiga_store_framebuffer_dirty_state(ana_front_buffer);
    ana_amiga_present_buffer(ana_framebuffers[ana_front_buffer]);
#endif
}

int ana_gfx_present_count(void)
{
    return ana_presented_frames;
}

ANA_RenderStats ana_render_stats(void)
{
    return ana_gfx_stats;
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

void* ana_gfx_native_window(void)
{
#ifdef ANA_TARGET_AMIGA
    return ana_amiga_window;
#else
    return NULL;
#endif
}

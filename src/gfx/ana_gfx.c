#include "ana/ana_gfx.h"

#include "ana_internal.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ANA_TARGET_AMIGA
#include <exec/memory.h>
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
#define ANA_AMIGA_IMAGE_MAX_DEST_BYTES ((ANA_IMAGE_MAX_WIDTH + 15) / 8)
#define ANA_FONT_HEADER_SIZE 16
#define ANA_AMIGA_MAX_DIRTY_RECTS 64
#define ANA_AMIGA_HARDWARE_SCROLL_MAX_WIDTH 2048
#define ANA_AMIGA_HARDWARE_SCROLL_BUFFER_COUNT 3
#define ANA_AMIGA_HARDWARE_SCROLL_CPU_FILL_MAX_AREA 2048
#define ANA_AMIGA_COOKIE_CUT_MINTERM 0xe2u
#define ANA_AMIGA_IMAGE_BLITTER_MIN_AREA 4096
#define ANA_AMIGA_RESTORE_CPU_MAX_AREA 4096
#define ANA_LAYER_DISABLED_VALUE 0x414e4101
#define ANA_COPY_8_PIXELS(dest, src) \
    do { \
        (dest)[0] = (src)[0]; \
        (dest)[1] = (src)[1]; \
        (dest)[2] = (src)[2]; \
        (dest)[3] = (src)[3]; \
        (dest)[4] = (src)[4]; \
        (dest)[5] = (src)[5]; \
        (dest)[6] = (src)[6]; \
        (dest)[7] = (src)[7]; \
    } while (0)

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
static ANA_RenderMode ana_gfx_render_mode = ANA_RENDER_DIRTY;
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
#ifdef ANA_TARGET_AMIGA
    struct BitMap* amiga_bitmaps;
    PLANEPTR* amiga_masks;
    unsigned char* amiga_shifted_planes;
    unsigned char* amiga_shifted_masks;
    int amiga_shifted_row_bytes;
    int amiga_native_ready;
    int amiga_native_failed;
    int amiga_shifted_ready;
    int amiga_shifted_failed;
#endif
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
#if defined(ANA_AMIGA_DIRECT_PRESENT) && !defined(ANA_AMIGA_DISABLE_NATIVE_SCROLL)
#define ANA_AMIGA_NATIVE_SCROLL_ENABLED 1
#endif

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
#ifdef ANA_AMIGA_NATIVE_SCROLL_ENABLED
static int ana_amiga_visible_scroll_pending = 0;
static int ana_amiga_native_scroll_requested = 0;
static int ana_amiga_native_scroll_sync_chunky = 1;
#endif

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

struct ANA_AmigaHardwareScrollState {
    struct BitMap bitmaps[ANA_AMIGA_HARDWARE_SCROLL_BUFFER_COUNT];
    struct BitMap background_bitmap;
    struct RastPort rastport;
    struct BitMap hud_cache_bitmap;
    struct RastPort hud_cache_rastport;
    int ready;
    int width;
    int height;
    int viewport_y;
    int viewport_h;
    int owner_valid;
    int map_width;
    int map_height;
    int tile_width;
    int tile_height;
    int draw_index;
    int visible_index;
    int draw_active;
    int draw_offset_x;
    int draw_offset_y;
    int offset_x;
    int committed_offset_x;
    int view_offset_dirty;
    int rastport_ready;
    int blit_pending;
    int background_ready;
    int draw_clip_active;
    int draw_clip_min_x;
    int draw_clip_min_y;
    int draw_clip_max_x;
    int draw_clip_max_y;
    int sync_chunky;
    int hud_cache_ready;
    int hud_cache_valid;
    int hud_cache_blit_pending;
    ANA_Rect hud_cache_viewport;
};

static struct ANA_AmigaBitmapState ana_amiga_bitmap_states[2];
static struct ANA_AmigaFramebufferState
    ana_amiga_framebuffer_states[ANA_FRAMEBUFFER_COUNT];
static struct ANA_AmigaHardwareScrollState ana_amiga_hardware_scroll;
static unsigned long ana_amiga_planar_pair_lut[4][256];
static int ana_amiga_planar_pair_lut_ready = 0;
static int ana_amiga_active_layer_valid = 0;
static ANA_LayerKind ana_amiga_active_layer_kind = ANA_LAYER_STATIC;
static ANA_Rect ana_amiga_active_layer_viewport;

static void ana_amiga_clear_bitmap(struct BitMap* bitmap);
static void ana_amiga_set_screen_bitmap_direct(struct BitMap* bitmap);
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
#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_DIRECT_PRESENT)
static int ana_amiga_hardware_scroll_draw_image_frame(
    ANA_Image image,
    int frame,
    int x,
    int y);
#endif

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

#ifdef ANA_TARGET_AMIGA
static void ana_amiga_image_copy_plane_rows(
    PLANEPTR dest,
    int dest_row_bytes,
    const unsigned char* source,
    int source_row_bytes,
    int height)
{
    unsigned char* dest_row;
    const unsigned char* source_row;
    int copy_bytes;
    int y;

    if (dest == NULL || dest_row_bytes <= 0 || height <= 0) {
        return;
    }

    copy_bytes = source_row_bytes < dest_row_bytes ?
        source_row_bytes :
        dest_row_bytes;

    for (y = 0; y < height; y++) {
        dest_row = ((unsigned char*)dest) + ((long)y * dest_row_bytes);
        memset(dest_row, 0, (size_t)dest_row_bytes);
        if (source != NULL && source_row_bytes > 0 && copy_bytes > 0) {
            source_row = source + ((long)y * source_row_bytes);
            memcpy(dest_row, source_row, (size_t)copy_bytes);
        }
    }
}

static unsigned char ana_amiga_image_shifted_bits_at(
    const unsigned char* row,
    int row_bytes,
    int width,
    int x)
{
    unsigned char out;
    int bit;

    out = 0u;
    if (row == NULL || row_bytes <= 0 || width <= 0) {
        return out;
    }

    for (bit = 0; bit < 8; bit++) {
        if (x + bit < 0 || x + bit >= width) {
            continue;
        }
        if (ana_image_bit_at(row, row_bytes, x + bit, 0)) {
            out = (unsigned char)(out | (0x80u >> bit));
        }
    }

    return out;
}

static unsigned char ana_amiga_image_shifted_full_mask_bits(int width, int x)
{
    unsigned char out;
    int bit;

    out = 0u;
    for (bit = 0; bit < 8; bit++) {
        if (x + bit >= 0 && x + bit < width) {
            out = (unsigned char)(out | (0x80u >> bit));
        }
    }

    return out;
}

static long ana_amiga_image_shifted_frame_offset(
    ANA_Image image,
    int frame,
    int shift)
{
    return ((long)frame * 8L + shift) *
        image->height *
        image->amiga_shifted_row_bytes;
}

static long ana_amiga_image_shifted_plane_offset(
    ANA_Image image,
    int frame,
    int shift,
    int plane)
{
    return (((long)frame * 8L + shift) *
        ANA_DEFAULT_BITPLANES + plane) *
        image->height *
        image->amiga_shifted_row_bytes;
}

static void ana_amiga_image_free_shifted(ANA_Image image)
{
    if (image == NULL) {
        return;
    }

    free(image->amiga_shifted_planes);
    image->amiga_shifted_planes = NULL;
    free(image->amiga_shifted_masks);
    image->amiga_shifted_masks = NULL;
    image->amiga_shifted_row_bytes = 0;
    image->amiga_shifted_ready = 0;
    image->amiga_shifted_failed = 0;
}

static void ana_amiga_image_free_native(ANA_Image image)
{
    struct BitMap* bitmap;
    int frame;
    int plane;

    if (image == NULL) {
        return;
    }

    if (GfxBase != NULL &&
            (image->amiga_bitmaps != NULL || image->amiga_masks != NULL)) {
        WaitBlit();
    }

    if (image->amiga_bitmaps != NULL) {
        for (frame = 0; frame < image->frame_count; frame++) {
            bitmap = &image->amiga_bitmaps[frame];
            for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
                if (bitmap->Planes[plane] != NULL) {
                    FreeRaster(
                        bitmap->Planes[plane],
                        image->width,
                        image->height);
                    bitmap->Planes[plane] = NULL;
                }
            }
        }
        free(image->amiga_bitmaps);
        image->amiga_bitmaps = NULL;
    }

    if (image->amiga_masks != NULL) {
        for (frame = 0; frame < image->frame_count; frame++) {
            if (image->amiga_masks[frame] != NULL) {
                FreeRaster(
                    image->amiga_masks[frame],
                    image->width,
                    image->height);
                image->amiga_masks[frame] = NULL;
            }
        }
        free(image->amiga_masks);
        image->amiga_masks = NULL;
    }

    image->amiga_native_ready = 0;
    image->amiga_native_failed = 0;
}

static int ana_amiga_image_should_use_cpu_shifted(int area)
{
    return area < ANA_AMIGA_IMAGE_BLITTER_MIN_AREA;
}

static int ana_amiga_image_should_use_native_blitter(int area)
{
    return area >= ANA_AMIGA_IMAGE_BLITTER_MIN_AREA;
}

static int ana_amiga_image_prepare_shifted(ANA_Image image)
{
    const unsigned char* source;
    const unsigned char* source_planes;
    const unsigned char* source_mask;
    unsigned char* dest;
    long mask_size;
    long planes_size;
    long offset;
    int frame;
    int shift;
    int plane;
    int y;
    int byte;

    if (image == NULL || image->data == NULL) {
        return 0;
    }

    if (image->amiga_shifted_ready) {
        return 1;
    }

    if (image->amiga_shifted_failed) {
        return 0;
    }

    image->amiga_shifted_row_bytes = (image->width + 15) / 8;
    mask_size = (long)image->frame_count *
        8L *
        image->height *
        image->amiga_shifted_row_bytes;
    planes_size = mask_size * ANA_DEFAULT_BITPLANES;
    if (image->amiga_shifted_row_bytes <= 0 ||
            mask_size <= 0L ||
            planes_size <= 0L) {
        image->amiga_shifted_failed = 1;
        return 0;
    }

    image->amiga_shifted_masks = (unsigned char*)malloc((size_t)mask_size);
    image->amiga_shifted_planes = (unsigned char*)malloc((size_t)planes_size);
    if (image->amiga_shifted_masks == NULL ||
            image->amiga_shifted_planes == NULL) {
        ana_amiga_image_free_shifted(image);
        image->amiga_shifted_failed = 1;
        return 0;
    }
    memset(image->amiga_shifted_masks, 0, (size_t)mask_size);
    memset(image->amiga_shifted_planes, 0, (size_t)planes_size);

    for (frame = 0; frame < image->frame_count; frame++) {
        source_planes = ana_image_planes_base(image, frame);
        source_mask = ana_image_mask_base(image, frame);
        for (shift = 0; shift < 8; shift++) {
            for (y = 0; y < image->height; y++) {
                offset = ana_amiga_image_shifted_frame_offset(
                    image,
                    frame,
                    shift) + ((long)y * image->amiga_shifted_row_bytes);
                dest = image->amiga_shifted_masks + offset;
                source = source_mask != NULL ?
                    source_mask + ((long)y * image->row_bytes) :
                    NULL;
                for (byte = 0; byte < image->amiga_shifted_row_bytes; byte++) {
                    dest[byte] = source != NULL ?
                        ana_amiga_image_shifted_bits_at(
                            source,
                            image->row_bytes,
                            image->width,
                            (byte * 8) - shift) :
                        ana_amiga_image_shifted_full_mask_bits(
                            image->width,
                            (byte * 8) - shift);
                }
            }

            for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
                for (y = 0; y < image->height; y++) {
                    offset = ana_amiga_image_shifted_plane_offset(
                        image,
                        frame,
                        shift,
                        plane) +
                        ((long)y * image->amiga_shifted_row_bytes);
                    dest = image->amiga_shifted_planes + offset;
                    source = NULL;
                    if (source_planes != NULL && plane < image->bitplanes) {
                        source = source_planes +
                            ((long)plane * image->plane_size) +
                            ((long)y * image->row_bytes);
                    }
                    for (byte = 0;
                            byte < image->amiga_shifted_row_bytes;
                            byte++) {
                        dest[byte] = ana_amiga_image_shifted_bits_at(
                            source,
                            image->row_bytes,
                            image->width,
                            (byte * 8) - shift);
                    }
                }
            }
        }
    }

    image->amiga_shifted_ready = 1;
    return 1;
}

static int ana_amiga_image_prepare_native(ANA_Image image)
{
    struct BitMap* bitmap;
    const unsigned char* source;
    const unsigned char* source_planes;
    int frame;
    int plane;
    int has_mask;

    if (image == NULL || image->data == NULL) {
        return 0;
    }

    if (image->amiga_native_ready) {
        return 1;
    }

    if (image->amiga_native_failed || GfxBase == NULL) {
        return 0;
    }

    image->amiga_bitmaps =
        (struct BitMap*)malloc(sizeof(struct BitMap) * image->frame_count);
    if (image->amiga_bitmaps == NULL) {
        image->amiga_native_failed = 1;
        return 0;
    }
    memset(
        image->amiga_bitmaps,
        0,
        sizeof(struct BitMap) * image->frame_count);

    has_mask = ana_image_has_mask(image);
    if (has_mask) {
        image->amiga_masks =
            (PLANEPTR*)malloc(sizeof(PLANEPTR) * image->frame_count);
        if (image->amiga_masks == NULL) {
            ana_amiga_image_free_native(image);
            image->amiga_native_failed = 1;
            return 0;
        }
        memset(image->amiga_masks, 0, sizeof(PLANEPTR) * image->frame_count);
    }

    for (frame = 0; frame < image->frame_count; frame++) {
        bitmap = &image->amiga_bitmaps[frame];
        InitBitMap(
            bitmap,
            ANA_DEFAULT_BITPLANES,
            image->width,
            image->height);

        source_planes = ana_image_planes_base(image, frame);
        for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
            bitmap->Planes[plane] = AllocRaster(image->width, image->height);
            if (bitmap->Planes[plane] == NULL) {
                ana_amiga_image_free_native(image);
                image->amiga_native_failed = 1;
                return 0;
            }

            source = NULL;
            if (source_planes != NULL && plane < image->bitplanes) {
                source = source_planes + ((long)plane * image->plane_size);
            }
            ana_amiga_image_copy_plane_rows(
                bitmap->Planes[plane],
                bitmap->BytesPerRow,
                source,
                image->row_bytes,
                image->height);
        }

        if (has_mask) {
            image->amiga_masks[frame] =
                AllocRaster(image->width, image->height);
            if (image->amiga_masks[frame] == NULL) {
                ana_amiga_image_free_native(image);
                image->amiga_native_failed = 1;
                return 0;
            }

            ana_amiga_image_copy_plane_rows(
                image->amiga_masks[frame],
                bitmap->BytesPerRow,
                ana_image_mask_base(image, frame),
                image->row_bytes,
                image->height);
        }
    }

    image->amiga_native_ready = 1;
    return 1;
}
#endif

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
#ifdef ANA_TARGET_AMIGA
    image->amiga_bitmaps = NULL;
    image->amiga_masks = NULL;
    image->amiga_shifted_planes = NULL;
    image->amiga_shifted_masks = NULL;
    image->amiga_shifted_row_bytes = 0;
    image->amiga_native_ready = 0;
    image->amiga_native_failed = 0;
    image->amiga_shifted_ready = 0;
    image->amiga_shifted_failed = 0;
#endif

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
#ifdef ANA_DEBUG_STATS
    ana_gfx_stats.perf_ticks_per_second =
        (long)ana_platform_perf_ticks_per_second();
#endif
}

void ana_gfx_reset_frame_stats(void)
{
    ana_presented_frames = 0;
    ana_gfx_reset_stats();
}

#ifdef ANA_TARGET_AMIGA
#ifdef ANA_DEBUG_STATS
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
#endif

static long ana_gfx_dirty_rect_area(const struct ANA_AmigaDirtyRect* rect)
{
    if (rect == NULL || rect->min_x >= rect->max_x ||
            rect->min_y >= rect->max_y) {
        return 0L;
    }

    return (long)(rect->max_x - rect->min_x) *
        (long)(rect->max_y - rect->min_y);
}

#ifdef ANA_DEBUG_STATS
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
#else
#define ana_gfx_record_chunky_clear_rects(rects, rect_count) ((void)0)
#define ana_gfx_record_chunky_clear_fullscreen() ((void)0)
#define ana_gfx_record_planar_clear_rects(rects, rect_count) ((void)0)
#define ana_gfx_record_planar_clear_fullscreen() ((void)0)
#define ana_gfx_record_present_dirty_rects(rects, rect_count) ((void)0)
#endif
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
#ifdef ANA_AMIGA_NATIVE_SCROLL_ENABLED
    ana_amiga_visible_scroll_pending = 0;
    ana_amiga_native_scroll_requested = 0;
    ana_amiga_native_scroll_sync_chunky = 1;
#endif
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

static int ana_amiga_normalize_dirty_rect(
    struct ANA_AmigaDirtyRect* rect,
    int min_x,
    int min_y,
    int max_x,
    int max_y)
{
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
        return 0;
    }

    rect->min_x = min_x;
    rect->min_y = min_y;
    rect->max_x = max_x;
    rect->max_y = max_y;
    return 1;
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

    if (!ana_amiga_normalize_dirty_rect(
            &rect,
            min_x,
            min_y,
            max_x,
            max_y)) {
        return;
    }

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

#ifdef ANA_AMIGA_NATIVE_SCROLL_ENABLED
static void ana_amiga_mark_dirty_rect_unmerged(
    const struct ANA_AmigaDirtyRect* rect)
{
    int i;

    for (i = 0; i < ana_amiga_dirty_count; i++) {
        if (ana_amiga_rect_contains(&ana_amiga_dirty_rects[i], rect)) {
            return;
        }

        if (ana_amiga_rect_contains(rect, &ana_amiga_dirty_rects[i])) {
            ana_amiga_dirty_rects[i] = *rect;
            return;
        }
    }

    if (ana_amiga_dirty_count < ANA_AMIGA_MAX_DIRTY_RECTS) {
        ana_amiga_dirty_rects[ana_amiga_dirty_count] = *rect;
        ana_amiga_dirty_count++;
        return;
    }

    ana_amiga_mark_rect(
        ana_amiga_dirty_rects,
        &ana_amiga_dirty_count,
        rect->min_x,
        rect->min_y,
        rect->max_x,
        rect->max_y);
}
#endif

static void ana_amiga_mark_dirty_rect(int min_x, int min_y, int max_x, int max_y)
{
    struct ANA_AmigaDirtyRect rect;

    if (!ana_amiga_normalize_dirty_rect(
            &rect,
            min_x,
            min_y,
            max_x,
            max_y)) {
        return;
    }

#ifdef ANA_AMIGA_NATIVE_SCROLL_ENABLED
    if (ana_amiga_visible_scroll_pending) {
        ana_amiga_mark_dirty_rect_unmerged(&rect);
        return;
    }
#endif

    ana_amiga_mark_rect(
        ana_amiga_dirty_rects,
        &ana_amiga_dirty_count,
        rect.min_x,
        rect.min_y,
        rect.max_x,
        rect.max_y);
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

static void ana_amiga_hardware_scroll_reset_state(void)
{
    memset(&ana_amiga_hardware_scroll, 0, sizeof(ana_amiga_hardware_scroll));
}

static struct BitMap* ana_amiga_hardware_scroll_bitmap_at(int index)
{
    if (index < 0 || index >= ANA_AMIGA_HARDWARE_SCROLL_BUFFER_COUNT) {
        return NULL;
    }

    return &ana_amiga_hardware_scroll.bitmaps[index];
}

static struct BitMap* ana_amiga_hardware_scroll_draw_bitmap(void)
{
    return ana_amiga_hardware_scroll_bitmap_at(
        ana_amiga_hardware_scroll.draw_index);
}

static int ana_amiga_hardware_scroll_next_draw_index(
    int old_visible_index,
    int new_visible_index)
{
    int index;

    for (index = 0;
            index < ANA_AMIGA_HARDWARE_SCROLL_BUFFER_COUNT;
            index++) {
        if (index != old_visible_index && index != new_visible_index) {
            return index;
        }
    }

    for (index = 0;
            index < ANA_AMIGA_HARDWARE_SCROLL_BUFFER_COUNT;
            index++) {
        if (index != new_visible_index) {
            return index;
        }
    }

    return new_visible_index;
}

static int ana_amiga_hardware_scroll_owns_bitmap(struct BitMap* bitmap)
{
    int i;

    if (bitmap == NULL) {
        return 0;
    }

    for (i = 0; i < ANA_AMIGA_HARDWARE_SCROLL_BUFFER_COUNT; i++) {
        if (bitmap == &ana_amiga_hardware_scroll.bitmaps[i]) {
            return 1;
        }
    }

    return 0;
}

static void ana_amiga_hardware_scroll_wait_blit(void)
{
    if (!ana_amiga_hardware_scroll.blit_pending) {
        return;
    }

    WaitBlit();
    ana_amiga_hardware_scroll.blit_pending = 0;
}

static void ana_amiga_hardware_scroll_wait_hud_cache_blit(void)
{
    if (!ana_amiga_hardware_scroll.hud_cache_blit_pending) {
        return;
    }

    WaitBlit();
    ana_amiga_hardware_scroll.hud_cache_blit_pending = 0;
}

static void ana_amiga_hardware_scroll_free_hud_cache(void)
{
    int plane;

    if (!ana_amiga_hardware_scroll.hud_cache_ready) {
        ana_amiga_hardware_scroll.hud_cache_valid = 0;
        return;
    }

    ana_amiga_hardware_scroll_wait_hud_cache_blit();
    for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
        if (ana_amiga_hardware_scroll.hud_cache_bitmap.Planes[plane] !=
                NULL) {
            FreeRaster(
                ana_amiga_hardware_scroll.hud_cache_bitmap.Planes[plane],
                ANA_DEFAULT_WIDTH,
                ana_amiga_hardware_scroll.hud_cache_viewport.h);
            ana_amiga_hardware_scroll.hud_cache_bitmap.Planes[plane] = NULL;
        }
    }

    ana_amiga_hardware_scroll.hud_cache_ready = 0;
    ana_amiga_hardware_scroll.hud_cache_valid = 0;
    ana_amiga_hardware_scroll.hud_cache_blit_pending = 0;
    ana_amiga_hardware_scroll.hud_cache_viewport =
        ana_rect_make(0, 0, 0, 0);
}

static void ana_amiga_hardware_scroll_free_background(void)
{
    int plane;

    if (!ana_amiga_hardware_scroll.background_ready) {
        return;
    }

    ana_amiga_hardware_scroll_wait_blit();
    for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
        if (ana_amiga_hardware_scroll.background_bitmap.Planes[plane] !=
                NULL) {
            FreeRaster(
                ana_amiga_hardware_scroll.background_bitmap.Planes[plane],
                ana_amiga_hardware_scroll.width,
                ana_amiga_hardware_scroll.height);
            ana_amiga_hardware_scroll.background_bitmap.Planes[plane] =
                NULL;
        }
    }

    ana_amiga_hardware_scroll.background_ready = 0;
}

static void ana_amiga_hardware_scroll_free(void)
{
    int buffer;
    int plane;

    if (!ana_amiga_hardware_scroll.ready) {
        ana_amiga_hardware_scroll_free_hud_cache();
        ana_amiga_hardware_scroll_free_background();
        ana_amiga_hardware_scroll_reset_state();
        return;
    }

    ana_amiga_hardware_scroll_wait_blit();
    ana_amiga_hardware_scroll_free_hud_cache();
    ana_amiga_hardware_scroll_free_background();

    for (buffer = 0;
            buffer < ANA_AMIGA_HARDWARE_SCROLL_BUFFER_COUNT;
            buffer++) {
        for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
            if (ana_amiga_hardware_scroll.bitmaps[buffer].Planes[plane] !=
                    NULL) {
                FreeRaster(
                    ana_amiga_hardware_scroll.bitmaps[buffer].Planes[plane],
                    ana_amiga_hardware_scroll.width,
                    ana_amiga_hardware_scroll.height);
                ana_amiga_hardware_scroll.bitmaps[buffer].Planes[plane] =
                    NULL;
            }
        }
    }

    ana_amiga_hardware_scroll_reset_state();
}

static int ana_amiga_hardware_scroll_active(void)
{
    return ana_amiga_hardware_scroll.ready &&
        ana_amiga_hardware_scroll.owner_valid;
}

static int ana_amiga_hardware_scroll_alloc_background(void)
{
    int plane;

    if (ana_amiga_hardware_scroll.background_ready) {
        return 1;
    }

    InitBitMap(
        &ana_amiga_hardware_scroll.background_bitmap,
        ANA_DEFAULT_BITPLANES,
        ana_amiga_hardware_scroll.width,
        ana_amiga_hardware_scroll.height);

    for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
        ana_amiga_hardware_scroll.background_bitmap.Planes[plane] =
            AllocRaster(
                ana_amiga_hardware_scroll.width,
                ana_amiga_hardware_scroll.height);
        if (ana_amiga_hardware_scroll.background_bitmap.Planes[plane] ==
                NULL) {
            ana_amiga_hardware_scroll.background_ready = 1;
            ana_amiga_hardware_scroll_free_background();
            return 0;
        }
    }

    ana_amiga_hardware_scroll.background_ready = 1;
    return 1;
}

static int ana_amiga_hardware_scroll_dest_offset_x(void)
{
    if (!ana_amiga_hardware_scroll_active()) {
        return 0;
    }

    return ana_amiga_hardware_scroll.offset_x;
}

static int ana_amiga_hardware_scroll_matches(const ANA_TileLayer* tile_layer)
{
    ANA_Rect viewport;
    int world_w;
    int world_h;

    if (tile_layer == NULL || !ana_amiga_hardware_scroll.ready ||
            !ana_amiga_hardware_scroll.owner_valid) {
        return 0;
    }

    viewport = tile_layer->layer.viewport;
    if (ana_rect_is_empty(viewport)) {
        viewport = ana_rect_make(
            tile_layer->layer.camera.view_x,
            tile_layer->layer.camera.view_y,
            tile_layer->layer.camera.view_w,
            tile_layer->layer.camera.view_h);
    }

    world_w = tile_layer->map_width * tile_layer->tile_width;
    world_h = tile_layer->map_height * tile_layer->tile_height;

    return ana_amiga_hardware_scroll.map_width == world_w &&
        ana_amiga_hardware_scroll.map_height == world_h &&
        ana_amiga_hardware_scroll.tile_width == tile_layer->tile_width &&
        ana_amiga_hardware_scroll.tile_height == tile_layer->tile_height &&
        ana_amiga_hardware_scroll.viewport_y == viewport.y &&
        ana_amiga_hardware_scroll.viewport_h == viewport.h;
}

static int ana_amiga_hardware_scroll_alloc(const ANA_TileLayer* tile_layer)
{
    ANA_Rect viewport;
    int buffer;
    int plane;
    int width;
    int world_w;
    int world_h;

    if (tile_layer == NULL) {
        return 0;
    }

    if (ana_amiga_hardware_scroll_matches(tile_layer)) {
        return 1;
    }

    ana_amiga_hardware_scroll_free();

    viewport = tile_layer->layer.viewport;
    if (ana_rect_is_empty(viewport)) {
        viewport = ana_rect_make(
            tile_layer->layer.camera.view_x,
            tile_layer->layer.camera.view_y,
            tile_layer->layer.camera.view_w,
            tile_layer->layer.camera.view_h);
    }

    world_w = tile_layer->map_width * tile_layer->tile_width;
    world_h = tile_layer->map_height * tile_layer->tile_height;
    width = world_w + viewport.x;
    if (width < ANA_DEFAULT_WIDTH) {
        width = ANA_DEFAULT_WIDTH;
    }

    for (buffer = 0;
            buffer < ANA_AMIGA_HARDWARE_SCROLL_BUFFER_COUNT;
            buffer++) {
        InitBitMap(
            &ana_amiga_hardware_scroll.bitmaps[buffer],
            ANA_DEFAULT_BITPLANES,
            width,
            ANA_DEFAULT_HEIGHT);

        for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
            ana_amiga_hardware_scroll.bitmaps[buffer].Planes[plane] =
                AllocRaster(width, ANA_DEFAULT_HEIGHT);
            if (ana_amiga_hardware_scroll.bitmaps[buffer].Planes[plane] ==
                    NULL) {
                ana_amiga_hardware_scroll.ready = 1;
                ana_amiga_hardware_scroll.width = width;
                ana_amiga_hardware_scroll.height = ANA_DEFAULT_HEIGHT;
                ana_amiga_hardware_scroll_free();
                return 0;
            }
        }
    }

    ana_amiga_hardware_scroll.ready = 1;
    ana_amiga_hardware_scroll.width = width;
    ana_amiga_hardware_scroll.height = ANA_DEFAULT_HEIGHT;
    ana_amiga_hardware_scroll.viewport_y = viewport.y;
    ana_amiga_hardware_scroll.viewport_h = viewport.h;
    ana_amiga_hardware_scroll.owner_valid = 1;
    ana_amiga_hardware_scroll.map_width = world_w;
    ana_amiga_hardware_scroll.map_height = world_h;
    ana_amiga_hardware_scroll.tile_width = tile_layer->tile_width;
    ana_amiga_hardware_scroll.tile_height = tile_layer->tile_height;
    ana_amiga_hardware_scroll.draw_index = 0;
    ana_amiga_hardware_scroll.visible_index = 0;
    ana_amiga_hardware_scroll.draw_active = 0;
    ana_amiga_hardware_scroll.draw_offset_x = 0;
    ana_amiga_hardware_scroll.draw_offset_y = 0;
    ana_amiga_hardware_scroll.offset_x = 0;
    ana_amiga_hardware_scroll.committed_offset_x = 0;
    ana_amiga_hardware_scroll.view_offset_dirty = 1;
    InitRastPort(&ana_amiga_hardware_scroll.rastport);
    ana_amiga_hardware_scroll.rastport.BitMap =
        ana_amiga_hardware_scroll_draw_bitmap();
    ana_amiga_hardware_scroll.rastport_ready = 1;
    ana_amiga_hardware_scroll.blit_pending = 0;
    ana_amiga_hardware_scroll.background_ready = 0;
    ana_amiga_hardware_scroll_alloc_background();
    ana_amiga_hardware_scroll.sync_chunky = 1;
    for (buffer = 0;
            buffer < ANA_AMIGA_HARDWARE_SCROLL_BUFFER_COUNT;
            buffer++) {
        ana_amiga_clear_bitmap(&ana_amiga_hardware_scroll.bitmaps[buffer]);
    }

    return 1;
}

static void ana_amiga_hardware_scroll_set_view_offset(int x)
{
    struct BitMap* draw_bitmap;

    if (!ana_amiga_hardware_scroll.ready || ana_amiga_screen == NULL ||
            ana_amiga_screen->ViewPort.RasInfo == NULL) {
        return;
    }

    if (x < 0) {
        x = 0;
    }
    if (x > ana_amiga_hardware_scroll.width - ANA_DEFAULT_WIDTH) {
        x = ana_amiga_hardware_scroll.width - ANA_DEFAULT_WIDTH;
    }

    draw_bitmap = ana_amiga_hardware_scroll_draw_bitmap();
    if (draw_bitmap == NULL) {
        return;
    }

    if (ana_amiga_hardware_scroll.offset_x != x ||
            ana_amiga_hardware_scroll.committed_offset_x != x ||
            ana_amiga_hardware_scroll.draw_index !=
                ana_amiga_hardware_scroll.visible_index ||
            ana_amiga_screen->ViewPort.RasInfo->BitMap !=
                draw_bitmap) {
        ana_amiga_hardware_scroll.view_offset_dirty = 1;
    }
    ana_amiga_hardware_scroll.offset_x = x;
}

static void ana_amiga_hardware_scroll_commit_view_offset(void)
{
    struct BitMap* draw_bitmap;
    int old_visible_index;

    if (!ana_amiga_hardware_scroll.ready || ana_amiga_screen == NULL ||
            ana_amiga_screen->ViewPort.RasInfo == NULL) {
        return;
    }

    draw_bitmap = ana_amiga_hardware_scroll_draw_bitmap();
    if (draw_bitmap == NULL) {
        return;
    }

    if (!ana_amiga_hardware_scroll.view_offset_dirty &&
            ana_amiga_hardware_scroll.committed_offset_x ==
                ana_amiga_hardware_scroll.offset_x &&
            ana_amiga_hardware_scroll.draw_index ==
                ana_amiga_hardware_scroll.visible_index &&
            ana_amiga_screen->ViewPort.RasInfo->BitMap ==
                draw_bitmap) {
        return;
    }

    ana_amiga_hardware_scroll_wait_hud_cache_blit();
    ana_amiga_hardware_scroll_wait_blit();
    old_visible_index = ana_amiga_hardware_scroll.visible_index;
    ana_amiga_visible_bitmap = draw_bitmap;
    ana_amiga_screen->RastPort.BitMap = draw_bitmap;
    ana_amiga_screen->ViewPort.RasInfo->BitMap = draw_bitmap;
    ana_amiga_screen->ViewPort.RasInfo->RxOffset =
        (WORD)ana_amiga_hardware_scroll.offset_x;
    ana_amiga_screen->ViewPort.RasInfo->RyOffset = 0;
    ScrollVPort(&ana_amiga_screen->ViewPort);
    ana_amiga_hardware_scroll.visible_index =
        ana_amiga_hardware_scroll.draw_index;
    ana_amiga_hardware_scroll.committed_offset_x =
        ana_amiga_hardware_scroll.offset_x;
    ana_amiga_hardware_scroll.view_offset_dirty = 0;
    ana_amiga_hardware_scroll.draw_index =
        ana_amiga_hardware_scroll_next_draw_index(
            old_visible_index,
            ana_amiga_hardware_scroll.visible_index);
    ana_amiga_hardware_scroll.rastport.BitMap =
        ana_amiga_hardware_scroll_draw_bitmap();
}

static void ana_amiga_hardware_scroll_deactivate(void)
{
    if (!ana_amiga_hardware_scroll_active() || ana_amiga_screen == NULL ||
            ana_amiga_original_bitmap == NULL) {
        return;
    }

    if (ana_amiga_screen->ViewPort.RasInfo != NULL) {
        ana_amiga_screen->ViewPort.RasInfo->RxOffset = 0;
        ana_amiga_screen->ViewPort.RasInfo->RyOffset = 0;
    }
    ana_amiga_set_screen_bitmap_direct(ana_amiga_original_bitmap);
    ana_amiga_visible_bitmap = ana_amiga_original_bitmap;
    ana_amiga_hardware_scroll.offset_x = 0;
    ana_amiga_hardware_scroll.committed_offset_x = 0;
    ana_amiga_hardware_scroll.view_offset_dirty = 0;
    ana_amiga_hardware_scroll.owner_valid = 0;
}

static void ana_amiga_hardware_scroll_begin_draw(
    const ANA_TileLayer* tile_layer)
{
    if (tile_layer == NULL || !ana_amiga_hardware_scroll.ready) {
        return;
    }

    ana_amiga_hardware_scroll.draw_active = 1;
    ana_amiga_hardware_scroll.draw_offset_x = tile_layer->layer.camera.x;
    ana_amiga_hardware_scroll.draw_offset_y = tile_layer->layer.camera.y;
    ana_amiga_hardware_scroll.draw_clip_active = 0;
}

static void ana_amiga_hardware_scroll_set_draw_clip(ANA_Rect rect)
{
    if (ana_rect_is_empty(rect)) {
        ana_amiga_hardware_scroll.draw_clip_active = 0;
        return;
    }

    ana_amiga_hardware_scroll.draw_clip_active = 1;
    ana_amiga_hardware_scroll.draw_clip_min_x = rect.x;
    ana_amiga_hardware_scroll.draw_clip_min_y = rect.y;
    ana_amiga_hardware_scroll.draw_clip_max_x = rect.x + rect.w;
    ana_amiga_hardware_scroll.draw_clip_max_y = rect.y + rect.h;
}

static int ana_amiga_hardware_scroll_alloc_hud_cache(ANA_Rect viewport)
{
    int plane;

    if (ana_rect_is_empty(viewport) ||
            viewport.y < 0 ||
            viewport.y + viewport.h > ANA_DEFAULT_HEIGHT ||
            viewport.w <= 0 ||
            viewport.x < 0 ||
            viewport.x + viewport.w > ANA_DEFAULT_WIDTH) {
        return 0;
    }

    if (ana_amiga_hardware_scroll.hud_cache_ready &&
            ana_amiga_hardware_scroll.hud_cache_viewport.x == viewport.x &&
            ana_amiga_hardware_scroll.hud_cache_viewport.y == viewport.y &&
            ana_amiga_hardware_scroll.hud_cache_viewport.w == viewport.w &&
            ana_amiga_hardware_scroll.hud_cache_viewport.h == viewport.h) {
        return 1;
    }

    ana_amiga_hardware_scroll_free_hud_cache();
    InitBitMap(
        &ana_amiga_hardware_scroll.hud_cache_bitmap,
        ANA_DEFAULT_BITPLANES,
        ANA_DEFAULT_WIDTH,
        viewport.h);

    for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
        ana_amiga_hardware_scroll.hud_cache_bitmap.Planes[plane] =
            AllocRaster(ANA_DEFAULT_WIDTH, viewport.h);
        if (ana_amiga_hardware_scroll.hud_cache_bitmap.Planes[plane] ==
                NULL) {
            ana_amiga_hardware_scroll.hud_cache_ready = 1;
            ana_amiga_hardware_scroll.hud_cache_viewport = viewport;
            ana_amiga_hardware_scroll_free_hud_cache();
            return 0;
        }
    }

    InitRastPort(&ana_amiga_hardware_scroll.hud_cache_rastport);
    ana_amiga_hardware_scroll.hud_cache_rastport.BitMap =
        &ana_amiga_hardware_scroll.hud_cache_bitmap;
    ana_amiga_hardware_scroll.hud_cache_ready = 1;
    ana_amiga_hardware_scroll.hud_cache_valid = 0;
    ana_amiga_hardware_scroll.hud_cache_blit_pending = 0;
    ana_amiga_hardware_scroll.hud_cache_viewport = viewport;
    ana_amiga_clear_bitmap(&ana_amiga_hardware_scroll.hud_cache_bitmap);

    return 1;
}

static void ana_amiga_hardware_scroll_update_hud_cache(
    unsigned char color,
    int min_x,
    int min_y,
    int max_x,
    int max_y)
{
    ANA_Rect viewport;
    ANA_Rect rect;
    ANA_Rect clipped;

    if (!ana_amiga_active_layer_valid ||
            ana_amiga_active_layer_kind != ANA_LAYER_HUD) {
        return;
    }

    viewport = ana_amiga_active_layer_viewport;
    if (!ana_amiga_hardware_scroll_alloc_hud_cache(viewport)) {
        return;
    }

    rect = ana_rect_make(min_x, min_y, max_x - min_x, max_y - min_y);
    clipped = ana_rect_clip(rect, viewport);
    if (ana_rect_is_empty(clipped)) {
        return;
    }

    ana_amiga_hardware_scroll_wait_hud_cache_blit();
    ana_amiga_hardware_scroll.hud_cache_rastport.BitMap =
        &ana_amiga_hardware_scroll.hud_cache_bitmap;
    SetAPen(&ana_amiga_hardware_scroll.hud_cache_rastport, color);
    RectFill(
        &ana_amiga_hardware_scroll.hud_cache_rastport,
        clipped.x,
        clipped.y - viewport.y,
        clipped.x + clipped.w - 1,
        clipped.y - viewport.y + clipped.h - 1);
    ana_amiga_hardware_scroll.hud_cache_blit_pending = 1;
    ana_amiga_hardware_scroll.hud_cache_valid = 1;
}

static void ana_amiga_hardware_scroll_restore_hud_cache(void)
{
    ANA_Rect viewport;
    struct BitMap* draw_bitmap;

    if (!ana_amiga_hardware_scroll_active() ||
            !ana_amiga_hardware_scroll.hud_cache_ready ||
            !ana_amiga_hardware_scroll.hud_cache_valid) {
        return;
    }

    draw_bitmap = ana_amiga_hardware_scroll_draw_bitmap();
    if (draw_bitmap == NULL) {
        return;
    }

    viewport = ana_amiga_hardware_scroll.hud_cache_viewport;
    ana_amiga_hardware_scroll_wait_hud_cache_blit();
    ana_amiga_hardware_scroll_wait_blit();
    BltBitMap(
        &ana_amiga_hardware_scroll.hud_cache_bitmap,
        viewport.x,
        0,
        draw_bitmap,
        ana_amiga_hardware_scroll.offset_x + viewport.x,
        viewport.y,
        viewport.w,
        viewport.h,
        0xc0,
        0xff,
        NULL);
    ana_amiga_hardware_scroll.blit_pending = 1;
}

static int ana_amiga_hardware_scroll_copy_draw_to_background_rect(
    ANA_Rect rect)
{
    struct BitMap* draw_bitmap;
    ANA_Rect clipped;
    ANA_Rect bounds;

    if (!ana_amiga_hardware_scroll_active() ||
            !ana_amiga_hardware_scroll.background_ready ||
            ana_rect_is_empty(rect)) {
        return 0;
    }

    bounds = ana_rect_make(
        0,
        0,
        ana_amiga_hardware_scroll.width,
        ana_amiga_hardware_scroll.height);
    clipped = ana_rect_clip(rect, bounds);
    if (ana_rect_is_empty(clipped)) {
        return 0;
    }

    draw_bitmap = ana_amiga_hardware_scroll_draw_bitmap();
    if (draw_bitmap == NULL) {
        return 0;
    }

    ana_amiga_hardware_scroll_wait_blit();
    BltBitMap(
        draw_bitmap,
        clipped.x,
        clipped.y,
        &ana_amiga_hardware_scroll.background_bitmap,
        clipped.x,
        clipped.y,
        clipped.w,
        clipped.h,
        0xc0,
        0xff,
        NULL);
    ana_amiga_hardware_scroll.blit_pending = 1;
    return 1;
}

static unsigned char ana_amiga_restore_byte_span_mask(int start_bit, int end_bit)
{
    unsigned char left_mask;
    unsigned char right_mask;

    left_mask = (unsigned char)(0xffu >> start_bit);
    right_mask = end_bit >= 8 ?
        0xffu :
        (unsigned char)(0xffu << (8 - end_bit));

    return (unsigned char)(left_mask & right_mask);
}

static int ana_amiga_hardware_scroll_restore_background_rect_cpu(
    ANA_Rect clipped,
    struct BitMap* draw_bitmap)
{
    unsigned char* source_row;
    unsigned char* dest_row;
    unsigned char* source_byte;
    unsigned char* dest_byte;
    unsigned char mask;
    int area;
    int byte_start;
    int byte_end;
    int byte_index;
    int byte_x;
    int start_bit;
    int end_bit;
    int plane;
    int y;

    if (draw_bitmap == NULL ||
            !ana_amiga_hardware_scroll.background_ready ||
            ana_rect_is_empty(clipped)) {
        return 0;
    }

    area = clipped.w * clipped.h;
    if (area > ANA_AMIGA_RESTORE_CPU_MAX_AREA) {
        return 0;
    }

    byte_start = clipped.x >> 3;
    byte_end = (clipped.x + clipped.w + 7) >> 3;
    if (byte_end <= byte_start) {
        return 0;
    }

    ana_amiga_hardware_scroll_wait_blit();
    for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
        if (draw_bitmap->Planes[plane] == NULL ||
                ana_amiga_hardware_scroll.background_bitmap.Planes[plane] ==
                    NULL) {
            continue;
        }

        for (y = clipped.y; y < clipped.y + clipped.h; y++) {
            source_row =
                ((unsigned char*)
                    ana_amiga_hardware_scroll.background_bitmap.Planes[plane]) +
                ((unsigned long)y *
                    ana_amiga_hardware_scroll.background_bitmap.BytesPerRow);
            dest_row =
                ((unsigned char*)draw_bitmap->Planes[plane]) +
                ((unsigned long)y * draw_bitmap->BytesPerRow);

            for (byte_index = byte_start; byte_index < byte_end; byte_index++) {
                byte_x = byte_index << 3;
                start_bit = clipped.x > byte_x ? clipped.x - byte_x : 0;
                end_bit = clipped.x + clipped.w < byte_x + 8 ?
                    clipped.x + clipped.w - byte_x :
                    8;
                mask = ana_amiga_restore_byte_span_mask(start_bit, end_bit);
                if (mask == 0u) {
                    continue;
                }

                source_byte = source_row + byte_index;
                dest_byte = dest_row + byte_index;
                if (mask == 0xffu) {
                    *dest_byte = *source_byte;
                } else {
                    *dest_byte = (unsigned char)(
                        (*dest_byte & (unsigned char)~mask) |
                        (*source_byte & mask));
                }
            }
        }
    }

    return 1;
}

static int ana_amiga_hardware_scroll_restore_background_rect(ANA_Rect rect)
{
    struct BitMap* draw_bitmap;
    ANA_Rect clipped;
    ANA_Rect bounds;

    if (!ana_amiga_hardware_scroll_active() ||
            !ana_amiga_hardware_scroll.background_ready ||
            ana_rect_is_empty(rect)) {
        return 0;
    }

    bounds = ana_rect_make(
        0,
        0,
        ana_amiga_hardware_scroll.width,
        ana_amiga_hardware_scroll.height);
    clipped = ana_rect_clip(rect, bounds);
    if (ana_rect_is_empty(clipped)) {
        return 0;
    }

    draw_bitmap = ana_amiga_hardware_scroll_draw_bitmap();
    if (draw_bitmap == NULL) {
        return 0;
    }

    if (ana_amiga_hardware_scroll_restore_background_rect_cpu(
            clipped,
            draw_bitmap)) {
        return 1;
    }

    ana_amiga_hardware_scroll_wait_blit();
    BltBitMap(
        &ana_amiga_hardware_scroll.background_bitmap,
        clipped.x,
        clipped.y,
        draw_bitmap,
        clipped.x,
        clipped.y,
        clipped.w,
        clipped.h,
        0xc0,
        0xff,
        NULL);
    ana_amiga_hardware_scroll.blit_pending = 1;
    return 1;
}

static void ana_amiga_hardware_scroll_end_draw(void)
{
    ana_amiga_hardware_scroll.draw_active = 0;
    ana_amiga_hardware_scroll.draw_offset_x = 0;
    ana_amiga_hardware_scroll.draw_offset_y = 0;
    ana_amiga_hardware_scroll.draw_clip_active = 0;
}

static int ana_amiga_hardware_scroll_fill_rect(
    unsigned char color,
    int x,
    int y,
    int width,
    int height)
{
    int start_x;
    int start_y;
    int end_x;
    int end_y;
    int transform_x;
    int transform_y;
    int py;
    int plane;
    int start_byte_x;
    int end_byte_x;
    int middle_start;
    int middle_count;
    unsigned char left_mask;
    unsigned char right_mask;
    unsigned char mask;
    unsigned char fill_byte;
    unsigned char* row;
    unsigned char* pixels;
    int screen_x;
    int screen_y;
    int shadow_start_x;
    int shadow_start_y;
    int shadow_end_x;
    int shadow_end_y;
    int shadow_row;
    struct BitMap* draw_bitmap;

    if (!ana_amiga_hardware_scroll.ready || width <= 0 || height <= 0) {
        return 0;
    }

    draw_bitmap = ana_amiga_hardware_scroll_draw_bitmap();
    if (draw_bitmap == NULL) {
        return 0;
    }

    transform_x = 0;
    transform_y = 0;
    if (ana_amiga_hardware_scroll.draw_active) {
        transform_x = ana_amiga_hardware_scroll.draw_offset_x;
        transform_y = ana_amiga_hardware_scroll.draw_offset_y;
    } else if (ana_amiga_hardware_scroll_active()) {
        transform_x = ana_amiga_hardware_scroll.offset_x;
    } else {
        return 0;
    }

    x += transform_x;
    y += transform_y;

    if (x >= ana_amiga_hardware_scroll.width ||
            y >= ana_amiga_hardware_scroll.height ||
            x + width <= 0 ||
            y + height <= 0) {
        return 1;
    }

    start_x = x < 0 ? 0 : x;
    start_y = y < 0 ? 0 : y;
    end_x = x + width;
    end_y = y + height;

    if (end_x > ana_amiga_hardware_scroll.width) {
        end_x = ana_amiga_hardware_scroll.width;
    }
    if (end_y > ana_amiga_hardware_scroll.height) {
        end_y = ana_amiga_hardware_scroll.height;
    }

    if (ana_amiga_hardware_scroll.draw_clip_active) {
        if (start_x < ana_amiga_hardware_scroll.draw_clip_min_x) {
            start_x = ana_amiga_hardware_scroll.draw_clip_min_x;
        }
        if (start_y < ana_amiga_hardware_scroll.draw_clip_min_y) {
            start_y = ana_amiga_hardware_scroll.draw_clip_min_y;
        }
        if (end_x > ana_amiga_hardware_scroll.draw_clip_max_x) {
            end_x = ana_amiga_hardware_scroll.draw_clip_max_x;
        }
        if (end_y > ana_amiga_hardware_scroll.draw_clip_max_y) {
            end_y = ana_amiga_hardware_scroll.draw_clip_max_y;
        }
    }

    if (start_x >= end_x || start_y >= end_y) {
        return 1;
    }

    color = (unsigned char)(color & 0x0f);

    screen_x = start_x - transform_x;
    screen_y = start_y - transform_y;
    ana_amiga_hardware_scroll_update_hud_cache(
        color,
        screen_x,
        screen_y,
        end_x - transform_x,
        end_y - transform_y);
    /*
     * Screen-space overlays still need a current chunky shadow so any later
     * C2P dirty rect cannot restore stale background over them.
     */
    if ((ana_amiga_hardware_scroll.sync_chunky ||
                !ana_amiga_hardware_scroll.draw_active) &&
            screen_x < ANA_DEFAULT_WIDTH &&
            screen_y < ANA_DEFAULT_HEIGHT &&
            end_x - transform_x > 0 &&
            end_y - transform_y > 0) {
        shadow_start_x = screen_x < 0 ? 0 : screen_x;
        shadow_start_y = screen_y < 0 ? 0 : screen_y;
        shadow_end_x = end_x - transform_x;
        shadow_end_y = end_y - transform_y;
        if (shadow_end_x > ANA_DEFAULT_WIDTH) {
            shadow_end_x = ANA_DEFAULT_WIDTH;
        }
        if (shadow_end_y > ANA_DEFAULT_HEIGHT) {
            shadow_end_y = ANA_DEFAULT_HEIGHT;
        }

        if (shadow_start_x < shadow_end_x &&
                shadow_start_y < shadow_end_y) {
            pixels = ana_framebuffers[ana_draw_buffer];
            for (shadow_row = shadow_start_y;
                    shadow_row < shadow_end_y;
                    shadow_row++) {
                memset(
                    pixels + ((long)shadow_row * ANA_DEFAULT_WIDTH) +
                        shadow_start_x,
                    color,
                    (size_t)(shadow_end_x - shadow_start_x));
            }
        }
    }

    if (ana_amiga_hardware_scroll.rastport_ready &&
            (end_x - start_x) * (end_y - start_y) >
                ANA_AMIGA_HARDWARE_SCROLL_CPU_FILL_MAX_AREA) {
        ana_amiga_hardware_scroll.rastport.BitMap = draw_bitmap;
        SetAPen(&ana_amiga_hardware_scroll.rastport, color);
        RectFill(
            &ana_amiga_hardware_scroll.rastport,
            start_x,
            start_y,
            end_x - 1,
            end_y - 1);
        ana_amiga_hardware_scroll.blit_pending = 1;
        return 1;
    }

    ana_amiga_hardware_scroll_wait_blit();

    start_byte_x = start_x >> 3;
    end_byte_x = (end_x - 1) >> 3;
    left_mask = (unsigned char)(0xffu >> (start_x & 7));
    right_mask = (unsigned char)(0xffu << (7 - ((end_x - 1) & 7)));

    for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
        if (draw_bitmap->Planes[plane] == NULL) {
            continue;
        }

        fill_byte = (color & (1u << plane)) != 0u ? 0xffu : 0x00u;
        for (py = start_y; py < end_y; py++) {
            row = ((unsigned char*)
                draw_bitmap->Planes[plane]) +
                ((unsigned long)py *
                    draw_bitmap->BytesPerRow);

            if (start_byte_x == end_byte_x) {
                mask = (unsigned char)(left_mask & right_mask);
                if (fill_byte != 0u) {
                    row[start_byte_x] =
                        (unsigned char)(row[start_byte_x] | mask);
                } else {
                    row[start_byte_x] =
                        (unsigned char)(row[start_byte_x] & ~mask);
                }
                continue;
            }

            if (fill_byte != 0u) {
                row[start_byte_x] =
                    (unsigned char)(row[start_byte_x] | left_mask);
                row[end_byte_x] =
                    (unsigned char)(row[end_byte_x] | right_mask);
            } else {
                row[start_byte_x] =
                    (unsigned char)(row[start_byte_x] & ~left_mask);
                row[end_byte_x] =
                    (unsigned char)(row[end_byte_x] & ~right_mask);
            }

            middle_start = start_byte_x + 1;
            middle_count = end_byte_x - middle_start;
            if (middle_count > 0) {
                memset(row + middle_start, fill_byte, (size_t)middle_count);
            }
        }
    }

    return 1;
}

#ifdef ANA_AMIGA_DIRECT_PRESENT
#define ANA_AMIGA_IMAGE_BITS8_AT(out, row, row_bytes, width, x) \
    do { \
        unsigned int ana_bits; \
        int ana_byte_x; \
        int ana_shift; \
        int ana_i; \
        (out) = 0u; \
        if ((row) != NULL && (row_bytes) > 0 && (width) > 0) { \
            if ((x) >= 0 && (x) + 7 < (width)) { \
                ana_byte_x = (x) >> 3; \
                ana_shift = (x) & 7; \
                ana_bits = (unsigned int)(row)[ana_byte_x] << 8; \
                if (ana_shift != 0 && ana_byte_x + 1 < (row_bytes)) { \
                    ana_bits |= (unsigned int)(row)[ana_byte_x + 1]; \
                } \
                (out) = (unsigned char)((ana_bits >> (8 - ana_shift)) & \
                    0xffu); \
            } else { \
                for (ana_i = 0; ana_i < 8; ana_i++) { \
                    if ((x) + ana_i < 0 || (x) + ana_i >= (width)) { \
                        continue; \
                    } \
                    if (ana_image_bit_at((row), (row_bytes), \
                            (x) + ana_i, 0)) { \
                        (out) = (unsigned char)((out) | \
                            (0x80u >> ana_i)); \
                    } \
                } \
            } \
        } \
    } while (0)

static unsigned char ana_amiga_byte_span_mask(int start_bit, int end_bit)
{
    unsigned char left_mask;
    unsigned char right_mask;

    left_mask = (unsigned char)(0xffu >> start_bit);
    right_mask = end_bit >= 8 ?
        0xffu :
        (unsigned char)(0xffu << (8 - end_bit));

    return (unsigned char)(left_mask & right_mask);
}

static int ana_amiga_hardware_scroll_draw_image_frame(
    ANA_Image image,
    int frame,
    int x,
    int y)
{
    struct BitMap* draw_bitmap;
    const unsigned char* frame_mask;
    const unsigned char* planes;
    const unsigned char* mask;
    const unsigned char* shifted_plane_rows[ANA_DEFAULT_BITPLANES];
    unsigned char* dest_plane_rows[ANA_DEFAULT_BITPLANES];
    unsigned char* plane_row;
    unsigned char valid_bits;
    unsigned char mask_bits;
    unsigned char src_bits;
    int image_x;
    int image_y;
    int transform_x;
    int transform_y;
    int start_x;
    int start_y;
    int end_x;
    int end_y;
    int dest_x;
    int dest_y;
    int dest_byte_x;
    int dest_start_bit;
    int dest_end_bit;
    int byte_start_x;
    int byte_end_x;
    int byte_count;
    int byte_index;
    int byte_src_x[ANA_AMIGA_IMAGE_MAX_DEST_BYTES];
    unsigned char byte_valid_bits[ANA_AMIGA_IMAGE_MAX_DEST_BYTES];
    int screen_x;
    int screen_y;
    int src_x;
    int src_y;
    int area;
    int plane;
    long shifted_offset;

    if (!ana_amiga_hardware_scroll_active() ||
            image == NULL ||
            frame < 0 ||
            frame >= image->frame_count) {
        return 0;
    }

    if (ana_amiga_active_layer_valid &&
            ana_amiga_active_layer_kind != ANA_LAYER_SPRITES) {
        return 0;
    }

    planes = ana_image_planes_base(image, frame);
    if (planes == NULL) {
        return 0;
    }

    draw_bitmap = ana_amiga_hardware_scroll_draw_bitmap();
    if (draw_bitmap == NULL) {
        return 0;
    }

    image_x = x;
    image_y = y;
    transform_x = ana_amiga_hardware_scroll.draw_active ?
        ana_amiga_hardware_scroll.draw_offset_x :
        ana_amiga_hardware_scroll.offset_x;
    transform_y = ana_amiga_hardware_scroll.draw_active ?
        ana_amiga_hardware_scroll.draw_offset_y :
        0;

    if (ana_amiga_hardware_scroll.draw_active) {
        x += transform_x;
        y += transform_y;

        if (x >= ana_amiga_hardware_scroll.width ||
                y >= ana_amiga_hardware_scroll.height ||
                x + image->width <= 0 ||
                y + image->height <= 0) {
            return 1;
        }

        start_x = x < 0 ? 0 : x;
        start_y = y < 0 ? 0 : y;
        end_x = x + image->width;
        end_y = y + image->height;

        if (end_x > ana_amiga_hardware_scroll.width) {
            end_x = ana_amiga_hardware_scroll.width;
        }
        if (end_y > ana_amiga_hardware_scroll.height) {
            end_y = ana_amiga_hardware_scroll.height;
        }
    } else {
        if (x >= ANA_DEFAULT_WIDTH ||
                y >= ANA_DEFAULT_HEIGHT ||
                x + image->width <= 0 ||
                y + image->height <= 0) {
            return 1;
        }

        screen_x = x < 0 ? 0 : x;
        screen_y = y < 0 ? 0 : y;
        start_x = screen_x + transform_x;
        start_y = screen_y;
        end_x = x + image->width;
        end_y = y + image->height;
        if (end_x > ANA_DEFAULT_WIDTH) {
            end_x = ANA_DEFAULT_WIDTH;
        }
        if (end_y > ANA_DEFAULT_HEIGHT) {
            end_y = ANA_DEFAULT_HEIGHT;
        }
        end_x += transform_x;
    }

    if (ana_amiga_hardware_scroll.draw_clip_active) {
        if (start_x < ana_amiga_hardware_scroll.draw_clip_min_x) {
            start_x = ana_amiga_hardware_scroll.draw_clip_min_x;
        }
        if (start_y < ana_amiga_hardware_scroll.draw_clip_min_y) {
            start_y = ana_amiga_hardware_scroll.draw_clip_min_y;
        }
        if (end_x > ana_amiga_hardware_scroll.draw_clip_max_x) {
            end_x = ana_amiga_hardware_scroll.draw_clip_max_x;
        }
        if (end_y > ana_amiga_hardware_scroll.draw_clip_max_y) {
            end_y = ana_amiga_hardware_scroll.draw_clip_max_y;
        }
    }

    if (start_x >= end_x || start_y >= end_y) {
        return 1;
    }

    src_x = start_x - image_x - transform_x;
    src_y = start_y - image_y - transform_y;
    area = (end_x - start_x) * (end_y - start_y);
    if (ana_amiga_image_should_use_cpu_shifted(area) &&
            start_x == image_x + transform_x &&
            start_y == image_y + transform_y &&
            end_x - start_x == image->width &&
            end_y - start_y == image->height &&
            ana_amiga_image_prepare_shifted(image)) {
        ana_amiga_hardware_scroll_wait_blit();
        byte_start_x = start_x >> 3;
        byte_count = ((start_x & 7) + image->width + 7) >> 3;
        for (dest_y = start_y; dest_y < end_y; dest_y++) {
            src_y = dest_y - start_y;
            shifted_offset = ana_amiga_image_shifted_frame_offset(
                image,
                frame,
                start_x & 7) +
                ((long)src_y * image->amiga_shifted_row_bytes);
            mask = image->amiga_shifted_masks + shifted_offset;

            for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
                if (draw_bitmap->Planes[plane] != NULL) {
                    dest_plane_rows[plane] =
                        ((unsigned char*)draw_bitmap->Planes[plane]) +
                        ((unsigned long)dest_y * draw_bitmap->BytesPerRow) +
                        byte_start_x;
                } else {
                    dest_plane_rows[plane] = NULL;
                }

                shifted_plane_rows[plane] =
                    image->amiga_shifted_planes +
                    ana_amiga_image_shifted_plane_offset(
                        image,
                        frame,
                        start_x & 7,
                        plane) +
                    ((long)src_y * image->amiga_shifted_row_bytes);
            }

            for (byte_index = 0; byte_index < byte_count; byte_index++) {
                mask_bits = mask[byte_index];
                if (mask_bits == 0u) {
                    continue;
                }

                for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
                    if (dest_plane_rows[plane] == NULL) {
                        continue;
                    }

                    plane_row = dest_plane_rows[plane] + byte_index;
                    if (plane < image->bitplanes) {
                        src_bits = shifted_plane_rows[plane][byte_index];
                    } else {
                        src_bits = 0u;
                    }

                    if (mask_bits == 0xffu) {
                        *plane_row = src_bits;
                    } else {
                        *plane_row = (unsigned char)(
                            (*plane_row & (unsigned char)~mask_bits) |
                            (src_bits & mask_bits));
                    }
                }
            }
        }
        return 1;
    }

    if (ana_amiga_image_should_use_native_blitter(area) &&
            ana_amiga_image_prepare_native(image)) {
        ana_amiga_hardware_scroll_wait_blit();
        if (ana_image_has_mask(image) &&
                image->amiga_masks != NULL &&
                image->amiga_masks[frame] != NULL &&
                ana_amiga_hardware_scroll.rastport_ready) {
            ana_amiga_hardware_scroll.rastport.BitMap = draw_bitmap;
            BltMaskBitMapRastPort(
                &image->amiga_bitmaps[frame],
                src_x,
                src_y,
                &ana_amiga_hardware_scroll.rastport,
                start_x,
                start_y,
                end_x - start_x,
                end_y - start_y,
                ANA_AMIGA_COOKIE_CUT_MINTERM,
                image->amiga_masks[frame]);
            ana_amiga_hardware_scroll.blit_pending = 1;
            return 1;
        }

        if (!ana_image_has_mask(image)) {
            BltBitMap(
                &image->amiga_bitmaps[frame],
                src_x,
                src_y,
                draw_bitmap,
                start_x,
                start_y,
                end_x - start_x,
                end_y - start_y,
                0xc0,
                0xff,
                NULL);
            ana_amiga_hardware_scroll.blit_pending = 1;
            return 1;
        }
    }

    frame_mask = ana_image_mask_base(image, frame);
    ana_amiga_hardware_scroll_wait_blit();

    byte_start_x = start_x >> 3;
    byte_end_x = (end_x + 7) >> 3;
    byte_count = byte_end_x - byte_start_x;
    if (byte_count <= 0) {
        return 1;
    }
    if (byte_count > ANA_AMIGA_IMAGE_MAX_DEST_BYTES) {
        return 0;
    }

    for (byte_index = 0; byte_index < byte_count; byte_index++) {
        dest_x = (byte_start_x + byte_index) << 3;
        dest_start_bit = start_x > dest_x ? start_x - dest_x : 0;
        dest_end_bit = end_x < dest_x + 8 ? end_x - dest_x : 8;
        byte_valid_bits[byte_index] =
            ana_amiga_byte_span_mask(dest_start_bit, dest_end_bit);
        byte_src_x[byte_index] = dest_x - image_x - transform_x;
    }

    for (dest_y = start_y; dest_y < end_y; dest_y++) {
        src_y = dest_y - image_y - transform_y;
        mask = frame_mask != NULL ?
            frame_mask + ((long)src_y * image->row_bytes) :
            NULL;

        for (byte_index = 0; byte_index < byte_count; byte_index++) {
            dest_byte_x = byte_start_x + byte_index;
            valid_bits = byte_valid_bits[byte_index];
            src_x = byte_src_x[byte_index];

            if (mask != NULL) {
                ANA_AMIGA_IMAGE_BITS8_AT(
                    mask_bits,
                    mask,
                    image->row_bytes,
                    image->width,
                    src_x);
            } else {
                mask_bits = 0xffu;
            }
            mask_bits = (unsigned char)(mask_bits & valid_bits);
            if (mask_bits == 0u) {
                continue;
            }

            for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
                if (draw_bitmap->Planes[plane] == NULL) {
                    continue;
                }

                if (plane < image->bitplanes) {
                    ANA_AMIGA_IMAGE_BITS8_AT(
                        src_bits,
                        planes +
                            ((long)plane * image->plane_size) +
                            ((long)src_y * image->row_bytes),
                        image->row_bytes,
                        image->width,
                        src_x);
                } else {
                    src_bits = 0u;
                }

                plane_row = ((unsigned char*)draw_bitmap->Planes[plane]) +
                    ((unsigned long)dest_y * draw_bitmap->BytesPerRow) +
                    dest_byte_x;
                *plane_row = (unsigned char)(
                    (*plane_row & (unsigned char)~mask_bits) |
                    (src_bits & mask_bits));
            }
        }
    }

    return 1;
}
#endif

static void ana_amiga_hardware_scroll_copy_draw_to_other_buffer(void)
{
    int buffer;
    struct BitMap* draw_bitmap;
    struct BitMap* other_bitmap;

    if (!ana_amiga_hardware_scroll.ready) {
        return;
    }

    draw_bitmap = ana_amiga_hardware_scroll_draw_bitmap();
    if (draw_bitmap == NULL) {
        return;
    }

    ana_amiga_hardware_scroll_wait_blit();
    for (buffer = 0;
            buffer < ANA_AMIGA_HARDWARE_SCROLL_BUFFER_COUNT;
            buffer++) {
        if (buffer == ana_amiga_hardware_scroll.draw_index) {
            continue;
        }

        other_bitmap = ana_amiga_hardware_scroll_bitmap_at(buffer);
        if (other_bitmap == NULL) {
            continue;
        }

        BltBitMap(
            draw_bitmap,
            0,
            0,
            other_bitmap,
            0,
            0,
            ana_amiga_hardware_scroll.width,
            ana_amiga_hardware_scroll.height,
            0xc0,
            0xff,
            NULL);
        ana_amiga_hardware_scroll.blit_pending = 1;
        ana_amiga_hardware_scroll_wait_blit();
    }
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
    window.IDCMPFlags = IDCMP_RAWKEY | IDCMP_VANILLAKEY;
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
    ana_amiga_hardware_scroll_reset_state();

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
    WindowToFront(ana_amiga_window);
    ActivateWindow(ana_amiga_window);
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

    ana_amiga_hardware_scroll_free();
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

static void ana_amiga_copy_chunky_rect_to_bitplanes_offset(
    struct BitMap* bitmap,
    const unsigned char* chunky,
    int min_x,
    int min_y,
    int max_x,
    int max_y,
    int dest_x_offset)
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
    int plane;
    int dest_x;
    int dest_byte_x;
    int dest_byte_count;
    int bit_shift;
    int dest_width;
    unsigned long packed;
    const unsigned long* lut0;
    const unsigned long* lut1;
    const unsigned long* lut2;
    const unsigned long* lut3;
    unsigned char plane_byte;
    unsigned char first_mask;
    unsigned char second_mask;
    unsigned char first_bits;
    unsigned char second_bits;
    unsigned char* out;
    const unsigned char* in;
    unsigned long row_offset;

    if (dest_x_offset == 0) {
        ana_amiga_copy_chunky_rect_to_bitplanes(
            bitmap,
            chunky,
            min_x,
            min_y,
            max_x,
            max_y);
        return;
    }

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

    dest_width = ana_amiga_hardware_scroll_owns_bitmap(bitmap) ?
        ana_amiga_hardware_scroll.width :
        ANA_DEFAULT_WIDTH;
    if (dest_x_offset < 0 ||
            min_x + dest_x_offset < 0 ||
            max_x + dest_x_offset > dest_width) {
        return;
    }

    start_byte_x = min_x / 8;
    end_byte_x = (max_x + 7) / 8;
    byte_count = end_byte_x - start_byte_x;
    dest_byte_count = (dest_width + 7) / 8;
    if (byte_count <= 0 || dest_byte_count <= 0) {
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

        for (byte_x = 0; byte_x < byte_count; byte_x++) {
            dest_x = ((start_byte_x + byte_x) * 8) + dest_x_offset;
            dest_byte_x = dest_x >> 3;
            bit_shift = dest_x & 7;

            if (dest_byte_x < 0 || dest_byte_x >= dest_byte_count) {
                in += 8;
                continue;
            }

            index0 = ((in[0] & 0x0f) << 4) | (in[1] & 0x0f);
            index1 = ((in[2] & 0x0f) << 4) | (in[3] & 0x0f);
            index2 = ((in[4] & 0x0f) << 4) | (in[5] & 0x0f);
            index3 = ((in[6] & 0x0f) << 4) | (in[7] & 0x0f);
            packed =
                lut0[index0] |
                lut1[index1] |
                lut2[index2] |
                lut3[index3];

            for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
                if (bitmap->Planes[plane] == NULL) {
                    continue;
                }

                plane_byte = (unsigned char)(
                    (packed >> (plane * 8)) & 0xffu);
                out = ((unsigned char*)bitmap->Planes[plane]) +
                    row_offset + dest_byte_x;

                if (bit_shift == 0) {
                    out[0] = plane_byte;
                } else {
                    first_mask = (unsigned char)(0xffu >> bit_shift);
                    second_mask = (unsigned char)(
                        0xffu << (8 - bit_shift));
                    first_bits = (unsigned char)(plane_byte >> bit_shift);
                    second_bits = (unsigned char)(
                        plane_byte << (8 - bit_shift));

                    out[0] = (unsigned char)(
                        (out[0] & (unsigned char)~first_mask) |
                        (first_bits & first_mask));
                    if (dest_byte_x + 1 < dest_byte_count) {
                        out[1] = (unsigned char)(
                            (out[1] & (unsigned char)~second_mask) |
                            (second_bits & second_mask));
                    }
                }
            }

            in += 8;
        }
    }
}

static void ana_amiga_present_buffer(const unsigned char* chunky)
{
    struct BitMap* next_visible;
    struct BitMap* previous_visible;
    struct ANA_AmigaBitmapState* next_state;
#ifdef ANA_DEBUG_STATS
    unsigned long total_start;
    unsigned long stage_start;
#endif
    int i;

    if (ana_amiga_screen == NULL) {
        return;
    }

#ifdef ANA_DEBUG_STATS
    total_start = ana_platform_perf_ticks();
#endif

#ifdef ANA_AMIGA_DIRECT_PRESENT_SYNC
#ifdef ANA_DEBUG_STATS
    stage_start = ana_platform_perf_ticks();
#endif
    WaitTOF();
#ifdef ANA_DEBUG_STATS
    ana_gfx_record_perf_ticks(
        &ana_gfx_stats.present_flip_perf_ticks,
        stage_start,
        ana_platform_perf_ticks());
#endif
#endif

#ifdef ANA_AMIGA_DIRECT_PRESENT
    if (ana_amiga_hardware_scroll_active()) {
        next_visible = ana_amiga_hardware_scroll_draw_bitmap();
    } else {
        next_visible = ana_amiga_visible_bitmap;
    }
    previous_visible = ana_amiga_visible_bitmap;
#else
    next_visible = ana_amiga_draw_bitmap;
    previous_visible = ana_amiga_visible_bitmap;
#endif

    if (next_visible == NULL || previous_visible == NULL) {
        return;
    }

    ana_amiga_hardware_scroll_wait_blit();

    next_state = ana_amiga_bitmap_state_for(next_visible);

#ifdef ANA_DEBUG_STATS
    stage_start = ana_platform_perf_ticks();
#endif
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
#ifdef ANA_DEBUG_STATS
    ana_gfx_record_perf_ticks(
        &ana_gfx_stats.present_clear_perf_ticks,
        stage_start,
        ana_platform_perf_ticks());
#endif

#ifdef ANA_DEBUG_STATS
    stage_start = ana_platform_perf_ticks();
#endif
    ana_gfx_record_present_dirty_rects(
        ana_amiga_dirty_rects,
        ana_amiga_dirty_count);

    for (i = 0; i < ana_amiga_dirty_count; i++) {
        ana_amiga_copy_chunky_rect_to_bitplanes_offset(
            next_visible,
            chunky,
            ana_amiga_dirty_rects[i].min_x,
            ana_amiga_dirty_rects[i].min_y,
            ana_amiga_dirty_rects[i].max_x,
            ana_amiga_dirty_rects[i].max_y,
            ana_amiga_hardware_scroll_dest_offset_x());
    }
#ifdef ANA_DEBUG_STATS
    ana_gfx_record_perf_ticks(
        &ana_gfx_stats.present_convert_perf_ticks,
        stage_start,
        ana_platform_perf_ticks());
#endif

    ana_amiga_store_bitmap_dirty_state(next_state);

#ifdef ANA_DEBUG_STATS
    stage_start = ana_platform_perf_ticks();
#endif
    ana_amiga_hardware_scroll_commit_view_offset();
#ifdef ANA_AMIGA_DIRECT_PRESENT
#ifndef ANA_AMIGA_DIRECT_PRESENT_SYNC
#ifdef ANA_DEBUG_STATS
    ana_gfx_stats.direct_flips++;
#endif
#endif
#else
    if (!ana_amiga_set_screen_bitmap(next_visible)) {
#ifdef ANA_DEBUG_STATS
        ana_gfx_stats.direct_flips++;
#endif
        WaitTOF();
    } else {
#ifdef ANA_DEBUG_STATS
        ana_gfx_stats.screen_buffer_flips++;
#endif
    }
#endif
#ifndef ANA_AMIGA_DIRECT_PRESENT_SYNC
#ifdef ANA_DEBUG_STATS
    ana_gfx_record_perf_ticks(
        &ana_gfx_stats.present_flip_perf_ticks,
        stage_start,
        ana_platform_perf_ticks());
#endif
#endif
#ifdef ANA_AMIGA_DIRECT_PRESENT_SYNC
#ifdef ANA_DEBUG_STATS
    ana_gfx_stats.direct_flips++;
#endif
#endif

#ifndef ANA_AMIGA_DIRECT_PRESENT
    ana_amiga_visible_bitmap = next_visible;
    ana_amiga_draw_bitmap = previous_visible;
#endif
    ana_amiga_reset_frame_state();
#ifdef ANA_DEBUG_STATS
    ana_gfx_record_perf_ticks(
        &ana_gfx_stats.present_total_perf_ticks,
        total_start,
        ana_platform_perf_ticks());
#endif
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
    ana_gfx_render_mode = profile->render_mode;

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

ANA_RenderMode ana_layer_default_render_mode(ANA_LayerKind kind)
{
    switch (kind) {
    case ANA_LAYER_SIDE_SCROLL:
        return ANA_RENDER_SIDE_SCROLL;
    case ANA_LAYER_VERTICAL_SCROLL:
        return ANA_RENDER_VERTICAL_SCROLL;
    case ANA_LAYER_TILE_4WAY:
        return ANA_RENDER_TILE_4WAY;
    case ANA_LAYER_RAYCAST_VIEW:
        return ANA_RENDER_RAYCAST;
    case ANA_LAYER_PARALLAX:
        return ANA_RENDER_TILE_SCROLL;
    case ANA_LAYER_SPRITES:
        return ANA_RENDER_BLITTER_BOBS;
    case ANA_LAYER_MENU:
        return ANA_RENDER_FULL_FRAME;
    case ANA_LAYER_STATIC:
    case ANA_LAYER_HUD:
    case ANA_LAYER_DEBUG:
    default:
        return ANA_RENDER_DIRTY;
    }
}

void ana_layer_init(ANA_Layer* layer, ANA_LayerKind kind, int z_order)
{
    if (layer == NULL) {
        return;
    }

    memset(layer, 0, sizeof(*layer));
    layer->kind = kind;
    layer->render_mode = ana_layer_default_render_mode(kind);
    layer->z_order = z_order;
}

void ana_layer_set_viewport(ANA_Layer* layer, ANA_Rect viewport)
{
    if (layer == NULL) {
        return;
    }

    layer->viewport = viewport;
}

ANA_Rect ana_layer_viewport(const ANA_Layer* layer)
{
    if (layer == NULL) {
        return ana_rect_make(0, 0, 0, 0);
    }

    return layer->viewport;
}

void ana_layer_set_camera(ANA_Layer* layer, const ANA_Camera* camera)
{
    if (layer == NULL || camera == NULL) {
        return;
    }

    layer->camera = *camera;
}

ANA_Camera ana_layer_camera(const ANA_Layer* layer)
{
    ANA_Camera camera;

    if (layer == NULL) {
        ana_camera_init(&camera, 0, 0, 0, 0, 0, 0);
        return camera;
    }

    return layer->camera;
}

void ana_layer_set_redraw(
    ANA_Layer* layer,
    ANA_RedrawCallback redraw,
    void* user_data)
{
    if (layer == NULL) {
        return;
    }

    layer->redraw = redraw;
    layer->user_data = user_data;
}

void ana_layer_set_enabled(ANA_Layer* layer, int enabled)
{
    if (layer == NULL) {
        return;
    }

    layer->disabled = enabled ? 0 : ANA_LAYER_DISABLED_VALUE;
}

int ana_layer_is_enabled(const ANA_Layer* layer)
{
    return layer != NULL && layer->disabled != ANA_LAYER_DISABLED_VALUE;
}

int ana_layer_is_dirty(const ANA_Layer* layer)
{
    return layer != NULL && layer->dirty;
}

void ana_layer_mark_dirty(ANA_Layer* layer)
{
    if (layer == NULL) {
        return;
    }

    layer->dirty = 1;
}

void ana_layer_draw_if_dirty(ANA_Layer* layer)
{
    ANA_Rect rect;
#ifdef ANA_TARGET_AMIGA
    int previous_layer_valid;
    ANA_LayerKind previous_layer_kind;
    ANA_Rect previous_layer_viewport;
#endif

    if (layer == NULL ||
            layer->disabled == ANA_LAYER_DISABLED_VALUE ||
            !layer->dirty) {
        return;
    }

    if (layer->redraw != NULL) {
        rect = ana_rect_make(0, 0, 0, 0);
#ifdef ANA_TARGET_AMIGA
        previous_layer_valid = ana_amiga_active_layer_valid;
        previous_layer_kind = ana_amiga_active_layer_kind;
        previous_layer_viewport = ana_amiga_active_layer_viewport;
        ana_amiga_active_layer_valid = 1;
        ana_amiga_active_layer_kind = layer->kind;
        ana_amiga_active_layer_viewport = layer->viewport;
#endif
        layer->redraw(rect, layer->user_data);
#ifdef ANA_TARGET_AMIGA
        ana_amiga_active_layer_valid = previous_layer_valid;
        ana_amiga_active_layer_kind = previous_layer_kind;
        ana_amiga_active_layer_viewport = previous_layer_viewport;
#endif
    }

    layer->dirty = 0;
}

static ANA_Rect ana_tile_layer_viewport_bounds(const ANA_TileLayer* tile_layer)
{
    ANA_Rect viewport;

    viewport = tile_layer->layer.viewport;
    if (ana_rect_is_empty(viewport)) {
        viewport = ana_rect_make(
            tile_layer->layer.camera.view_x,
            tile_layer->layer.camera.view_y,
            tile_layer->layer.camera.view_w,
            tile_layer->layer.camera.view_h);
    }

    return viewport;
}

static ANA_Rect ana_tile_layer_world_bounds(const ANA_TileLayer* tile_layer)
{
    return ana_rect_make(
        0,
        0,
        tile_layer->map_width * tile_layer->tile_width,
        tile_layer->map_height * tile_layer->tile_height);
}

static void ana_tile_layer_draw_tiles_in_world_rect(
    ANA_TileLayer* tile_layer,
    ANA_Rect world_rect)
{
    ANA_Rect clipped;
    ANA_Rect world_bounds;
    ANA_Rect viewport;
    int tx0;
    int ty0;
    int tx1;
    int ty1;
    int tx;
    int ty;
    int sx;
    int sy;
    unsigned char tile;

    if (tile_layer == NULL ||
            tile_layer->read_tile == NULL ||
            tile_layer->draw_tile == NULL ||
            tile_layer->tile_width <= 0 ||
            tile_layer->tile_height <= 0) {
        return;
    }

    viewport = ana_tile_layer_viewport_bounds(tile_layer);
    world_bounds = ana_tile_layer_world_bounds(tile_layer);
    clipped = ana_rect_clip(world_rect, world_bounds);
    if (ana_rect_is_empty(clipped)) {
        return;
    }

    tx0 = clipped.x / tile_layer->tile_width;
    ty0 = clipped.y / tile_layer->tile_height;
    tx1 = (clipped.x + clipped.w - 1) / tile_layer->tile_width;
    ty1 = (clipped.y + clipped.h - 1) / tile_layer->tile_height;

    if (tx0 < 0) {
        tx0 = 0;
    }
    if (ty0 < 0) {
        ty0 = 0;
    }
    if (tx1 >= tile_layer->map_width) {
        tx1 = tile_layer->map_width - 1;
    }
    if (ty1 >= tile_layer->map_height) {
        ty1 = tile_layer->map_height - 1;
    }

    for (ty = ty0; ty <= ty1; ty++) {
        for (tx = tx0; tx <= tx1; tx++) {
            tile = tile_layer->read_tile(tx, ty, tile_layer->user_data);
            if (tile != 0u) {
                sx = tx * tile_layer->tile_width -
                    tile_layer->layer.camera.x +
                    viewport.x;
                sy = ty * tile_layer->tile_height -
                    tile_layer->layer.camera.y +
                    viewport.y;
                tile_layer->draw_tile(tile, sx, sy, tile_layer->user_data);
            }
        }
    }
}

static void ana_tile_layer_clear_world_rect(
    ANA_TileLayer* tile_layer,
    ANA_Rect world_rect)
{
    ANA_Rect screen_rect;
    ANA_Rect clipped;
    ANA_Rect viewport;

    if (tile_layer == NULL) {
        return;
    }

    screen_rect = ana_camera_world_to_screen_rect(
        &tile_layer->layer.camera,
        world_rect);
    viewport = ana_tile_layer_viewport_bounds(tile_layer);
    clipped = ana_rect_clip(screen_rect, viewport);

    if (!ana_rect_is_empty(clipped)) {
        ana_fill_rect(
            tile_layer->clear_color,
            clipped.x,
            clipped.y,
            clipped.w,
            clipped.h);
    }
}

static void ana_tile_layer_redraw_exposed_scroll_strips(
    ANA_TileLayer* tile_layer,
    int screen_dx,
    int screen_dy)
{
    ANA_Camera* camera;
    ANA_Rect strip;

    camera = &tile_layer->layer.camera;

    if (screen_dx < 0) {
        strip = ana_rect_make(
            camera->x + camera->view_w + screen_dx,
            camera->y,
            -screen_dx,
            camera->view_h);
        ana_tile_layer_redraw_world_rect(tile_layer, strip);
    } else if (screen_dx > 0) {
        strip = ana_rect_make(
            camera->x,
            camera->y,
            screen_dx,
            camera->view_h);
        ana_tile_layer_redraw_world_rect(tile_layer, strip);
    }

    if (screen_dy < 0) {
        strip = ana_rect_make(
            camera->x,
            camera->y + camera->view_h + screen_dy,
            camera->view_w,
            -screen_dy);
        ana_tile_layer_redraw_world_rect(tile_layer, strip);
    } else if (screen_dy > 0) {
        strip = ana_rect_make(
            camera->x,
            camera->y,
            camera->view_w,
            screen_dy);
        ana_tile_layer_redraw_world_rect(tile_layer, strip);
    }
}

static int ana_tile_layer_can_use_native_scroll(const ANA_TileLayer* tile_layer)
{
#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_NATIVE_SCROLL_ENABLED)
    if (tile_layer == NULL) {
        return 0;
    }

    switch (tile_layer->layer.kind) {
    case ANA_LAYER_SIDE_SCROLL:
    case ANA_LAYER_VERTICAL_SCROLL:
    case ANA_LAYER_PARALLAX:
    case ANA_LAYER_TILE_4WAY:
        break;
    default:
        return 0;
    }

    if (ana_amiga_visible_bitmap == NULL) {
        return 0;
    }

    return 1;
#else
    (void)tile_layer;
#endif

    return 0;
}

static int ana_tile_layer_should_request_native_scroll(
    const ANA_TileLayer* tile_layer)
{
    if (tile_layer == NULL ||
            tile_layer->scroll_backend != ANA_SCROLL_BACKEND_NATIVE) {
        return 0;
    }

    /*
     * Keep the visible-bitmap bridge explicit. It is a useful diagnostic path,
     * but it is not the dedicated planar hardware-scroll backend and it is too
     * sensitive to redraw ordering to sit behind AUTO or HARDWARE.
     */
    return ana_tile_layer_can_use_native_scroll(tile_layer);
}

#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_NATIVE_SCROLL_ENABLED)
static int ana_tile_layer_native_scroll_sync_chunky(
    const ANA_TileLayer* tile_layer)
{
    if (tile_layer == NULL) {
        return 1;
    }

    switch (tile_layer->scroll_sync) {
    case ANA_SCROLL_SYNC_DIRTY:
        return 0;
    case ANA_SCROLL_SYNC_CHUNKY:
        return 1;
    case ANA_SCROLL_SYNC_AUTO:
    default:
        return 1;
    }
}
#endif

static int ana_tile_layer_can_use_hardware_scroll(
    const ANA_TileLayer* tile_layer)
{
#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_DIRECT_PRESENT)
    ANA_Rect viewport;
    int world_w;

    if (tile_layer == NULL ||
            ana_amiga_screen == NULL ||
            ana_amiga_screen->ViewPort.RasInfo == NULL ||
            tile_layer->read_tile == NULL ||
            tile_layer->draw_tile == NULL ||
            tile_layer->tile_width <= 0 ||
            tile_layer->tile_height <= 0 ||
            tile_layer->map_width <= 0 ||
            tile_layer->map_height <= 0 ||
            tile_layer->layer.kind != ANA_LAYER_SIDE_SCROLL) {
        return 0;
    }

    viewport = ana_tile_layer_viewport_bounds(tile_layer);
    if (viewport.x != 0 ||
            viewport.y < 0 ||
            viewport.w != ANA_DEFAULT_WIDTH ||
            viewport.h <= 0 ||
            viewport.y + viewport.h > ANA_DEFAULT_HEIGHT ||
            tile_layer->layer.camera.y != 0) {
        return 0;
    }

    world_w = tile_layer->map_width * tile_layer->tile_width;
    if (world_w < ANA_DEFAULT_WIDTH ||
            world_w > ANA_AMIGA_HARDWARE_SCROLL_MAX_WIDTH ||
            world_w + viewport.x > ANA_AMIGA_HARDWARE_SCROLL_MAX_WIDTH) {
        return 0;
    }

    return 1;
#else
    (void)tile_layer;
#endif

    return 0;
}

static int ana_tile_layer_should_request_hardware_scroll(
    const ANA_TileLayer* tile_layer)
{
    if (tile_layer == NULL ||
            (tile_layer->scroll_backend != ANA_SCROLL_BACKEND_AUTO &&
                tile_layer->scroll_backend != ANA_SCROLL_BACKEND_HARDWARE)) {
        return 0;
    }

    return ana_tile_layer_can_use_hardware_scroll(tile_layer);
}

#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_DIRECT_PRESENT)
static void ana_tile_layer_clear_hardware_world_rect(
    ANA_TileLayer* tile_layer,
    ANA_Rect world_rect)
{
    ANA_Rect viewport;

    if (tile_layer == NULL || ana_rect_is_empty(world_rect)) {
        return;
    }

    viewport = ana_tile_layer_viewport_bounds(tile_layer);
    ana_fill_rect(
        tile_layer->clear_color,
        world_rect.x - tile_layer->layer.camera.x + viewport.x,
        world_rect.y - tile_layer->layer.camera.y + viewport.y,
        world_rect.w,
        world_rect.h);
}

static void ana_tile_layer_redraw_hardware_world_rect(
    ANA_TileLayer* tile_layer,
    ANA_Rect world_rect)
{
    ANA_Rect world;
    ANA_Rect viewport;
    ANA_Rect clipped;
    ANA_Rect bitmap_clip;

    if (tile_layer == NULL || !ana_amiga_hardware_scroll_active()) {
        return;
    }

    world = ana_tile_layer_world_bounds(tile_layer);
    clipped = ana_rect_clip(world_rect, world);
    if (ana_rect_is_empty(clipped)) {
        return;
    }

    viewport = ana_tile_layer_viewport_bounds(tile_layer);
    bitmap_clip = ana_rect_make(
        clipped.x + viewport.x,
        clipped.y + viewport.y,
        clipped.w,
        clipped.h);

    ana_amiga_hardware_scroll_begin_draw(tile_layer);
    ana_amiga_hardware_scroll_set_draw_clip(bitmap_clip);
    ana_tile_layer_clear_hardware_world_rect(tile_layer, clipped);
    ana_tile_layer_draw_tiles_in_world_rect(tile_layer, clipped);
    ana_amiga_hardware_scroll_end_draw();
    ana_amiga_hardware_scroll_copy_draw_to_background_rect(bitmap_clip);
}

static int ana_tile_layer_draw_hardware_scroll(ANA_TileLayer* tile_layer)
{
    ANA_Rect world;
    int redraw_all;

    if (!ana_tile_layer_can_use_hardware_scroll(tile_layer)) {
        ana_amiga_hardware_scroll_deactivate();
        return 0;
    }

    redraw_all = tile_layer->previous_camera_x < 0 ||
        tile_layer->previous_camera_y < 0 ||
        tile_layer->layer.dirty ||
        !ana_amiga_hardware_scroll_matches(tile_layer);

    if (!ana_amiga_hardware_scroll_alloc(tile_layer)) {
        ana_amiga_hardware_scroll_deactivate();
        return 0;
    }

    ana_amiga_hardware_scroll_set_view_offset(tile_layer->layer.camera.x);

    if (redraw_all) {
        ana_amiga_clear_bitmap(ana_amiga_hardware_scroll_draw_bitmap());
        world = ana_tile_layer_world_bounds(tile_layer);
        ana_amiga_hardware_scroll_begin_draw(tile_layer);
        ana_tile_layer_draw_tiles_in_world_rect(tile_layer, world);
        ana_amiga_hardware_scroll_end_draw();
        ana_amiga_hardware_scroll_copy_draw_to_background_rect(
            ana_rect_make(
                0,
                0,
                ana_amiga_hardware_scroll.width,
                ana_amiga_hardware_scroll.height));
        ana_amiga_hardware_scroll_copy_draw_to_other_buffer();
    }

    ana_amiga_hardware_scroll.sync_chunky =
        tile_layer->scroll_sync == ANA_SCROLL_SYNC_DIRTY ? 0 : 1;
    ana_amiga_hardware_scroll_restore_hud_cache();
    tile_layer->hardware_scroll_active = 1;
    tile_layer->native_scroll_active = 0;
    tile_layer->previous_camera_x = tile_layer->layer.camera.x;
    tile_layer->previous_camera_y = tile_layer->layer.camera.y;
    tile_layer->layer.dirty = 0;

    return 1;
}
#endif

void ana_tile_layer_init(
    ANA_TileLayer* tile_layer,
    ANA_LayerKind kind,
    int z_order,
    int tile_width,
    int tile_height,
    int map_width,
    int map_height)
{
    if (tile_layer == NULL) {
        return;
    }

    memset(tile_layer, 0, sizeof(*tile_layer));
    ana_layer_init(&tile_layer->layer, kind, z_order);
    tile_layer->tile_width = tile_width;
    tile_layer->tile_height = tile_height;
    tile_layer->map_width = map_width;
    tile_layer->map_height = map_height;
    tile_layer->previous_camera_x = -1;
    tile_layer->previous_camera_y = -1;
    tile_layer->scroll_backend = ANA_SCROLL_BACKEND_AUTO;
    tile_layer->scroll_sync = ANA_SCROLL_SYNC_AUTO;
    tile_layer->native_scroll_active = 0;
    tile_layer->hardware_scroll_active = 0;
    tile_layer->clear_color = 0u;
}

void ana_tile_layer_set_callbacks(
    ANA_TileLayer* tile_layer,
    ANA_TileReadCallback read_tile,
    ANA_TileDrawCallback draw_tile,
    void* user_data)
{
    if (tile_layer == NULL) {
        return;
    }

    tile_layer->read_tile = read_tile;
    tile_layer->draw_tile = draw_tile;
    tile_layer->user_data = user_data;
}

void ana_tile_layer_set_viewport(ANA_TileLayer* tile_layer, ANA_Rect viewport)
{
    if (tile_layer == NULL) {
        return;
    }

    ana_layer_set_viewport(&tile_layer->layer, viewport);
}

void ana_tile_layer_set_camera(
    ANA_TileLayer* tile_layer,
    const ANA_Camera* camera)
{
    if (tile_layer == NULL || camera == NULL) {
        return;
    }

    ana_layer_set_camera(&tile_layer->layer, camera);
}

void ana_tile_layer_set_clear_color(
    ANA_TileLayer* tile_layer,
    unsigned char clear_color)
{
    if (tile_layer == NULL) {
        return;
    }

    tile_layer->clear_color = (unsigned char)(clear_color % ANA_DEFAULT_COLORS);
}

void ana_tile_layer_invalidate(ANA_TileLayer* tile_layer)
{
    if (tile_layer == NULL) {
        return;
    }

    tile_layer->previous_camera_x = -1;
    tile_layer->previous_camera_y = -1;
    ana_layer_mark_dirty(&tile_layer->layer);
}

void ana_tile_layer_set_scroll_backend(
    ANA_TileLayer* tile_layer,
    ANA_ScrollBackend backend)
{
    if (tile_layer == NULL) {
        return;
    }

    if (backend < ANA_SCROLL_BACKEND_AUTO ||
            backend > ANA_SCROLL_BACKEND_HARDWARE) {
        backend = ANA_SCROLL_BACKEND_AUTO;
    }

    if (tile_layer->scroll_backend == backend) {
        return;
    }

    tile_layer->scroll_backend = backend;
    tile_layer->native_scroll_active = 0;
    tile_layer->hardware_scroll_active = 0;
    ana_tile_layer_invalidate(tile_layer);
}

ANA_ScrollBackend ana_tile_layer_scroll_backend(const ANA_TileLayer* tile_layer)
{
    if (tile_layer == NULL) {
        return ANA_SCROLL_BACKEND_AUTO;
    }

    return tile_layer->scroll_backend;
}

void ana_tile_layer_set_scroll_sync(
    ANA_TileLayer* tile_layer,
    ANA_ScrollSync sync)
{
    if (tile_layer == NULL) {
        return;
    }

    if (sync < ANA_SCROLL_SYNC_AUTO || sync > ANA_SCROLL_SYNC_DIRTY) {
        sync = ANA_SCROLL_SYNC_AUTO;
    }

    if (tile_layer->scroll_sync == sync) {
        return;
    }

    tile_layer->scroll_sync = sync;
    ana_tile_layer_invalidate(tile_layer);
}

ANA_ScrollSync ana_tile_layer_scroll_sync(const ANA_TileLayer* tile_layer)
{
    if (tile_layer == NULL) {
        return ANA_SCROLL_SYNC_AUTO;
    }

    return tile_layer->scroll_sync;
}

int ana_tile_layer_native_scroll_available(const ANA_TileLayer* tile_layer)
{
    return ana_tile_layer_can_use_native_scroll(tile_layer);
}

int ana_tile_layer_native_scroll_active(const ANA_TileLayer* tile_layer)
{
    return tile_layer != NULL && tile_layer->native_scroll_active;
}

int ana_tile_layer_hardware_scroll_available(const ANA_TileLayer* tile_layer)
{
    return ana_tile_layer_can_use_hardware_scroll(tile_layer);
}

int ana_tile_layer_hardware_scroll_active(const ANA_TileLayer* tile_layer)
{
    return tile_layer != NULL && tile_layer->hardware_scroll_active;
}

int ana_tile_layer_hardware_scroll_frame_slot(const ANA_TileLayer* tile_layer)
{
#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_DIRECT_PRESENT)
    if (tile_layer != NULL &&
            tile_layer->hardware_scroll_active &&
            ana_amiga_hardware_scroll_active()) {
        return ana_amiga_hardware_scroll.draw_index;
    }
#else
    (void)tile_layer;
#endif

    return 0;
}

void ana_tile_layer_redraw_world_rect(
    ANA_TileLayer* tile_layer,
    ANA_Rect world_rect)
{
    ANA_Rect view;
    ANA_Rect clipped;

    if (tile_layer == NULL ||
            tile_layer->layer.disabled == ANA_LAYER_DISABLED_VALUE) {
        return;
    }

#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_DIRECT_PRESENT)
    if (tile_layer->hardware_scroll_active &&
            ana_amiga_hardware_scroll_active()) {
        ana_tile_layer_redraw_hardware_world_rect(tile_layer, world_rect);
        return;
    }
#endif

    view = ana_camera_world_view(&tile_layer->layer.camera);
    clipped = ana_rect_clip(world_rect, view);
    if (ana_rect_is_empty(clipped)) {
        return;
    }

    ana_tile_layer_clear_world_rect(tile_layer, clipped);
    ana_tile_layer_draw_tiles_in_world_rect(tile_layer, clipped);
}

void ana_tile_layer_restore_world_rect(
    ANA_TileLayer* tile_layer,
    ANA_Rect world_rect)
{
#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_DIRECT_PRESENT)
    ANA_Rect world;
    ANA_Rect viewport;
    ANA_Rect clipped;
    ANA_Rect bitmap_rect;
#endif

    if (tile_layer == NULL ||
            tile_layer->layer.disabled == ANA_LAYER_DISABLED_VALUE ||
            ana_rect_is_empty(world_rect)) {
        return;
    }

#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_DIRECT_PRESENT)
    if (tile_layer->hardware_scroll_active &&
            ana_amiga_hardware_scroll_active()) {
        world = ana_tile_layer_world_bounds(tile_layer);
        clipped = ana_rect_clip(world_rect, world);
        if (ana_rect_is_empty(clipped)) {
            return;
        }

        viewport = ana_tile_layer_viewport_bounds(tile_layer);
        bitmap_rect = ana_rect_make(
            clipped.x + viewport.x,
            clipped.y + viewport.y,
            clipped.w,
            clipped.h);

        if (ana_amiga_hardware_scroll_restore_background_rect(bitmap_rect)) {
            return;
        }
    }
#endif

    ana_tile_layer_redraw_world_rect(tile_layer, world_rect);
}

void ana_tile_layer_draw(ANA_TileLayer* tile_layer)
{
    ANA_Rect viewport;
    ANA_Rect view;
    int screen_dx;
    int screen_dy;
    int native_scroll_requested;
    int hardware_scroll_requested;

    if (tile_layer == NULL ||
            tile_layer->layer.disabled == ANA_LAYER_DISABLED_VALUE ||
            tile_layer->read_tile == NULL ||
            tile_layer->draw_tile == NULL) {
        return;
    }

    viewport = ana_tile_layer_viewport_bounds(tile_layer);
    view = ana_camera_world_view(&tile_layer->layer.camera);
    tile_layer->native_scroll_active = 0;
    tile_layer->hardware_scroll_active = 0;
    hardware_scroll_requested =
        ana_tile_layer_should_request_hardware_scroll(tile_layer);
#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_DIRECT_PRESENT)
    if (hardware_scroll_requested &&
            ana_tile_layer_draw_hardware_scroll(tile_layer)) {
        return;
    }
    if (!hardware_scroll_requested) {
        ana_amiga_hardware_scroll_deactivate();
    }
#endif

    if (tile_layer->previous_camera_x < 0 ||
            tile_layer->previous_camera_y < 0 ||
            tile_layer->layer.dirty) {
        ana_fill_rect(
            tile_layer->clear_color,
            viewport.x,
            viewport.y,
            viewport.w,
            viewport.h);
        ana_tile_layer_draw_tiles_in_world_rect(tile_layer, view);
        tile_layer->previous_camera_x = tile_layer->layer.camera.x;
        tile_layer->previous_camera_y = tile_layer->layer.camera.y;
        tile_layer->layer.dirty = 0;
        return;
    }

    screen_dx = tile_layer->previous_camera_x - tile_layer->layer.camera.x;
    screen_dy = tile_layer->previous_camera_y - tile_layer->layer.camera.y;

    if (screen_dx == 0 && screen_dy == 0) {
        tile_layer->layer.dirty = 0;
        return;
    }

    if (abs(screen_dx) >= viewport.w || abs(screen_dy) >= viewport.h) {
        ana_fill_rect(
            tile_layer->clear_color,
            viewport.x,
            viewport.y,
            viewport.w,
            viewport.h);
        ana_tile_layer_draw_tiles_in_world_rect(tile_layer, view);
    } else {
        native_scroll_requested = 0;
        if (!hardware_scroll_requested) {
            native_scroll_requested =
                ana_tile_layer_should_request_native_scroll(tile_layer);
        }
        tile_layer->hardware_scroll_active = hardware_scroll_requested;
        tile_layer->native_scroll_active = native_scroll_requested;
#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_NATIVE_SCROLL_ENABLED)
        ana_amiga_native_scroll_requested = native_scroll_requested;
        ana_amiga_native_scroll_sync_chunky =
            native_scroll_requested ?
                ana_tile_layer_native_scroll_sync_chunky(tile_layer) :
                1;
#endif
        ana_scroll_rect(
            viewport.x,
            viewport.y,
            viewport.w,
            viewport.h,
            screen_dx,
            screen_dy,
            tile_layer->clear_color);
#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_NATIVE_SCROLL_ENABLED)
        ana_amiga_native_scroll_requested = 0;
        ana_amiga_native_scroll_sync_chunky = 1;
#endif
        ana_tile_layer_redraw_exposed_scroll_strips(
            tile_layer,
            screen_dx,
            screen_dy);
    }

    tile_layer->previous_camera_x = tile_layer->layer.camera.x;
    tile_layer->previous_camera_y = tile_layer->layer.camera.y;
    tile_layer->layer.dirty = 0;
}

void ana_label_init(
    ANA_Label* label,
    ANA_Font font,
    int x,
    int y,
    int clear_width)
{
    if (label == NULL) {
        return;
    }

    label->font = font;
    label->x = x;
    label->y = y;
    label->color = (unsigned char)ANA_DEFAULT_COLORS;
    label->clear_color = 0u;
    label->clear_width = clear_width;
    label->text[0] = '\0';
    label->dirty = 1;
}

void ana_label_set_text(ANA_Label* label, const char* text)
{
    const char* source;

    if (label == NULL) {
        return;
    }

    source = text != NULL ? text : "";
    if (strcmp(label->text, source) == 0) {
        return;
    }

    strncpy(label->text, source, sizeof(label->text) - 1u);
    label->text[sizeof(label->text) - 1u] = '\0';
    label->dirty = 1;
}

void ana_label_draw_if_dirty(ANA_Label* label)
{
    int clear_height;

    if (label == NULL || !label->dirty) {
        return;
    }

    if (label->font == NULL) {
        label->dirty = 0;
        return;
    }

    clear_height = label->font->char_height;
    if (label->clear_width > 0 && clear_height > 0) {
        ana_fill_rect(
            label->clear_color,
            label->x,
            label->y,
            label->clear_width,
            clear_height);
    }

    if (label->color < ANA_DEFAULT_COLORS) {
        ana_set_font_color(label->font, label->color);
    }

    if (label->text[0] != '\0') {
        ana_draw_text(label->font, label->x, label->y, label->text);
    }

    label->dirty = 0;
}

void ana_free_image(ANA_Image image)
{
    if (image == NULL) {
        return;
    }

#ifdef ANA_TARGET_AMIGA
    ana_amiga_image_free_shifted(image);
    ana_amiga_image_free_native(image);
#endif
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
#ifdef ANA_AMIGA_DIRECT_PRESENT
    if (ana_amiga_hardware_scroll_draw_image_frame(image, frame, x, y)) {
        return;
    }
#endif
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
    const unsigned char* mask_row;
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
        source_row = pixels;
        dest_row =
            ana_framebuffers[ana_draw_buffer] +
            ((long)y * ANA_DEFAULT_WIDTH) +
            x;

        if (image->width == 2) {
            for (row = 0; row < image->height; row++) {
                dest_row[0] = source_row[0];
                dest_row[1] = source_row[1];
                source_row += 2;
                dest_row += ANA_DEFAULT_WIDTH;
            }

            return 1;
        }

        if (image->width == 16) {
            for (row = 0; row < image->height; row++) {
                ANA_COPY_8_PIXELS(dest_row, source_row);
                ANA_COPY_8_PIXELS(dest_row + 8, source_row + 8);
                source_row += 16;
                dest_row += ANA_DEFAULT_WIDTH;
            }

            return 1;
        }

        for (row = 0; row < image->height; row++) {
            memcpy(
                dest_row,
                source_row,
                (size_t)image->width);
            source_row += image->width;
            dest_row += ANA_DEFAULT_WIDTH;
        }

        return 1;
    }

    if (image->row_bytes == 1 && image->width == 2) {
        mask_row = mask;
        source_row = pixels;
        dest_row =
            ana_framebuffers[ana_draw_buffer] +
            ((long)y * ANA_DEFAULT_WIDTH) +
            x;

        for (row = 0; row < image->height; row++) {
            bits = *mask_row;
            if (bits != 0u) {
                if ((bits & 0x80u) != 0u) {
                    dest_row[0] = source_row[0];
                }
                if ((bits & 0x40u) != 0u) {
                    dest_row[1] = source_row[1];
                }
            }

            mask_row++;
            source_row += 2;
            dest_row += ANA_DEFAULT_WIDTH;
        }

        return 1;
    }

    if (image->row_bytes == 1 && image->width <= 8) {
        mask_row = mask;
        source_row = pixels;
        dest_row =
            ana_framebuffers[ana_draw_buffer] +
            ((long)y * ANA_DEFAULT_WIDTH) +
            x;

        for (row = 0; row < image->height; row++) {
            bits = *mask_row;
            if (bits != 0u) {
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

            mask_row++;
            source_row += image->width;
            dest_row += ANA_DEFAULT_WIDTH;
        }

        return 1;
    }

    if (image->row_bytes == 2 && image->width == 16) {
        mask_row = mask;
        source_row = pixels;
        dest_row =
            ana_framebuffers[ana_draw_buffer] +
            ((long)y * ANA_DEFAULT_WIDTH) +
            x;

        for (row = 0; row < image->height; row++) {
            bits = mask_row[0];
            if (bits == 0xffu) {
                ANA_COPY_8_PIXELS(dest_row, source_row);
            } else if (bits != 0u) {
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
            }

            bits = mask_row[1];
            if (bits == 0xffu) {
                ANA_COPY_8_PIXELS(dest_row + 8, source_row + 8);
            } else if (bits != 0u) {
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

            mask_row += 2;
            source_row += 16;
            dest_row += ANA_DEFAULT_WIDTH;
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

int ana_image_pixel_visible(ANA_Image image, int frame, int x, int y)
{
    const unsigned char* mask;

    if (image == NULL ||
            image->data == NULL ||
            frame < 0 ||
            frame >= image->frame_count ||
            x < 0 ||
            x >= image->width ||
            y < 0 ||
            y >= image->height) {
        return 0;
    }

    mask = ana_image_mask_base(image, frame);
    if (mask == NULL) {
        return 1;
    }

    return ana_image_bit_at(
        mask + ((long)y * image->row_bytes),
        image->row_bytes,
        x,
        0);
}

int ana_image_pixel_index(ANA_Image image, int frame, int x, int y)
{
    if (!ana_image_pixel_visible(image, frame, x, y)) {
        return -1;
    }

    return (int)ana_image_pixel_at(image, frame, x, y);
}

static void ana_retained_redraw_layers(
    ANA_Rect rect,
    const ANA_RetainedLayer* layers,
    int layer_count)
{
    int i;

    if (layers == NULL || layer_count <= 0) {
        return;
    }

    for (i = 0; i < layer_count; i++) {
        if (layers[i].redraw != NULL) {
            layers[i].redraw(rect, layers[i].user_data);
        }
    }
}

ANA_Rect ana_retained_clear_rect(
    ANA_Rect rect,
    unsigned char clear_color,
    const ANA_RetainedLayer* layers,
    int layer_count)
{
    if (ana_rect_is_empty(rect)) {
        return rect;
    }

    ana_fill_rect(clear_color, rect.x, rect.y, rect.w, rect.h);
    ana_retained_redraw_layers(rect, layers, layer_count);

    return rect;
}

ANA_Rect ana_retained_clear_rect_x8(
    ANA_Rect rect,
    unsigned char clear_color,
    int min_x,
    int max_x,
    const ANA_RetainedLayer* layers,
    int layer_count)
{
    rect = ana_rect_align_x8(rect, min_x, max_x);
    return ana_retained_clear_rect(rect, clear_color, layers, layer_count);
}

void ana_bob_init(ANA_Bob* bob, ANA_Image image)
{
    if (bob == NULL) {
        return;
    }

    bob->image = image;
    bob->previous_image = NULL;
    bob->frame = 0;
    bob->x = 0;
    bob->y = 0;
    bob->previous_x = 0;
    bob->previous_y = 0;
    bob->previous_frame = 0;
    bob->previous_w = 0;
    bob->previous_h = 0;
    bob->visible = 1;
    bob->previous_visible = 0;
    bob->clear_color = 0u;
}

void ana_bob_set_image(ANA_Bob* bob, ANA_Image image)
{
    if (bob == NULL) {
        return;
    }

    bob->image = image;
}

void ana_bob_set_position(ANA_Bob* bob, int x, int y)
{
    if (bob == NULL) {
        return;
    }

    bob->x = x;
    bob->y = y;
}

void ana_bob_set_frame(ANA_Bob* bob, int frame)
{
    if (bob == NULL) {
        return;
    }

    bob->frame = frame;
}

void ana_bob_set_visible(ANA_Bob* bob, int visible)
{
    if (bob == NULL) {
        return;
    }

    bob->visible = visible ? 1 : 0;
}

int ana_bob_is_unchanged(const ANA_Bob* bob)
{
    if (bob == NULL) {
        return 1;
    }

    if (bob->visible != bob->previous_visible) {
        return 0;
    }

    if (!bob->visible) {
        return 1;
    }

    return bob->x == bob->previous_x &&
        bob->y == bob->previous_y &&
        bob->frame == bob->previous_frame &&
        bob->image == bob->previous_image;
}

ANA_Rect ana_bob_rect(const ANA_Bob* bob)
{
    if (bob == NULL || bob->image == NULL || !bob->visible) {
        return ana_rect_make(0, 0, 0, 0);
    }

    return ana_rect_make(
        bob->x,
        bob->y,
        ana_image_width(bob->image),
        ana_image_height(bob->image));
}

ANA_Rect ana_bob_previous_rect(const ANA_Bob* bob)
{
    if (bob == NULL || !bob->previous_visible) {
        return ana_rect_make(0, 0, 0, 0);
    }

    return ana_rect_make(
        bob->previous_x,
        bob->previous_y,
        bob->previous_w,
        bob->previous_h);
}

static int ana_fill_image_mask(
    ANA_Image image,
    int frame,
    int x,
    int y,
    unsigned char color_index,
    ANA_Rect dirty_rect)
{
    ANA_Rect image_rect;
    ANA_Rect clip_rect;
    ANA_Rect screen_rect;
    const unsigned char* mask;
    const unsigned char* mask_row;
    unsigned char* dest_row;
    int dest_x;
    int dest_y;
    int src_x;
    int src_y;

    if (image == NULL ||
            frame < 0 ||
            frame >= image->frame_count ||
            !ana_image_has_mask(image)) {
        return 0;
    }

    image_rect = ana_rect_make(x, y, image->width, image->height);
    screen_rect = ana_rect_make(0, 0, ANA_DEFAULT_WIDTH, ANA_DEFAULT_HEIGHT);
    clip_rect = ana_rect_clip(image_rect, screen_rect);
    if (ana_rect_is_empty(clip_rect)) {
        return 1;
    }

    mask = ana_image_mask_base(image, frame);
    if (mask == NULL) {
        return 0;
    }

    color_index = (unsigned char)(color_index & 0x0f);

#ifdef ANA_TARGET_AMIGA
    if (!ana_rect_is_empty(dirty_rect)) {
        ana_amiga_mark_dirty_rect(
            dirty_rect.x,
            dirty_rect.y,
            dirty_rect.x + dirty_rect.w,
            dirty_rect.y + dirty_rect.h);
    }
#else
    (void)dirty_rect;
#endif

    for (dest_y = clip_rect.y; dest_y < clip_rect.y + clip_rect.h; dest_y++) {
        src_y = dest_y - y;
        mask_row = mask + ((long)src_y * image->row_bytes);
        dest_row =
            ana_framebuffers[ana_draw_buffer] +
            ((long)dest_y * ANA_DEFAULT_WIDTH);

        for (dest_x = clip_rect.x;
                dest_x < clip_rect.x + clip_rect.w;
                dest_x++) {
            src_x = dest_x - x;
            if ((mask_row[src_x >> 3] & (0x80u >> (src_x & 7))) != 0u) {
                dest_row[dest_x] = color_index;
            }
        }
    }

    return 1;
}

void ana_bob_clear_previous(const ANA_Bob* bob)
{
    ANA_Rect rect;

    if (bob == NULL) {
        return;
    }

    rect = ana_bob_previous_rect(bob);
    if (!ana_rect_is_empty(rect)) {
        ana_fill_rect(bob->clear_color, rect.x, rect.y, rect.w, rect.h);
    }
}

void ana_bob_clear_previous_with_layers(
    const ANA_Bob* bob,
    const ANA_RetainedLayer* layers,
    int layer_count)
{
    ANA_Rect rect;

    if (bob == NULL) {
        return;
    }

    rect = ana_bob_previous_rect(bob);
    ana_retained_clear_rect(rect, bob->clear_color, layers, layer_count);
}

ANA_Rect ana_bob_clear_previous_x8_with_layers(
    const ANA_Bob* bob,
    int min_x,
    int max_x,
    const ANA_RetainedLayer* layers,
    int layer_count)
{
    ANA_Rect rect;

    if (bob == NULL) {
        return ana_rect_make(0, 0, 0, 0);
    }

    rect = ana_bob_previous_rect(bob);
    return ana_retained_clear_rect_x8(
        rect,
        bob->clear_color,
        min_x,
        max_x,
        layers,
        layer_count);
}

ANA_Rect ana_bob_clear_previous_masked_x8_with_layers(
    const ANA_Bob* bob,
    int min_x,
    int max_x,
    const ANA_RetainedLayer* layers,
    int layer_count)
{
    ANA_Rect rect;
    ANA_Rect aligned_rect;

    if (bob == NULL) {
        return ana_rect_make(0, 0, 0, 0);
    }

    rect = ana_bob_previous_rect(bob);
    aligned_rect = ana_rect_align_x8(rect, min_x, max_x);
    if (ana_rect_is_empty(aligned_rect)) {
        return aligned_rect;
    }

    if (ana_fill_image_mask(
            bob->previous_image,
            bob->previous_frame,
            bob->previous_x,
            bob->previous_y,
            bob->clear_color,
            aligned_rect)) {
        ana_retained_redraw_layers(aligned_rect, layers, layer_count);
        return aligned_rect;
    }

    return ana_retained_clear_rect(
        aligned_rect,
        bob->clear_color,
        layers,
        layer_count);
}

void ana_bob_draw(const ANA_Bob* bob)
{
    if (bob == NULL || bob->image == NULL || !bob->visible) {
        return;
    }

    ana_draw_image_frame(bob->image, bob->frame, bob->x, bob->y);
}

void ana_bob_commit(ANA_Bob* bob)
{
    ANA_Rect rect;

    if (bob == NULL) {
        return;
    }

    rect = ana_bob_rect(bob);
    bob->previous_image = bob->image;
    bob->previous_x = bob->x;
    bob->previous_y = bob->y;
    bob->previous_frame = bob->frame;
    bob->previous_w = rect.w;
    bob->previous_h = rect.h;
    bob->previous_visible = bob->visible;
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
#ifdef ANA_AMIGA_NATIVE_SCROLL_ENABLED
    ana_amiga_visible_scroll_pending = 0;
    ana_amiga_native_scroll_requested = 0;
    ana_amiga_native_scroll_sync_chunky = 1;
#endif
    ana_amiga_clear_draw_framebuffer(color_index);
    ana_amiga_clear_requested = 1;
    ana_amiga_clear_color = color_index;
#else
    memset(ana_framebuffers[ana_draw_buffer], color_index, ANA_FRAMEBUFFER_PIXELS);
#endif
}

static int ana_clip_screen_rect(int* x, int* y, int* width, int* height)
{
    int end_x;
    int end_y;

    if (*width <= 0 || *height <= 0) {
        return 0;
    }

    if (*x >= ANA_DEFAULT_WIDTH || *y >= ANA_DEFAULT_HEIGHT ||
            *x + *width <= 0 || *y + *height <= 0) {
        return 0;
    }

    end_x = *x + *width;
    end_y = *y + *height;

    if (*x < 0) {
        *x = 0;
    }
    if (*y < 0) {
        *y = 0;
    }
    if (end_x > ANA_DEFAULT_WIDTH) {
        end_x = ANA_DEFAULT_WIDTH;
    }
    if (end_y > ANA_DEFAULT_HEIGHT) {
        end_y = ANA_DEFAULT_HEIGHT;
    }

    *width = end_x - *x;
    *height = end_y - *y;
    return *width > 0 && *height > 0;
}

static void ana_scroll_chunky_rect(
    int x,
    int y,
    int width,
    int height,
    int dx,
    int dy)
{
    int copy_w;
    int copy_h;
    int src_x;
    int src_y;
    int dst_x;
    int dst_y;
    int row;
    int row_start;
    int row_end;
    int row_step;
    unsigned char* pixels;

    copy_w = width - abs(dx);
    copy_h = height - abs(dy);

    if (copy_w <= 0 || copy_h <= 0) {
        return;
    }

    src_x = dx > 0 ? x : x - dx;
    dst_x = dx > 0 ? x + dx : x;
    src_y = dy > 0 ? y : y - dy;
    dst_y = dy > 0 ? y + dy : y;

    pixels = ana_framebuffers[ana_draw_buffer];

    if (dy > 0) {
        row_start = copy_h - 1;
        row_end = -1;
        row_step = -1;
    } else {
        row_start = 0;
        row_end = copy_h;
        row_step = 1;
    }

    for (row = row_start; row != row_end; row += row_step) {
        memmove(
            pixels + ((long)(dst_y + row) * ANA_DEFAULT_WIDTH) + dst_x,
            pixels + ((long)(src_y + row) * ANA_DEFAULT_WIDTH) + src_x,
            (size_t)copy_w);
    }
}

#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_NATIVE_SCROLL_ENABLED)
static int ana_amiga_scroll_visible_bitmap_rect(
    int x,
    int y,
    int width,
    int height,
    int dx,
    int dy)
{
    struct ANA_AmigaBitmapState* state;
    struct BitMap* bitmap;
    int copy_w;
    int copy_h;
    int src_x;
    int src_y;
    int dst_x;
    int dst_y;

    bitmap = ana_amiga_visible_bitmap;
    if (bitmap == NULL) {
        return 0;
    }

    copy_w = width - abs(dx);
    copy_h = height - abs(dy);
    if (copy_w <= 0 || copy_h <= 0) {
        return 0;
    }

    src_x = dx > 0 ? x : x - dx;
    dst_x = dx > 0 ? x + dx : x;
    src_y = dy > 0 ? y : y - dy;
    dst_y = dy > 0 ? y + dy : y;

    WaitBlit();
    BltBitMap(
        bitmap,
        src_x,
        src_y,
        bitmap,
        dst_x,
        dst_y,
        copy_w,
        copy_h,
        0xc0,
        0xff,
        NULL);
    WaitBlit();

    state = ana_amiga_bitmap_state_for(bitmap);
    if (state != NULL) {
        state->clear_color_valid = 0;
        state->dirty_count = 0;
    }

    return 1;
}
#endif

void ana_scroll_rect(
    int x,
    int y,
    int width,
    int height,
    int dx,
    int dy,
    unsigned char clear_color)
{
#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_NATIVE_SCROLL_ENABLED)
    int visible_scroll_done;
    int scroll_chunky;
#endif

    if (!ana_gfx_opened || (dx == 0 && dy == 0)) {
        return;
    }

    if (!ana_clip_screen_rect(&x, &y, &width, &height)) {
        return;
    }

    clear_color = (unsigned char)(clear_color & 0x0f);

    if (abs(dx) >= width || abs(dy) >= height) {
        ana_fill_rect(clear_color, x, y, width, height);
        return;
    }

#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_NATIVE_SCROLL_ENABLED)
    visible_scroll_done = 0;
    scroll_chunky = 1;
    if (ana_amiga_native_scroll_requested &&
            (ana_gfx_render_mode == ANA_RENDER_TILE_SCROLL ||
                ana_gfx_render_mode == ANA_RENDER_SIDE_SCROLL ||
                ana_gfx_render_mode == ANA_RENDER_VERTICAL_SCROLL ||
                ana_gfx_render_mode == ANA_RENDER_TILE_4WAY)) {
        visible_scroll_done =
            ana_amiga_scroll_visible_bitmap_rect(x, y, width, height, dx, dy);
        if (visible_scroll_done) {
            ana_amiga_visible_scroll_pending = 1;
            if (!ana_amiga_native_scroll_sync_chunky) {
                scroll_chunky = 0;
            }
        } else {
            ana_amiga_visible_scroll_pending = 0;
            ana_amiga_mark_dirty_rect(x, y, x + width, y + height);
        }
    } else {
        ana_amiga_visible_scroll_pending = 0;
        ana_amiga_mark_dirty_rect(x, y, x + width, y + height);
    }
#else
#ifdef ANA_TARGET_AMIGA
    ana_amiga_mark_dirty_rect(x, y, x + width, y + height);
#endif
#endif

    /*
     * The conservative Amiga bridge keeps chunky synchronized after visible
     * bitmap scrolls so later C2P writes cannot restore stale pixels. Tile
     * layers that redraw every exposed/dirty region from callbacks can opt into
     * ANA_SCROLL_SYNC_DIRTY and skip this copy.
     */
#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_NATIVE_SCROLL_ENABLED)
    if (scroll_chunky) {
        ana_scroll_chunky_rect(x, y, width, height, dx, dy);
    }
#else
    ana_scroll_chunky_rect(x, y, width, height, dx, dy);
#endif

    if (dy > 0) {
        ana_fill_rect(clear_color, x, y, width, dy);
    } else if (dy < 0) {
        ana_fill_rect(clear_color, x, y + height + dy, width, -dy);
    }

    if (dx > 0) {
        ana_fill_rect(clear_color, x, y, dx, height);
    } else if (dx < 0) {
        ana_fill_rect(clear_color, x + width + dx, y, -dx, height);
    }
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

#ifdef ANA_TARGET_AMIGA
    if (ana_amiga_hardware_scroll_fill_rect(
            color_index,
            x,
            y,
            width,
            height)) {
        return;
    }
#endif

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
    if (color_index == 0u &&
            ANA_AMIGA_BLACK_FILL_USES_PLANAR_CLEAR &&
            !ana_amiga_hardware_scroll_active()) {
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
#if !defined(ANA_TARGET_AMIGA) || !defined(ANA_AMIGA_DIRECT_PRESENT)
    int previous_front;
#endif

    if (!ana_gfx_opened) {
        return;
    }

#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_DIRECT_PRESENT)
    ana_front_buffer = ana_draw_buffer;
#else
    previous_front = ana_front_buffer;
    ana_front_buffer = ana_draw_buffer;
    ana_draw_buffer = previous_front;
#endif
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

void* ana_gfx_native_viewport(void)
{
#ifdef ANA_TARGET_AMIGA
    if (ana_amiga_screen == NULL) {
        return NULL;
    }

    return &ana_amiga_screen->ViewPort;
#else
    return NULL;
#endif
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

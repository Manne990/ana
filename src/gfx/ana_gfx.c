#include "ana/ana_gfx.h"

#include "ana_internal.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ANA_TARGET_AMIGA
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
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
#endif

static unsigned char ana_framebuffers[ANA_FRAMEBUFFER_COUNT][ANA_FRAMEBUFFER_PIXELS];
static ANA_Color ana_palette[ANA_DEFAULT_COLORS];
static int ana_gfx_opened = 0;
static int ana_draw_buffer = 0;
static int ana_front_buffer = 1;
static int ana_presented_frames = 0;

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
static unsigned short ana_amiga_rgb4[ANA_DEFAULT_COLORS];
static int ana_amiga_hidden_bitmap_ready = 0;
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
    unsigned char* data;
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
#endif

static void ana_draw_image_frame_internal(
    ANA_Image image,
    int frame,
    int x,
    int y,
    int mark_dirty);

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

    return image->row_bytes > 0 &&
        image->plane_size > 0L &&
        image->planes_size > 0L &&
        image->frame_size > 0L &&
        image->data_size > 0L;
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

#ifdef ANA_TARGET_AMIGA
static void ana_amiga_reset_frame_state(void)
{
    ana_amiga_dirty_count = 0;
    ana_amiga_clear_requested = 0;
    ana_amiga_clear_color = 0;
}

static void ana_amiga_mark_dirty_rect(int min_x, int min_y, int max_x, int max_y)
{
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

    if (ana_amiga_dirty_count < ANA_AMIGA_MAX_DIRTY_RECTS) {
        ana_amiga_dirty_rects[ana_amiga_dirty_count].min_x = min_x;
        ana_amiga_dirty_rects[ana_amiga_dirty_count].min_y = min_y;
        ana_amiga_dirty_rects[ana_amiga_dirty_count].max_x = max_x;
        ana_amiga_dirty_rects[ana_amiga_dirty_count].max_y = max_y;
        ana_amiga_dirty_count++;
        return;
    }

    if (min_x < ana_amiga_dirty_rects[0].min_x) {
        ana_amiga_dirty_rects[0].min_x = min_x;
    }

    if (min_y < ana_amiga_dirty_rects[0].min_y) {
        ana_amiga_dirty_rects[0].min_y = min_y;
    }

    if (max_x > ana_amiga_dirty_rects[0].max_x) {
        ana_amiga_dirty_rects[0].max_x = max_x;
    }

    if (max_y > ana_amiga_dirty_rects[0].max_y) {
        ana_amiga_dirty_rects[0].max_y = max_y;
    }
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

static void ana_amiga_set_screen_bitmap(struct BitMap* bitmap)
{
    if (ana_amiga_screen == NULL || bitmap == NULL) {
        return;
    }

    ana_amiga_screen->RastPort.BitMap = bitmap;
    if (ana_amiga_screen->ViewPort.RasInfo != NULL) {
        ana_amiga_screen->ViewPort.RasInfo->BitMap = bitmap;
    }

    MakeScreen(ana_amiga_screen);
    RethinkDisplay();
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

    if (!ana_amiga_open_window()) {
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

    if (ana_amiga_screen != NULL) {
        CloseScreen(ana_amiga_screen);
        ana_amiga_screen = NULL;
    }

    ana_amiga_free_hidden_bitmap();
    ana_amiga_original_bitmap = NULL;
    ana_amiga_visible_bitmap = NULL;
    ana_amiga_draw_bitmap = NULL;
    ana_amiga_reset_frame_state();

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
    int bit;
    int plane;
    int pixel_x;
    int color;
    unsigned char plane_bytes[ANA_DEFAULT_BITPLANES];
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

    for (y = min_y; y < max_y; y++) {
        row_offset = (unsigned long)y * bitmap->BytesPerRow;

        for (byte_x = start_byte_x; byte_x < end_byte_x; byte_x++) {
            for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
                plane_bytes[plane] = 0u;
            }

            for (bit = 0; bit < 8; bit++) {
                pixel_x = (byte_x * 8) + bit;
                color = chunky[(y * ANA_DEFAULT_WIDTH) + pixel_x] & 0x0f;

                for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
                    if ((color & (1 << plane)) != 0) {
                        plane_bytes[plane] = (unsigned char)(
                            plane_bytes[plane] | (1u << (7 - bit)));
                    }
                }
            }

            for (plane = 0; plane < ANA_DEFAULT_BITPLANES; plane++) {
                if (bitmap->Planes[plane] != NULL) {
                    ((unsigned char*)bitmap->Planes[plane])
                        [row_offset + (unsigned long)byte_x] =
                        plane_bytes[plane];
                }
            }
        }
    }
}

static void ana_amiga_present_buffer(const unsigned char* chunky)
{
    struct BitMap* next_visible;
    struct BitMap* previous_visible;
    int i;

    if (ana_amiga_screen == NULL) {
        return;
    }

    next_visible = ana_amiga_draw_bitmap;
    previous_visible = ana_amiga_visible_bitmap;

    if (next_visible == NULL || previous_visible == NULL) {
        return;
    }

    if (ana_amiga_clear_requested) {
        ana_amiga_fill_bitmap(next_visible, ana_amiga_clear_color);
    }

    for (i = 0; i < ana_amiga_dirty_count; i++) {
        ana_amiga_copy_chunky_rect_to_bitplanes(
            next_visible,
            chunky,
            ana_amiga_dirty_rects[i].min_x,
            ana_amiga_dirty_rects[i].min_y,
            ana_amiga_dirty_rects[i].max_x,
            ana_amiga_dirty_rects[i].max_y);
    }

    WaitTOF();
    ana_amiga_set_screen_bitmap(next_visible);

    ana_amiga_visible_bitmap = next_visible;
    ana_amiga_draw_bitmap = previous_visible;
    ana_amiga_reset_frame_state();
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

    return image;
}

ANA_Image ana_load_image_data(const unsigned char* bytes, long size)
{
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

    return ana_image_create_from_payload(
        width,
        height,
        frame_count,
        bitplanes,
        flags,
        bytes + ANA_IMAGE_HEADER_SIZE,
        size - ANA_IMAGE_HEADER_SIZE);
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

#ifdef ANA_TARGET_AMIGA
    ana_amiga_mark_dirty_rect(start_x, start_y, end_x, end_y);
#endif

    mask = ana_image_mask_base(image, frame);

    for (dest_y = start_y; dest_y < end_y; dest_y++) {
        src_y = dest_y - y;

        for (dest_x = start_x; dest_x < end_x; dest_x++) {
            src_x = dest_x - x;

            if (mask != NULL) {
                draw_pixel = ana_image_bit_at(
                    mask,
                    image->row_bytes,
                    src_x,
                    src_y);
            } else {
                draw_pixel =
                    ana_image_pixel_at(image, frame, src_x, src_y) != 0u;
            }

            if (draw_pixel) {
                ana_framebuffers[ana_draw_buffer]
                    [(dest_y * ANA_DEFAULT_WIDTH) + dest_x] =
                    font->color_index;
            }
        }
    }
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
    memset(ana_framebuffers[ana_draw_buffer], color_index, ANA_FRAMEBUFFER_PIXELS);

#ifdef ANA_TARGET_AMIGA
    ana_amiga_clear_requested = 1;
    ana_amiga_clear_color = color_index;
#endif
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

#ifdef ANA_TARGET_AMIGA
    ana_amiga_present_buffer(ana_framebuffers[ana_front_buffer]);
#endif
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

void* ana_gfx_native_window(void)
{
#ifdef ANA_TARGET_AMIGA
    return ana_amiga_window;
#else
    return NULL;
#endif
}

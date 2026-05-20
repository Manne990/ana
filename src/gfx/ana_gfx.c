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
            memset(bitmap->Planes[plane], 0, (size_t)plane_size);
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

    ana_amiga_close_libraries();
}

static void ana_amiga_copy_chunky_to_bitplanes(
    struct BitMap* bitmap,
    const unsigned char* chunky)
{
    int y;
    int byte_x;
    int bit;
    int plane;
    int pixel_x;
    int color;
    unsigned char plane_bytes[ANA_DEFAULT_BITPLANES];
    unsigned long row_offset;

    if (bitmap == NULL || chunky == NULL) {
        return;
    }

    for (y = 0; y < ANA_DEFAULT_HEIGHT; y++) {
        row_offset = (unsigned long)y * bitmap->BytesPerRow;

        for (byte_x = 0; byte_x < ANA_DEFAULT_WIDTH / 8; byte_x++) {
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

    if (ana_amiga_screen == NULL) {
        return;
    }

    next_visible = ana_amiga_draw_bitmap;
    previous_visible = ana_amiga_visible_bitmap;

    if (next_visible == NULL || previous_visible == NULL) {
        return;
    }

    ana_amiga_copy_chunky_to_bitplanes(next_visible, chunky);

    WaitTOF();
    ana_amiga_set_screen_bitmap(next_visible);

    ana_amiga_visible_bitmap = next_visible;
    ana_amiga_draw_bitmap = previous_visible;
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

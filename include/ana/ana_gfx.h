#ifndef ANA_GFX_H
#define ANA_GFX_H

#include "ana_helpers.h"
#include "ana_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ANA_Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} ANA_Color;

typedef struct ANA_RenderStats {
    long frames;
    long perf_ticks_per_second;
    long dirty_rects;
    long converted_pixels;
    long planar_clear_rects;
    long planar_clear_pixels;
    long chunky_clear_rects;
    long chunky_clear_pixels;
    long max_dirty_rects;
    long max_converted_pixels;
    long max_planar_clear_pixels;
    long max_chunky_clear_pixels;
    long present_total_perf_ticks;
    long present_clear_perf_ticks;
    long present_convert_perf_ticks;
    long present_flip_perf_ticks;
    long screen_buffer_flips;
    long direct_flips;
} ANA_RenderStats;

typedef void (*ANA_RedrawCallback)(ANA_Rect rect, void* user_data);

typedef struct ANA_RetainedLayer {
    ANA_RedrawCallback redraw;
    void* user_data;
} ANA_RetainedLayer;

typedef struct ANA_Bob {
    ANA_Image image;
    ANA_Image previous_image;
    int frame;
    int x;
    int y;
    int previous_x;
    int previous_y;
    int previous_frame;
    int previous_w;
    int previous_h;
    int visible;
    int previous_visible;
    unsigned char clear_color;
} ANA_Bob;

typedef struct ANA_DrawLayer {
    ANA_RedrawCallback redraw;
    void* user_data;
    int dirty;
} ANA_DrawLayer;

typedef struct ANA_Label {
    ANA_Font font;
    int x;
    int y;
    unsigned char color;
    unsigned char clear_color;
    int clear_width;
    char text[32];
    int dirty;
} ANA_Label;

void ana_set_palette(const ANA_Color* colors, int count);
void ana_clear(unsigned char color_index);
void ana_fill_rect(unsigned char color_index, int x, int y, int width, int height);
void ana_present(void);
ANA_RenderStats ana_render_stats(void);

ANA_Image ana_load_image(const char* path);
ANA_Image ana_load_image_data(const unsigned char* bytes, long size);
void ana_free_image(ANA_Image image);
void ana_draw_image(ANA_Image image, int x, int y);
void ana_draw_image_frame(ANA_Image image, int frame, int x, int y);
int ana_image_width(ANA_Image image);
int ana_image_height(ANA_Image image);
int ana_image_frame_count(ANA_Image image);

ANA_Rect ana_retained_clear_rect(
    ANA_Rect rect,
    unsigned char clear_color,
    const ANA_RetainedLayer* layers,
    int layer_count);
ANA_Rect ana_retained_clear_rect_x8(
    ANA_Rect rect,
    unsigned char clear_color,
    int min_x,
    int max_x,
    const ANA_RetainedLayer* layers,
    int layer_count);

void ana_bob_init(ANA_Bob* bob, ANA_Image image);
void ana_bob_set_image(ANA_Bob* bob, ANA_Image image);
void ana_bob_set_position(ANA_Bob* bob, int x, int y);
void ana_bob_set_frame(ANA_Bob* bob, int frame);
void ana_bob_set_visible(ANA_Bob* bob, int visible);
int ana_bob_is_unchanged(const ANA_Bob* bob);
ANA_Rect ana_bob_rect(const ANA_Bob* bob);
ANA_Rect ana_bob_previous_rect(const ANA_Bob* bob);
void ana_bob_clear_previous(const ANA_Bob* bob);
void ana_bob_clear_previous_with_layers(
    const ANA_Bob* bob,
    const ANA_RetainedLayer* layers,
    int layer_count);
ANA_Rect ana_bob_clear_previous_x8_with_layers(
    const ANA_Bob* bob,
    int min_x,
    int max_x,
    const ANA_RetainedLayer* layers,
    int layer_count);
void ana_bob_draw(const ANA_Bob* bob);
void ana_bob_commit(ANA_Bob* bob);

ANA_Font ana_load_font(const char* path);
ANA_Font ana_load_font_data(const unsigned char* bytes, long size);
void ana_free_font(ANA_Font font);
void ana_set_font_color(ANA_Font font, unsigned char color_index);
void ana_draw_text(ANA_Font font, int x, int y, const char* text);
void ana_draw_int(ANA_Font font, int x, int y, int value);
int ana_text_width(ANA_Font font, const char* text);

void ana_layer_mark_dirty(ANA_DrawLayer* layer);
void ana_layer_draw_if_dirty(ANA_DrawLayer* layer);

void ana_label_init(
    ANA_Label* label,
    ANA_Font font,
    int x,
    int y,
    int clear_width);
void ana_label_set_text(ANA_Label* label, const char* text);
void ana_label_draw_if_dirty(ANA_Label* label);

#ifdef __cplusplus
}
#endif

#endif

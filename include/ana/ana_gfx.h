#ifndef ANA_GFX_H
#define ANA_GFX_H

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

void ana_set_palette(const ANA_Color* colors, int count);
void ana_clear(unsigned char color_index);
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

ANA_Font ana_load_font(const char* path);
ANA_Font ana_load_font_data(const unsigned char* bytes, long size);
void ana_free_font(ANA_Font font);
void ana_set_font_color(ANA_Font font, unsigned char color_index);
void ana_draw_text(ANA_Font font, int x, int y, const char* text);
void ana_draw_int(ANA_Font font, int x, int y, int value);
int ana_text_width(ANA_Font font, const char* text);

#ifdef __cplusplus
}
#endif

#endif

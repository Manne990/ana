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

void ana_set_palette(const ANA_Color* colors, int count);
void ana_clear(unsigned char color_index);
void ana_present(void);

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
void ana_draw_text(ANA_Font font, int x, int y, const char* text);
void ana_draw_int(ANA_Font font, int x, int y, int value);
int ana_text_width(ANA_Font font, const char* text);

#ifdef __cplusplus
}
#endif

#endif

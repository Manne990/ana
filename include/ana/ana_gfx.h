#ifndef ANA_GFX_H
#define ANA_GFX_H

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

#ifdef __cplusplus
}
#endif

#endif


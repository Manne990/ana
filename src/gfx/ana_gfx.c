#include "ana/ana_gfx.h"

#include "ana_internal.h"

#include <stddef.h>
#include <string.h>

#define ANA_FRAMEBUFFER_PIXELS (ANA_DEFAULT_WIDTH * ANA_DEFAULT_HEIGHT)
#define ANA_FRAMEBUFFER_COUNT 2

static unsigned char ana_framebuffers[ANA_FRAMEBUFFER_COUNT][ANA_FRAMEBUFFER_PIXELS];
static ANA_Color ana_palette[ANA_DEFAULT_COLORS];
static int ana_gfx_opened = 0;
static int ana_draw_buffer = 0;
static int ana_front_buffer = 1;
static int ana_presented_frames = 0;

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


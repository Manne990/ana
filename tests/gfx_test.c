#include "ana.h"
#include "ana_internal.h"

#include <assert.h>

static void test_clear_and_present(void)
{
    const ANA_Profile* profile;

    profile = ana_default_profile();
    assert(ana_gfx_open(profile) == ANA_OK);
    assert(ana_gfx_is_open());

    ana_clear(3);
    assert(ana_gfx_draw_pixel(0, 0) == 3);
    assert(ana_gfx_draw_pixel(319, 255) == 3);

    ana_present();
    assert(ana_gfx_present_count() == 1);
    assert(ana_gfx_front_pixel(0, 0) == 3);
    assert(ana_gfx_front_pixel(319, 255) == 3);

    ana_clear(20);
    ana_present();
    assert(ana_gfx_front_pixel(10, 10) == 4);

    ana_gfx_close();
    assert(!ana_gfx_is_open());
}

static void test_palette_accepts_supported_colors(void)
{
    ANA_Color colors[ANA_DEFAULT_COLORS];
    int i;

    for (i = 0; i < ANA_DEFAULT_COLORS; i++) {
        colors[i].r = (unsigned char)i;
        colors[i].g = (unsigned char)(i * 2);
        colors[i].b = (unsigned char)(i * 3);
    }

    assert(ana_gfx_open(ana_default_profile()) == ANA_OK);
    ana_set_palette(colors, ANA_DEFAULT_COLORS);
    ana_set_palette(colors, ANA_DEFAULT_COLORS + 4);
    ana_set_palette(0, ANA_DEFAULT_COLORS);
    ana_set_palette(colors, 0);
    ana_gfx_close();
}

int main(void)
{
    test_clear_and_present();
    test_palette_accepts_supported_colors();

    return 0;
}


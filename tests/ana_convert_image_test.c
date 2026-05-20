#include "ana.h"
#include "ana_internal.h"

#include <assert.h>

int main(int argc, char** argv)
{
    ANA_Image image;

    assert(argc == 2);

    image = ana_load_image(argv[1]);
    assert(image != 0);
    assert(ana_image_width(image) == 3);
    assert(ana_image_height(image) == 2);
    assert(ana_image_frame_count(image) == 2);

    assert(ana_gfx_open(ana_default_profile()) == ANA_OK);

    ana_clear(9);
    ana_draw_image_frame(image, 0, 2, 3);
    assert(ana_gfx_draw_pixel(2, 3) == 0);
    assert(ana_gfx_draw_pixel(3, 3) == 1);
    assert(ana_gfx_draw_pixel(4, 3) == 9);
    assert(ana_gfx_draw_pixel(2, 4) == 9);
    assert(ana_gfx_draw_pixel(3, 4) == 5);
    assert(ana_gfx_draw_pixel(4, 4) == 6);

    ana_clear(9);
    ana_draw_image_frame(image, 1, 10, 11);
    assert(ana_gfx_draw_pixel(10, 11) == 2);
    assert(ana_gfx_draw_pixel(11, 11) == 3);
    assert(ana_gfx_draw_pixel(12, 11) == 4);
    assert(ana_gfx_draw_pixel(10, 12) == 9);
    assert(ana_gfx_draw_pixel(11, 12) == 2);
    assert(ana_gfx_draw_pixel(12, 12) == 0);

    ana_gfx_close();
    ana_free_image(image);

    return 0;
}

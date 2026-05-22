#include "ana.h"
#include "ana_internal.h"

#include <assert.h>
#include <stdio.h>

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

    ana_clear(0);
    ana_fill_rect(6, 2, 3, 4, 5);
    assert(ana_gfx_draw_pixel(1, 3) == 0);
    assert(ana_gfx_draw_pixel(2, 3) == 6);
    assert(ana_gfx_draw_pixel(5, 7) == 6);
    assert(ana_gfx_draw_pixel(6, 7) == 0);

    ana_fill_rect(23, -2, -2, 4, 4);
    assert(ana_gfx_draw_pixel(0, 0) == 7);
    assert(ana_gfx_draw_pixel(1, 1) == 7);
    assert(ana_gfx_draw_pixel(2, 2) == 0);

    ana_present();
    assert(ana_gfx_front_pixel(2, 3) == 6);
    assert(ana_gfx_front_pixel(0, 0) == 7);

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

static void write_u16_le(FILE* file, int value)
{
    fputc(value & 0xff, file);
    fputc((value >> 8) & 0xff, file);
}

static const unsigned char memory_test_image_bytes[] = {
    0x41, 0x4e, 0x41, 0x49, 0x4d, 0x47, 0x30, 0x31,
    0x03, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0xc0, 0x60, 0xa0, 0xc0, 0x60, 0x40,
    0xe0, 0xe0, 0x00, 0xe0, 0xe0, 0xe0
};

static const unsigned char memory_test_font_bytes[] = {
    0x41, 0x4e, 0x41, 0x46, 0x4e, 0x54, 0x30, 0x31,
    0x03, 0x00, 0x02, 0x00, 0x30, 0x03, 0x00, 0x00,
    0x41, 0x4e, 0x41, 0x49, 0x4d, 0x47, 0x30, 0x31,
    0x03, 0x00, 0x02, 0x00, 0x03, 0x00, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0xe0, 0xe0, 0xe0, 0xe0,
    0x40, 0x40, 0x40, 0x40,
    0xa0, 0xa0, 0xa0, 0xa0
};

static void write_test_image_file(const char* path)
{
    FILE* file;

    file = fopen(path, "wb");
    assert(file != 0);

    fwrite("ANAIMG01", 1u, 8u, file);
    write_u16_le(file, 3);
    write_u16_le(file, 2);
    write_u16_le(file, 2);
    fputc(2, file);
    fputc(1, file);
    fputc(0, file);
    fputc(0, file);
    fputc(0, file);
    fputc(0, file);

    fputc(0xc0, file);
    fputc(0x60, file);
    fputc(0xa0, file);
    fputc(0xc0, file);
    fputc(0x60, file);
    fputc(0x40, file);

    fputc(0xe0, file);
    fputc(0xe0, file);
    fputc(0x00, file);
    fputc(0xe0, file);
    fputc(0xe0, file);
    fputc(0xe0, file);

    fclose(file);
}

static void test_image_loading_and_drawing(void)
{
    const char* path;
    ANA_Image image;
    ANA_Image memory_image;

    path = "build/tests/gfx_test_image.anaimg";
    write_test_image_file(path);

    image = ana_load_image(path);
    assert(image != 0);
    assert(ana_image_width(image) == 3);
    assert(ana_image_height(image) == 2);
    assert(ana_image_frame_count(image) == 2);

    assert(ana_gfx_open(ana_default_profile()) == ANA_OK);
    ana_clear(9);

    ana_draw_image(image, 5, 6);
    assert(ana_gfx_draw_pixel(5, 6) == 1);
    assert(ana_gfx_draw_pixel(6, 6) == 2);
    assert(ana_gfx_draw_pixel(7, 6) == 9);
    assert(ana_gfx_draw_pixel(5, 7) == 9);
    assert(ana_gfx_draw_pixel(6, 7) == 3);
    assert(ana_gfx_draw_pixel(7, 7) == 0);

    ana_clear(4);
    ana_draw_image_frame(image, 1, -1, -1);
    assert(ana_gfx_draw_pixel(0, 0) == 3);
    assert(ana_gfx_draw_pixel(1, 0) == 3);
    assert(ana_gfx_draw_pixel(2, 0) == 4);

    ana_draw_image_frame(image, 99, 0, 0);
    assert(ana_gfx_draw_pixel(0, 0) == 3);

    ana_draw_image(image, 400, 400);
    assert(ana_gfx_draw_pixel(0, 0) == 3);

    memory_image = ana_load_image_data(
        memory_test_image_bytes,
        (long)sizeof(memory_test_image_bytes));
    assert(memory_image != 0);
    assert(ana_image_width(memory_image) == 3);
    assert(ana_image_height(memory_image) == 2);
    assert(ana_image_frame_count(memory_image) == 2);

    ana_clear(8);
    ana_draw_image_frame(memory_image, 1, 1, 1);
    assert(ana_gfx_draw_pixel(1, 1) == 2);
    assert(ana_gfx_draw_pixel(2, 1) == 2);
    assert(ana_gfx_draw_pixel(3, 1) == 2);
    assert(ana_gfx_draw_pixel(1, 2) == 3);
    assert(ana_gfx_draw_pixel(2, 2) == 3);
    assert(ana_gfx_draw_pixel(3, 2) == 3);

    ana_gfx_close();
    ana_free_image(memory_image);
    ana_free_image(image);
    remove(path);
}

static void test_font_loading_and_drawing(void)
{
    ANA_Font font;
    ANA_Font file_font;
    FILE* file;
    const char* path;

    font = ana_load_font_data(
        memory_test_font_bytes,
        (long)sizeof(memory_test_font_bytes));
    assert(font != 0);
    assert(ana_text_width(font, "012") == 9);
    assert(ana_text_width(font, 0) == 0);

    path = "build/tests/gfx_test_font.anafnt";
    file = fopen(path, "wb");
    assert(file != 0);
    fwrite(
        memory_test_font_bytes,
        1u,
        sizeof(memory_test_font_bytes),
        file);
    fclose(file);

    file_font = ana_load_font(path);
    assert(file_font != 0);

    assert(ana_gfx_open(ana_default_profile()) == ANA_OK);
    ana_clear(0);
    ana_draw_text(file_font, 1, 1, "012");
    assert(ana_gfx_draw_pixel(1, 1) == 1);
    assert(ana_gfx_draw_pixel(2, 1) == 1);
    assert(ana_gfx_draw_pixel(3, 1) == 1);
    assert(ana_gfx_draw_pixel(4, 1) == 0);
    assert(ana_gfx_draw_pixel(5, 1) == 1);
    assert(ana_gfx_draw_pixel(6, 1) == 0);
    assert(ana_gfx_draw_pixel(7, 1) == 1);
    assert(ana_gfx_draw_pixel(8, 1) == 0);
    assert(ana_gfx_draw_pixel(9, 1) == 1);

    ana_clear(0);
    ana_draw_int(file_font, 2, 4, 20);
    assert(ana_gfx_draw_pixel(2, 4) == 1);
    assert(ana_gfx_draw_pixel(3, 4) == 0);
    assert(ana_gfx_draw_pixel(4, 4) == 1);
    assert(ana_gfx_draw_pixel(5, 4) == 1);
    assert(ana_gfx_draw_pixel(6, 4) == 1);
    assert(ana_gfx_draw_pixel(7, 4) == 1);

    ana_set_font_color(file_font, 5);
    ana_clear(0);
    ana_draw_text(file_font, 1, 1, "0");
    assert(ana_gfx_draw_pixel(1, 1) == 5);
    assert(ana_gfx_draw_pixel(2, 1) == 5);
    assert(ana_gfx_draw_pixel(3, 1) == 5);

    ana_set_font_color(file_font, 18);
    ana_clear(0);
    ana_draw_int(file_font, 2, 4, 2);
    assert(ana_gfx_draw_pixel(2, 4) == 2);
    assert(ana_gfx_draw_pixel(3, 4) == 0);
    assert(ana_gfx_draw_pixel(4, 4) == 2);

    ana_gfx_close();
    ana_free_font(file_font);
    ana_free_font(font);
    remove(path);
}

static void test_image_rejects_invalid_files(void)
{
    const char* path;
    FILE* file;

    path = "build/tests/gfx_invalid_image.anaimg";
    file = fopen(path, "wb");
    assert(file != 0);
    fwrite("NOPE", 1u, 4u, file);
    fclose(file);

    assert(ana_load_image(0) == 0);
    assert(ana_load_image("build/tests/missing_image.anaimg") == 0);
    assert(ana_load_image(path) == 0);
    assert(ana_load_image_data(0, 0L) == 0);
    assert(ana_load_image_data(memory_test_image_bytes, 4L) == 0);
    assert(ana_load_font(0) == 0);
    assert(ana_load_font("build/tests/missing_font.anafnt") == 0);
    assert(ana_load_font_data(0, 0L) == 0);
    assert(ana_load_font_data(memory_test_font_bytes, 4L) == 0);
    assert(ana_image_width(0) == 0);
    assert(ana_image_height(0) == 0);
    assert(ana_image_frame_count(0) == 0);

    remove(path);
}

int main(void)
{
    test_clear_and_present();
    test_palette_accepts_supported_colors();
    test_image_loading_and_drawing();
    test_font_loading_and_drawing();
    test_image_rejects_invalid_files();

    return 0;
}

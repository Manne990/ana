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

static const unsigned char memory_test_mask16_image_bytes[] = {
    0x41, 0x4e, 0x41, 0x49, 0x4d, 0x47, 0x30, 0x31,
    0x10, 0x00, 0x02, 0x00, 0x01, 0x00, 0x04, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0xff, 0x00, 0x00, 0xff,
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

static const unsigned char memory_test_unmasked2_image_bytes[] = {
    0x41, 0x4e, 0x41, 0x49, 0x4d, 0x47, 0x30, 0x31,
    0x02, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x80, 0x80, 0x40, 0x80
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

static int retained_redraw_calls;
static ANA_Rect retained_redraw_rect;

static void retained_test_redraw(ANA_Rect rect, void* user_data)
{
    int* marker;

    marker = (int*)user_data;
    retained_redraw_calls++;
    retained_redraw_rect = rect;
    if (marker != 0) {
        *marker = 123;
    }

    if (!ana_rect_is_empty(rect)) {
        ana_fill_rect(6, rect.x, rect.y, rect.w, rect.h);
    }
}

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
    ANA_Image mask16_image;
    ANA_Image unmasked2_image;

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

    mask16_image = ana_load_image_data(
        memory_test_mask16_image_bytes,
        (long)sizeof(memory_test_mask16_image_bytes));
    assert(mask16_image != 0);
    assert(ana_image_width(mask16_image) == 16);
    assert(ana_image_height(mask16_image) == 2);

    ana_clear(8);
    ana_draw_image(mask16_image, 10, 10);
    assert(ana_gfx_draw_pixel(10, 10) == 1);
    assert(ana_gfx_draw_pixel(17, 10) == 1);
    assert(ana_gfx_draw_pixel(18, 10) == 8);
    assert(ana_gfx_draw_pixel(25, 10) == 8);
    assert(ana_gfx_draw_pixel(10, 11) == 8);
    assert(ana_gfx_draw_pixel(17, 11) == 8);
    assert(ana_gfx_draw_pixel(18, 11) == 1);
    assert(ana_gfx_draw_pixel(25, 11) == 1);

    unmasked2_image = ana_load_image_data(
        memory_test_unmasked2_image_bytes,
        (long)sizeof(memory_test_unmasked2_image_bytes));
    assert(unmasked2_image != 0);
    assert(ana_image_width(unmasked2_image) == 2);
    assert(ana_image_height(unmasked2_image) == 2);

    ana_clear(8);
    ana_draw_image(unmasked2_image, 30, 30);
    assert(ana_gfx_draw_pixel(30, 30) == 1);
    assert(ana_gfx_draw_pixel(31, 30) == 2);
    assert(ana_gfx_draw_pixel(30, 31) == 3);
    assert(ana_gfx_draw_pixel(31, 31) == 0);

    ana_gfx_close();
    ana_free_image(unmasked2_image);
    ana_free_image(mask16_image);
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

static void test_retained_render_helpers(void)
{
    ANA_Image image;
    ANA_Font font;
    ANA_Bob bob;
    ANA_RetainedLayer retained_layer;
    ANA_DrawLayer draw_layer;
    ANA_Label label;
    ANA_Rect rect;
    int marker;

    image = ana_load_image_data(
        memory_test_image_bytes,
        (long)sizeof(memory_test_image_bytes));
    assert(image != 0);

    font = ana_load_font_data(
        memory_test_font_bytes,
        (long)sizeof(memory_test_font_bytes));
    assert(font != 0);

    assert(ana_gfx_open(ana_default_profile()) == ANA_OK);
    ana_clear(9);

    ana_bob_init(&bob, image);
    bob.clear_color = 4u;
    ana_bob_set_frame(&bob, 1);
    ana_bob_set_position(&bob, 10, 11);
    rect = ana_bob_rect(&bob);
    assert(rect.x == 10);
    assert(rect.y == 11);
    assert(rect.w == 3);
    assert(rect.h == 2);
    assert(ana_rect_is_empty(ana_bob_previous_rect(&bob)));

    ana_bob_draw(&bob);
    assert(ana_gfx_draw_pixel(10, 11) == 2);
    ana_bob_commit(&bob);
    assert(ana_bob_is_unchanged(&bob));
    ana_bob_set_image(&bob, 0);
    rect = ana_bob_previous_rect(&bob);
    assert(rect.x == 10);
    assert(rect.y == 11);
    assert(rect.w == 3);
    assert(rect.h == 2);
    ana_bob_set_image(&bob, image);

    ana_bob_set_position(&bob, 20, 21);
    assert(!ana_bob_is_unchanged(&bob));
    ana_bob_clear_previous(&bob);
    assert(ana_gfx_draw_pixel(10, 11) == 4);
    ana_bob_draw(&bob);
    assert(ana_gfx_draw_pixel(20, 21) == 2);
    ana_bob_commit(&bob);

    retained_redraw_calls = 0;
    marker = 0;
    retained_layer.redraw = retained_test_redraw;
    retained_layer.user_data = &marker;
    ana_bob_set_position(&bob, 30, 31);
    rect = ana_bob_clear_previous_x8_with_layers(
        &bob,
        0,
        ANA_DEFAULT_WIDTH,
        &retained_layer,
        1);
    assert(rect.x == 16);
    assert(rect.y == 21);
    assert(rect.w == 8);
    assert(rect.h == 2);
    assert(retained_redraw_calls == 1);
    assert(retained_redraw_rect.x == 16);
    assert(marker == 123);
    ana_bob_clear_previous_with_layers(&bob, &retained_layer, 1);
    assert(retained_redraw_calls == 2);
    assert(retained_redraw_rect.x == 20);
    assert(retained_redraw_rect.y == 21);
    assert(marker == 123);
    assert(ana_gfx_draw_pixel(20, 21) == 6);

    ana_clear(9);
    bob.clear_color = 4u;
    ana_bob_set_frame(&bob, 0);
    ana_bob_set_position(&bob, 40, 40);
    ana_bob_draw(&bob);
    ana_bob_commit(&bob);
    ana_bob_set_position(&bob, 50, 50);
    rect = ana_bob_clear_previous_masked_x8_with_layers(
        &bob,
        0,
        ANA_DEFAULT_WIDTH,
        0,
        0);
    assert(rect.x == 40);
    assert(rect.y == 40);
    assert(rect.w == 8);
    assert(rect.h == 2);
    assert(ana_gfx_draw_pixel(40, 40) == 4);
    assert(ana_gfx_draw_pixel(41, 40) == 4);
    assert(ana_gfx_draw_pixel(42, 40) == 9);
    assert(ana_gfx_draw_pixel(40, 41) == 9);
    assert(ana_gfx_draw_pixel(41, 41) == 4);
    assert(ana_gfx_draw_pixel(42, 41) == 4);

    draw_layer.redraw = retained_test_redraw;
    draw_layer.user_data = &marker;
    draw_layer.dirty = 0;
    ana_layer_mark_dirty(&draw_layer);
    assert(draw_layer.dirty);
    ana_layer_draw_if_dirty(&draw_layer);
    assert(!draw_layer.dirty);
    assert(retained_redraw_calls == 3);
    assert(ana_rect_is_empty(retained_redraw_rect));

    ana_label_init(&label, font, 5, 6, 24);
    label.color = 5u;
    ana_label_set_text(&label, "012");
    assert(label.dirty);
    ana_label_draw_if_dirty(&label);
    assert(!label.dirty);
    assert(ana_gfx_draw_pixel(5, 6) == 5);
    ana_label_set_text(&label, "012");
    assert(!label.dirty);

    ana_label_set_text(&label, "1");
    assert(label.dirty);
    ana_label_draw_if_dirty(&label);
    assert(!label.dirty);
    assert(ana_gfx_draw_pixel(5, 6) == 0);
    assert(ana_gfx_draw_pixel(6, 6) == 5);

    ana_gfx_close();
    ana_free_font(font);
    ana_free_image(image);
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
    test_retained_render_helpers();
    test_image_rejects_invalid_files();

    return 0;
}

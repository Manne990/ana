#include "ana.h"

#include <stdio.h>

#define HUD_FONT_WIDTH 6
#define HUD_FONT_HEIGHT 7
#define HUD_FONT_FIRST_CHAR '0'
#define HUD_FONT_CHAR_COUNT 43
#define HUD_FONT_HEADER_SIZE 16
#define HUD_FONT_IMAGE_HEADER_SIZE 20
#define HUD_FONT_FRAME_SIZE (HUD_FONT_HEIGHT * 2)
#define HUD_FONT_DATA_SIZE \
    (HUD_FONT_HEADER_SIZE + HUD_FONT_IMAGE_HEADER_SIZE + \
        (HUD_FONT_CHAR_COUNT * HUD_FONT_FRAME_SIZE))

static const unsigned char player_image_data[] = {
    0x41, 0x4e, 0x41, 0x49, 0x4d, 0x47, 0x30, 0x31,
    0x10, 0x00, 0x0a, 0x00, 0x01, 0x00, 0x04, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x07, 0x80, 0x0f, 0xc0, 0x1f, 0xe0,
    0x3f, 0xf0, 0x7f, 0xf8, 0xff, 0xfc, 0x7f, 0xf8,
    0x3b, 0x70, 0x04, 0x80,
    0x03, 0x00, 0x07, 0x80, 0x0f, 0xc0, 0x1f, 0xe0,
    0x3f, 0xf0, 0x7f, 0xf8, 0xff, 0xfc, 0x7f, 0xf8,
    0x38, 0x70, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x07, 0x80,
    0x0f, 0xc0, 0x1f, 0xe0, 0x3f, 0xf0, 0x10, 0x20,
    0x03, 0x00, 0x04, 0x80,
    0x03, 0x00, 0x07, 0x80, 0x0c, 0xc0, 0x18, 0x60,
    0x30, 0x30, 0x60, 0x18, 0xc0, 0x0c, 0x6f, 0xd8,
    0x38, 0x70, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

static const unsigned char bullet_image_data[] = {
    0x41, 0x4e, 0x41, 0x49, 0x4d, 0x47, 0x30, 0x31,
    0x02, 0x00, 0x06, 0x00, 0x01, 0x00, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0
};

static const unsigned char hud_font_glyphs[HUD_FONT_CHAR_COUNT][HUD_FONT_HEIGHT] = {
    { 0x70, 0x88, 0x98, 0xa8, 0xc8, 0x88, 0x70 },
    { 0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x70 },
    { 0x70, 0x88, 0x08, 0x10, 0x20, 0x40, 0xf8 },
    { 0xf0, 0x08, 0x08, 0x70, 0x08, 0x08, 0xf0 },
    { 0x10, 0x30, 0x50, 0x90, 0xf8, 0x10, 0x10 },
    { 0xf8, 0x80, 0x80, 0xf0, 0x08, 0x08, 0xf0 },
    { 0x70, 0x80, 0x80, 0xf0, 0x88, 0x88, 0x70 },
    { 0xf8, 0x08, 0x10, 0x20, 0x40, 0x40, 0x40 },
    { 0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70 },
    { 0x70, 0x88, 0x88, 0x78, 0x08, 0x08, 0x70 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x70, 0x88, 0x88, 0xf8, 0x88, 0x88, 0x88 },
    { 0xf0, 0x88, 0x88, 0xf0, 0x88, 0x88, 0xf0 },
    { 0x78, 0x80, 0x80, 0x80, 0x80, 0x80, 0x78 },
    { 0xf0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xf0 },
    { 0xf8, 0x80, 0x80, 0xf0, 0x80, 0x80, 0xf8 },
    { 0xf8, 0x80, 0x80, 0xf0, 0x80, 0x80, 0x80 },
    { 0x78, 0x80, 0x80, 0x98, 0x88, 0x88, 0x78 },
    { 0x88, 0x88, 0x88, 0xf8, 0x88, 0x88, 0x88 },
    { 0x70, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70 },
    { 0x08, 0x08, 0x08, 0x08, 0x88, 0x88, 0x70 },
    { 0x88, 0x90, 0xa0, 0xc0, 0xa0, 0x90, 0x88 },
    { 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xf8 },
    { 0x88, 0xd8, 0xa8, 0xa8, 0x88, 0x88, 0x88 },
    { 0x88, 0xc8, 0xa8, 0x98, 0x88, 0x88, 0x88 },
    { 0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70 },
    { 0xf0, 0x88, 0x88, 0xf0, 0x80, 0x80, 0x80 },
    { 0x70, 0x88, 0x88, 0x88, 0xa8, 0x90, 0x68 },
    { 0xf0, 0x88, 0x88, 0xf0, 0xa0, 0x90, 0x88 },
    { 0x78, 0x80, 0x80, 0x70, 0x08, 0x08, 0xf0 },
    { 0xf8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 },
    { 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70 },
    { 0x88, 0x88, 0x88, 0x88, 0x88, 0x50, 0x20 },
    { 0x88, 0x88, 0x88, 0xa8, 0xa8, 0xa8, 0x50 },
    { 0x88, 0x88, 0x50, 0x20, 0x50, 0x88, 0x88 },
    { 0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x20 },
    { 0xf8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xf8 }
};

static unsigned char hud_font_data[HUD_FONT_DATA_SIZE];
static ANA_Image player_image = 0;
static ANA_Image bullet_image = 0;
static ANA_Font hud_font = 0;
static int player_x = 152;
static int player_y = 220;
static int bullet_x = 0;
static int bullet_y = 0;
static int bullet_active = 0;
static int score = 0;
static int lives = 3;
static int demo_ticks = 0;
static int invaders_assets_loaded = 0;

static void invaders_write_u16_le(unsigned char* bytes, int value)
{
    bytes[0] = (unsigned char)(value & 0xff);
    bytes[1] = (unsigned char)((value >> 8) & 0xff);
}

static void invaders_build_hud_font_data(void)
{
    unsigned char* out;
    int glyph;
    int row;

    hud_font_data[0] = 'A';
    hud_font_data[1] = 'N';
    hud_font_data[2] = 'A';
    hud_font_data[3] = 'F';
    hud_font_data[4] = 'N';
    hud_font_data[5] = 'T';
    hud_font_data[6] = '0';
    hud_font_data[7] = '1';
    invaders_write_u16_le(hud_font_data + 8, HUD_FONT_WIDTH);
    invaders_write_u16_le(hud_font_data + 10, HUD_FONT_HEIGHT);
    hud_font_data[12] = HUD_FONT_FIRST_CHAR;
    hud_font_data[13] = HUD_FONT_CHAR_COUNT;
    hud_font_data[14] = 0u;
    hud_font_data[15] = 0u;

    hud_font_data[16] = 'A';
    hud_font_data[17] = 'N';
    hud_font_data[18] = 'A';
    hud_font_data[19] = 'I';
    hud_font_data[20] = 'M';
    hud_font_data[21] = 'G';
    hud_font_data[22] = '0';
    hud_font_data[23] = '1';
    invaders_write_u16_le(hud_font_data + 24, HUD_FONT_WIDTH);
    invaders_write_u16_le(hud_font_data + 26, HUD_FONT_HEIGHT);
    invaders_write_u16_le(hud_font_data + 28, HUD_FONT_CHAR_COUNT);
    hud_font_data[30] = 1u;
    hud_font_data[31] = 1u;
    hud_font_data[32] = 0u;
    hud_font_data[33] = 0u;
    hud_font_data[34] = 0u;
    hud_font_data[35] = 0u;

    out = hud_font_data + HUD_FONT_HEADER_SIZE + HUD_FONT_IMAGE_HEADER_SIZE;
    for (glyph = 0; glyph < HUD_FONT_CHAR_COUNT; glyph++) {
        for (row = 0; row < HUD_FONT_HEIGHT; row++) {
            *out++ = hud_font_glyphs[glyph][row];
        }

        for (row = 0; row < HUD_FONT_HEIGHT; row++) {
            *out++ = hud_font_glyphs[glyph][row];
        }
    }
}

static void invaders_init(void)
{
    player_x = 152;
    player_y = 220;
    bullet_x = 0;
    bullet_y = 0;
    bullet_active = 0;
    score = 0;
    lives = 3;
    demo_ticks = 0;
    invaders_assets_loaded = 0;

    ana_input_clear_key_map();
    ana_input_map_key_to_direction(ANA_KEY_LEFT, ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT);
    ana_input_map_key_to_direction(ANA_KEY_A, ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT);
    ana_input_map_key_to_direction(ANA_KEY_RIGHT, ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT);
    ana_input_map_key_to_direction(ANA_KEY_D, ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT);
    ana_input_map_key_to_action(ANA_KEY_SPACE, ANA_INPUT_DEVICE_0, ANA_ACTION_1);
    ana_input_map_key_to_quit(ANA_KEY_ESCAPE);
}

static void invaders_load(void)
{
    invaders_build_hud_font_data();

    player_image = ana_load_image_data(
        player_image_data,
        (long)sizeof(player_image_data));
    bullet_image = ana_load_image_data(
        bullet_image_data,
        (long)sizeof(bullet_image_data));
    hud_font = ana_load_font_data(
        hud_font_data,
        (long)sizeof(hud_font_data));
    if (hud_font != 0) {
        ana_set_font_color(hud_font, 5);
    }

    invaders_assets_loaded =
        player_image != 0 && bullet_image != 0 && hud_font != 0;
}

static void invaders_update(ANA_Time time)
{
    int player_width;
    int bullet_width;
    int bullet_height;

    demo_ticks = time.tick;
    player_width = player_image != 0 ? ana_image_width(player_image) : 16;
    bullet_width = bullet_image != 0 ? ana_image_width(bullet_image) : 2;
    bullet_height = bullet_image != 0 ? ana_image_height(bullet_image) : 6;

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        player_x -= 2;
    }

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        player_x += 2;
    }

    if (player_x < 0) {
        player_x = 0;
    }

    if (player_x > ANA_DEFAULT_WIDTH - player_width) {
        player_x = ANA_DEFAULT_WIDTH - player_width;
    }

    if (ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_1) &&
            !bullet_active) {
        bullet_active = 1;
        bullet_x = player_x + (player_width / 2) - (bullet_width / 2);
        bullet_y = player_y - bullet_height;
        score += 10;
    }

    if (bullet_active) {
        bullet_y -= 4;
        if (bullet_y <= -bullet_height) {
            bullet_active = 0;
        }
    }

    if (ana_quit_requested()) {
        ana_quit();
    }

    if (time.tick >= 3000) {
        ana_quit();
    }
}

static void invaders_draw(void)
{
    ana_clear(0);

    if (hud_font != 0) {
        ana_draw_text(hud_font, 16, 16, "SCORE");
        ana_draw_int(
            hud_font,
            16 + ana_text_width(hud_font, "SCORE "),
            16,
            score);
        ana_draw_text(hud_font, 200, 16, "LIVES");
        ana_draw_int(
            hud_font,
            200 + ana_text_width(hud_font, "LIVES "),
            16,
            lives);
        ana_draw_text(hud_font, 16, 30, "STATUS");
        ana_draw_text(
            hud_font,
            16 + ana_text_width(hud_font, "STATUS "),
            30,
            bullet_active ? "SHOT" : "READY");
    }

    if (player_image != 0) {
        ana_draw_image(player_image, player_x, player_y);
    }

    if (bullet_active && bullet_image != 0) {
        ana_draw_image(bullet_image, bullet_x, bullet_y);
    }
}

static void invaders_shutdown(void)
{
    ana_free_font(hud_font);
    ana_free_image(bullet_image);
    ana_free_image(player_image);
    hud_font = 0;
    bullet_image = 0;
    player_image = 0;
}

int main(void)
{
    ANA_Game game;
    int result;

    game.init = invaders_init;
    game.load = invaders_load;
    game.update = invaders_update;
    game.draw = invaders_draw;
    game.shutdown = invaders_shutdown;
    game.width = 320;
    game.height = 256;
    game.fps = 50;
    game.colors = 16;
    game.screen_mode = ANA_SCREEN_PAL_LORES;

    printf("ANA invaders started.\n");
    printf("Keyboard mapping: cursor/A-D movement, Space action, Esc quit.\n");
    printf("Move left/right and press Space/fire to shoot.\n");

    result = ana_run(&game);

    printf("ANA invaders finished with %s.\n", ana_result_name((ANA_Result)result));
    printf("Frames simulated: %d\n", demo_ticks + 1);
    printf("Player X: %d\n", player_x);
    printf("Score: %d\n", score);
    printf("Assets loaded: %s\n", invaders_assets_loaded ? "yes" : "no");
    printf("Type invaders to run it again.\n");

    if (result == ANA_OK && !invaders_assets_loaded) {
        return ANA_ERROR_NOT_IMPLEMENTED;
    }

    return result;
}

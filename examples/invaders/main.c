#include "ana.h"

#include <stdio.h>

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

static ANA_Image player_image = 0;
static int player_x = 152;
static int player_y = 220;
static int demo_ticks = 0;

static void invaders_init(void)
{
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
    player_image = ana_load_image_data(
        player_image_data,
        (long)sizeof(player_image_data));
}

static void invaders_update(ANA_Time time)
{
    int player_width;

    demo_ticks = time.tick;
    player_width = player_image != 0 ? ana_image_width(player_image) : 16;

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

    if (player_image != 0) {
        ana_draw_image(player_image, player_x, player_y);
    }
}

static void invaders_shutdown(void)
{
    ana_free_image(player_image);
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
    printf("Move the player sprite left and right.\n");

    result = ana_run(&game);

    printf("ANA invaders finished with %s.\n", ana_result_name((ANA_Result)result));
    printf("Frames simulated: %d\n", demo_ticks + 1);
    printf("Player X: %d\n", player_x);
    printf("Type invaders to run it again.\n");

    return result;
}

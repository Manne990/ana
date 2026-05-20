#include "ana.h"

static int player_x = 152;
static int demo_ticks = 0;

static void invaders_update(ANA_Time time)
{
    demo_ticks = time.tick;

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        player_x--;
    }

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        player_x++;
    }

    if (ana_quit_requested()) {
        ana_quit();
    }
}

static void invaders_draw(void)
{
    unsigned char color;

    color = (unsigned char)((demo_ticks + (player_x & 0x0f)) & 0x0f);
    ana_clear(color);
}

int main(void)
{
    ANA_Game game;

    game.init = 0;
    game.load = 0;
    game.update = invaders_update;
    game.draw = invaders_draw;
    game.shutdown = 0;
    game.width = 320;
    game.height = 256;
    game.fps = 50;
    game.colors = 16;
    game.screen_mode = ANA_SCREEN_PAL_LORES;

    return ana_run(&game);
}

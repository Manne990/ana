#include "ana.h"

static int hello_ticks = 0;

static void hello_update(ANA_Time time)
{
    hello_ticks = time.tick;

    if (time.tick >= 2) {
        ana_quit();
    }
}

static void hello_draw(void)
{
    ana_clear((unsigned char)(hello_ticks & 0x0f));
}

int main(void)
{
    ANA_Game game;

    game.init = 0;
    game.load = 0;
    game.update = hello_update;
    game.draw = hello_draw;
    game.shutdown = 0;
    game.width = ANA_DEFAULT_WIDTH;
    game.height = ANA_DEFAULT_HEIGHT;
    game.fps = ANA_DEFAULT_FPS;
    game.colors = ANA_DEFAULT_COLORS;
    game.screen_mode = ANA_SCREEN_PAL_LORES;

    return ana_run(&game);
}


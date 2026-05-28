#include "ana.h"

#include <stdio.h>

static int hello_ticks = 0;

static void hello_update(ANA_Time time)
{
    hello_ticks = time.tick;

    if (time.tick >= 250) {
        ana_quit();
    }
}

static void hello_draw(void)
{
    ana_clear((unsigned char)(hello_ticks & 0x0f));
}

int main(void)
{
    ANA_Game game = {0};
    int result;

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
    game.render_mode = ANA_RENDER_DIRTY;
    game.debug_stats = 1;

    printf("ANA hello started.\n");
    printf("Opening ANA PAL lores screen for about five seconds.\n");

    result = ana_run(&game);

    printf("ANA hello finished with %s.\n", ana_result_name((ANA_Result)result));
    printf("Type hello to run it again.\n");

    return result;
}

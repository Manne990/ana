#include "ana.h"
#include "invaders_game.h"

#include <stdio.h>

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
    invaders_print_run_summary();
#ifdef ANA_INVADERS_DEBUG_STATS
    invaders_print_run_stats();
#endif
    printf("Player X: %d\n", invaders_player_x());
    printf("Score: %d\n", invaders_score());
    printf("Invaders remaining: %d\n", invaders_remaining_count());
    printf("Assets loaded: %s\n", invaders_assets_are_loaded() ? "yes" : "no");
    printf("Type invaders to run it again.\n");

    if (result == ANA_OK && !invaders_assets_are_loaded()) {
        return ANA_ERROR_NOT_IMPLEMENTED;
    }

    return result;
}

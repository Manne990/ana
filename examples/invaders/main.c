/* Entry point for the ANA Invaders example. Wires the game callbacks and
   runtime profile into ANA, then prints a small AmigaDOS run summary. */

#include "ana.h"
#include "invaders_game.h"

#include <stdio.h>

int main(void)
{
    ANA_Game game = {0};
    int result;

    game.init = invaders_init;
    game.load = invaders_load;
    game.update = invaders_update;
    game.draw = invaders_draw;
    game.shutdown = invaders_shutdown;
    game.width = ANA_DEFAULT_WIDTH;
    game.height = ANA_DEFAULT_HEIGHT;
    game.fps = ANA_DEFAULT_FPS;
    game.colors = ANA_DEFAULT_COLORS;
    game.screen_mode = ANA_SCREEN_PAL_LORES;
    game.debug_stats = 1;

    printf("ANA invaders started.\n");
    printf("Keyboard mapping: cursor/A-D movement, Space action, Esc quit.\n");
    printf("Move left/right and press Space/fire to shoot.\n");

    result = ana_run(&game);

    printf("ANA invaders finished with %s.\n", ana_result_name((ANA_Result)result));
    printf("Assets loaded: %s\n", invaders_assets_are_loaded() ? "yes" : "no");
    printf("Type invaders to run it again.\n");

    if (result == ANA_OK && !invaders_assets_are_loaded()) {
        return ANA_ERROR_NOT_IMPLEMENTED;
    }

    return result;
}

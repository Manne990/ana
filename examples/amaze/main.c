/* Entry point for the ANA AMAze example. Wires the game callbacks and runtime
   profile into ANA, then prints a small AmigaDOS run summary. */

#include "ana.h"
#include "amaze_game.h"

#include <stdio.h>

int main(void)
{
    ANA_Game game = {0};
    int result;

    game.init = amaze_init;
    game.load = amaze_load;
    game.update = amaze_update;
    game.draw = amaze_draw;
    game.shutdown = amaze_shutdown;
    game.width = ANA_DEFAULT_WIDTH;
    game.height = ANA_DEFAULT_HEIGHT;
    game.fps = ANA_DEFAULT_FPS;
    game.colors = ANA_DEFAULT_COLORS;
    game.screen_mode = ANA_SCREEN_PAL_LORES;
    game.render_mode = ANA_RENDER_DIRTY;
    game.debug_stats = 1;

    printf("ANA AMAze started.\n");
    printf("Keyboard mapping: cursor/WASD movement, Space reset, Esc quit.\n");
    printf("Collect coins and gold bags while avoiding the collectors.\n");

    result = ana_run(&game);

    printf("ANA AMAze finished with %s.\n", ana_result_name((ANA_Result)result));
    printf("Assets loaded: %s\n", amaze_assets_are_loaded() ? "yes" : "no");
    printf("Type amaze to run it again.\n");

    if (result == ANA_OK && !amaze_assets_are_loaded()) {
        return ANA_ERROR_NOT_IMPLEMENTED;
    }

    return result;
}

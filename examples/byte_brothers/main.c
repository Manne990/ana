#include "ana.h"

#include "byte_brothers_assets.h"
#include "byte_brothers_game.h"
#include "byte_brothers_internal.h"

#include <stdio.h>

/* Program entry point for the Byte Brothers platformer sample. */

int main(void)
{
    ANA_Game game = {0};
    ANA_Result result;

    game.load = byte_brothers_load;
    game.update = byte_brothers_update;
    game.draw = byte_brothers_draw;
    game.shutdown = byte_brothers_shutdown;
    game.width = BB_SCREEN_W;
    game.height = BB_SCREEN_H;
    game.fps = 50;
    game.colors = 16;
    game.screen_mode = ANA_SCREEN_PAL_LORES;
    game.render_mode = ANA_RENDER_TILE_SCROLL;
    game.debug_stats = 1;

    printf("ANA Byte Brothers started.\n");
    printf("Keyboard mapping: cursor/A-D movement, Space jump, X dash, Esc quit\n");
    printf(".\n");
    printf("Run, jump, collect code fragments, and enter the portal.\n");

    result = ana_run(&game);

    printf("ANA Byte Brothers finished with %s.\n", ana_result_name(result));
    printf("Assets loaded: %s\n", bb_assets_loaded() ? "yes" : "no");
    printf("Type byte_brothers to run it again.\n");

    if (result == ANA_OK && !bb_assets_loaded()) {
        return 1;
    }

    return result == ANA_OK ? 0 : 1;
}

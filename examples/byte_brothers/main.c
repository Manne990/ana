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

    game.init = byte_brothers_init;
    game.load = byte_brothers_load;
    game.update = byte_brothers_update;
    game.draw = byte_brothers_draw;
    game.shutdown = byte_brothers_shutdown;
    game.width = BB_SCREEN_W;
    game.height = BB_SCREEN_H;
    game.fps = ANA_DEFAULT_FPS;
    game.colors = ANA_DEFAULT_COLORS;
    game.screen_mode = ANA_SCREEN_PAL_LORES;
    game.render_mode = ANA_RENDER_SIDE_SCROLL;
    game.debug_stats = 1;
    game.warmup_frames = 1;

    printf(
        "ANA Byte Brothers %s sprite-profile-2 started.\n",
        ANA_VERSION_STRING);
    printf("Keyboard mapping: cursor/A-D movement, Space/up jump, X/down dash, Esc/C/Q quit\n");
    printf(".\n");
    printf("Run, jump, collect code fragments, and enter the portal.\n");

    result = ana_run(&game);

    printf("ANA Byte Brothers finished with %s.\n", ana_result_name(result));
    printf("Assets loaded: %s\n", bb_assets_loaded() ? "yes" : "no");
    printf("Type byte_brothers to run it again.\n");

    if (result == ANA_OK && !bb_assets_loaded()) {
        return ANA_ERROR_NOT_IMPLEMENTED;
    }

    return result == ANA_OK ? 0 : 1;
}

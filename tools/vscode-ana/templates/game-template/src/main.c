#include <ana.h>
#include "game.h"

int main(void)
{
    ANA_Game game = {0};

    game.init = game_init;
    game.update = game_update;
    game.draw = game_draw;
    game.width = ANA_DEFAULT_WIDTH;
    game.height = ANA_DEFAULT_HEIGHT;
    game.fps = ANA_DEFAULT_FPS;
    game.colors = ANA_DEFAULT_COLORS;
    game.screen_mode = ANA_SCREEN_PAL_LORES;
    game.render_mode = ANA_RENDER_DIRTY;

    return ana_run(&game);
}

#include "ana.h"
#include "voidstrike_game.h"
int main(void) { ANA_Game game = {0}; game.init=voidstrike_init; game.load=voidstrike_load; game.update=voidstrike_update; game.draw=voidstrike_draw; game.shutdown=voidstrike_shutdown; game.width=ANA_DEFAULT_WIDTH; game.height=ANA_DEFAULT_HEIGHT; game.fps=ANA_DEFAULT_FPS; game.colors=ANA_DEFAULT_COLORS; game.screen_mode=ANA_SCREEN_PAL_LORES; game.render_mode=ANA_RENDER_DIRTY; game.debug_stats=1; return ana_run(&game); }

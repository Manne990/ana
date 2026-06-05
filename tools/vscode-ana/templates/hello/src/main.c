#include <ana.h>

static void game_init(void)
{
    ana_input_clear_key_map();
    ana_input_map_default_keys(ANA_INPUT_DEVICE_0);
}

static void game_update(ANA_Time time)
{
    (void)time;
}

static void game_draw(void)
{
    ana_clear(0);
    ana_fill_rect(2, 32, 80, 256, 48);
    ana_fill_rect(4, 48, 96, 224, 16);
}

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

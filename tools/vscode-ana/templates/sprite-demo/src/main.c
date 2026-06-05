#include <ana.h>

static ANA_Image player;
static int player_x = 120;

static void game_init(void)
{
    ana_input_clear_key_map();
    ana_input_map_default_keys(ANA_INPUT_DEVICE_0);
}

static void game_load(void)
{
    player = ana_load_image("assets/player.anaimg");
}

static void game_update(ANA_Time time)
{
    (void)time;

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        player_x--;
    }

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        player_x++;
    }
}

static void game_draw(void)
{
    ana_clear(0);
    ana_draw_image(player, player_x, 120);
}

static void game_shutdown(void)
{
    ana_free_image(player);
}

int main(void)
{
    ANA_Game game = {0};

    game.init = game_init;
    game.load = game_load;
    game.update = game_update;
    game.draw = game_draw;
    game.shutdown = game_shutdown;
    game.width = ANA_DEFAULT_WIDTH;
    game.height = ANA_DEFAULT_HEIGHT;
    game.fps = ANA_DEFAULT_FPS;
    game.colors = ANA_DEFAULT_COLORS;
    game.screen_mode = ANA_SCREEN_PAL_LORES;
    game.render_mode = ANA_RENDER_DIRTY;

    return ana_run(&game);
}

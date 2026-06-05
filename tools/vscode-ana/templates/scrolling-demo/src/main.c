#include <ana.h>

static int camera_x = 0;

static void game_init(void)
{
    ana_input_clear_key_map();
    ana_input_map_default_keys(ANA_INPUT_DEVICE_0);
}

static void game_update(ANA_Time time)
{
    (void)time;

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        camera_x--;
    }

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        camera_x++;
    }
}

static void game_draw(void)
{
    int x;

    ana_clear(0);

    for (x = 0; x < ANA_DEFAULT_WIDTH; x += 16) {
        ana_fill_rect((unsigned char)((x + camera_x) / 16 % 15 + 1), x, 160, 12, 12);
    }
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
    game.render_mode = ANA_RENDER_SIDE_SCROLL;

    return ana_run(&game);
}

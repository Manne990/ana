#include "game.h"

static int player_x = 100;

void game_init(void)
{
    ana_input_clear_key_map();
    ana_input_map_default_keys(ANA_INPUT_DEVICE_0);
}

void game_update(ANA_Time time)
{
    (void)time;

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        player_x--;
    }

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        player_x++;
    }
}

void game_draw(void)
{
    ana_clear(0);
    ana_fill_rect(2, player_x, 220, 16, 8);
}

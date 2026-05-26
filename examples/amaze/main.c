/* AMAze is a compact maze-chase sample for ANA. It demonstrates fixed-tick
   grid movement, semantic input, simple SFX assets, and rudimentary BFS-based
   enemy pathfinding without using copyrighted maze-game assets. */

#include "ana.h"

#include <stdio.h>
#include <string.h>

#ifdef ANA_TARGET_AMIGA
#define AMAZE_ASSET_ROOT "assets/"
#else
#define AMAZE_ASSET_ROOT "build/assets/amaze/assets/"
#endif

#define AMAZE_ASSET_PATH(name) AMAZE_ASSET_ROOT name

#define AMAZE_MAP_W 21
#define AMAZE_MAP_H 17
#define AMAZE_TILE 8
#define AMAZE_ORIGIN_X 76
#define AMAZE_ORIGIN_Y 48
#define AMAZE_PLAYER_STEP_TICKS 5
#define AMAZE_CHASER_STEP_TICKS 8
#define AMAZE_SMOKE_TICKS 900
#define AMAZE_CHASERS 3

typedef struct AMAzeActor {
    int tx;
    int ty;
    unsigned char color;
} AMAzeActor;

static const char amaze_template[AMAZE_MAP_H][AMAZE_MAP_W + 1] = {
    "#####################",
    "#.........#.........#",
    "#.###.###.#.###.###.#",
    "#...................#",
    "#.###.#.#####.#.###.#",
    "#.....#...#...#.....#",
    "#####.#.......#.#####",
    "#.........#.........#",
    "#.###.###.#.###.###.#",
    "#...#.....#.....#...#",
    "#.###.#.#####.#.###.#",
    "#...................#",
    "#.###.###.#.###.###.#",
    "#.........#.........#",
    "#.###.#.......#.###.#",
    "#.........#.........#",
    "#####################"
};

static char amaze_map[AMAZE_MAP_H][AMAZE_MAP_W + 1];
static int amaze_distance[AMAZE_MAP_H][AMAZE_MAP_W];
static int amaze_queue_x[AMAZE_MAP_W * AMAZE_MAP_H];
static int amaze_queue_y[AMAZE_MAP_W * AMAZE_MAP_H];
static AMAzeActor amaze_player;
static AMAzeActor amaze_chasers[AMAZE_CHASERS];
static int amaze_player_dx;
static int amaze_player_dy;
static int amaze_queued_dx;
static int amaze_queued_dy;
static int amaze_score;
static int amaze_lives;
static int amaze_pellets;
static int amaze_assets_loaded;
static ANA_Sound amaze_eat_sound;
static ANA_Sound amaze_death_sound;
static ANA_Sound amaze_level_sound;

static int amaze_is_open(int tx, int ty)
{
    if (tx < 0 || tx >= AMAZE_MAP_W || ty < 0 || ty >= AMAZE_MAP_H) {
        return 0;
    }

    return amaze_map[ty][tx] != '#';
}

static void amaze_copy_map(void)
{
    int x;
    int y;

    amaze_pellets = 0;

    for (y = 0; y < AMAZE_MAP_H; ++y) {
        strcpy(amaze_map[y], amaze_template[y]);
        for (x = 0; x < AMAZE_MAP_W; ++x) {
            if (amaze_map[y][x] == '.') {
                ++amaze_pellets;
            }
        }
    }
}

static void amaze_reset_positions(void)
{
    amaze_player.tx = 9;
    amaze_player.ty = 15;
    amaze_player.color = 2;

    amaze_player_dx = 0;
    amaze_player_dy = 0;
    amaze_queued_dx = 0;
    amaze_queued_dy = 0;

    amaze_chasers[0].tx = 9;
    amaze_chasers[0].ty = 7;
    amaze_chasers[0].color = 3;
    amaze_chasers[1].tx = 11;
    amaze_chasers[1].ty = 7;
    amaze_chasers[1].color = 4;
    amaze_chasers[2].tx = 9;
    amaze_chasers[2].ty = 9;
    amaze_chasers[2].color = 6;
}

static void amaze_reset_game(void)
{
    amaze_copy_map();
    amaze_reset_positions();
    amaze_score = 0;
    amaze_lives = 3;
}

static void amaze_eat_current_tile(void)
{
    if (amaze_map[amaze_player.ty][amaze_player.tx] == '.') {
        amaze_map[amaze_player.ty][amaze_player.tx] = ' ';
        --amaze_pellets;
        amaze_score += 10;

        if (amaze_eat_sound != 0) {
            ana_play_sound(amaze_eat_sound);
        }

        if (amaze_pellets <= 0) {
            if (amaze_level_sound != 0) {
                ana_play_sound(amaze_level_sound);
            }
            amaze_copy_map();
            amaze_reset_positions();
        }
    }
}

static int amaze_player_hit(void)
{
    int i;

    for (i = 0; i < AMAZE_CHASERS; ++i) {
        if (amaze_chasers[i].tx == amaze_player.tx &&
                amaze_chasers[i].ty == amaze_player.ty) {
            return 1;
        }
    }

    return 0;
}

static void amaze_handle_hit(void)
{
    if (!amaze_player_hit()) {
        return;
    }

    if (amaze_death_sound != 0) {
        ana_play_sound(amaze_death_sound);
    }

    --amaze_lives;
    if (amaze_lives <= 0) {
        amaze_reset_game();
    } else {
        amaze_reset_positions();
    }
}

static void amaze_update_input(void)
{
    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        amaze_queued_dx = -1;
        amaze_queued_dy = 0;
    } else if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        amaze_queued_dx = 1;
        amaze_queued_dy = 0;
    } else if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_UP)) {
        amaze_queued_dx = 0;
        amaze_queued_dy = -1;
    } else if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_DOWN)) {
        amaze_queued_dx = 0;
        amaze_queued_dy = 1;
    }

    if (ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_1)) {
        amaze_reset_game();
    }
}

static void amaze_move_player(void)
{
    int next_x;
    int next_y;

    next_x = amaze_player.tx + amaze_queued_dx;
    next_y = amaze_player.ty + amaze_queued_dy;
    if ((amaze_queued_dx != 0 || amaze_queued_dy != 0) &&
            amaze_is_open(next_x, next_y)) {
        amaze_player_dx = amaze_queued_dx;
        amaze_player_dy = amaze_queued_dy;
    }

    next_x = amaze_player.tx + amaze_player_dx;
    next_y = amaze_player.ty + amaze_player_dy;
    if ((amaze_player_dx != 0 || amaze_player_dy != 0) &&
            amaze_is_open(next_x, next_y)) {
        amaze_player.tx = next_x;
        amaze_player.ty = next_y;
        amaze_eat_current_tile();
    }
}

static void amaze_build_distance_map(void)
{
    int head;
    int tail;
    int x;
    int y;
    int i;
    static const int dx[4] = { -1, 1, 0, 0 };
    static const int dy[4] = { 0, 0, -1, 1 };

    for (y = 0; y < AMAZE_MAP_H; ++y) {
        for (x = 0; x < AMAZE_MAP_W; ++x) {
            amaze_distance[y][x] = -1;
        }
    }

    head = 0;
    tail = 0;
    amaze_distance[amaze_player.ty][amaze_player.tx] = 0;
    amaze_queue_x[tail] = amaze_player.tx;
    amaze_queue_y[tail] = amaze_player.ty;
    ++tail;

    while (head < tail) {
        x = amaze_queue_x[head];
        y = amaze_queue_y[head];
        ++head;

        for (i = 0; i < 4; ++i) {
            int nx;
            int ny;

            nx = x + dx[i];
            ny = y + dy[i];
            if (amaze_is_open(nx, ny) && amaze_distance[ny][nx] < 0) {
                amaze_distance[ny][nx] = amaze_distance[y][x] + 1;
                amaze_queue_x[tail] = nx;
                amaze_queue_y[tail] = ny;
                ++tail;
            }
        }
    }
}

static void amaze_move_chaser(AMAzeActor* chaser)
{
    static const int dx[4] = { -1, 1, 0, 0 };
    static const int dy[4] = { 0, 0, -1, 1 };
    int best_x;
    int best_y;
    int best_distance;
    int i;

    best_x = chaser->tx;
    best_y = chaser->ty;
    best_distance = 10000;

    for (i = 0; i < 4; ++i) {
        int nx;
        int ny;
        int distance;

        nx = chaser->tx + dx[i];
        ny = chaser->ty + dy[i];
        if (amaze_is_open(nx, ny)) {
            distance = amaze_distance[ny][nx];
            if (distance >= 0 && distance < best_distance) {
                best_distance = distance;
                best_x = nx;
                best_y = ny;
            }
        }
    }

    chaser->tx = best_x;
    chaser->ty = best_y;
}

static void amaze_move_chasers(void)
{
    int i;

    amaze_build_distance_map();
    for (i = 0; i < AMAZE_CHASERS; ++i) {
        amaze_move_chaser(&amaze_chasers[i]);
    }
}

static void amaze_draw_actor(const AMAzeActor* actor)
{
    int x;
    int y;

    x = AMAZE_ORIGIN_X + actor->tx * AMAZE_TILE;
    y = AMAZE_ORIGIN_Y + actor->ty * AMAZE_TILE;
    ana_fill_rect(actor->color, x + 1, y + 1, AMAZE_TILE - 2, AMAZE_TILE - 2);
}

static void amaze_draw_maze(void)
{
    int x;
    int y;

    for (y = 0; y < AMAZE_MAP_H; ++y) {
        for (x = 0; x < AMAZE_MAP_W; ++x) {
            int px;
            int py;

            px = AMAZE_ORIGIN_X + x * AMAZE_TILE;
            py = AMAZE_ORIGIN_Y + y * AMAZE_TILE;

            if (amaze_map[y][x] == '#') {
                ana_fill_rect(1, px, py, AMAZE_TILE, AMAZE_TILE);
            } else if (amaze_map[y][x] == '.') {
                ana_fill_rect(5, px + 3, py + 3, 2, 2);
            }
        }
    }
}

static void amaze_draw_status(void)
{
    int i;
    int score_blocks;

    ana_fill_rect(5, 12, 12, 40, 3);
    score_blocks = amaze_score / 100;
    if (score_blocks > 18) {
        score_blocks = 18;
    }

    for (i = 0; i < score_blocks; ++i) {
        ana_fill_rect(2, 12 + i * 6, 18, 4, 4);
    }

    for (i = 0; i < amaze_lives; ++i) {
        ana_fill_rect(2, 248 + i * 14, 14, 10, 6);
    }
}

static void amaze_init(void)
{
    ANA_Color palette[16];

    memset(palette, 0, sizeof(palette));
    palette[0].r = 0;
    palette[0].g = 0;
    palette[0].b = 0;
    palette[1].r = 0;
    palette[1].g = 70;
    palette[1].b = 220;
    palette[2].r = 255;
    palette[2].g = 230;
    palette[2].b = 0;
    palette[3].r = 255;
    palette[3].g = 40;
    palette[3].b = 40;
    palette[4].r = 255;
    palette[4].g = 130;
    palette[4].b = 255;
    palette[5].r = 255;
    palette[5].g = 255;
    palette[5].b = 255;
    palette[6].r = 0;
    palette[6].g = 230;
    palette[6].b = 255;

    ana_set_palette(palette, 16);
    ana_input_clear_key_map();
    ana_input_map_default_keys(ANA_INPUT_DEVICE_0);
    amaze_reset_game();
}

static void amaze_load(void)
{
    ANA_AudioConfig audio;

    audio.music_channels = 0;
    audio.sfx_channels = ANA_AUDIO_ALL_CHANNELS;
    audio.sfx_can_steal_music = 0;
    audio.music_can_use_free_sfx_channels = 0;
    ana_configure_audio(&audio);
    ana_set_sound_volume(48);

    amaze_eat_sound = ana_load_sound(AMAZE_ASSET_PATH("eat.anasnd"));
    amaze_death_sound = ana_load_sound(AMAZE_ASSET_PATH("death.anasnd"));
    amaze_level_sound = ana_load_sound(AMAZE_ASSET_PATH("level.anasnd"));

    amaze_assets_loaded = (amaze_eat_sound != 0 &&
            amaze_death_sound != 0 &&
            amaze_level_sound != 0);
}

static void amaze_update(ANA_Time time)
{
    amaze_update_input();

    if (time.tick % AMAZE_PLAYER_STEP_TICKS == 0) {
        amaze_move_player();
    }

    if (time.tick % AMAZE_CHASER_STEP_TICKS == 0) {
        amaze_move_chasers();
    }

    amaze_handle_hit();

    if (ana_quit_requested()) {
        ana_quit();
    }

#ifndef ANA_TARGET_AMIGA
    if (time.tick >= AMAZE_SMOKE_TICKS) {
        ana_quit();
    }
#endif
}

static void amaze_draw(void)
{
    int i;

    ana_clear(0);
    amaze_draw_status();
    amaze_draw_maze();
    amaze_draw_actor(&amaze_player);

    for (i = 0; i < AMAZE_CHASERS; ++i) {
        amaze_draw_actor(&amaze_chasers[i]);
    }
}

static void amaze_shutdown(void)
{
    ana_free_sound(amaze_level_sound);
    ana_free_sound(amaze_death_sound);
    ana_free_sound(amaze_eat_sound);
    amaze_level_sound = 0;
    amaze_death_sound = 0;
    amaze_eat_sound = 0;
}

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
    game.debug_stats = 1;

    printf("ANA AMAze started.\n");
    printf("Keyboard mapping: cursor/WASD movement, Space reset, Esc quit.\n");
    printf("Eat dots and avoid the chasers.\n");

    result = ana_run(&game);

    printf("ANA AMAze finished with %s.\n", ana_result_name((ANA_Result)result));
    printf("Assets loaded: %s\n", amaze_assets_loaded ? "yes" : "no");
    printf("Type amaze to run it again.\n");

    if (result == ANA_OK && !amaze_assets_loaded) {
        return ANA_ERROR_NOT_IMPLEMENTED;
    }

    return result;
}

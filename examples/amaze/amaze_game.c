/* Gameplay rules for AMAze: map state, input, pellet scoring, collision
   handling, simple BFS pathfinding, and runtime lifecycle callbacks. */

#include "amaze_internal.h"

#include <string.h>

static const char amaze_template[AMAZE_MAP_H][AMAZE_MAP_W + 1] = {
    "#####################",
    "#o........#........o#",
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
    "#o........#........o#",
    "#####################"
};

static const unsigned char amaze_chaser_normal_colors[AMAZE_CHASERS] = {3, 4, 6};

char amaze_map[AMAZE_MAP_H][AMAZE_MAP_W + 1];
AMAzeActor amaze_player;
AMAzeActor amaze_chasers[AMAZE_CHASERS];
int amaze_score = 0;
int amaze_lives = 0;
int amaze_power_ticks = 0;

static int amaze_distance[AMAZE_MAP_H][AMAZE_MAP_W];
static int amaze_queue_x[AMAZE_MAP_W * AMAZE_MAP_H];
static int amaze_queue_y[AMAZE_MAP_W * AMAZE_MAP_H];
static int amaze_player_dx = 0;
static int amaze_player_dy = 0;
static int amaze_queued_dx = 0;
static int amaze_queued_dy = 0;
static int amaze_pellets = 0;
static int amaze_chaser_vulnerable[AMAZE_CHASERS];

static int amaze_is_open(int tx, int ty)
{
    if (tx < 0 || tx >= AMAZE_MAP_W || ty < 0 || ty >= AMAZE_MAP_H) {
        return 0;
    }

    return amaze_map[ty][tx] != '#';
}

static void amaze_update_chaser_colors(void)
{
    int i;

    for (i = 0; i < AMAZE_CHASERS; ++i) {
        amaze_chasers[i].color = amaze_chaser_vulnerable[i] ?
                AMAZE_CHASER_FLEE_COLOR :
                amaze_chaser_normal_colors[i];
    }
}

static void amaze_set_chaser_vulnerability(int vulnerable)
{
    int i;

    for (i = 0; i < AMAZE_CHASERS; ++i) {
        amaze_chaser_vulnerable[i] = vulnerable;
    }
    amaze_update_chaser_colors();
}

static void amaze_copy_map(void)
{
    int x;
    int y;

    amaze_pellets = 0;

    for (y = 0; y < AMAZE_MAP_H; ++y) {
        strcpy(amaze_map[y], amaze_template[y]);
        for (x = 0; x < AMAZE_MAP_W; ++x) {
            if (amaze_map[y][x] == '.' || amaze_map[y][x] == 'o') {
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
    amaze_chasers[1].tx = 11;
    amaze_chasers[1].ty = 7;
    amaze_chasers[2].tx = 9;
    amaze_chasers[2].ty = 9;
    amaze_update_chaser_colors();

    amaze_render_request_full_redraw();
}

static void amaze_reset_game(void)
{
    amaze_power_ticks = 0;
    amaze_set_chaser_vulnerability(0);
    amaze_copy_map();
    amaze_reset_positions();
    amaze_score = 0;
    amaze_lives = 3;
}

static void amaze_eat_current_tile(void)
{
    char tile;

    tile = amaze_map[amaze_player.ty][amaze_player.tx];
    if (tile == '.' || tile == 'o') {
        amaze_map[amaze_player.ty][amaze_player.tx] = ' ';
        --amaze_pellets;

        if (tile == 'o') {
            amaze_score += AMAZE_POWER_SCORE;
            amaze_power_ticks = AMAZE_POWER_TICKS;
            amaze_set_chaser_vulnerability(1);
            amaze_play_bag_sound();
        } else {
            amaze_score += AMAZE_DOT_SCORE;
            amaze_play_coin_sound();
        }

        if (amaze_pellets <= 0) {
            amaze_power_ticks = 0;
            amaze_set_chaser_vulnerability(0);
            amaze_copy_map();
            amaze_reset_positions();
        }
    }
}

static void amaze_capture_chaser(int index)
{
    amaze_score += AMAZE_CAPTURE_SCORE;
    amaze_chasers[index].tx = index == 1 ? 11 : 9;
    amaze_chasers[index].ty = index == 2 ? 9 : 7;
    amaze_chaser_vulnerable[index] = 0;
    amaze_update_chaser_colors();
    amaze_play_yippie_sound();
}

static int amaze_resolve_player_collision(void)
{
    int i;
    int captured;

    captured = 0;
    for (i = 0; i < AMAZE_CHASERS; ++i) {
        if (amaze_chasers[i].tx == amaze_player.tx &&
                amaze_chasers[i].ty == amaze_player.ty &&
                amaze_chaser_vulnerable[i]) {
            amaze_capture_chaser(i);
            captured = 1;
        }
    }

    if (captured) {
        return AMAZE_COLLISION_CAPTURE;
    }

    for (i = 0; i < AMAZE_CHASERS; ++i) {
        if (amaze_chasers[i].tx == amaze_player.tx &&
                amaze_chasers[i].ty == amaze_player.ty) {
            return AMAZE_COLLISION_DEATH;
        }
    }

    return AMAZE_COLLISION_NONE;
}

static int amaze_handle_hit(void)
{
    int collision;

    collision = amaze_resolve_player_collision();
    if (collision != AMAZE_COLLISION_DEATH) {
        return collision;
    }

    amaze_play_death_sound();

    --amaze_lives;
    if (amaze_lives <= 0) {
        amaze_reset_game();
    } else {
        amaze_power_ticks = 0;
        amaze_set_chaser_vulnerability(0);
        amaze_reset_positions();
    }

    return AMAZE_COLLISION_DEATH;
}

static void amaze_update_power_timer(void)
{
    if (amaze_power_ticks > 0) {
        --amaze_power_ticks;
        if (amaze_power_ticks == 0) {
            amaze_set_chaser_vulnerability(0);
        }
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
    static const int dx[4] = { -1, 1, 0, 0 };
    static const int dy[4] = { 0, 0, -1, 1 };
    int head;
    int tail;
    int x;
    int y;
    int i;

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

static void amaze_move_chaser(int index)
{
    static const int dx[4] = { -1, 1, 0, 0 };
    static const int dy[4] = { 0, 0, -1, 1 };
    AMAzeActor* chaser;
    int best_x;
    int best_y;
    int best_distance;
    int flee;
    int i;

    chaser = &amaze_chasers[index];
    best_x = chaser->tx;
    best_y = chaser->ty;
    flee = amaze_chaser_vulnerable[index];
    best_distance = flee ? -1 : 10000;

    for (i = 0; i < 4; ++i) {
        int nx;
        int ny;
        int distance;

        nx = chaser->tx + dx[i];
        ny = chaser->ty + dy[i];
        if (amaze_is_open(nx, ny)) {
            distance = amaze_distance[ny][nx];
            if (distance >= 0 &&
                    ((flee && distance > best_distance) ||
                    (!flee && distance < best_distance))) {
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
        amaze_move_chaser(i);
    }
}

void amaze_init(void)
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
    palette[7].r = 40;
    palette[7].g = 80;
    palette[7].b = 255;
    palette[8].r = 20;
    palette[8].g = 24;
    palette[8].b = 32;
    palette[9].r = 255;
    palette[9].g = 190;
    palette[9].b = 120;
    palette[10].r = 180;
    palette[10].g = 0;
    palette[10].b = 0;

    ana_set_palette(palette, 16);
    ana_input_clear_key_map();
    ana_input_map_default_keys(ANA_INPUT_DEVICE_0);
    amaze_reset_game();
}

void amaze_load(void)
{
    amaze_load_assets();
}

void amaze_update(ANA_Time time)
{
    int collision;

    collision = AMAZE_COLLISION_NONE;
    amaze_update_input();

    if (time.tick % AMAZE_PLAYER_STEP_TICKS == 0) {
        amaze_move_player();
        collision = amaze_handle_hit();
    }

    if (collision == AMAZE_COLLISION_NONE &&
            time.tick % AMAZE_CHASER_STEP_TICKS == 0) {
        amaze_move_chasers();
        amaze_handle_hit();
    }

    amaze_update_power_timer();

    if (ana_quit_requested()) {
        ana_quit();
    }

#ifndef ANA_TARGET_AMIGA
    if (time.tick >= AMAZE_SMOKE_TICKS) {
        ana_quit();
    }
#endif
}

void amaze_shutdown(void)
{
    amaze_free_assets();
}

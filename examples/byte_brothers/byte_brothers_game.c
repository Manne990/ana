#include "byte_brothers_game.h"

#include "byte_brothers_assets.h"
#include "byte_brothers_internal.h"
#include "byte_brothers_render.h"

#include <string.h>

/* Platforming rules, text-map loading, collision, enemies, and progression. */

char bb_map[BB_MAP_H][BB_MAP_W + 1];
BB_Player bb_player;
BB_Enemy bb_enemies[BB_MAX_ENEMIES];
int bb_enemy_count = 0;
int bb_level_index = 0;
int bb_score = 0;
int bb_lives = 3;
int bb_fragments_left = 0;
ANA_Camera bb_camera;
int bb_camera_x = 0;
int bb_camera_y = 0;
int bb_frame = 0;

static const char* bb_levels[BB_LEVEL_COUNT][BB_MAP_H] = {
    {
        "................................................................",
        "................................................................",
        ".......................................................*........",
        "......................*......?.......................-----....E.",
        ".................----------.............*....................###",
        "....................................--------....................",
        "..........*..................H..........................v.......",
        "......---------...........########..............----............",
        "................................................................",
        "....................v....................*......................",
        "....S...........----------............----------................",
        "########.......................?...........................#####",
        "###########.............---------------..............###########",
        "###############..................................###############",
        "####################.............D...........###################",
        "################################################################"
    },
    {
        "................................................................",
        ".......................................................G....E...",
        "......................................................#######...",
        "..............*.........................D.......................",
        "..........---------..............---------------................",
        "................................................................",
        "....................~~~~~...................*...................",
        "....S.....-----.................v.......----------..............",
        "########........................................................",
        "###########............?.........................H..............",
        ".................-------------..............#########...........",
        "................................................................",
        "...........*..................v.........................*.......",
        ".......---------...........########.................---------...",
        "................................................................",
        "################################################################"
    },
    {
        "................................................................",
        "............................................................E...",
        ".......................................................#########",
        ".............*.................B.B.B.....................*......",
        ".........---------..........#########...............---------...",
        ".................................?..............................",
        "....S...........B.B...........--------.........v................",
        "########.....########...............................########....",
        "................................................................",
        "........H....................*.............B.B.B................",
        ".....########.............----------.....#########..............",
        "........................v.......................................",
        "...............*......................*.........................",
        "..........------------............------------..................",
        "......................................................o.........",
        "################################################################"
    },
    {
        "................................................................",
        ".......................................................E........",
        ".....................................................#####......",
        "..........*.............^...........*...........................",
        "......----------......#####.....----------..............T.......",
        "....................................................########....",
        "..................v......................~~~~~..................",
        "....S.....----..........?...........---------------.............",
        "########........................................................",
        "###########............H....................*...................",
        ".................-------------..........----------..............",
        "..............................v.................................",
        "...........*.............^^^^^^......................*..........",
        ".......---------........########.................---------......",
        ".....................................o..........................",
        "################################################################"
    }
};

static int bb_start_x = 0;
static int bb_start_y = 0;

static void bb_load_level(int level);

static int bb_actor_tile_x(int tile_x, int width)
{
    return tile_x * BB_TILE + ((BB_TILE - width) / 2);
}

static int bb_player_tile_y(int tile_y)
{
    return (tile_y + 1) * BB_TILE - BB_PLAYER_H - 1;
}

static int bb_enemy_tile_y(int tile_y)
{
    return (tile_y + 1) * BB_TILE - BB_ENEMY_H - 2;
}

static void bb_sync_camera_compat(void)
{
    bb_camera_x = bb_camera.x;
    bb_camera_y = bb_camera.y;
}

static void bb_reset_camera(void)
{
    ana_camera_init(
        &bb_camera,
        0,
        BB_HUD_H,
        BB_SCREEN_W,
        BB_SCREEN_H - BB_HUD_H,
        BB_WORLD_W,
        BB_WORLD_H);
    ana_camera_set_snap(&bb_camera, BB_CAMERA_SNAP_X, BB_CAMERA_SNAP_Y);
    ana_camera_set_position(&bb_camera, 0, 0);
    bb_sync_camera_compat();
}

char bb_tile_at(int tx, int ty)
{
    if (tx < 0 || tx >= BB_MAP_W || ty < 0 || ty >= BB_MAP_H) {
        return '#';
    }

    return bb_map[ty][tx];
}

void bb_set_tile(int tx, int ty, char value)
{
    if (tx < 0 || tx >= BB_MAP_W || ty < 0 || ty >= BB_MAP_H) {
        return;
    }

    bb_map[ty][tx] = value;
    bb_render_invalidate();
}

int bb_tile_is_solid(char tile)
{
    return tile == '#' || tile == '?' || tile == 'B' || tile == 'G';
}

int bb_tile_is_platform(char tile)
{
    return tile == '-';
}

int bb_tile_is_hazard(char tile)
{
    return tile == '^' || tile == '~';
}

int bb_tile_is_collectible(char tile)
{
    return tile == '*' || tile == 'o' || tile == 'D';
}

int bb_tile_is_goal(char tile)
{
    return tile == 'E';
}

static void bb_set_palette(void)
{
    ANA_Color colors[16];

    colors[0].r = 0; colors[0].g = 0; colors[0].b = 0;
    colors[1].r = 255; colors[1].g = 255; colors[1].b = 255;
    colors[2].r = 0; colors[2].g = 42; colors[2].b = 154;
    colors[3].r = 0; colors[3].g = 96; colors[3].b = 220;
    colors[4].r = 58; colors[4].g = 204; colors[4].b = 255;
    colors[5].r = 255; colors[5].g = 239; colors[5].b = 0;
    colors[6].r = 255; colors[6].g = 132; colors[6].b = 0;
    colors[7].r = 255; colors[7].g = 40; colors[7].b = 74;
    colors[8].r = 186; colors[8].g = 65; colors[8].b = 255;
    colors[9].r = 0; colors[9].g = 220; colors[9].b = 84;
    colors[10].r = 0; colors[10].g = 126; colors[10].b = 58;
    colors[11].r = 112; colors[11].g = 80; colors[11].b = 52;
    colors[12].r = 190; colors[12].g = 190; colors[12].b = 190;
    colors[13].r = 72; colors[13].g = 72; colors[13].b = 88;
    colors[14].r = 20; colors[14].g = 16; colors[14].b = 42;
    colors[15].r = 255; colors[15].g = 164; colors[15].b = 216;

    ana_set_palette(colors, 16);
}

static int bb_rect_intersects(
    int ax,
    int ay,
    int aw,
    int ah,
    int bx,
    int by,
    int bw,
    int bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static int bb_rect_hits_solid(int x, int y, int w, int h)
{
    int tx0;
    int ty0;
    int tx1;
    int ty1;
    int tx;
    int ty;
    char tile;

    tx0 = x / BB_TILE;
    ty0 = y / BB_TILE;
    tx1 = (x + w - 1) / BB_TILE;
    ty1 = (y + h - 1) / BB_TILE;

    for (ty = ty0; ty <= ty1; ty++) {
        for (tx = tx0; tx <= tx1; tx++) {
            tile = bb_tile_at(tx, ty);
            if (bb_tile_is_solid(tile)) {
                return 1;
            }
        }
    }

    return 0;
}

static int bb_rect_hits_hazard(int x, int y, int w, int h)
{
    int tx0;
    int ty0;
    int tx1;
    int ty1;
    int tx;
    int ty;

    tx0 = x / BB_TILE;
    ty0 = y / BB_TILE;
    tx1 = (x + w - 1) / BB_TILE;
    ty1 = (y + h - 1) / BB_TILE;

    for (ty = ty0; ty <= ty1; ty++) {
        for (tx = tx0; tx <= tx1; tx++) {
            if (bb_tile_is_hazard(bb_tile_at(tx, ty))) {
                return 1;
            }
        }
    }

    return 0;
}

static int bb_has_floor_at(int x, int y)
{
    int tx;
    int ty;
    char tile;

    tx = x / BB_TILE;
    ty = y / BB_TILE;
    tile = bb_tile_at(tx, ty);
    return bb_tile_is_solid(tile) || bb_tile_is_platform(tile);
}

static int bb_hits_platform_on_descent(int x, int old_y, int new_y, int w, int h)
{
    int tx0;
    int tx1;
    int tx;
    int old_bottom;
    int new_bottom;
    int platform_top;
    int ty;

    old_bottom = old_y + h;
    new_bottom = new_y + h;
    if (new_bottom <= old_bottom) {
        return 0;
    }

    tx0 = (x + 1) / BB_TILE;
    tx1 = (x + w - 2) / BB_TILE;
    ty = new_bottom / BB_TILE;
    platform_top = ty * BB_TILE;

    if (old_bottom > platform_top || new_bottom < platform_top) {
        return 0;
    }

    for (tx = tx0; tx <= tx1; tx++) {
        if (bb_tile_is_platform(bb_tile_at(tx, ty))) {
            return platform_top;
        }
    }

    return 0;
}

static int bb_bump_head_tiles(int head_y)
{
    int tx0;
    int tx1;
    int tx;
    int ty;
    char tile;
    int bumped;

    tx0 = (bb_player.x + 1) / BB_TILE;
    tx1 = (bb_player.x + BB_PLAYER_W - 2) / BB_TILE;
    ty = head_y / BB_TILE;
    bumped = 0;

    for (tx = tx0; tx <= tx1; tx++) {
        tile = bb_tile_at(tx, ty);
        if (tile == '?' || tile == 'H' || tile == 'B') {
            bb_set_tile(tx, ty, '.');
            bb_score += tile == 'B' ? 5 : 25;
            bb_assets_play_power();
            bumped = 1;
        }
    }

    return bumped;
}

static void bb_reset_player(void)
{
    bb_player.x = bb_start_x;
    bb_player.y = bb_start_y;
    bb_player.vx = 0;
    bb_player.vy = 0;
    bb_player.on_ground = 0;
    bb_player.dash_ticks = 0;
    bb_player.facing = 1;
    bb_player.invuln_ticks = BB_INVULN_TICKS;
    bb_render_invalidate();
}

static void bb_restart_level(void)
{
    bb_load_level(bb_level_index);
}

static void bb_player_hit(void)
{
    if (bb_player.invuln_ticks > 0) {
        return;
    }

    bb_lives--;
    bb_assets_play_hit();

    if (bb_lives <= 0) {
        bb_lives = 3;
        bb_score = 0;
        bb_level_index = 0;
        bb_restart_level();
    } else {
        bb_reset_player();
    }
}

static void bb_move_player_h(int dx)
{
    int step;
    int count;

    if (dx == 0) {
        return;
    }

    step = dx > 0 ? 1 : -1;
    count = dx > 0 ? dx : -dx;

    while (count > 0) {
        if (!bb_rect_hits_solid(
                bb_player.x + step,
                bb_player.y,
                BB_PLAYER_W,
                BB_PLAYER_H)) {
            bb_player.x += step;
        } else {
            bb_player.vx = 0;
            return;
        }

        count--;
    }
}

static void bb_move_player_v(int dy)
{
    int step;
    int count;
    int platform_top;

    if (dy == 0) {
        return;
    }

    step = dy > 0 ? 1 : -1;
    count = dy > 0 ? dy : -dy;
    bb_player.on_ground = 0;

    while (count > 0) {
        platform_top = 0;
        if (step > 0) {
            platform_top = bb_hits_platform_on_descent(
                bb_player.x,
                bb_player.y,
                bb_player.y + step,
                BB_PLAYER_W,
                BB_PLAYER_H);
        }

        if (platform_top != 0) {
            bb_player.y = platform_top - BB_PLAYER_H;
            bb_player.vy = 0;
            bb_player.on_ground = 1;
            return;
        }

        if (step < 0 && bb_bump_head_tiles(bb_player.y + step)) {
            bb_player.vy = 0;
            return;
        }

        if (!bb_rect_hits_solid(
                bb_player.x,
                bb_player.y + step,
                BB_PLAYER_W,
                BB_PLAYER_H)) {
            bb_player.y += step;
        } else {
            if (step < 0) {
                bb_bump_head_tiles(bb_player.y + step);
            } else {
                bb_player.on_ground = 1;
            }
            bb_player.vy = 0;
            return;
        }

        count--;
    }
}

static void bb_clamp_player_to_world(void)
{
    if (bb_player.x < 0) {
        bb_player.x = 0;
    }
    if (bb_player.x > BB_WORLD_W - BB_PLAYER_W) {
        bb_player.x = BB_WORLD_W - BB_PLAYER_W;
    }
    if (bb_player.y > BB_WORLD_H) {
        bb_player_hit();
    }
}

static void bb_collect_at_player(void)
{
    int tx0;
    int tx1;
    int ty0;
    int ty1;
    int tx;
    int ty;
    char tile;

    tx0 = bb_player.x / BB_TILE;
    tx1 = (bb_player.x + BB_PLAYER_W - 1) / BB_TILE;
    ty0 = bb_player.y / BB_TILE;
    ty1 = (bb_player.y + BB_PLAYER_H - 1) / BB_TILE;

    for (ty = ty0; ty <= ty1; ty++) {
        for (tx = tx0; tx <= tx1; tx++) {
            tile = bb_tile_at(tx, ty);
            if (tile == '*') {
                bb_score += 10;
                bb_fragments_left--;
                bb_set_tile(tx, ty, '.');
                bb_assets_play_collect();
            } else if (tile == 'o') {
                bb_score += 100;
                bb_fragments_left--;
                bb_set_tile(tx, ty, '.');
                bb_assets_play_power();
            } else if (tile == 'D') {
                bb_score += 50;
                bb_set_tile(tx, ty, '.');
                bb_assets_play_power();
            }
        }
    }
}

static int bb_player_on_goal(void)
{
    int tx0;
    int tx1;
    int ty0;
    int ty1;
    int tx;
    int ty;

    tx0 = bb_player.x / BB_TILE;
    tx1 = (bb_player.x + BB_PLAYER_W - 1) / BB_TILE;
    ty0 = bb_player.y / BB_TILE;
    ty1 = (bb_player.y + BB_PLAYER_H - 1) / BB_TILE;

    for (ty = ty0; ty <= ty1; ty++) {
        for (tx = tx0; tx <= tx1; tx++) {
            if (bb_tile_is_goal(bb_tile_at(tx, ty))) {
                return 1;
            }
        }
    }

    return 0;
}

static void bb_update_camera(void)
{
    int target;

    target = bb_player.x - 136;
    ana_camera_set_position(&bb_camera, target, 0);
    bb_sync_camera_compat();
}

static void bb_update_enemies(void)
{
    int i;
    BB_Enemy* enemy;
    int next_x;
    int probe_x;
    int probe_y;

    for (i = 0; i < bb_enemy_count; i++) {
        enemy = &bb_enemies[i];
        if (!enemy->alive) {
            continue;
        }

        if ((bb_frame & 1) == 0) {
            next_x = enemy->x + enemy->vx;
            if (bb_rect_hits_solid(
                    next_x,
                    enemy->y,
                    BB_ENEMY_W,
                    BB_ENEMY_H)) {
                enemy->vx = -enemy->vx;
            } else {
                probe_x = next_x + (enemy->vx > 0 ? BB_ENEMY_W : -1);
                probe_y = enemy->y + BB_ENEMY_H + 2;
                if (!bb_has_floor_at(probe_x, probe_y)) {
                    enemy->vx = -enemy->vx;
                } else {
                    enemy->x = next_x;
                }
            }
        }

        if (bb_rect_intersects(
                bb_player.x,
                bb_player.y,
                BB_PLAYER_W,
                BB_PLAYER_H,
                enemy->x,
                enemy->y,
                BB_ENEMY_W,
                BB_ENEMY_H)) {
            bb_player_hit();
        }
    }
}

static void bb_load_level(int level)
{
    int y;
    int x;
    char tile;

    bb_enemy_count = 0;
    bb_fragments_left = 0;
    bb_reset_camera();

    for (y = 0; y < BB_MAP_H; y++) {
        memcpy(bb_map[y], bb_levels[level][y], BB_MAP_W + 1);
        for (x = 0; x < BB_MAP_W; x++) {
            tile = bb_map[y][x];
            if (tile == 'S') {
                bb_start_x = bb_actor_tile_x(x, BB_PLAYER_W);
                bb_start_y = bb_player_tile_y(y);
                bb_map[y][x] = '.';
            } else if (tile == 'v' || tile == 'T') {
                if (bb_enemy_count < BB_MAX_ENEMIES) {
                    bb_enemies[bb_enemy_count].x =
                        bb_actor_tile_x(x, BB_ENEMY_W);
                    bb_enemies[bb_enemy_count].y = bb_enemy_tile_y(y);
                    bb_enemies[bb_enemy_count].vx = (bb_enemy_count & 1) ? -1 : 1;
                    bb_enemies[bb_enemy_count].alive = 1;
                    bb_enemy_count++;
                }
                bb_map[y][x] = '.';
            } else if (bb_tile_is_collectible(tile)) {
                bb_fragments_left++;
            }
        }
    }

    bb_reset_player();
    bb_render_reset();
}

void byte_brothers_load(void)
{
    bb_set_palette();
    ana_input_map_default_keys(ANA_INPUT_DEVICE_0);
    ana_input_map_key_to_action(ANA_KEY_X, ANA_INPUT_DEVICE_0, ANA_ACTION_2);
    ana_input_map_key_to_action(ANA_KEY_Z, ANA_INPUT_DEVICE_0, ANA_ACTION_2);
    bb_assets_load();

    bb_level_index = 0;
    bb_score = 0;
    bb_lives = 3;
    bb_frame = 0;
    bb_load_level(bb_level_index);
}

void byte_brothers_update(ANA_Time time)
{
    int move;
    int dash_pressed;
    int jump_pressed;

    (void)time;

    bb_frame++;

    move = 0;
    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        move--;
    }
    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        move++;
    }

    if (move < 0) {
        bb_player.facing = -1;
    } else if (move > 0) {
        bb_player.facing = 1;
    }

    jump_pressed = ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_1);
    dash_pressed = ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_2);

    if (jump_pressed && bb_player.on_ground) {
        bb_player.vy = BB_JUMP_SPEED;
        bb_player.on_ground = 0;
        bb_assets_play_jump();
    }

    if (dash_pressed && bb_player.dash_ticks == 0) {
        bb_player.dash_ticks = BB_DASH_TICKS;
        bb_assets_play_power();
    }

    if (bb_player.dash_ticks > 0) {
        bb_player.vx = bb_player.facing * BB_DASH_SPEED;
        bb_player.dash_ticks--;
    } else {
        bb_player.vx = move * BB_PLAYER_SPEED;
    }

    bb_player.vy += BB_GRAVITY;
    if (bb_player.vy > BB_MAX_FALL) {
        bb_player.vy = BB_MAX_FALL;
    }

    bb_move_player_h(bb_player.vx);
    bb_move_player_v(bb_player.vy);
    bb_clamp_player_to_world();

    if (bb_player.invuln_ticks > 0) {
        bb_player.invuln_ticks--;
    }

    bb_collect_at_player();
    if (bb_rect_hits_hazard(
            bb_player.x,
            bb_player.y,
            BB_PLAYER_W,
            BB_PLAYER_H)) {
        bb_player_hit();
    }

    bb_update_enemies();

    if (bb_player_on_goal()) {
        bb_assets_play_goal();
        bb_level_index++;
        if (bb_level_index >= BB_LEVEL_COUNT) {
            bb_level_index = 0;
        }
        bb_load_level(bb_level_index);
    }

    bb_update_camera();

#ifndef ANA_TARGET_AMIGA
    if (bb_frame >= 80) {
        ana_quit();
    }
#endif
}

void byte_brothers_draw(void)
{
    bb_render_draw();
}

void byte_brothers_shutdown(void)
{
    bb_assets_unload();
}

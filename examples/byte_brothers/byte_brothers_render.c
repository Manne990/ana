#include "byte_brothers_render.h"

#include "byte_brothers_internal.h"

/* Dirty redraw renderer for the Byte Brothers scrolling platform sample. */

static int bb_full_redraw = 1;
static int bb_prev_camera_x = -1;
static int bb_prev_camera_y = -1;
static int bb_prev_player_x = 0;
static int bb_prev_player_y = 0;
static int bb_prev_enemy_x[BB_MAX_ENEMIES];
static int bb_prev_enemy_y[BB_MAX_ENEMIES];
static int bb_prev_score = -1;
static int bb_prev_lives = -1;
static int bb_prev_level = -1;
static int bb_prev_fragments = -1;

static ANA_Rect bb_world_rect(int x, int y, int w, int h)
{
    return ana_rect_make(x, y, w, h);
}

static ANA_Rect bb_view_bounds(void)
{
    return ana_rect_make(0, BB_HUD_H, BB_SCREEN_W, BB_SCREEN_H - BB_HUD_H);
}

static int bb_rect_visible_world(ANA_Rect rect)
{
    ANA_Rect view;

    view = ana_rect_make(bb_camera_x, bb_camera_y, BB_SCREEN_W, BB_SCREEN_H - BB_HUD_H);
    return ana_rect_intersects(rect, view);
}

static void bb_draw_digit(int digit, int x, int y, unsigned char color)
{
    static const unsigned char bits[10][5] = {
        {7, 5, 5, 5, 7},
        {2, 6, 2, 2, 7},
        {7, 1, 7, 4, 7},
        {7, 1, 7, 1, 7},
        {5, 5, 7, 1, 1},
        {7, 4, 7, 1, 7},
        {7, 4, 7, 5, 7},
        {7, 1, 1, 2, 2},
        {7, 5, 7, 5, 7},
        {7, 5, 7, 1, 7}
    };
    int row;
    int col;

    if (digit < 0 || digit > 9) {
        return;
    }

    for (row = 0; row < 5; row++) {
        for (col = 0; col < 3; col++) {
            if ((bits[digit][row] & (1u << (2 - col))) != 0u) {
                ana_fill_rect(color, x + col * 2, y + row * 2, 2, 2);
            }
        }
    }
}

static void bb_draw_number(int value, int x, int y, unsigned char color)
{
    char digits[8];
    int count;
    int i;
    int v;

    if (value < 0) {
        value = 0;
    }

    v = value;
    count = 0;
    do {
        digits[count] = (char)('0' + (v % 10));
        count++;
        v /= 10;
    } while (v > 0 && count < 8);

    for (i = count - 1; i >= 0; i--) {
        bb_draw_digit(digits[i] - '0', x, y, color);
        x += 8;
    }
}

static void bb_draw_hud(void)
{
    int i;
    int x;

    ana_fill_rect(0, 0, 0, BB_SCREEN_W, BB_HUD_H);
    ana_fill_rect(3, 0, BB_HUD_H - 2, BB_SCREEN_W, 2);

    ana_fill_rect(5, 6, 3, 4, 4);
    bb_draw_number(bb_score, 14, 3, 5);

    ana_fill_rect(4, 135, 3, 4, 4);
    bb_draw_number(bb_level_index + 1, 143, 3, 4);

    x = 246;
    for (i = 0; i < bb_lives; i++) {
        ana_fill_rect(7, x, 3, 8, 6);
        ana_fill_rect(5, x + 2, 1, 4, 10);
        x += 12;
    }

    bb_draw_number(bb_fragments_left, 292, 3, 1);
}

static int bb_tile_screen_x(int tx)
{
    return tx * BB_TILE - bb_camera_x;
}

static int bb_tile_screen_y(int ty)
{
    return ty * BB_TILE - bb_camera_y + BB_HUD_H;
}

static void bb_draw_tile_at(char tile, int sx, int sy)
{
    if (tile == '#') {
        ana_fill_rect(2, sx, sy, BB_TILE, BB_TILE);
        ana_fill_rect(3, sx, sy, BB_TILE, 2);
        ana_fill_rect(14, sx, sy + BB_TILE - 2, BB_TILE, 2);
    } else if (tile == '-') {
        ana_fill_rect(3, sx, sy + 6, BB_TILE, 4);
        ana_fill_rect(4, sx, sy + 5, BB_TILE, 1);
    } else if (tile == '?') {
        ana_fill_rect(6, sx + 1, sy + 1, BB_TILE - 2, BB_TILE - 2);
        ana_fill_rect(5, sx + 6, sy + 4, 4, 4);
        ana_fill_rect(5, sx + 6, sy + 11, 4, 2);
    } else if (tile == 'B') {
        ana_fill_rect(8, sx, sy, BB_TILE, BB_TILE);
        ana_fill_rect(15, sx + 2, sy + 2, BB_TILE - 4, 2);
        ana_fill_rect(13, sx, sy + 8, BB_TILE, 2);
    } else if (tile == '*') {
        ana_fill_rect(1, sx + 6, sy + 6, 4, 4);
    } else if (tile == 'o') {
        ana_fill_rect(5, sx + 4, sy + 3, 8, 10);
        ana_fill_rect(6, sx + 2, sy + 6, 12, 4);
    } else if (tile == '^') {
        ana_fill_rect(7, sx + 6, sy + 4, 4, 10);
        ana_fill_rect(7, sx + 3, sy + 10, 10, 4);
    } else if (tile == '~') {
        ana_fill_rect(8, sx + 1, sy + 6, 5, 3);
        ana_fill_rect(8, sx + 6, sy + 9, 5, 3);
        ana_fill_rect(8, sx + 11, sy + 6, 4, 3);
    } else if (tile == 'D') {
        ana_fill_rect(12, sx + 5, sy + 3, 7, 10);
        ana_fill_rect(4, sx + 7, sy + 5, 3, 2);
    } else if (tile == 'G') {
        ana_fill_rect(10, sx + 2, sy + 1, 12, 14);
        ana_fill_rect(0, sx + 5, sy + 4, 6, 8);
    } else if (tile == 'E') {
        ana_fill_rect(9, sx + 2, sy + 2, 12, 12);
        ana_fill_rect(4, sx + 5, sy + 5, 6, 6);
    }
}

static void bb_draw_tiles_in_world_rect(ANA_Rect rect)
{
    int tx0;
    int ty0;
    int tx1;
    int ty1;
    int tx;
    int ty;
    char tile;

    tx0 = rect.x / BB_TILE;
    ty0 = rect.y / BB_TILE;
    tx1 = (rect.x + rect.w - 1) / BB_TILE;
    ty1 = (rect.y + rect.h - 1) / BB_TILE;

    if (tx0 < 0) {
        tx0 = 0;
    }
    if (ty0 < 0) {
        ty0 = 0;
    }
    if (tx1 >= BB_MAP_W) {
        tx1 = BB_MAP_W - 1;
    }
    if (ty1 >= BB_MAP_H) {
        ty1 = BB_MAP_H - 1;
    }

    for (ty = ty0; ty <= ty1; ty++) {
        for (tx = tx0; tx <= tx1; tx++) {
            tile = bb_tile_at(tx, ty);
            if (tile != '.' && tile != 'H') {
                bb_draw_tile_at(tile, bb_tile_screen_x(tx), bb_tile_screen_y(ty));
            }
        }
    }
}

static void bb_clear_world_rect(ANA_Rect rect)
{
    ANA_Rect screen_rect;
    ANA_Rect clipped;

    screen_rect.x = rect.x - bb_camera_x;
    screen_rect.y = rect.y - bb_camera_y + BB_HUD_H;
    screen_rect.w = rect.w;
    screen_rect.h = rect.h;
    clipped = ana_rect_clip(screen_rect, bb_view_bounds());

    if (!ana_rect_is_empty(clipped)) {
        ana_fill_rect(0, clipped.x, clipped.y, clipped.w, clipped.h);
    }
}

static void bb_redraw_world_rect(ANA_Rect rect)
{
    ANA_Rect padded;
    ANA_Rect world_view;

    padded = ana_rect_make(rect.x - 2, rect.y - 2, rect.w + 4, rect.h + 4);
    world_view = ana_rect_make(bb_camera_x, bb_camera_y, BB_SCREEN_W, BB_SCREEN_H - BB_HUD_H);
    padded = ana_rect_clip(padded, world_view);

    if (ana_rect_is_empty(padded)) {
        return;
    }

    bb_clear_world_rect(padded);
    bb_draw_tiles_in_world_rect(padded);
}

static void bb_draw_all_tiles(void)
{
    ANA_Rect view;

    view = ana_rect_make(bb_camera_x, bb_camera_y, BB_SCREEN_W, BB_SCREEN_H - BB_HUD_H);
    bb_draw_tiles_in_world_rect(view);
}

static void bb_draw_player(void)
{
    int sx;
    int sy;

    if (bb_player.invuln_ticks > 0 && (bb_frame & 4) != 0) {
        return;
    }

    sx = bb_player.x - bb_camera_x;
    sy = bb_player.y - bb_camera_y + BB_HUD_H;

    ana_fill_rect(4, sx + 2, sy + 4, 6, 8);
    ana_fill_rect(5, sx + 3, sy, 5, 5);
    ana_fill_rect(1, sx + 4, sy + 2, 2, 1);
    ana_fill_rect(13, sx + 1, sy + 12, 4, 2);
    ana_fill_rect(13, sx + 6, sy + 12, 4, 2);

    if (bb_player.dash_ticks > 0) {
        if (bb_player.facing > 0) {
            ana_fill_rect(4, sx - 5, sy + 6, 3, 2);
        } else {
            ana_fill_rect(4, sx + BB_PLAYER_W + 2, sy + 6, 3, 2);
        }
    }
}

static void bb_draw_enemy(const BB_Enemy* enemy)
{
    int sx;
    int sy;

    if (!enemy->alive) {
        return;
    }

    sx = enemy->x - bb_camera_x;
    sy = enemy->y - bb_camera_y + BB_HUD_H;
    ana_fill_rect(7, sx + 1, sy + 3, 10, 8);
    ana_fill_rect(15, sx + 3, sy + 1, 6, 3);
    ana_fill_rect(1, sx + 3, sy + 5, 2, 2);
    ana_fill_rect(1, sx + 7, sy + 5, 2, 2);
    ana_fill_rect(13, sx + 2, sy + 11, 3, 2);
    ana_fill_rect(13, sx + 7, sy + 11, 3, 2);
}

static void bb_draw_actors(void)
{
    int i;
    ANA_Rect rect;

    for (i = 0; i < bb_enemy_count; i++) {
        rect = bb_world_rect(bb_enemies[i].x, bb_enemies[i].y, 12, 12);
        if (bb_rect_visible_world(rect)) {
            bb_draw_enemy(&bb_enemies[i]);
        }
    }

    rect = bb_world_rect(bb_player.x, bb_player.y, BB_PLAYER_W, BB_PLAYER_H);
    if (bb_rect_visible_world(rect)) {
        bb_draw_player();
    }
}

static void bb_commit_state(void)
{
    int i;

    bb_prev_camera_x = bb_camera_x;
    bb_prev_camera_y = bb_camera_y;
    bb_prev_player_x = bb_player.x;
    bb_prev_player_y = bb_player.y;
    bb_prev_score = bb_score;
    bb_prev_lives = bb_lives;
    bb_prev_level = bb_level_index;
    bb_prev_fragments = bb_fragments_left;

    for (i = 0; i < BB_MAX_ENEMIES; i++) {
        bb_prev_enemy_x[i] = bb_enemies[i].x;
        bb_prev_enemy_y[i] = bb_enemies[i].y;
    }
}

void bb_render_reset(void)
{
    bb_full_redraw = 1;
    bb_prev_camera_x = -1;
    bb_prev_camera_y = -1;
    bb_prev_score = -1;
    bb_prev_lives = -1;
    bb_prev_level = -1;
    bb_prev_fragments = -1;
}

void bb_render_invalidate(void)
{
    bb_full_redraw = 1;
}

void bb_render_draw(void)
{
    int i;
    int hud_dirty;
    ANA_Rect rect;

    hud_dirty = bb_score != bb_prev_score ||
        bb_lives != bb_prev_lives ||
        bb_level_index != bb_prev_level ||
        bb_fragments_left != bb_prev_fragments;

    if (bb_full_redraw ||
            bb_camera_x != bb_prev_camera_x ||
            bb_camera_y != bb_prev_camera_y) {
        ana_fill_rect(0, 0, 0, BB_SCREEN_W, BB_SCREEN_H);
        bb_draw_hud();
        bb_draw_all_tiles();
        bb_draw_actors();
        ana_present();
        bb_full_redraw = 0;
        bb_commit_state();
        return;
    }

    rect = bb_world_rect(bb_prev_player_x, bb_prev_player_y, BB_PLAYER_W, BB_PLAYER_H);
    bb_redraw_world_rect(rect);
    rect = bb_world_rect(bb_player.x, bb_player.y, BB_PLAYER_W, BB_PLAYER_H);
    bb_redraw_world_rect(rect);

    for (i = 0; i < bb_enemy_count; i++) {
        rect = bb_world_rect(bb_prev_enemy_x[i], bb_prev_enemy_y[i], 12, 12);
        bb_redraw_world_rect(rect);
        rect = bb_world_rect(bb_enemies[i].x, bb_enemies[i].y, 12, 12);
        bb_redraw_world_rect(rect);
    }

    if (hud_dirty) {
        bb_draw_hud();
    }

    bb_draw_actors();
    ana_present();
    bb_commit_state();
}

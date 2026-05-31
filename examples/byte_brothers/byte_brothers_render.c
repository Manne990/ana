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
static ANA_TileLayer bb_playfield_layer;
static ANA_Layer bb_sprite_layer;
static ANA_Layer bb_hud_layer;

static ANA_Rect bb_world_rect(int x, int y, int w, int h)
{
    return ana_rect_make(x, y, w, h);
}

static ANA_Rect bb_view_bounds(void)
{
    return ana_rect_make(
        bb_camera.view_x,
        bb_camera.view_y,
        bb_camera.view_w,
        bb_camera.view_h);
}

static int bb_rect_visible_world(ANA_Rect rect)
{
    ANA_Rect view;

    view = ana_camera_world_view(&bb_camera);
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

static void bb_draw_hud_layer(ANA_Rect rect, void* user_data)
{
    (void)rect;
    (void)user_data;

    bb_draw_hud();
}

static void bb_sync_layers(void)
{
    ANA_Rect viewport;

    viewport = bb_view_bounds();
    ana_tile_layer_set_viewport(&bb_playfield_layer, viewport);
    ana_layer_set_viewport(&bb_sprite_layer, viewport);
    ana_tile_layer_set_camera(&bb_playfield_layer, &bb_camera);
    ana_layer_set_camera(&bb_sprite_layer, &bb_camera);
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

static unsigned char bb_read_tile_for_layer(int tx, int ty, void* user_data)
{
    char tile;

    (void)user_data;

    tile = bb_tile_at(tx, ty);
    if (tile == '.' || tile == 'H') {
        return 0u;
    }

    return (unsigned char)tile;
}

static void bb_draw_tile_for_layer(
    unsigned char tile,
    int x,
    int y,
    void* user_data)
{
    (void)user_data;

    bb_draw_tile_at((char)tile, x, y);
}

static long bb_rect_area(ANA_Rect rect)
{
    if (ana_rect_is_empty(rect)) {
        return 0L;
    }

    return (long)rect.w * rect.h;
}

static int bb_redraw_rects_should_merge(ANA_Rect a, ANA_Rect b)
{
    ANA_Rect merged;
    long merged_area;
    long separate_area;

    if (ana_rect_intersects(a, b)) {
        return 1;
    }

    merged = ana_rect_union(a, b);
    merged_area = bb_rect_area(merged);
    separate_area = bb_rect_area(a) + bb_rect_area(b);
    return merged_area <= separate_area + 64L;
}

static void bb_coalesce_redraw_rects(
    ANA_Rect* rects,
    int* rect_count,
    int index)
{
    int i;

    i = 0;
    while (i < *rect_count) {
        if (i == index) {
            i++;
            continue;
        }

        if (ana_rect_contains(rects[index], rects[i]) ||
                ana_rect_contains(rects[i], rects[index]) ||
                bb_redraw_rects_should_merge(rects[index], rects[i])) {
            rects[index] = ana_rect_union(rects[index], rects[i]);
            if (i < *rect_count - 1) {
                rects[i] = rects[*rect_count - 1];
            }
            (*rect_count)--;
            if (i < index) {
                index--;
            }
            i = 0;
            continue;
        }

        i++;
    }
}

static void bb_add_actor_redraw_rect(
    ANA_Rect* rects,
    int* rect_count,
    ANA_Rect rect)
{
    ANA_Rect padded;
    ANA_Rect world_view;
    int i;

    padded = ana_rect_make(rect.x - 2, rect.y - 2, rect.w + 4, rect.h + 4);
    world_view = ana_camera_world_view(&bb_camera);
    padded = ana_rect_clip(padded, world_view);

    if (ana_rect_is_empty(padded)) {
        return;
    }

    for (i = 0; i < *rect_count; i++) {
        if (ana_rect_contains(rects[i], padded)) {
            return;
        }

        if (ana_rect_contains(padded, rects[i]) ||
                bb_redraw_rects_should_merge(rects[i], padded)) {
            rects[i] = ana_rect_union(rects[i], padded);
            bb_coalesce_redraw_rects(rects, rect_count, i);
            return;
        }
    }

    if (*rect_count < 2 + (BB_MAX_ENEMIES * 2)) {
        rects[*rect_count] = padded;
        (*rect_count)++;
        return;
    }

    rects[0] = ana_rect_union(rects[0], padded);
    bb_coalesce_redraw_rects(rects, rect_count, 0);
}

static void bb_redraw_previous_and_current_actors(void)
{
    int i;
    int rect_count;
    ANA_Rect rect;
    ANA_Rect redraw_rects[2 + (BB_MAX_ENEMIES * 2)];

    rect_count = 0;

    rect = bb_world_rect(bb_prev_player_x, bb_prev_player_y, BB_PLAYER_W, BB_PLAYER_H);
    bb_add_actor_redraw_rect(redraw_rects, &rect_count, rect);
    rect = bb_world_rect(bb_player.x, bb_player.y, BB_PLAYER_W, BB_PLAYER_H);
    bb_add_actor_redraw_rect(redraw_rects, &rect_count, rect);

    for (i = 0; i < bb_enemy_count; i++) {
        rect = bb_world_rect(bb_prev_enemy_x[i], bb_prev_enemy_y[i], 12, 12);
        bb_add_actor_redraw_rect(redraw_rects, &rect_count, rect);
        rect = bb_world_rect(bb_enemies[i].x, bb_enemies[i].y, 12, 12);
        bb_add_actor_redraw_rect(redraw_rects, &rect_count, rect);
    }

    for (i = 0; i < rect_count; i++) {
        ana_tile_layer_redraw_world_rect(&bb_playfield_layer, redraw_rects[i]);
    }
}

static void bb_draw_player(void)
{
    ANA_Rect rect;
    int sx;
    int sy;

    if (bb_player.invuln_ticks > 0 && (bb_frame & 4) != 0) {
        return;
    }

    rect = ana_camera_world_to_screen_rect(
        &bb_camera,
        bb_world_rect(bb_player.x, bb_player.y, BB_PLAYER_W, BB_PLAYER_H));
    sx = rect.x;
    sy = rect.y;

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
    ANA_Rect rect;
    int sx;
    int sy;

    if (!enemy->alive) {
        return;
    }

    rect = ana_camera_world_to_screen_rect(
        &bb_camera,
        bb_world_rect(enemy->x, enemy->y, 12, 12));
    sx = rect.x;
    sy = rect.y;
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
    ANA_Rect hud_viewport;

    ana_tile_layer_init(
        &bb_playfield_layer,
        ANA_LAYER_SIDE_SCROLL,
        0,
        BB_TILE,
        BB_TILE,
        BB_MAP_W,
        BB_MAP_H);
    ana_tile_layer_set_callbacks(
        &bb_playfield_layer,
        bb_read_tile_for_layer,
        bb_draw_tile_for_layer,
        0);
    ana_tile_layer_set_clear_color(&bb_playfield_layer, 0u);
    ana_tile_layer_set_scroll_backend(
        &bb_playfield_layer,
        ANA_SCROLL_BACKEND_HARDWARE);
    ana_tile_layer_set_scroll_sync(
        &bb_playfield_layer,
        ANA_SCROLL_SYNC_DIRTY);
    ana_layer_init(&bb_sprite_layer, ANA_LAYER_SPRITES, 10);
    ana_layer_init(&bb_hud_layer, ANA_LAYER_HUD, 100);
    hud_viewport = ana_rect_make(0, 0, BB_SCREEN_W, BB_HUD_H);
    ana_layer_set_viewport(&bb_hud_layer, hud_viewport);
    ana_layer_set_redraw(&bb_hud_layer, bb_draw_hud_layer, 0);
    bb_sync_layers();

    bb_full_redraw = 1;
    bb_prev_camera_x = -1;
    bb_prev_camera_y = -1;
    bb_prev_score = -1;
    bb_prev_lives = -1;
    bb_prev_level = -1;
    bb_prev_fragments = -1;
    ana_tile_layer_invalidate(&bb_playfield_layer);
}

void bb_render_invalidate(void)
{
    bb_full_redraw = 1;
    ana_tile_layer_invalidate(&bb_playfield_layer);
}

void bb_render_draw(void)
{
    int hud_dirty;
    int hardware_scroll_available;

    hud_dirty = bb_score != bb_prev_score ||
        bb_lives != bb_prev_lives ||
        bb_level_index != bb_prev_level ||
        bb_fragments_left != bb_prev_fragments;
    bb_sync_layers();
    hardware_scroll_available =
        ana_tile_layer_hardware_scroll_available(&bb_playfield_layer);

    if (bb_full_redraw || bb_prev_camera_x < 0 || bb_prev_camera_y < 0) {
        if (!hardware_scroll_available) {
            ana_fill_rect(0, 0, 0, BB_SCREEN_W, BB_SCREEN_H);
        }
        ana_tile_layer_invalidate(&bb_playfield_layer);
        ana_tile_layer_draw(&bb_playfield_layer);
        ana_layer_mark_dirty(&bb_hud_layer);
        ana_layer_draw_if_dirty(&bb_hud_layer);
        bb_draw_actors();
        bb_full_redraw = 0;
        bb_commit_state();
        return;
    }

    if (bb_camera_x != bb_prev_camera_x || bb_camera_y != bb_prev_camera_y) {
        ana_tile_layer_draw(&bb_playfield_layer);
        bb_redraw_previous_and_current_actors();
    } else {
        bb_redraw_previous_and_current_actors();
    }

    if (hud_dirty) {
        ana_layer_mark_dirty(&bb_hud_layer);
    }
    ana_layer_draw_if_dirty(&bb_hud_layer);

    bb_draw_actors();
    bb_commit_state();
}

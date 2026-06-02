#include "byte_brothers_render.h"

#include "byte_brothers_internal.h"

/* Dirty redraw renderer for the Byte Brothers scrolling platform sample. */

#define BB_RENDER_SLOTS 3
#define BB_TILE_DIRTY_RECTS 16
#define BB_RENDER_ALL_SLOT_MASK ((1u << BB_RENDER_SLOTS) - 1u)

static int bb_full_redraw = 1;
static int bb_prev_camera_x = -1;
static int bb_prev_camera_y = -1;
static ANA_Rect bb_tile_dirty_rects[BB_TILE_DIRTY_RECTS];
static unsigned char bb_tile_dirty_slot_mask[BB_TILE_DIRTY_RECTS];
static int bb_tile_dirty_count = 0;
static int bb_prev_actor_valid[BB_RENDER_SLOTS];
static int bb_prev_player_x[BB_RENDER_SLOTS];
static int bb_prev_player_y[BB_RENDER_SLOTS];
static int bb_prev_player_dash_ticks[BB_RENDER_SLOTS];
static int bb_prev_player_facing[BB_RENDER_SLOTS];
static int bb_prev_player_anim_frame[BB_RENDER_SLOTS];
static int bb_prev_enemy_x[BB_RENDER_SLOTS][BB_MAX_ENEMIES];
static int bb_prev_enemy_y[BB_RENDER_SLOTS][BB_MAX_ENEMIES];
static int bb_prev_enemy_alive[BB_RENDER_SLOTS][BB_MAX_ENEMIES];
static int bb_prev_score = -1;
static int bb_prev_lives = -1;
static int bb_prev_level = -1;
static int bb_prev_fragments = -1;
static ANA_TileLayer bb_playfield_layer;
static ANA_Layer bb_sprite_layer;
static ANA_Layer bb_hud_layer;

static void bb_draw_player(void);
static void bb_draw_enemy(const BB_Enemy* enemy);

static ANA_Rect bb_world_rect(int x, int y, int w, int h)
{
    return ana_rect_make(x, y, w, h);
}

static ANA_Rect bb_world_bounds(void)
{
    return bb_world_rect(0, 0, BB_MAP_W * BB_TILE, BB_MAP_H * BB_TILE);
}

static void bb_remove_tile_dirty_rect(int index)
{
    int i;

    if (index < 0 || index >= bb_tile_dirty_count) {
        return;
    }

    for (i = index; i < bb_tile_dirty_count - 1; i++) {
        bb_tile_dirty_rects[i] = bb_tile_dirty_rects[i + 1];
        bb_tile_dirty_slot_mask[i] = bb_tile_dirty_slot_mask[i + 1];
    }
    bb_tile_dirty_count--;
}

static void bb_clear_tile_dirty_rects(void)
{
    bb_tile_dirty_count = 0;
}

static int bb_render_slot(void)
{
    int slot;

    slot = ana_tile_layer_hardware_scroll_frame_slot(&bb_playfield_layer);
    if (slot < 0 || slot >= BB_RENDER_SLOTS) {
        return 0;
    }

    return slot;
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
    ana_fill_rect(9, 164, 2, 3, 10);
    ana_fill_rect(9, 169, 2, 3, 10);

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
    ANA_Rect world_bounds;
    int i;

    padded = ana_rect_make(rect.x - 2, rect.y - 2, rect.w + 4, rect.h + 4);
    world_bounds = bb_world_bounds();
    padded = ana_rect_clip(padded, world_bounds);

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

static void bb_add_tile_dirty_redraw_rects(
    ANA_Rect* rects,
    int* rect_count,
    int slot,
    int all_slots)
{
    unsigned char slot_mask;
    int i;

    slot_mask = (unsigned char)(1u << slot);
    i = 0;
    while (i < bb_tile_dirty_count) {
        if ((bb_tile_dirty_slot_mask[i] & slot_mask) == 0u) {
            i++;
            continue;
        }

        bb_add_actor_redraw_rect(rects, rect_count, bb_tile_dirty_rects[i]);
        if (all_slots) {
            bb_tile_dirty_slot_mask[i] = 0u;
        } else {
            bb_tile_dirty_slot_mask[i] =
                (unsigned char)(bb_tile_dirty_slot_mask[i] & ~slot_mask);
        }

        if (bb_tile_dirty_slot_mask[i] == 0u) {
            bb_remove_tile_dirty_rect(i);
            continue;
        }

        i++;
    }
}

static ANA_Rect bb_player_redraw_rect(int x, int y)
{
    return bb_world_rect(x - 10, y, BB_PLAYER_W + 20, BB_PLAYER_H);
}

static int bb_player_anim_frame(void)
{
    if (!bb_player.on_ground) {
        return 3;
    }

    if (bb_player.vx != 0) {
        return 1 + ((bb_frame >> 2) & 1);
    }

    return 0;
}

static int bb_rect_intersects_any(
    ANA_Rect rect,
    const ANA_Rect* redraw_rects,
    int rect_count)
{
    int i;

    for (i = 0; i < rect_count; i++) {
        if (ana_rect_intersects(redraw_rects[i], rect)) {
            return 1;
        }
    }

    return 0;
}

static void bb_draw_actors_in_world_rects(
    const ANA_Rect* redraw_rects,
    int rect_count)
{
    int i;
    ANA_Rect rect;

    for (i = 0; i < bb_enemy_count; i++) {
        rect = bb_world_rect(
            bb_enemies[i].x,
            bb_enemies[i].y,
            BB_ENEMY_W,
            BB_ENEMY_H);
        if (bb_rect_intersects_any(rect, redraw_rects, rect_count) &&
                bb_rect_visible_world(rect)) {
            bb_draw_enemy(&bb_enemies[i]);
        }
    }

    rect = bb_player_redraw_rect(bb_player.x, bb_player.y);
    if (bb_rect_intersects_any(rect, redraw_rects, rect_count) &&
            bb_rect_visible_world(rect)) {
        bb_draw_player();
    }
}

static void bb_redraw_previous_and_current_actors(int slot, int all_slots)
{
    int i;
    int rect_count;
    int player_changed;
    int player_anim_frame;
    int enemy_changed;
    ANA_Rect rect;
    ANA_Rect redraw_rects[2 + (BB_MAX_ENEMIES * 2)];

    rect_count = 0;
    player_anim_frame = bb_player_anim_frame();
    bb_add_tile_dirty_redraw_rects(
        redraw_rects,
        &rect_count,
        slot,
        all_slots);

    player_changed = !bb_prev_actor_valid[slot] ||
        bb_prev_player_x[slot] != bb_player.x ||
        bb_prev_player_y[slot] != bb_player.y ||
        bb_prev_player_dash_ticks[slot] > 0 ||
        bb_player.dash_ticks > 0 ||
        bb_prev_player_facing[slot] != bb_player.facing ||
        bb_prev_player_anim_frame[slot] != player_anim_frame;

    if (player_changed && bb_prev_actor_valid[slot]) {
        rect = bb_player_redraw_rect(
            bb_prev_player_x[slot],
            bb_prev_player_y[slot]);
        bb_add_actor_redraw_rect(redraw_rects, &rect_count, rect);
    }
    if (player_changed) {
        rect = bb_player_redraw_rect(bb_player.x, bb_player.y);
        bb_add_actor_redraw_rect(redraw_rects, &rect_count, rect);
    }

    for (i = 0; i < bb_enemy_count; i++) {
        enemy_changed = !bb_prev_actor_valid[slot] ||
            bb_prev_enemy_x[slot][i] != bb_enemies[i].x ||
            bb_prev_enemy_y[slot][i] != bb_enemies[i].y ||
            bb_prev_enemy_alive[slot][i] != bb_enemies[i].alive;

        if (enemy_changed &&
                bb_prev_actor_valid[slot] &&
                bb_prev_enemy_alive[slot][i]) {
            rect = bb_world_rect(
                bb_prev_enemy_x[slot][i],
                bb_prev_enemy_y[slot][i],
                BB_ENEMY_W,
                BB_ENEMY_H);
            bb_add_actor_redraw_rect(redraw_rects, &rect_count, rect);
        }
        if (enemy_changed && bb_enemies[i].alive) {
            rect = bb_world_rect(
                bb_enemies[i].x,
                bb_enemies[i].y,
                BB_ENEMY_W,
                BB_ENEMY_H);
            bb_add_actor_redraw_rect(redraw_rects, &rect_count, rect);
        }
    }

    for (i = 0; i < rect_count; i++) {
        ana_tile_layer_redraw_world_rect(&bb_playfield_layer, redraw_rects[i]);
    }

    bb_draw_actors_in_world_rects(redraw_rects, rect_count);
}

static void bb_draw_player(void)
{
    ANA_Rect rect;
    int anim_frame;
    int eye_x;
    int front_arm_x;
    int back_arm_x;
    int sx;
    int sy;

    rect = ana_camera_world_to_screen_rect(
        &bb_camera,
        bb_world_rect(bb_player.x, bb_player.y, BB_PLAYER_W, BB_PLAYER_H));
    sx = rect.x;
    sy = rect.y;
    anim_frame = bb_player_anim_frame();
    eye_x = bb_player.facing >= 0 ? sx + 14 : sx + 5;
    front_arm_x = bb_player.facing >= 0 ? sx + 15 : sx + 1;
    back_arm_x = bb_player.facing >= 0 ? sx + 1 : sx + 15;

    ana_fill_rect(1, sx + 3, sy, 14, 2);
    ana_fill_rect(13, sx + 2, sy + 2, 16, 10);
    ana_fill_rect(5, sx + 4, sy + 3, 12, 8);
    ana_fill_rect(6, sx + 3, sy + 2, 14, 3);
    ana_fill_rect(1, eye_x, sy + 4, 2, 2);
    ana_fill_rect(13, sx + 2, sy + 10, 16, 15);
    ana_fill_rect(4, sx + 4, sy + 12, 12, 12);
    ana_fill_rect(3, sx + 4, sy + 20, 12, 3);
    ana_fill_rect(1, sx + 6, sy + 14, 8, 2);

    ana_fill_rect(13, back_arm_x - 1, sy + 12, 4, 11);
    ana_fill_rect(4, back_arm_x - 1, sy + 13, 4, 8);
    if (anim_frame == 2) {
        ana_fill_rect(13, front_arm_x, sy + 14, 4, 10);
        ana_fill_rect(4, front_arm_x, sy + 14, 4, 7);
    } else {
        ana_fill_rect(13, front_arm_x, sy + 11, 4, 11);
        ana_fill_rect(4, front_arm_x, sy + 12, 4, 8);
    }

    if (anim_frame == 1) {
        ana_fill_rect(13, sx + 4, sy + 21, 5, 7);
        ana_fill_rect(13, sx + 12, sy + 20, 5, 6);
        ana_fill_rect(13, sx + 2, sy + 25, 8, 3);
        ana_fill_rect(13, sx + 12, sy + 24, 7, 3);
    } else if (anim_frame == 2) {
        ana_fill_rect(13, sx + 4, sy + 20, 5, 6);
        ana_fill_rect(13, sx + 12, sy + 21, 5, 7);
        ana_fill_rect(13, sx + 3, sy + 24, 7, 3);
        ana_fill_rect(13, sx + 11, sy + 25, 8, 3);
    } else if (anim_frame == 3) {
        ana_fill_rect(13, sx + 4, sy + 21, 6, 4);
        ana_fill_rect(13, sx + 12, sy + 21, 6, 4);
        ana_fill_rect(13, sx + 3, sy + 24, 7, 3);
        ana_fill_rect(13, sx + 12, sy + 24, 7, 3);
    } else {
        ana_fill_rect(13, sx + 4, sy + 21, 5, 7);
        ana_fill_rect(13, sx + 12, sy + 21, 5, 7);
        ana_fill_rect(13, sx + 3, sy + 25, 7, 3);
        ana_fill_rect(13, sx + 11, sy + 25, 7, 3);
    }

    if (bb_player.dash_ticks > 0) {
        if (bb_player.facing > 0) {
            ana_fill_rect(3, sx - 8, sy + 12, 6, 3);
            ana_fill_rect(4, sx - 5, sy + 16, 3, 2);
        } else {
            ana_fill_rect(3, sx + BB_PLAYER_W + 2, sy + 12, 6, 3);
            ana_fill_rect(4, sx + BB_PLAYER_W + 2, sy + 16, 3, 2);
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
        bb_world_rect(enemy->x, enemy->y, BB_ENEMY_W, BB_ENEMY_H));
    sx = rect.x;
    sy = rect.y;

    ana_fill_rect(1, sx + 5, sy + 2, 5, 4);
    ana_fill_rect(1, sx + 14, sy + 2, 5, 4);
    ana_fill_rect(13, sx + 2, sy + 6, 20, 16);
    ana_fill_rect(7, sx + 3, sy + 7, 18, 14);
    ana_fill_rect(15, sx + 6, sy + 3, 12, 7);
    ana_fill_rect(7, sx + 8, sy + 2, 8, 3);
    ana_fill_rect(1, sx + 6, sy + 11, 4, 4);
    ana_fill_rect(1, sx + 14, sy + 11, 4, 4);
    ana_fill_rect(13, sx + 8, sy + 12, 2, 2);
    ana_fill_rect(13, sx + 16, sy + 12, 2, 2);
    ana_fill_rect(1, sx + 9, sy + 18, 6, 2);
    ana_fill_rect(13, sx + 5, sy + 19, 5, 4);
    ana_fill_rect(13, sx + 14, sy + 19, 5, 4);
    ana_fill_rect(15, sx + 4, sy + 6, 3, 3);
    ana_fill_rect(15, sx + 17, sy + 6, 3, 3);
}

static void bb_draw_actors(void)
{
    int i;
    ANA_Rect rect;

    for (i = 0; i < bb_enemy_count; i++) {
        rect = bb_world_rect(
            bb_enemies[i].x,
            bb_enemies[i].y,
            BB_ENEMY_W,
            BB_ENEMY_H);
        if (bb_rect_visible_world(rect)) {
            bb_draw_enemy(&bb_enemies[i]);
        }
    }

    rect = bb_world_rect(bb_player.x, bb_player.y, BB_PLAYER_W, BB_PLAYER_H);
    if (bb_rect_visible_world(rect)) {
        bb_draw_player();
    }
}

static void bb_commit_state(int slot)
{
    int i;

    bb_prev_camera_x = bb_camera_x;
    bb_prev_camera_y = bb_camera_y;
    bb_prev_player_x[slot] = bb_player.x;
    bb_prev_player_y[slot] = bb_player.y;
    bb_prev_player_dash_ticks[slot] = bb_player.dash_ticks;
    bb_prev_player_facing[slot] = bb_player.facing;
    bb_prev_player_anim_frame[slot] = bb_player_anim_frame();
    bb_prev_score = bb_score;
    bb_prev_lives = bb_lives;
    bb_prev_level = bb_level_index;
    bb_prev_fragments = bb_fragments_left;

    for (i = 0; i < BB_MAX_ENEMIES; i++) {
        bb_prev_enemy_x[slot][i] = bb_enemies[i].x;
        bb_prev_enemy_y[slot][i] = bb_enemies[i].y;
        bb_prev_enemy_alive[slot][i] = bb_enemies[i].alive;
    }
    bb_prev_actor_valid[slot] = 1;
}

void bb_render_reset(void)
{
    ANA_Rect hud_viewport;
    int i;

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
    for (i = 0; i < BB_RENDER_SLOTS; i++) {
        bb_prev_actor_valid[i] = 0;
    }
    bb_clear_tile_dirty_rects();
    ana_tile_layer_invalidate(&bb_playfield_layer);
}

void bb_render_invalidate(void)
{
    bb_full_redraw = 1;
    bb_clear_tile_dirty_rects();
    ana_tile_layer_invalidate(&bb_playfield_layer);
}

void bb_render_tile_changed(int tx, int ty)
{
    ANA_Rect rect;
    int i;

    if (tx < 0 || tx >= BB_MAP_W || ty < 0 || ty >= BB_MAP_H) {
        return;
    }

    rect = bb_world_rect(tx * BB_TILE, ty * BB_TILE, BB_TILE, BB_TILE);
    for (i = 0; i < bb_tile_dirty_count; i++) {
        if (ana_rect_contains(bb_tile_dirty_rects[i], rect)) {
            bb_tile_dirty_slot_mask[i] =
                (unsigned char)BB_RENDER_ALL_SLOT_MASK;
            return;
        }

        if (ana_rect_contains(rect, bb_tile_dirty_rects[i]) ||
                bb_redraw_rects_should_merge(bb_tile_dirty_rects[i], rect)) {
            bb_tile_dirty_rects[i] =
                ana_rect_union(bb_tile_dirty_rects[i], rect);
            bb_tile_dirty_slot_mask[i] =
                (unsigned char)BB_RENDER_ALL_SLOT_MASK;
            return;
        }
    }

    if (bb_tile_dirty_count >= BB_TILE_DIRTY_RECTS) {
        bb_render_invalidate();
        return;
    }

    bb_tile_dirty_rects[bb_tile_dirty_count] = rect;
    bb_tile_dirty_slot_mask[bb_tile_dirty_count] =
        (unsigned char)BB_RENDER_ALL_SLOT_MASK;
    bb_tile_dirty_count++;
}

void bb_render_draw(void)
{
    int hud_dirty;
    int hardware_scroll_available;
    int slot;

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
        slot = bb_render_slot();
        ana_layer_mark_dirty(&bb_hud_layer);
        ana_layer_draw_if_dirty(&bb_hud_layer);
        bb_draw_actors();
        bb_full_redraw = 0;
        bb_clear_tile_dirty_rects();
        bb_commit_state(slot);
        return;
    }

    if (hardware_scroll_available ||
            bb_camera_x != bb_prev_camera_x ||
            bb_camera_y != bb_prev_camera_y) {
        ana_tile_layer_draw(&bb_playfield_layer);
    }
    slot = bb_render_slot();
    bb_redraw_previous_and_current_actors(slot, !hardware_scroll_available);

    if (hud_dirty) {
        ana_layer_mark_dirty(&bb_hud_layer);
    }
    ana_layer_draw_if_dirty(&bb_hud_layer);

    bb_commit_state(slot);
}

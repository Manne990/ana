#include "byte_brothers_render.h"

#include "byte_brothers_assets.h"
#include "byte_brothers_internal.h"

#ifndef BB_RENDER_FINE_PROFILING
#define BB_RENDER_FINE_PROFILING 0
#endif

#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
#include "ana_internal.h"
#endif

#include <stdio.h>

#ifdef ANA_TARGET_AMIGA
#include <string.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/sprite.h>
#include <graphics/view.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#endif

/* Dirty redraw renderer for the Byte Brothers scrolling platform sample. */

#define BB_RENDER_SLOTS 3
#define BB_TILE_DIRTY_RECTS 16
#define BB_REDRAW_MERGE_SLACK 64
#define BB_ACTOR_REDRAW_PADDING 2
#define BB_PLAYER_LEFT_FRAME_OFFSET 4
#define BB_RENDER_ALL_SLOT_MASK ((1u << BB_RENDER_SLOTS) - 1u)
#ifndef BB_INPUT_DEBUG_OVERLAY
#define BB_INPUT_DEBUG_OVERLAY 0
#endif

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
static int bb_prev_enemy_visible[BB_RENDER_SLOTS][BB_MAX_ENEMIES];
static int bb_prev_enemy_hardware[BB_RENDER_SLOTS][BB_MAX_ENEMIES];
static int bb_prev_enemy_count[BB_RENDER_SLOTS];
static int bb_prev_score = -1;
static int bb_prev_lives = -1;
static int bb_prev_level = -1;
static int bb_prev_fragments = -1;
static ANA_TileLayer bb_playfield_layer;
static ANA_Layer bb_sprite_layer;
static ANA_Layer bb_hud_layer;
static long bb_debug_bitmap_enemy_draws = 0L;
static long bb_debug_full_redraws = 0L;
static long bb_debug_tile_layer_draws = 0L;
static long bb_debug_tile_hardware_unavailable_draws = 0L;
static long bb_debug_tile_hardware_draws = 0L;
static long bb_debug_tile_software_draws = 0L;
static long bb_debug_tile_redraw_world_rects = 0L;
static long bb_debug_tile_restore_world_rects = 0L;
static long bb_debug_actor_redraw_passes = 0L;
static long bb_debug_player_draws = 0L;
static long bb_debug_tile_perf_ticks = 0L;
static long bb_debug_hud_perf_ticks = 0L;
static long bb_debug_actor_perf_ticks = 0L;
static long bb_debug_hw_player_perf_ticks = 0L;
static long bb_debug_hw_enemy_perf_ticks = 0L;

#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
static void bb_record_perf_ticks(long* total, unsigned long start_ticks)
{
    if (total == 0) {
        return;
    }

    *total += (long)(ana_platform_perf_ticks() - start_ticks);
}
#endif

static void bb_draw_player(void);
static void bb_draw_enemy(const BB_Enemy* enemy);
static int bb_player_anim_frame(void);
#if BB_INPUT_DEBUG_OVERLAY
static void bb_draw_input_debug_overlay(void);
#endif

#ifdef ANA_TARGET_AMIGA
#ifndef BB_HW_DIRECT_SPRITE_POS
#define BB_HW_DIRECT_SPRITE_POS 1
#endif
#ifndef BB_HW_DIRECT_SECOND_HALF_POS
#define BB_HW_DIRECT_SECOND_HALF_POS 1
#endif
#ifndef BB_HW_SYNC_SPRITE_TOF
#define BB_HW_SYNC_SPRITE_TOF 0
#endif
#define BB_HW_PLAYER_SPRITES 2
#define BB_HW_PLAYER_MAX_FRAMES 8
#define BB_HW_PLAYER_SPRITE_WORDS ((BB_PLAYER_H * 2) + 4)
#define BB_HW_PLAYER_SPRITE_BYTES \
    (BB_HW_PLAYER_SPRITE_WORDS * (int)sizeof(UWORD))
#define BB_HW_ENEMY_FIRST_CHANNEL 2
#ifndef BB_HW_ENEMY_MAX
#define BB_HW_ENEMY_MAX 6
#endif
#define BB_HW_ENEMY_SPRITES BB_HW_ENEMY_MAX
#define BB_HW_ENEMY_SOURCE_X ((BB_ENEMY_W - 16) / 2)
#define BB_HW_ENEMY_SPRITE_WORDS ((BB_ENEMY_H * 2) + 4)
#define BB_HW_ENEMY_SPRITE_BYTES \
    (BB_HW_ENEMY_SPRITE_WORDS * (int)sizeof(UWORD))

typedef struct BB_HardwareEnemySprite {
    struct SimpleSprite sprite;
    UWORD* data;
    int channel;
    int ready;
    int allocated;
} BB_HardwareEnemySprite;

typedef struct BB_HardwarePlayerSprite {
    struct SimpleSprite sprite;
    UWORD* data;
    int channel;
    int ready;
    int allocated;
} BB_HardwarePlayerSprite;

static BB_HardwarePlayerSprite bb_hw_player_sprites[BB_HW_PLAYER_SPRITES];
static UWORD bb_hw_player_static_data
    [BB_HW_PLAYER_SPRITES][BB_HW_PLAYER_MAX_FRAMES][BB_HW_PLAYER_SPRITE_WORDS];
static BB_HardwareEnemySprite bb_hw_enemy_sprites[BB_HW_ENEMY_SPRITES];
static UWORD bb_hw_enemy_static_data
    [BB_HW_ENEMY_SPRITES][BB_HW_ENEMY_SPRITE_WORDS];
static int bb_hw_player_ready = 0;
static int bb_hw_player_failed = 0;
static int bb_hw_player_failure_reported = 0;
static int bb_hw_player_sprite_count = 0;
static int bb_hw_player_visible = 0;
static int bb_hw_player_current_frame = -1;
static int bb_hw_player_last_x = -1000;
static int bb_hw_player_last_y = -1000;
static int bb_hw_player_frame_count = 0;
static long bb_hw_player_update_calls = 0L;
static long bb_hw_player_visible_moves = 0L;
static const char* bb_hw_player_failure_reason = "not attempted";
static int bb_hw_enemy_ready = 0;
static int bb_hw_enemy_failed = 0;
static int bb_hw_enemy_failure_reported = 0;
static int bb_hw_enemy_sprite_count = 0;
static int bb_hw_enemy_slot_count = 0;
static int bb_hw_enemy_slot_visible[BB_HW_ENEMY_MAX];
static int bb_hw_enemy_slot_last_x[BB_HW_ENEMY_MAX];
static int bb_hw_enemy_slot_last_y[BB_HW_ENEMY_MAX];
static long bb_hw_enemy_update_calls = 0L;
static long bb_hw_enemy_visible_moves = 0L;
static const char* bb_hw_enemy_failure_reason = "not attempted";

static void bb_hw_wait_for_sprite_update(void)
{
#if BB_HW_SYNC_SPRITE_TOF
    WaitTOF();
#endif
}

#if BB_HW_DIRECT_SPRITE_POS || BB_HW_DIRECT_SECOND_HALF_POS
static void bb_hw_sprite_write_pos(
    struct SimpleSprite* sprite,
    int x,
    int y,
    int height)
{
    int hstart;
    int vstart;
    int vstop;
    UWORD* data;

    if (sprite == 0 || sprite->posctldata == 0 || height <= 0) {
        return;
    }

    hstart = x + 64;
    vstart = y + 44;
    vstop = vstart + height;
    if (hstart < 0) {
        hstart = 0;
    }
    if (vstart < 0) {
        vstart = 0;
    }
    if (vstop < 0) {
        vstop = 0;
    }

    data = sprite->posctldata;
    data[0] = (UWORD)(((vstart & 0xff) << 8) | ((hstart >> 1) & 0xff));
    data[1] = (UWORD)(((vstop & 0xff) << 8) |
        ((vstart & 0x100) != 0 ? 0x0004 : 0x0000) |
        ((vstop & 0x100) != 0 ? 0x0002 : 0x0000) |
        (hstart & 1));
    sprite->x = (WORD)x;
    sprite->y = (WORD)y;
}
#endif

static void bb_hw_sprite_move(
    struct ViewPort* viewport,
    struct SimpleSprite* sprite,
    int x,
    int y,
    int height)
{
#if BB_HW_DIRECT_SPRITE_POS
    (void)viewport;
    bb_hw_sprite_write_pos(sprite, x, y, height);
#else
    (void)height;
    MoveSprite(viewport, sprite, x, y);
#endif
}

static void bb_hw_sprite_move_second_half(
    struct ViewPort* viewport,
    struct SimpleSprite* sprite,
    int x,
    int y,
    int height)
{
#if BB_HW_DIRECT_SECOND_HALF_POS
    (void)viewport;
    bb_hw_sprite_write_pos(sprite, x, y, height);
#else
    bb_hw_sprite_move(viewport, sprite, x, y, height);
#endif
}
#endif

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
    return rect.x < bb_camera.x + bb_camera.view_w &&
        rect.x + rect.w > bb_camera.x &&
        rect.y < bb_camera.y + bb_camera.view_h &&
        rect.y + rect.h > bb_camera.y;
}

#ifdef ANA_TARGET_AMIGA
static int bb_tile_dirty_slot_pending(int slot)
{
    unsigned char slot_mask;
    int i;

    if (slot < 0 || slot >= BB_RENDER_SLOTS) {
        return 0;
    }

    slot_mask = (unsigned char)(1u << slot);
    for (i = 0; i < bb_tile_dirty_count; i++) {
        if ((bb_tile_dirty_slot_mask[i] & slot_mask) != 0u) {
            return 1;
        }
    }

    return 0;
}

static int bb_world_to_screen_x(int x)
{
    return x - bb_camera.x + bb_camera.view_x;
}

static int bb_world_to_screen_y(int y)
{
    return y - bb_camera.y + bb_camera.view_y;
}
#endif

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

#if BB_INPUT_DEBUG_OVERLAY
static unsigned char bb_debug_glyph_bits(char ch, int row)
{
    switch (ch) {
    case '0': {
        static const unsigned char bits[5] = {7, 5, 5, 5, 7};
        return bits[row];
    }
    case '1': {
        static const unsigned char bits[5] = {2, 6, 2, 2, 7};
        return bits[row];
    }
    case '2': {
        static const unsigned char bits[5] = {7, 1, 7, 4, 7};
        return bits[row];
    }
    case '3': {
        static const unsigned char bits[5] = {7, 1, 7, 1, 7};
        return bits[row];
    }
    case '4': {
        static const unsigned char bits[5] = {5, 5, 7, 1, 1};
        return bits[row];
    }
    case '5': {
        static const unsigned char bits[5] = {7, 4, 7, 1, 7};
        return bits[row];
    }
    case '6': {
        static const unsigned char bits[5] = {7, 4, 7, 5, 7};
        return bits[row];
    }
    case '7': {
        static const unsigned char bits[5] = {7, 1, 1, 2, 2};
        return bits[row];
    }
    case '8': {
        static const unsigned char bits[5] = {7, 5, 7, 5, 7};
        return bits[row];
    }
    case '9': {
        static const unsigned char bits[5] = {7, 5, 7, 1, 7};
        return bits[row];
    }
    case 'A': {
        static const unsigned char bits[5] = {7, 5, 7, 5, 5};
        return bits[row];
    }
    case 'B': {
        static const unsigned char bits[5] = {6, 5, 6, 5, 6};
        return bits[row];
    }
    case 'C': {
        static const unsigned char bits[5] = {7, 4, 4, 4, 7};
        return bits[row];
    }
    case 'D': {
        static const unsigned char bits[5] = {6, 5, 5, 5, 6};
        return bits[row];
    }
    case 'E': {
        static const unsigned char bits[5] = {7, 4, 6, 4, 7};
        return bits[row];
    }
    case 'F': {
        static const unsigned char bits[5] = {7, 4, 6, 4, 4};
        return bits[row];
    }
    case 'I': {
        static const unsigned char bits[5] = {7, 2, 2, 2, 7};
        return bits[row];
    }
    case 'J': {
        static const unsigned char bits[5] = {1, 1, 1, 5, 7};
        return bits[row];
    }
    case 'K': {
        static const unsigned char bits[5] = {5, 5, 6, 5, 5};
        return bits[row];
    }
    case 'N': {
        static const unsigned char bits[5] = {5, 7, 7, 7, 5};
        return bits[row];
    }
    case 'P': {
        static const unsigned char bits[5] = {6, 5, 6, 4, 4};
        return bits[row];
    }
    case 'Q': {
        static const unsigned char bits[5] = {7, 5, 5, 7, 1};
        return bits[row];
    }
    case 'R': {
        static const unsigned char bits[5] = {6, 5, 6, 5, 5};
        return bits[row];
    }
    case 'S': {
        static const unsigned char bits[5] = {7, 4, 7, 1, 7};
        return bits[row];
    }
    case 'V': {
        static const unsigned char bits[5] = {5, 5, 5, 5, 2};
        return bits[row];
    }
    case 'X': {
        static const unsigned char bits[5] = {5, 5, 2, 5, 5};
        return bits[row];
    }
    default:
        return 0u;
    }
}

static void bb_draw_debug_char(char ch, int x, int y, unsigned char color)
{
    int row;
    int col;
    unsigned char bits;

    if (ch == ' ') {
        return;
    }

    for (row = 0; row < 5; row++) {
        bits = bb_debug_glyph_bits(ch, row);
        for (col = 0; col < 3; col++) {
            if ((bits & (1u << (2 - col))) != 0u) {
                ana_fill_rect(color, x + col * 2, y + row * 2, 2, 2);
            }
        }
    }
}

static void bb_draw_debug_text(int x, int y, const char* text)
{
    while (*text != '\0') {
        bb_draw_debug_char(*text, x, y, 1u);
        x += 8;
        text++;
    }
}

static void bb_draw_input_debug_overlay(void)
{
    ANA_InputDebug debug;
    char line[48];

    ana_input_debug_snapshot(&debug);

    ana_fill_rect(0, 0, BB_HUD_H, BB_SCREEN_W, 26);
    sprintf(
        line,
        "N%03X C%02X R%02X K%02X D%d",
        debug.event_count & 0xfff,
        debug.last_class & 0xff,
        debug.last_code & 0xff,
        debug.last_key & 0xff,
        debug.last_is_down ? 1 : 0);
    bb_draw_debug_text(2, BB_HUD_H + 2, line);

    sprintf(
        line,
        "S0%02X B0%02X S1%02X B1%02X T%d",
        (int)(debug.current_state[ANA_INPUT_DEVICE_0] & 0xffu),
        (int)(debug.backend_state[ANA_INPUT_DEVICE_0] & 0xffu),
        (int)(debug.current_state[ANA_INPUT_DEVICE_1] & 0xffu),
        (int)(debug.backend_state[ANA_INPUT_DEVICE_1] & 0xffu),
        debug.quit_requested ? 1 : 0);
    bb_draw_debug_text(2, BB_HUD_H + 14, line);
}
#endif

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
    if (tile == BB_TILE_SOLID) {
        ana_fill_rect(2, sx, sy, BB_TILE, BB_TILE);
        ana_fill_rect(3, sx, sy, BB_TILE, 2);
        ana_fill_rect(14, sx, sy + BB_TILE - 2, BB_TILE, 2);
    } else if (tile == BB_TILE_PLATFORM) {
        ana_fill_rect(3, sx, sy + 6, BB_TILE, 4);
        ana_fill_rect(4, sx, sy + 5, BB_TILE, 1);
    } else if (tile == BB_TILE_POWER) {
        ana_fill_rect(6, sx + 1, sy + 1, BB_TILE - 2, BB_TILE - 2);
        ana_fill_rect(5, sx + 6, sy + 4, 4, 4);
        ana_fill_rect(5, sx + 6, sy + 11, 4, 2);
    } else if (tile == BB_TILE_BONUS) {
        ana_fill_rect(8, sx, sy, BB_TILE, BB_TILE);
        ana_fill_rect(15, sx + 2, sy + 2, BB_TILE - 4, 2);
        ana_fill_rect(13, sx, sy + 8, BB_TILE, 2);
    } else if (tile == BB_TILE_FRAGMENT) {
        ana_fill_rect(1, sx + 6, sy + 6, 4, 4);
    } else if (tile == BB_TILE_RARE_FRAGMENT) {
        ana_fill_rect(5, sx + 4, sy + 3, 8, 10);
        ana_fill_rect(6, sx + 2, sy + 6, 12, 4);
    } else if (tile == BB_TILE_SPIKE) {
        ana_fill_rect(7, sx + 6, sy + 4, 4, 10);
        ana_fill_rect(7, sx + 3, sy + 10, 10, 4);
    } else if (tile == BB_TILE_WATER) {
        ana_fill_rect(8, sx + 1, sy + 6, 5, 3);
        ana_fill_rect(8, sx + 6, sy + 9, 5, 3);
        ana_fill_rect(8, sx + 11, sy + 6, 4, 3);
    } else if (tile == BB_TILE_CODE_FRAGMENT) {
        ana_fill_rect(12, sx + 5, sy + 3, 7, 10);
        ana_fill_rect(4, sx + 7, sy + 5, 3, 2);
    } else if (tile == BB_TILE_GATE) {
        ana_fill_rect(10, sx + 2, sy + 1, 12, 14);
        ana_fill_rect(0, sx + 5, sy + 4, 6, 8);
    } else if (tile == BB_TILE_EXIT) {
        ana_fill_rect(9, sx + 2, sy + 2, 12, 12);
        ana_fill_rect(4, sx + 5, sy + 5, 6, 6);
    }
}

static unsigned char bb_read_tile_for_layer(int tx, int ty, void* user_data)
{
    char tile;

    (void)user_data;

    tile = bb_tile_at(tx, ty);
    if (tile == BB_TILE_EMPTY || tile == BB_TILE_HIDDEN) {
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

static void bb_add_tile_dirty_redraw_rects(
    ANA_RectList* tile_rects,
    ANA_RectList* redraw_rects,
    int slot,
    int all_slots)
{
    ANA_Rect rect;
    unsigned char slot_mask;
    int i;

    slot_mask = (unsigned char)(1u << slot);
    i = 0;
    while (i < bb_tile_dirty_count) {
        if ((bb_tile_dirty_slot_mask[i] & slot_mask) == 0u) {
            i++;
            continue;
        }

        rect = bb_tile_dirty_rects[i];
        ana_rect_list_add_padded(tile_rects, rect, BB_ACTOR_REDRAW_PADDING);
        if (redraw_rects != 0 && redraw_rects != tile_rects) {
            ana_rect_list_add_padded(
                redraw_rects,
                rect,
                BB_ACTOR_REDRAW_PADDING);
        }
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

static ANA_Rect bb_player_redraw_rect_at(
    int x,
    int y,
    int dash_ticks,
    int facing)
{
    if (dash_ticks <= 0) {
        return bb_world_rect(x, y, BB_PLAYER_W, BB_PLAYER_H);
    }

    if (facing > 0) {
        return bb_world_rect(x - 8, y, BB_PLAYER_W + 8, BB_PLAYER_H);
    }

    return bb_world_rect(x, y, BB_PLAYER_W + 8, BB_PLAYER_H);
}

static ANA_Rect bb_enemy_rect(const BB_Enemy* enemy)
{
    return bb_world_rect(enemy->x, enemy->y, BB_ENEMY_W, BB_ENEMY_H);
}

static int bb_enemy_visible(const BB_Enemy* enemy)
{
    return enemy != 0 &&
        enemy->alive &&
        enemy->x < bb_camera.x + bb_camera.view_w &&
        enemy->x + BB_ENEMY_W > bb_camera.x &&
        enemy->y < bb_camera.y + bb_camera.view_h &&
        enemy->y + BB_ENEMY_H > bb_camera.y;
}

#ifdef ANA_TARGET_AMIGA
static int bb_hw_player_color_code(int color)
{
    switch (color) {
    case 13:
    case 3:
        return 1;
    case 1:
    case 4:
        return 2;
    case 5:
    case 6:
        return 3;
    default:
        return color > 0 ? 1 : 0;
    }
}

static void bb_hw_player_set_sprite_colors(
    struct ViewPort* viewport,
    int channel)
{
    int base;

    if (viewport == 0) {
        return;
    }

    base = 17 + ((channel >> 1) * 4);
    SetRGB4(viewport, (LONG)base, 4, 4, 5);
    SetRGB4(viewport, (LONG)(base + 1), 3, 12, 15);
    SetRGB4(viewport, (LONG)(base + 2), 15, 14, 0);
}

static UWORD* bb_hw_player_frame_data(int sprite_index, int frame)
{
    if (sprite_index < 0 ||
            sprite_index >= BB_HW_PLAYER_SPRITES ||
            frame < 0 ||
            frame >= bb_hw_player_frame_count ||
            bb_hw_player_sprites[sprite_index].data == 0) {
        return 0;
    }

    return bb_hw_player_sprites[sprite_index].data +
        ((long)frame * BB_HW_PLAYER_SPRITE_WORDS);
}

static int bb_hw_player_build_data(UWORD* data, int frame, int half)
{
    int x;
    int y;
    int source_x;
    int color;
    int code;
    UWORD bit;
    UWORD plane0;
    UWORD plane1;

    if (data == 0 || bb_player_image == 0) {
        return 0;
    }

    data[0] = 0;
    data[1] = 0;

    for (y = 0; y < BB_PLAYER_H; y++) {
        plane0 = 0u;
        plane1 = 0u;
        for (x = 0; x < 16; x++) {
            source_x = (half * 16) + x;
            if (source_x >= BB_PLAYER_W ||
                    !ana_image_pixel_visible(
                        bb_player_image,
                        frame,
                        source_x,
                        y)) {
                continue;
            }

            color = ana_image_pixel_index(bb_player_image, frame, source_x, y);
            code = bb_hw_player_color_code(color);
            bit = (UWORD)(0x8000u >> x);
            if ((code & 1) != 0) {
                plane0 = (UWORD)(plane0 | bit);
            }
            if ((code & 2) != 0) {
                plane1 = (UWORD)(plane1 | bit);
            }
        }

        data[2 + (y * 2)] = plane0;
        data[3 + (y * 2)] = plane1;
    }

    data[2 + (BB_PLAYER_H * 2)] = 0;
    data[3 + (BB_PLAYER_H * 2)] = 0;
    return 1;
}

static void bb_hw_player_hide(void)
{
    struct ViewPort* viewport;
    int i;

    if (!bb_hw_player_ready) {
        return;
    }

    viewport = (struct ViewPort*)ana_gfx_native_viewport();
    if (viewport == 0) {
        return;
    }

    for (i = 0; i < bb_hw_player_sprite_count; i++) {
        if (bb_hw_player_sprites[i].ready) {
            if (i == 0) {
                bb_hw_sprite_move(
                    viewport,
                    &bb_hw_player_sprites[i].sprite,
                    -32,
                    0,
                    BB_PLAYER_H);
            } else {
                bb_hw_sprite_move_second_half(
                    viewport,
                    &bb_hw_player_sprites[i].sprite,
                    -32,
                    0,
                    BB_PLAYER_H);
            }
        }
    }
    bb_hw_player_visible = 0;
    bb_hw_player_last_x = -1000;
    bb_hw_player_last_y = -1000;
}

static void bb_hw_player_shutdown(void)
{
    int i;

    bb_hw_player_hide();
    for (i = 0; i < BB_HW_PLAYER_SPRITES; i++) {
        if (bb_hw_player_sprites[i].ready) {
            FreeSprite((LONG)bb_hw_player_sprites[i].channel);
        }
        if (bb_hw_player_sprites[i].data != 0 &&
                bb_hw_player_sprites[i].allocated) {
            FreeMem(
                bb_hw_player_sprites[i].data,
                (ULONG)(BB_HW_PLAYER_MAX_FRAMES * BB_HW_PLAYER_SPRITE_BYTES));
        }
        bb_hw_player_sprites[i].data = 0;
        bb_hw_player_sprites[i].channel = -1;
        bb_hw_player_sprites[i].ready = 0;
        bb_hw_player_sprites[i].allocated = 0;
    }

    bb_hw_player_sprite_count = 0;
    bb_hw_player_ready = 0;
    bb_hw_player_failed = 0;
    bb_hw_player_failure_reported = 0;
    bb_hw_player_visible = 0;
    bb_hw_player_current_frame = -1;
    bb_hw_player_last_x = -1000;
    bb_hw_player_last_y = -1000;
    bb_hw_player_frame_count = 0;
}

static void bb_hw_player_fail(const char* reason)
{
    bb_hw_player_shutdown();
    bb_hw_player_failed = 1;
    bb_hw_player_failure_reason = reason;
    if (!bb_hw_player_failure_reported) {
        printf(
            "Byte Brothers hardware player sprite unavailable: %s\n",
            reason);
        bb_hw_player_failure_reported = 1;
    }
}

static int bb_hw_player_try_sprite_pair(int first_channel)
{
    int sprite_index;
    int channel;

    if (first_channel < 0 || first_channel + 1 >= 8) {
        return 0;
    }

    for (sprite_index = 0; sprite_index < BB_HW_PLAYER_SPRITES; sprite_index++) {
        channel = GetSprite(
            &bb_hw_player_sprites[sprite_index].sprite,
            (LONG)(first_channel + sprite_index));
        if (channel < 0) {
            while (sprite_index > 0) {
                sprite_index--;
                if (bb_hw_player_sprites[sprite_index].ready) {
                    FreeSprite(
                        (LONG)bb_hw_player_sprites[sprite_index].channel);
                }
                bb_hw_player_sprites[sprite_index].channel = -1;
                bb_hw_player_sprites[sprite_index].ready = 0;
            }
            bb_hw_player_sprite_count = 0;
            return 0;
        }

        bb_hw_player_sprites[sprite_index].channel = channel;
        bb_hw_player_sprites[sprite_index].ready = 1;
        bb_hw_player_sprite_count++;
    }

    return 1;
}

static int bb_hw_player_init(void)
{
    struct ViewPort* viewport;
    static const int preferred_pairs[] = {0, 6, 4, 2};
    int sprite_index;
    int frame;
    int pair_index;
    UWORD* frame_data;

    if (bb_hw_player_ready) {
        return 1;
    }
    if (bb_hw_player_failed) {
        return 0;
    }

    bb_hw_player_failure_reason = "initializing";
    viewport = (struct ViewPort*)ana_gfx_native_viewport();
    if (viewport == 0) {
        bb_hw_player_fail("no viewport");
        return 0;
    }
    if (bb_player_image == 0) {
        bb_hw_player_fail("no player image");
        return 0;
    }

    bb_hw_player_frame_count = ana_image_frame_count(bb_player_image);
    if (bb_hw_player_frame_count <= 0 ||
            bb_hw_player_frame_count > BB_HW_PLAYER_MAX_FRAMES) {
        bb_hw_player_fail("player frame count");
        return 0;
    }

    for (sprite_index = 0; sprite_index < BB_HW_PLAYER_SPRITES; sprite_index++) {
        bb_hw_player_sprites[sprite_index].data =
            (UWORD*)AllocMem(
                (ULONG)(BB_HW_PLAYER_MAX_FRAMES * BB_HW_PLAYER_SPRITE_BYTES),
                MEMF_CHIP | MEMF_CLEAR);
        if (bb_hw_player_sprites[sprite_index].data == 0) {
            if ((TypeOfMem(bb_hw_player_static_data[sprite_index]) &
                    MEMF_CHIP) == 0u) {
                bb_hw_player_fail("player sprite chip allocation");
                return 0;
            }
            bb_hw_player_sprites[sprite_index].data =
                &bb_hw_player_static_data[sprite_index][0][0];
            memset(
                bb_hw_player_sprites[sprite_index].data,
                0,
                (size_t)(BB_HW_PLAYER_MAX_FRAMES *
                    BB_HW_PLAYER_SPRITE_BYTES));
            bb_hw_player_sprites[sprite_index].allocated = 0;
        } else {
            bb_hw_player_sprites[sprite_index].allocated = 1;
        }

        for (frame = 0; frame < bb_hw_player_frame_count; frame++) {
            frame_data = bb_hw_player_frame_data(sprite_index, frame);
            if (!bb_hw_player_build_data(frame_data, frame, sprite_index)) {
                bb_hw_player_fail("player sprite data build");
                return 0;
            }
        }

        frame_data = bb_hw_player_frame_data(sprite_index, 0);
        bb_hw_player_sprites[sprite_index].sprite.posctldata = frame_data;
        bb_hw_player_sprites[sprite_index].sprite.height = BB_PLAYER_H;
        bb_hw_player_sprites[sprite_index].sprite.x = 0;
        bb_hw_player_sprites[sprite_index].sprite.y = 0;
        bb_hw_player_sprites[sprite_index].sprite.num = 0;
    }

    for (pair_index = 0;
            pair_index <
                (int)(sizeof(preferred_pairs) / sizeof(preferred_pairs[0]));
            pair_index++) {
        if (bb_hw_player_try_sprite_pair(preferred_pairs[pair_index])) {
            break;
        }
    }
    if (bb_hw_player_sprite_count != BB_HW_PLAYER_SPRITES) {
        bb_hw_player_fail("player sprite channel");
        return 0;
    }

    bb_hw_player_set_sprite_colors(
        viewport,
        bb_hw_player_sprites[0].channel);
    for (sprite_index = 0; sprite_index < BB_HW_PLAYER_SPRITES; sprite_index++) {
        frame_data = bb_hw_player_frame_data(sprite_index, 0);
        ChangeSprite(
            viewport,
            &bb_hw_player_sprites[sprite_index].sprite,
            frame_data);
        if (sprite_index == 0) {
            bb_hw_sprite_move(
                viewport,
                &bb_hw_player_sprites[sprite_index].sprite,
                -32,
                0,
                BB_PLAYER_H);
        } else {
            bb_hw_sprite_move_second_half(
                viewport,
                &bb_hw_player_sprites[sprite_index].sprite,
                -32,
                0,
                BB_PLAYER_H);
        }
    }

    bb_hw_player_ready = 1;
    bb_hw_player_failure_reason = "ready";
    return 1;
}

static int bb_hw_player_is_hardware(void)
{
    if (!bb_hw_player_ready || bb_hw_player_failed) {
        return 0;
    }

    return bb_rect_visible_world(
        bb_world_rect(bb_player.x, bb_player.y, BB_PLAYER_W, BB_PLAYER_H));
}

static void bb_hw_player_update(int move_sprites)
{
    struct ViewPort* viewport;
    UWORD* frame_data;
    int frame;
    int frame_changed;
    int i;
    int screen_x;
    int screen_y;

    bb_hw_player_update_calls++;
    if (!bb_hw_player_init()) {
        return;
    }

    if (!bb_rect_visible_world(
            bb_world_rect(bb_player.x, bb_player.y, BB_PLAYER_W, BB_PLAYER_H))) {
        if (move_sprites) {
            bb_hw_player_hide();
        } else {
            bb_hw_player_visible = 0;
        }
        return;
    }

    frame = bb_player_anim_frame();
    if (bb_player.facing < 0) {
        frame += BB_PLAYER_LEFT_FRAME_OFFSET;
    }
    if (frame < 0 || frame >= bb_hw_player_frame_count) {
        bb_hw_player_visible = 0;
        return;
    }

    bb_hw_player_visible = 1;
    if (!move_sprites) {
        return;
    }

    viewport = (struct ViewPort*)ana_gfx_native_viewport();
    if (viewport == 0) {
        return;
    }

    frame_changed = frame != bb_hw_player_current_frame;
    if (frame_changed) {
        for (i = 0; i < bb_hw_player_sprite_count; i++) {
            frame_data = bb_hw_player_frame_data(i, frame);
            if (frame_data != 0 && bb_hw_player_sprites[i].ready) {
                ChangeSprite(
                    viewport,
                    &bb_hw_player_sprites[i].sprite,
                    frame_data);
            }
        }
        bb_hw_player_current_frame = frame;
    }

    screen_x = bb_world_to_screen_x(bb_player.x);
    screen_y = bb_world_to_screen_y(bb_player.y);
    if (!frame_changed &&
            bb_hw_player_last_x == screen_x &&
            bb_hw_player_last_y == screen_y) {
        return;
    }

    for (i = 0; i < bb_hw_player_sprite_count; i++) {
        if (bb_hw_player_sprites[i].ready) {
            if (i == 0) {
                bb_hw_sprite_move(
                    viewport,
                    &bb_hw_player_sprites[i].sprite,
                    screen_x,
                    screen_y,
                    BB_PLAYER_H);
            } else {
                bb_hw_sprite_move_second_half(
                    viewport,
                    &bb_hw_player_sprites[i].sprite,
                    screen_x + (i * 16),
                    screen_y,
                    BB_PLAYER_H);
            }
        }
    }
    bb_hw_player_last_x = screen_x;
    bb_hw_player_last_y = screen_y;
    bb_hw_player_visible_moves++;
}

static int bb_hw_enemy_color_code(int color)
{
    switch (color) {
    case 7:
    case 15:
        return 1;
    case 1:
        return 2;
    case 13:
        return 3;
    default:
        return color > 0 ? 1 : 0;
    }
}

static void bb_hw_enemy_set_sprite_colors(struct ViewPort* viewport, int channel)
{
    int base;

    if (viewport == 0 || channel < 0) {
        return;
    }

    base = 17 + ((channel >> 1) * 4);
    SetRGB4(viewport, (LONG)base, 15, 2, 4);
    SetRGB4(viewport, (LONG)(base + 1), 15, 15, 15);
    SetRGB4(viewport, (LONG)(base + 2), 4, 4, 5);
}

static int bb_hw_enemy_build_data(UWORD* data)
{
    int x;
    int y;
    int source_x;
    int color;
    int code;
    UWORD bit;
    UWORD plane0;
    UWORD plane1;

    if (data == 0 || bb_enemy_image == 0) {
        return 0;
    }

    data[0] = 0;
    data[1] = 0;

    for (y = 0; y < BB_ENEMY_H; y++) {
        plane0 = 0u;
        plane1 = 0u;
        for (x = 0; x < 16; x++) {
            source_x = BB_HW_ENEMY_SOURCE_X + x;
            if (source_x >= BB_ENEMY_W ||
                    !ana_image_pixel_visible(
                        bb_enemy_image,
                        0,
                        source_x,
                        y)) {
                continue;
            }

            color = ana_image_pixel_index(bb_enemy_image, 0, source_x, y);
            code = bb_hw_enemy_color_code(color);
            bit = (UWORD)(0x8000u >> x);
            if ((code & 1) != 0) {
                plane0 = (UWORD)(plane0 | bit);
            }
            if ((code & 2) != 0) {
                plane1 = (UWORD)(plane1 | bit);
            }
        }

        data[2 + (y * 2)] = plane0;
        data[3 + (y * 2)] = plane1;
    }

    data[2 + (BB_ENEMY_H * 2)] = 0;
    data[3 + (BB_ENEMY_H * 2)] = 0;
    return 1;
}

static void bb_hw_enemy_hide_from(int start_slot)
{
    struct ViewPort* viewport;
    int slot;

    if (!bb_hw_enemy_ready) {
        return;
    }

    viewport = (struct ViewPort*)ana_gfx_native_viewport();
    if (viewport == 0) {
        return;
    }

    for (slot = start_slot; slot < bb_hw_enemy_slot_count; slot++) {
        if (!bb_hw_enemy_slot_visible[slot]) {
            continue;
        }

        if (bb_hw_enemy_sprites[slot].ready) {
            bb_hw_sprite_move(
                viewport,
                &bb_hw_enemy_sprites[slot].sprite,
                -32,
                0,
                BB_ENEMY_H);
        }
        bb_hw_enemy_slot_visible[slot] = 0;
        bb_hw_enemy_slot_last_x[slot] = -1000;
        bb_hw_enemy_slot_last_y[slot] = -1000;
    }
}

static void bb_hw_enemy_shutdown(void)
{
    struct ViewPort* viewport;
    int i;

    viewport = (struct ViewPort*)ana_gfx_native_viewport();
    if (viewport != 0) {
        bb_hw_enemy_hide_from(0);
    }

    for (i = 0; i < BB_HW_ENEMY_SPRITES; i++) {
        if (bb_hw_enemy_sprites[i].ready) {
            FreeSprite((LONG)bb_hw_enemy_sprites[i].channel);
        }
        if (bb_hw_enemy_sprites[i].data != 0 &&
                bb_hw_enemy_sprites[i].allocated) {
            FreeMem(
                bb_hw_enemy_sprites[i].data,
                (ULONG)BB_HW_ENEMY_SPRITE_BYTES);
        }
        bb_hw_enemy_sprites[i].data = 0;
        bb_hw_enemy_sprites[i].channel = -1;
        bb_hw_enemy_sprites[i].ready = 0;
        bb_hw_enemy_sprites[i].allocated = 0;
    }

    for (i = 0; i < BB_HW_ENEMY_MAX; i++) {
        bb_hw_enemy_slot_visible[i] = 0;
        bb_hw_enemy_slot_last_x[i] = -1000;
        bb_hw_enemy_slot_last_y[i] = -1000;
    }
    bb_hw_enemy_sprite_count = 0;
    bb_hw_enemy_slot_count = 0;
    bb_hw_enemy_ready = 0;
    bb_hw_enemy_failed = 0;
    bb_hw_enemy_failure_reported = 0;
}

static void bb_hw_enemy_fail(const char* reason)
{
    bb_hw_enemy_shutdown();
    bb_hw_enemy_failed = 1;
    bb_hw_enemy_failure_reason = reason;
    if (!bb_hw_enemy_failure_reported) {
        printf(
            "Byte Brothers hardware enemy sprites unavailable: %s\n",
            reason);
        bb_hw_enemy_failure_reported = 1;
    }
}

static int bb_hw_enemy_init(void)
{
    struct ViewPort* viewport;
    int sprite_index;
    int desired_channel;
    int channel;

    if (bb_hw_enemy_ready) {
        return 1;
    }

    if (bb_hw_enemy_failed) {
        return 0;
    }

    bb_hw_enemy_failure_reason = "initializing";
    viewport = (struct ViewPort*)ana_gfx_native_viewport();
    if (viewport == 0) {
        bb_hw_enemy_fail("no viewport");
        return 0;
    }
    if (bb_enemy_image == 0) {
        bb_hw_enemy_fail("no enemy image");
        return 0;
    }

    for (desired_channel = BB_HW_ENEMY_FIRST_CHANNEL;
            desired_channel < 8 &&
                bb_hw_enemy_sprite_count < BB_HW_ENEMY_SPRITES;
            desired_channel++) {
        sprite_index = bb_hw_enemy_sprite_count;
        bb_hw_enemy_sprites[sprite_index].data =
            (UWORD*)AllocMem(
                (ULONG)BB_HW_ENEMY_SPRITE_BYTES,
                MEMF_CHIP | MEMF_CLEAR);
        if (bb_hw_enemy_sprites[sprite_index].data == 0) {
            if ((TypeOfMem(bb_hw_enemy_static_data[sprite_index]) &
                    MEMF_CHIP) == 0u) {
                bb_hw_enemy_fail("sprite chip allocation");
                return 0;
            }
            bb_hw_enemy_sprites[sprite_index].data =
                bb_hw_enemy_static_data[sprite_index];
            memset(
                bb_hw_enemy_sprites[sprite_index].data,
                0,
                (size_t)BB_HW_ENEMY_SPRITE_BYTES);
            bb_hw_enemy_sprites[sprite_index].allocated = 0;
        } else {
            bb_hw_enemy_sprites[sprite_index].allocated = 1;
        }
        if (!bb_hw_enemy_build_data(bb_hw_enemy_sprites[sprite_index].data)) {
            bb_hw_enemy_fail("sprite data build");
            return 0;
        }

        bb_hw_enemy_sprites[sprite_index].sprite.posctldata =
            bb_hw_enemy_sprites[sprite_index].data;
        bb_hw_enemy_sprites[sprite_index].sprite.height = BB_ENEMY_H;
        bb_hw_enemy_sprites[sprite_index].sprite.x = 0;
        bb_hw_enemy_sprites[sprite_index].sprite.y = 0;
        bb_hw_enemy_sprites[sprite_index].sprite.num = 0;
        channel = GetSprite(
            &bb_hw_enemy_sprites[sprite_index].sprite,
            (LONG)desired_channel);
        if (channel < 0) {
            if (bb_hw_enemy_sprites[sprite_index].allocated) {
                FreeMem(
                    bb_hw_enemy_sprites[sprite_index].data,
                    (ULONG)BB_HW_ENEMY_SPRITE_BYTES);
            }
            bb_hw_enemy_sprites[sprite_index].data = 0;
            bb_hw_enemy_sprites[sprite_index].allocated = 0;
            continue;
        }

        bb_hw_enemy_sprites[sprite_index].channel = channel;
        bb_hw_enemy_sprites[sprite_index].ready = 1;
        bb_hw_enemy_sprite_count++;
        bb_hw_enemy_set_sprite_colors(viewport, channel);
        ChangeSprite(
            viewport,
            &bb_hw_enemy_sprites[sprite_index].sprite,
            bb_hw_enemy_sprites[sprite_index].data);
        bb_hw_sprite_move(
            viewport,
            &bb_hw_enemy_sprites[sprite_index].sprite,
            -32,
            0,
            BB_ENEMY_H);
    }

    bb_hw_enemy_slot_count = bb_hw_enemy_sprite_count;
    if (bb_hw_enemy_slot_count <= 0) {
        bb_hw_enemy_fail("no sprite channels");
        return 0;
    }

    bb_hw_enemy_ready = 1;
    bb_hw_enemy_failure_reason = "ready";
    printf(
        "Byte Brothers hardware enemy sprites: channels=%d, enemies=%d\n",
        bb_hw_enemy_sprite_count,
        bb_hw_enemy_slot_count);
    return 1;
}

static int bb_hw_enemy_slot_for_index(int index)
{
    int i;
    int slot;

    if (index < 0 || index >= BB_MAX_ENEMIES) {
        return -1;
    }
    if (!bb_hw_enemy_ready || bb_hw_enemy_failed ||
            !bb_enemy_visible(&bb_enemies[index])) {
        return -1;
    }

    slot = 0;
    for (i = 0; i < bb_enemy_count && i <= index; i++) {
        if (!bb_enemy_visible(&bb_enemies[i])) {
            continue;
        }
        if (i == index) {
            return slot < bb_hw_enemy_slot_count ? slot : -1;
        }
        slot++;
    }

    return -1;
}

static int bb_hw_enemy_is_hardware(int index)
{
    return bb_hw_enemy_slot_for_index(index) >= 0;
}

static void bb_hw_enemy_update(int move_sprites)
{
    struct ViewPort* viewport;
    int i;
    int slot;
    int screen_x;
    int screen_y;
    int sprite_x;

    bb_hw_enemy_update_calls++;
    if (!bb_hw_enemy_init()) {
        return;
    }

    viewport = move_sprites ? (struct ViewPort*)ana_gfx_native_viewport() : 0;
    if (move_sprites && viewport == 0) {
        return;
    }

    slot = 0;
    for (i = 0; i < bb_enemy_count && slot < bb_hw_enemy_slot_count; i++) {
        if (!bb_enemy_visible(&bb_enemies[i])) {
            continue;
        }

        screen_x = bb_world_to_screen_x(bb_enemies[i].x);
        screen_y = bb_world_to_screen_y(bb_enemies[i].y);
        sprite_x = screen_x + BB_HW_ENEMY_SOURCE_X;
        if (move_sprites &&
                (!bb_hw_enemy_slot_visible[slot] ||
                    bb_hw_enemy_slot_last_x[slot] != sprite_x ||
                    bb_hw_enemy_slot_last_y[slot] != screen_y)) {
            bb_hw_sprite_move(
                viewport,
                &bb_hw_enemy_sprites[slot].sprite,
                sprite_x,
                screen_y,
                BB_ENEMY_H);
            bb_hw_enemy_slot_visible[slot] = 1;
            bb_hw_enemy_slot_last_x[slot] = sprite_x;
            bb_hw_enemy_slot_last_y[slot] = screen_y;
            bb_hw_enemy_visible_moves++;
        }
        slot++;
    }

    if (move_sprites) {
        bb_hw_enemy_hide_from(slot);
    }
}
#else
static int bb_hw_player_is_hardware(void)
{
    return 0;
}

static void bb_hw_player_update(int move_sprites)
{
    (void)move_sprites;
}

static void bb_hw_player_shutdown(void)
{
}

static int bb_hw_enemy_is_hardware(int index)
{
    (void)index;
    return 0;
}

static void bb_hw_enemy_update(int move_sprites)
{
    (void)move_sprites;
}

static void bb_hw_enemy_shutdown(void)
{
}
#endif

#ifdef ANA_TARGET_AMIGA
static int bb_visible_enemy_count(void)
{
    int count;
    int i;

    count = 0;
    for (i = 0; i < bb_enemy_count; i++) {
        if (bb_enemy_visible(&bb_enemies[i])) {
            count++;
        }
    }

    return count;
}

static int bb_previous_visible_enemy_count(int slot)
{
    int count;
    int i;

    if (slot < 0 || slot >= BB_RENDER_SLOTS ||
            !bb_prev_actor_valid[slot]) {
        return 0;
    }

    count = 0;
    for (i = 0; i < bb_prev_enemy_count[slot]; i++) {
        if (bb_prev_enemy_visible[slot][i]) {
            count++;
        }
    }

    return count;
}
#endif

static int bb_can_skip_actor_redraw(int slot)
{
#ifdef ANA_TARGET_AMIGA
    if (slot < 0 || slot >= BB_RENDER_SLOTS ||
            bb_tile_dirty_slot_pending(slot) ||
            !bb_hw_player_is_hardware() ||
            !bb_hw_enemy_ready ||
            bb_hw_enemy_failed ||
            bb_hw_enemy_slot_count <= 0) {
        return 0;
    }

    return bb_visible_enemy_count() <= bb_hw_enemy_slot_count &&
        bb_previous_visible_enemy_count(slot) <= bb_hw_enemy_slot_count;
#else
    (void)slot;
    return 0;
#endif
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
    const ANA_RectList* redraw_rects)
{
    ANA_Rect redraw_rect;
    int i;

    for (i = 0; i < ana_rect_list_count(redraw_rects); i++) {
        redraw_rect = ana_rect_list_rect(redraw_rects, i);
        if (ana_rect_intersects(redraw_rect, rect)) {
            return 1;
        }
    }

    return 0;
}

static void bb_draw_actors_in_world_rects(
    const ANA_RectList* redraw_rects)
{
    int i;
    ANA_Rect rect;

    for (i = 0; i < bb_enemy_count; i++) {
        if (bb_hw_enemy_is_hardware(i)) {
            continue;
        }
        rect = bb_enemy_rect(&bb_enemies[i]);
        if (bb_rect_intersects_any(rect, redraw_rects) &&
                bb_enemy_visible(&bb_enemies[i])) {
            bb_draw_enemy(&bb_enemies[i]);
        }
    }

    rect = bb_player_redraw_rect_at(
        bb_player.x,
        bb_player.y,
        bb_player.dash_ticks,
        bb_player.facing);
    if (!bb_hw_player_is_hardware() &&
            bb_rect_intersects_any(rect, redraw_rects) &&
            bb_rect_visible_world(rect)) {
        bb_draw_player();
    }
}

static void bb_redraw_previous_and_current_actors(
    int slot,
    int all_slots,
    int force_bitmap_actor_redraw)
{
    int i;
    int player_changed;
    int player_anim_frame;
    int enemy_changed;
    int current_enemy_hardware;
    int previous_enemy_hardware;
    int enemy_visible;
    int previous_enemy_visible;
    ANA_Rect rect;
    ANA_Rect tile_rect_storage[BB_TILE_DIRTY_RECTS];
    ANA_Rect restore_rect_storage[2 + (BB_MAX_ENEMIES * 2)];
    ANA_Rect redraw_rect_storage[BB_TILE_DIRTY_RECTS + 2 + (BB_MAX_ENEMIES * 2)];
    ANA_RectList tile_rects;
    ANA_RectList restore_rects;
    ANA_RectList redraw_rects;

    bb_debug_actor_redraw_passes++;
    ana_rect_list_init(
        &tile_rects,
        tile_rect_storage,
        (int)(sizeof(tile_rect_storage) / sizeof(tile_rect_storage[0])));
    ana_rect_list_init(
        &restore_rects,
        restore_rect_storage,
        (int)(sizeof(restore_rect_storage) / sizeof(restore_rect_storage[0])));
    ana_rect_list_init(
        &redraw_rects,
        redraw_rect_storage,
        (int)(sizeof(redraw_rect_storage) / sizeof(redraw_rect_storage[0])));
    ana_rect_list_set_bounds(&tile_rects, bb_world_bounds());
    ana_rect_list_set_bounds(&restore_rects, bb_world_bounds());
    ana_rect_list_set_bounds(&redraw_rects, bb_world_bounds());
    ana_rect_list_set_merge_slack(&tile_rects, BB_REDRAW_MERGE_SLACK);
    ana_rect_list_set_merge_slack(&restore_rects, BB_REDRAW_MERGE_SLACK);
    ana_rect_list_set_merge_slack(&redraw_rects, BB_REDRAW_MERGE_SLACK);
    player_anim_frame = bb_player_anim_frame();
    bb_add_tile_dirty_redraw_rects(
        &tile_rects,
        &redraw_rects,
        slot,
        all_slots);

    player_changed = !bb_prev_actor_valid[slot] ||
        bb_prev_player_x[slot] != bb_player.x ||
        bb_prev_player_y[slot] != bb_player.y ||
        bb_prev_player_dash_ticks[slot] > 0 ||
        bb_player.dash_ticks > 0 ||
        bb_prev_player_facing[slot] != bb_player.facing ||
        bb_prev_player_anim_frame[slot] != player_anim_frame;
    if (bb_hw_player_is_hardware()) {
        player_changed = 0;
    } else if (force_bitmap_actor_redraw) {
        player_changed = 1;
    }

    if (player_changed && bb_prev_actor_valid[slot]) {
        rect = bb_player_redraw_rect_at(
            bb_prev_player_x[slot],
            bb_prev_player_y[slot],
            bb_prev_player_dash_ticks[slot],
            bb_prev_player_facing[slot]);
        ana_rect_list_add_padded(
            &restore_rects,
            rect,
            BB_ACTOR_REDRAW_PADDING);
        ana_rect_list_add_padded(
            &redraw_rects,
            rect,
            BB_ACTOR_REDRAW_PADDING);
    }
    if (player_changed) {
        rect = bb_player_redraw_rect_at(
            bb_player.x,
            bb_player.y,
            bb_player.dash_ticks,
            bb_player.facing);
        ana_rect_list_add_padded(
            &restore_rects,
            rect,
            BB_ACTOR_REDRAW_PADDING);
        ana_rect_list_add_padded(
            &redraw_rects,
            rect,
            BB_ACTOR_REDRAW_PADDING);
    }

    for (i = 0; i < bb_enemy_count; i++) {
        current_enemy_hardware = bb_hw_enemy_is_hardware(i);
        previous_enemy_hardware =
            bb_prev_actor_valid[slot] && i < bb_prev_enemy_count[slot] ?
                bb_prev_enemy_hardware[slot][i] :
                0;

        if (current_enemy_hardware && previous_enemy_hardware) {
            continue;
        }

        enemy_visible = bb_enemy_visible(&bb_enemies[i]);
        previous_enemy_visible =
            bb_prev_actor_valid[slot] && i < bb_prev_enemy_count[slot] ?
                bb_prev_enemy_visible[slot][i] :
                0;
        enemy_changed = !bb_prev_actor_valid[slot] ||
            bb_prev_enemy_x[slot][i] != bb_enemies[i].x ||
            bb_prev_enemy_y[slot][i] != bb_enemies[i].y ||
            bb_prev_enemy_alive[slot][i] != bb_enemies[i].alive ||
            previous_enemy_visible != enemy_visible;
        if (!enemy_changed &&
                force_bitmap_actor_redraw &&
                ((enemy_visible && !current_enemy_hardware) ||
                    (previous_enemy_visible && !previous_enemy_hardware))) {
            enemy_changed = 1;
        }

        if (enemy_changed &&
                bb_prev_actor_valid[slot] &&
                previous_enemy_visible &&
                !previous_enemy_hardware) {
            rect = bb_world_rect(
                bb_prev_enemy_x[slot][i],
                bb_prev_enemy_y[slot][i],
                BB_ENEMY_W,
                BB_ENEMY_H);
            ana_rect_list_add_padded(
                &restore_rects,
                rect,
                BB_ACTOR_REDRAW_PADDING);
            ana_rect_list_add_padded(
                &redraw_rects,
                rect,
                BB_ACTOR_REDRAW_PADDING);
        }
        if (enemy_changed && enemy_visible && !current_enemy_hardware) {
            rect = bb_enemy_rect(&bb_enemies[i]);
            ana_rect_list_add_padded(
                &restore_rects,
                rect,
                BB_ACTOR_REDRAW_PADDING);
            ana_rect_list_add_padded(
                &redraw_rects,
                rect,
                BB_ACTOR_REDRAW_PADDING);
        }
    }

    if (all_slots) {
        for (i = 0; i < ana_rect_list_count(&redraw_rects); i++) {
            bb_debug_tile_redraw_world_rects++;
            ana_tile_layer_redraw_world_rect(
                &bb_playfield_layer,
                ana_rect_list_rect(&redraw_rects, i));
        }
    } else {
        for (i = 0; i < ana_rect_list_count(&tile_rects); i++) {
            bb_debug_tile_redraw_world_rects++;
            ana_tile_layer_redraw_world_rect(
                &bb_playfield_layer,
                ana_rect_list_rect(&tile_rects, i));
        }
        for (i = 0; i < ana_rect_list_count(&restore_rects); i++) {
            bb_debug_tile_restore_world_rects++;
            ana_tile_layer_restore_world_rect(
                &bb_playfield_layer,
                ana_rect_list_rect(&restore_rects, i));
        }
    }

    bb_draw_actors_in_world_rects(&redraw_rects);
}

static void bb_draw_player(void)
{
    ANA_Rect rect;
    int anim_frame;
    int sx;
    int sy;

    rect = ana_camera_world_to_screen_rect(
        &bb_camera,
        bb_world_rect(bb_player.x, bb_player.y, BB_PLAYER_W, BB_PLAYER_H));
    sx = rect.x;
    sy = rect.y;
    anim_frame = bb_player_anim_frame();
    bb_debug_player_draws++;

    if (bb_player.facing < 0) {
        anim_frame += BB_PLAYER_LEFT_FRAME_OFFSET;
    }
    ana_draw_image_frame(bb_player_image, anim_frame, sx, sy);

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

    bb_debug_bitmap_enemy_draws++;
    rect = ana_camera_world_to_screen_rect(
        &bb_camera,
        bb_enemy_rect(enemy));
    sx = rect.x;
    sy = rect.y;

    ana_draw_image(bb_enemy_image, sx, sy);
}

static void bb_draw_actors(void)
{
    int i;
    ANA_Rect rect;

    for (i = 0; i < bb_enemy_count; i++) {
        if (bb_hw_enemy_is_hardware(i)) {
            continue;
        }
        rect = bb_enemy_rect(&bb_enemies[i]);
        if (bb_enemy_visible(&bb_enemies[i])) {
            bb_draw_enemy(&bb_enemies[i]);
        }
    }

    rect = bb_world_rect(bb_player.x, bb_player.y, BB_PLAYER_W, BB_PLAYER_H);
    if (!bb_hw_player_is_hardware() && bb_rect_visible_world(rect)) {
        bb_draw_player();
    }
}

static void bb_commit_state(int slot)
{
    int i;

    bb_prev_camera_x = bb_camera.x;
    bb_prev_camera_y = bb_camera.y;
    bb_prev_player_x[slot] = bb_player.x;
    bb_prev_player_y[slot] = bb_player.y;
    bb_prev_player_dash_ticks[slot] = bb_player.dash_ticks;
    bb_prev_player_facing[slot] = bb_player.facing;
    bb_prev_player_anim_frame[slot] = bb_player_anim_frame();
    bb_prev_score = bb_score;
    bb_prev_lives = bb_lives;
    bb_prev_level = bb_level_index;
    bb_prev_fragments = bb_fragments_left;

    for (i = 0; i < bb_enemy_count; i++) {
        bb_prev_enemy_x[slot][i] = bb_enemies[i].x;
        bb_prev_enemy_y[slot][i] = bb_enemies[i].y;
        bb_prev_enemy_alive[slot][i] = bb_enemies[i].alive;
        bb_prev_enemy_visible[slot][i] = bb_enemy_visible(&bb_enemies[i]);
        bb_prev_enemy_hardware[slot][i] = bb_hw_enemy_is_hardware(i);
    }
    bb_prev_enemy_count[slot] = bb_enemy_count;
    bb_prev_actor_valid[slot] = 1;
}

static void bb_tile_layer_draw_profiled(void)
{
#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
    unsigned long start_ticks;

    start_ticks = ana_platform_perf_ticks();
#endif
    ana_tile_layer_draw(&bb_playfield_layer);
#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
    bb_record_perf_ticks(&bb_debug_tile_perf_ticks, start_ticks);
#endif
}

static void bb_hud_draw_profiled(void)
{
#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
    unsigned long start_ticks;

    start_ticks = ana_platform_perf_ticks();
#endif
    ana_layer_draw_if_dirty(&bb_hud_layer);
#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
    bb_record_perf_ticks(&bb_debug_hud_perf_ticks, start_ticks);
#endif
}

static void bb_hw_enemy_update_profiled(int move_sprites)
{
#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
    unsigned long start_ticks;

    start_ticks = ana_platform_perf_ticks();
#endif
    bb_hw_enemy_update(move_sprites);
#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
    bb_record_perf_ticks(&bb_debug_hw_enemy_perf_ticks, start_ticks);
#endif
}

static void bb_hw_player_update_profiled(int move_sprites)
{
#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
    unsigned long start_ticks;

    start_ticks = ana_platform_perf_ticks();
#endif
    bb_hw_player_update(move_sprites);
#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
    bb_record_perf_ticks(&bb_debug_hw_player_perf_ticks, start_ticks);
#endif
}

static void bb_draw_actors_profiled(void)
{
#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
    unsigned long start_ticks;

    start_ticks = ana_platform_perf_ticks();
#endif
    bb_draw_actors();
#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
    bb_record_perf_ticks(&bb_debug_actor_perf_ticks, start_ticks);
#endif
}

static void bb_redraw_actors_profiled(
    int slot,
    int all_slots,
    int force_bitmap_actor_redraw)
{
#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
    unsigned long start_ticks;

    start_ticks = ana_platform_perf_ticks();
#endif
    bb_redraw_previous_and_current_actors(
        slot,
        all_slots,
        force_bitmap_actor_redraw);
#if defined(ANA_DEBUG_STATS) && BB_RENDER_FINE_PROFILING
    bb_record_perf_ticks(&bb_debug_actor_perf_ticks, start_ticks);
#endif
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
        bb_prev_enemy_count[i] = 0;
    }
    bb_debug_bitmap_enemy_draws = 0L;
    bb_debug_full_redraws = 0L;
    bb_debug_tile_layer_draws = 0L;
    bb_debug_tile_hardware_unavailable_draws = 0L;
    bb_debug_tile_hardware_draws = 0L;
    bb_debug_tile_software_draws = 0L;
    bb_debug_tile_redraw_world_rects = 0L;
    bb_debug_tile_restore_world_rects = 0L;
    bb_debug_actor_redraw_passes = 0L;
    bb_debug_player_draws = 0L;
    bb_debug_tile_perf_ticks = 0L;
    bb_debug_hud_perf_ticks = 0L;
    bb_debug_actor_perf_ticks = 0L;
    bb_debug_hw_player_perf_ticks = 0L;
    bb_debug_hw_enemy_perf_ticks = 0L;
    bb_clear_tile_dirty_rects();
#ifdef ANA_TARGET_AMIGA
    bb_hw_player_hide();
    bb_hw_enemy_hide_from(0);
    bb_hw_enemy_update(0);
    bb_hw_player_update(0);
#endif
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
                ana_rect_should_merge(
                    bb_tile_dirty_rects[i],
                    rect,
                    BB_REDRAW_MERGE_SLACK)) {
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
    int hardware_scroll_active;
    int camera_changed;
    int scroll_bitmap_redrawn;
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
        bb_debug_full_redraws++;
        bb_debug_tile_layer_draws++;
        if (!hardware_scroll_available) {
            bb_debug_tile_hardware_unavailable_draws++;
        }
        bb_tile_layer_draw_profiled();
        if (ana_tile_layer_hardware_scroll_active(&bb_playfield_layer)) {
            bb_debug_tile_hardware_draws++;
        } else {
            bb_debug_tile_software_draws++;
        }
        hardware_scroll_active =
            ana_tile_layer_hardware_scroll_active(&bb_playfield_layer);
        slot = bb_render_slot();
        ana_layer_mark_dirty(&bb_hud_layer);
        bb_hud_draw_profiled();
        bb_draw_actors_profiled();
#if BB_INPUT_DEBUG_OVERLAY
        bb_draw_input_debug_overlay();
#endif
        bb_full_redraw = 0;
        bb_clear_tile_dirty_rects();
        bb_commit_state(slot);
#ifdef ANA_TARGET_AMIGA
        bb_hw_wait_for_sprite_update();
#endif
        bb_hw_enemy_update_profiled(1);
        bb_hw_player_update_profiled(1);
        return;
    }

    scroll_bitmap_redrawn = 0;
    camera_changed = bb_camera.x != bb_prev_camera_x ||
        bb_camera.y != bb_prev_camera_y;
    if (camera_changed) {
        if (!hardware_scroll_available ||
                !ana_tile_layer_hardware_scroll_update_view(
                    &bb_playfield_layer)) {
            scroll_bitmap_redrawn = 1;
            bb_debug_tile_layer_draws++;
            if (!hardware_scroll_available) {
                bb_debug_tile_hardware_unavailable_draws++;
            }
            bb_tile_layer_draw_profiled();
            if (ana_tile_layer_hardware_scroll_active(&bb_playfield_layer)) {
                bb_debug_tile_hardware_draws++;
            } else {
                bb_debug_tile_software_draws++;
            }
        }
    }
    hardware_scroll_active =
        ana_tile_layer_hardware_scroll_active(&bb_playfield_layer);
    slot = bb_render_slot();
    if (!bb_can_skip_actor_redraw(slot)) {
        bb_redraw_actors_profiled(
            slot,
            !hardware_scroll_active,
            camera_changed || scroll_bitmap_redrawn);
    }

    if (hud_dirty) {
        ana_layer_mark_dirty(&bb_hud_layer);
        bb_hud_draw_profiled();
    }
#if BB_INPUT_DEBUG_OVERLAY
    bb_draw_input_debug_overlay();
#endif
#ifdef ANA_TARGET_AMIGA
    bb_hw_wait_for_sprite_update();
#endif
    bb_hw_enemy_update_profiled(1);
    bb_hw_player_update_profiled(1);

    bb_commit_state(slot);
}

BB_RenderStats bb_render_stats(void)
{
    BB_RenderStats stats;

    stats.bitmap_enemy_draws = bb_debug_bitmap_enemy_draws;
    stats.full_redraws = bb_debug_full_redraws;
    stats.tile_layer_draws = bb_debug_tile_layer_draws;
    stats.tile_hardware_unavailable_draws =
        bb_debug_tile_hardware_unavailable_draws;
    stats.tile_hardware_draws = bb_debug_tile_hardware_draws;
    stats.tile_software_draws = bb_debug_tile_software_draws;
    stats.tile_redraw_world_rects = bb_debug_tile_redraw_world_rects;
    stats.tile_restore_world_rects = bb_debug_tile_restore_world_rects;
    stats.actor_redraw_passes = bb_debug_actor_redraw_passes;
    stats.player_draws = bb_debug_player_draws;
    stats.tile_perf_ticks = bb_debug_tile_perf_ticks;
    stats.hud_perf_ticks = bb_debug_hud_perf_ticks;
    stats.actor_perf_ticks = bb_debug_actor_perf_ticks;
    stats.hw_player_perf_ticks = bb_debug_hw_player_perf_ticks;
    stats.hw_enemy_perf_ticks = bb_debug_hw_enemy_perf_ticks;
#ifdef ANA_TARGET_AMIGA
    stats.hw_player_update_calls = bb_hw_player_update_calls;
    stats.hw_player_visible_moves = bb_hw_player_visible_moves;
    stats.hw_player_ready = bb_hw_player_ready;
    stats.hw_player_failed = bb_hw_player_failed;
    stats.hw_player_sprite_count = bb_hw_player_sprite_count;
    stats.hw_player_status = bb_hw_player_failure_reason;
    stats.hw_enemy_update_calls = bb_hw_enemy_update_calls;
    stats.hw_enemy_visible_moves = bb_hw_enemy_visible_moves;
    stats.hw_enemy_ready = bb_hw_enemy_ready;
    stats.hw_enemy_failed = bb_hw_enemy_failed;
    stats.hw_enemy_sprite_count = bb_hw_enemy_sprite_count;
    stats.hw_enemy_slot_count = bb_hw_enemy_slot_count;
    stats.hw_enemy_status = bb_hw_enemy_failure_reason;
#else
    stats.hw_player_update_calls = 0L;
    stats.hw_player_visible_moves = 0L;
    stats.hw_player_ready = 0;
    stats.hw_player_failed = 0;
    stats.hw_player_sprite_count = 0;
    stats.hw_player_status = "host";
    stats.hw_enemy_update_calls = 0L;
    stats.hw_enemy_visible_moves = 0L;
    stats.hw_enemy_ready = 0;
    stats.hw_enemy_failed = 0;
    stats.hw_enemy_sprite_count = 0;
    stats.hw_enemy_slot_count = 0;
    stats.hw_enemy_status = "host";
#endif

    return stats;
}

void bb_render_shutdown(void)
{
#ifdef ANA_TARGET_AMIGA
    BB_RenderStats stats;

    stats = bb_render_stats();
    printf(
        "Byte Brothers sprite backend: hw_calls=%ld, ready=%d, failed=%d, "
        "channels=%d, hw_slots=%d, hw_enemy_moves=%ld, "
        "bitmap_enemy_draws=%ld, status=%s\n",
        stats.hw_enemy_update_calls,
        stats.hw_enemy_ready,
        stats.hw_enemy_failed,
        stats.hw_enemy_sprite_count,
        stats.hw_enemy_slot_count,
        stats.hw_enemy_visible_moves,
        stats.bitmap_enemy_draws,
        stats.hw_enemy_status);
#endif
    bb_hw_player_shutdown();
    bb_hw_enemy_shutdown();
}

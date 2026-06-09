#include "byte_brothers_render.h"

#include "byte_brothers_assets.h"
#include "byte_brothers_internal.h"

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
#ifdef ANA_DEBUG_STATS
#define BB_INPUT_DEBUG_OVERLAY 1
#else
#define BB_INPUT_DEBUG_OVERLAY 0
#endif
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
static long bb_debug_tile_redraw_world_rects = 0L;
static long bb_debug_tile_restore_world_rects = 0L;
static long bb_debug_actor_redraw_passes = 0L;
static long bb_debug_player_draws = 0L;

static void bb_draw_player(void);
static void bb_draw_enemy(const BB_Enemy* enemy);
#if BB_INPUT_DEBUG_OVERLAY
static void bb_draw_input_debug_overlay(void);
#endif

#ifdef ANA_TARGET_AMIGA
#define BB_HW_ENEMY_FIRST_CHANNEL 2
#define BB_HW_ENEMY_MAX 3
#define BB_HW_ENEMY_SPRITES (BB_HW_ENEMY_MAX * 2)
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

static BB_HardwareEnemySprite bb_hw_enemy_sprites[BB_HW_ENEMY_SPRITES];
static UWORD bb_hw_enemy_static_data
    [BB_HW_ENEMY_SPRITES][BB_HW_ENEMY_SPRITE_WORDS];
static int bb_hw_enemy_ready = 0;
static int bb_hw_enemy_failed = 0;
static int bb_hw_enemy_failure_reported = 0;
static int bb_hw_enemy_sprite_count = 0;
static int bb_hw_enemy_slot_count = 0;
static int bb_hw_enemy_visible[BB_MAX_ENEMIES];
static long bb_hw_enemy_update_calls = 0L;
static long bb_hw_enemy_visible_moves = 0L;
static const char* bb_hw_enemy_failure_reason = "not attempted";
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
        bb_rect_visible_world(bb_enemy_rect(enemy));
}

#ifdef ANA_TARGET_AMIGA
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

static int bb_hw_enemy_build_data(UWORD* data, int half)
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
            source_x = (half * 16) + x;
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

static void bb_hw_enemy_hide_from(int start)
{
    struct ViewPort* viewport;
    int i;

    if (!bb_hw_enemy_ready) {
        return;
    }

    viewport = (struct ViewPort*)ana_gfx_native_viewport();
    if (viewport == 0) {
        return;
    }

    for (i = start; i < bb_hw_enemy_sprite_count; i++) {
        if (bb_hw_enemy_sprites[i].ready) {
            MoveSprite(
                viewport,
                &bb_hw_enemy_sprites[i].sprite,
                -32,
                0);
        }
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

    for (i = 0; i < BB_MAX_ENEMIES; i++) {
        bb_hw_enemy_visible[i] = 0;
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
        if (!bb_hw_enemy_build_data(
                bb_hw_enemy_sprites[sprite_index].data,
                sprite_index & 1)) {
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
        MoveSprite(
            viewport,
            &bb_hw_enemy_sprites[sprite_index].sprite,
            -32,
            0);
    }

    bb_hw_enemy_slot_count = bb_hw_enemy_sprite_count / 2;
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

static int bb_hw_enemy_is_hardware(int index)
{
    if (index < 0 || index >= BB_MAX_ENEMIES) {
        return 0;
    }

    return bb_hw_enemy_ready && bb_hw_enemy_visible[index];
}

static void bb_hw_enemy_update(int move_sprites)
{
    struct ViewPort* viewport;
    ANA_Rect rect;
    int i;
    int slot;

    bb_hw_enemy_update_calls++;
    if (!bb_hw_enemy_init()) {
        return;
    }

    viewport = move_sprites ? (struct ViewPort*)ana_gfx_native_viewport() : 0;
    if (move_sprites && viewport == 0) {
        return;
    }

    for (i = 0; i < BB_MAX_ENEMIES; i++) {
        bb_hw_enemy_visible[i] = 0;
    }

    slot = 0;
    for (i = 0; i < bb_enemy_count && slot < bb_hw_enemy_slot_count; i++) {
        if (!bb_enemy_visible(&bb_enemies[i])) {
            continue;
        }

        rect = ana_camera_world_to_screen_rect(
            &bb_camera,
            bb_enemy_rect(&bb_enemies[i]));
        bb_hw_enemy_visible[i] = 1;
        if (move_sprites) {
            MoveSprite(
                viewport,
                &bb_hw_enemy_sprites[slot * 2].sprite,
                rect.x,
                rect.y);
            MoveSprite(
                viewport,
                &bb_hw_enemy_sprites[(slot * 2) + 1].sprite,
                rect.x + 16,
                rect.y);
        }
        if (move_sprites) {
            bb_hw_enemy_visible_moves++;
        }
        slot++;
    }

    if (move_sprites) {
        bb_hw_enemy_hide_from(slot * 2);
    }
}
#else
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
    if (bb_rect_intersects_any(rect, redraw_rects) &&
            bb_rect_visible_world(rect)) {
        bb_draw_player();
    }
}

static void bb_redraw_previous_and_current_actors(int slot, int all_slots)
{
    int i;
    int player_changed;
    int player_anim_frame;
    int enemy_changed;
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
        if (bb_hw_enemy_is_hardware(i)) {
            continue;
        }
        enemy_visible = bb_enemy_visible(&bb_enemies[i]);
        previous_enemy_visible = bb_prev_actor_valid[slot] ?
            bb_prev_enemy_visible[slot][i] :
            0;
        enemy_changed = !bb_prev_actor_valid[slot] ||
            bb_prev_enemy_x[slot][i] != bb_enemies[i].x ||
            bb_prev_enemy_y[slot][i] != bb_enemies[i].y ||
            bb_prev_enemy_alive[slot][i] != bb_enemies[i].alive ||
            previous_enemy_visible != enemy_visible;

        if (enemy_changed &&
                bb_prev_actor_valid[slot] &&
                previous_enemy_visible) {
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
        if (enemy_changed && enemy_visible) {
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
    if (bb_rect_visible_world(rect)) {
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

    for (i = 0; i < BB_MAX_ENEMIES; i++) {
        bb_prev_enemy_x[slot][i] = bb_enemies[i].x;
        bb_prev_enemy_y[slot][i] = bb_enemies[i].y;
        bb_prev_enemy_alive[slot][i] = bb_enemies[i].alive;
        bb_prev_enemy_visible[slot][i] = bb_enemy_visible(&bb_enemies[i]);
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
    bb_debug_bitmap_enemy_draws = 0L;
    bb_debug_full_redraws = 0L;
    bb_debug_tile_layer_draws = 0L;
    bb_debug_tile_redraw_world_rects = 0L;
    bb_debug_tile_restore_world_rects = 0L;
    bb_debug_actor_redraw_passes = 0L;
    bb_debug_player_draws = 0L;
    bb_clear_tile_dirty_rects();
#ifdef ANA_TARGET_AMIGA
    bb_hw_enemy_hide_from(0);
    bb_hw_enemy_update(0);
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
        ana_tile_layer_draw(&bb_playfield_layer);
        slot = bb_render_slot();
        ana_layer_mark_dirty(&bb_hud_layer);
        ana_layer_draw_if_dirty(&bb_hud_layer);
        bb_hw_enemy_update(0);
        bb_draw_actors();
#if BB_INPUT_DEBUG_OVERLAY
        bb_draw_input_debug_overlay();
#endif
        bb_full_redraw = 0;
        bb_clear_tile_dirty_rects();
        bb_commit_state(slot);
        bb_hw_enemy_update(1);
        return;
    }

    bb_hw_enemy_update(0);

    if (bb_camera.x != bb_prev_camera_x ||
            bb_camera.y != bb_prev_camera_y) {
        bb_debug_tile_layer_draws++;
        ana_tile_layer_draw(&bb_playfield_layer);
    }
    slot = bb_render_slot();
    bb_redraw_previous_and_current_actors(slot, !hardware_scroll_available);

    if (hud_dirty) {
        ana_layer_mark_dirty(&bb_hud_layer);
    }
    ana_layer_draw_if_dirty(&bb_hud_layer);
#if BB_INPUT_DEBUG_OVERLAY
    bb_draw_input_debug_overlay();
#endif
    bb_hw_enemy_update(1);

    bb_commit_state(slot);
}

BB_RenderStats bb_render_stats(void)
{
    BB_RenderStats stats;

    stats.bitmap_enemy_draws = bb_debug_bitmap_enemy_draws;
    stats.full_redraws = bb_debug_full_redraws;
    stats.tile_layer_draws = bb_debug_tile_layer_draws;
    stats.tile_redraw_world_rects = bb_debug_tile_redraw_world_rects;
    stats.tile_restore_world_rects = bb_debug_tile_restore_world_rects;
    stats.actor_redraw_passes = bb_debug_actor_redraw_passes;
    stats.player_draws = bb_debug_player_draws;
#ifdef ANA_TARGET_AMIGA
    stats.hw_enemy_update_calls = bb_hw_enemy_update_calls;
    stats.hw_enemy_visible_moves = bb_hw_enemy_visible_moves;
    stats.hw_enemy_ready = bb_hw_enemy_ready;
    stats.hw_enemy_failed = bb_hw_enemy_failed;
    stats.hw_enemy_sprite_count = bb_hw_enemy_sprite_count;
    stats.hw_enemy_slot_count = bb_hw_enemy_slot_count;
    stats.hw_enemy_status = bb_hw_enemy_failure_reason;
#else
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
    bb_hw_enemy_shutdown();
}

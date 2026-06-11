#include "byte_brothers_game.h"

#include "byte_brothers_assets.h"
#include "byte_brothers_internal.h"
#include "byte_brothers_levels.h"
#include "byte_brothers_render.h"

#ifdef BB_EMULATOR_HARNESS
#include "ana_internal.h"
#endif

#include <stdio.h>
#include <string.h>

#if defined(ANA_TARGET_AMIGA) && defined(BB_EMULATOR_HARNESS)
#include <dos/dos.h>
#include <exec/types.h>
#include <proto/dos.h>
#endif

/* Platforming rules, text-map loading, collision, enemies, and progression. */

char bb_map[BB_MAP_H][BB_MAP_W + 1];
static unsigned char bb_map_flags[BB_MAP_H][BB_MAP_W];
BB_Player bb_player;
BB_Enemy bb_enemies[BB_MAX_ENEMIES];
int bb_enemy_count = 0;
int bb_level_index = 0;
int bb_score = 0;
int bb_lives = 3;
int bb_fragments_left = 0;
ANA_Camera bb_camera;
int bb_frame = 0;

static int bb_start_x = 0;
static int bb_start_y = 0;
static int bb_jump_latched = 0;

static void bb_load_level(int level);
static int bb_player_tile_y(int tile_y);
static int bb_enemy_tile_y(int tile_y);

static void bb_map_input(void)
{
    ana_input_clear_key_map();
    ana_input_map_default_keys(ANA_INPUT_DEVICE_0);
    ana_input_map_key_to_action(ANA_KEY_X, ANA_INPUT_DEVICE_0, ANA_ACTION_2);
    ana_input_map_key_to_action(ANA_KEY_Z, ANA_INPUT_DEVICE_0, ANA_ACTION_2);
    ana_input_map_key_to_quit(ANA_KEY_ESCAPE);
    ana_input_map_key_to_quit(ANA_KEY_C);
    ana_input_map_key_to_quit(ANA_KEY_Q);
    ana_input_map_action_to_quit(ANA_INPUT_DEVICE_0, ANA_ACTION_3);
}

#define BB_TILE_FLAG_SOLID 0x01u
#define BB_TILE_FLAG_PLATFORM 0x02u
#define BB_TILE_FLAG_HAZARD 0x04u
#define BB_TILE_FLAG_COLLECTIBLE 0x08u
#define BB_TILE_FLAG_GOAL 0x10u

#ifndef BB_MAX_ACTIVE_ENEMIES
#define BB_MAX_ACTIVE_ENEMIES BB_MAX_ENEMIES
#endif

#ifdef BB_EMULATOR_HARNESS
#define BB_HARNESS_SCENARIO_STATIC 0
#define BB_HARNESS_SCENARIO_SCROLL 1
#define BB_HARNESS_SCENARIO_INPUT 2
#define BB_HARNESS_SCENARIO_ENEMY_OVERFLOW 3
#ifndef BB_HARNESS_FRAMES
#define BB_HARNESS_FRAMES 100
#endif
#ifndef BB_HARNESS_SCENARIO
#define BB_HARNESS_SCENARIO BB_HARNESS_SCENARIO_STATIC
#endif
#define BB_HARNESS_RESULT_FILE "ana_byte_brothers_result.txt"
static int bb_harness_start_player_x = 0;
static int bb_harness_start_player_y = 0;
static int bb_harness_start_camera_x = 0;
static int bb_harness_start_camera_y = 0;
static int bb_harness_last_camera_x = 0;
static int bb_harness_last_camera_y = 0;
static int bb_harness_scroll_frames = 0;
static int bb_harness_frames_with_visible_enemies = 0;
static int bb_harness_max_visible_enemies = 0;
static int bb_harness_input_jump_seen = 0;
static int bb_harness_input_dash_seen = 0;
static int bb_harness_input_move_seen = 0;
static int bb_harness_input_right_sent = 0;
static int bb_harness_input_right_down_seen = 0;
static int bb_harness_input_right_start_x = 0;
static int bb_harness_input_right_vx = 0;
static int bb_harness_input_quit_scheduled = 0;
static int bb_harness_input_quit_frame = 0;

static const char* bb_harness_scenario_name(void)
{
#if BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_SCROLL
    return "scroll";
#elif BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_INPUT
    return "input";
#elif BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_ENEMY_OVERFLOW
    return "enemy-overflow";
#else
    return "static";
#endif
}

#if BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_ENEMY_OVERFLOW
static void bb_harness_setup_enemy_overflow(void)
{
    int i;

    bb_player.x = 360;
    bb_player.y = bb_player_tile_y(10);
    bb_player.vx = 0;
    bb_player.vy = 0;
    bb_player.on_ground = 1;
    bb_player.facing = 1;
    ana_camera_set_position(&bb_camera, bb_player.x - BB_CAMERA_TARGET_X, 0);

    bb_enemy_count = 4;
    for (i = 0; i < bb_enemy_count; i++) {
        bb_enemies[i].x = bb_player.x + 48 + (i * 38);
        bb_enemies[i].y = bb_enemy_tile_y(14);
        bb_enemies[i].vx = (i & 1) ? -1 : 1;
        bb_enemies[i].alive = 1;
    }
}
#endif

static void bb_harness_begin(void)
{
#if BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_ENEMY_OVERFLOW
    bb_harness_setup_enemy_overflow();
#endif
    bb_harness_start_player_x = bb_player.x;
    bb_harness_start_player_y = bb_player.y;
    bb_harness_start_camera_x = bb_camera.x;
    bb_harness_start_camera_y = bb_camera.y;
    bb_harness_last_camera_x = bb_camera.x;
    bb_harness_last_camera_y = bb_camera.y;
    bb_harness_scroll_frames = 0;
    bb_harness_frames_with_visible_enemies = 0;
    bb_harness_max_visible_enemies = 0;
    bb_harness_input_jump_seen = 0;
    bb_harness_input_dash_seen = 0;
    bb_harness_input_move_seen = 0;
    bb_harness_input_right_sent = 0;
    bb_harness_input_right_down_seen = 0;
    bb_harness_input_right_start_x = bb_player.x;
    bb_harness_input_right_vx = 0;
    bb_harness_input_quit_scheduled = 0;
    bb_harness_input_quit_frame = 0;
#if BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_SCROLL || \
        BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_INPUT || \
        BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_ENEMY_OVERFLOW
    bb_player.invuln_ticks = (BB_HARNESS_FRAMES * 4) + BB_INVULN_TICKS;
#endif
}

static void bb_harness_apply_controls(int* move)
{
#if BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_SCROLL || \
        BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_ENEMY_OVERFLOW
    *move = 1;
#elif BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_STATIC
    *move = 0;
#else
    (void)move;
#endif
}

static void bb_harness_schedule_input(void)
{
#if BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_INPUT
    if (!bb_harness_input_move_seen && bb_frame < 25) {
        if (!bb_harness_input_right_sent) {
            bb_harness_input_right_sent = 1;
            bb_harness_input_right_start_x = bb_player.x;
        }
        ana_input_pulse_key_event(ANA_KEY_RIGHT);
    } else if (bb_harness_input_move_seen &&
            !bb_harness_input_jump_seen &&
            bb_frame < 55) {
        ana_input_pulse_key_event(ANA_KEY_SPACE);
    } else if (bb_harness_input_move_seen &&
            bb_harness_input_jump_seen &&
            !bb_harness_input_dash_seen &&
            bb_frame < 75) {
        ana_input_pulse_key_event(ANA_KEY_X);
    } else if (bb_frame >= 20 &&
            !bb_harness_input_quit_scheduled &&
            bb_harness_input_move_seen &&
            bb_harness_input_jump_seen &&
            bb_harness_input_dash_seen) {
        bb_harness_input_quit_scheduled = 1;
        bb_harness_input_quit_frame = bb_frame;
        ana_input_pulse_key_event(ANA_KEY_C);
    }
#endif
}

static int bb_harness_visible_enemy_count(void)
{
    ANA_Rect view;
    ANA_Rect enemy_rect;
    int visible;
    int i;

    view = ana_camera_world_view(&bb_camera);
    visible = 0;
    for (i = 0; i < bb_enemy_count; i++) {
        if (!bb_enemies[i].alive) {
            continue;
        }
        enemy_rect = ana_rect_make(
            bb_enemies[i].x,
            bb_enemies[i].y,
            BB_ENEMY_W,
            BB_ENEMY_H);
        if (ana_rect_intersects(view, enemy_rect)) {
            visible++;
        }
    }

    return visible;
}

static void bb_harness_update_stats(void)
{
    int visible;

    if (bb_camera.x != bb_harness_last_camera_x ||
            bb_camera.y != bb_harness_last_camera_y) {
        bb_harness_scroll_frames++;
    }
    bb_harness_last_camera_x = bb_camera.x;
    bb_harness_last_camera_y = bb_camera.y;

    visible = bb_harness_visible_enemy_count();
    if (visible > 0) {
        bb_harness_frames_with_visible_enemies++;
    }
    if (visible > bb_harness_max_visible_enemies) {
        bb_harness_max_visible_enemies = visible;
    }

#if BB_HARNESS_SCENARIO == BB_HARNESS_SCENARIO_INPUT
    if (bb_player.y < bb_harness_start_player_y ||
            bb_player.vy < 0) {
        bb_harness_input_jump_seen = 1;
    }
    if (bb_player.dash_ticks > 0 ||
            bb_player.vx >= BB_DASH_SPEED) {
        bb_harness_input_dash_seen = 1;
    }
    if (bb_harness_input_right_sent &&
            ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        bb_harness_input_right_down_seen = 1;
        bb_harness_input_right_vx = bb_player.vx;
        if (bb_player.vx == BB_PLAYER_SPEED ||
                bb_player.x > bb_harness_input_right_start_x) {
            bb_harness_input_move_seen = 1;
        }
    }
#endif
}
#endif

#if defined(ANA_TARGET_AMIGA) && defined(BB_EMULATOR_HARNESS)
static const char* bb_harness_result_phase = "shutdown";
static int bb_harness_result_complete = 1;

static void bb_harness_write_text(BPTR file, const char* text)
{
    Write(file, (APTR)text, (LONG)strlen(text));
}

static void bb_harness_write_unsigned_long(BPTR file, unsigned long value)
{
    char digits[16];
    int count;
    int i;

    count = 0;
    do {
        digits[count] = (char)('0' + (value % 10ul));
        count++;
        value /= 10ul;
    } while (value > 0ul && count < (int)sizeof(digits));

    for (i = count - 1; i >= 0; i--) {
        Write(file, (APTR)&digits[i], 1);
    }
}

static void bb_harness_write_string_line(
    BPTR file,
    const char* label,
    const char* value)
{
    bb_harness_write_text(file, label);
    bb_harness_write_text(file, "=");
    bb_harness_write_text(file, value);
    bb_harness_write_text(file, "\n");
}

static void bb_harness_write_long_line(
    BPTR file,
    const char* label,
    long value)
{
    bb_harness_write_text(file, label);
    bb_harness_write_text(file, "=");
    if (value < 0L) {
        bb_harness_write_text(file, "-");
        bb_harness_write_unsigned_long(file, (unsigned long)(0L - value));
    } else {
        bb_harness_write_unsigned_long(file, (unsigned long)value);
    }
    bb_harness_write_text(file, "\n");
}

static void bb_harness_write_int_line(
    BPTR file,
    const char* label,
    int value)
{
    bb_harness_write_long_line(file, label, (long)value);
}

static void bb_harness_write_result(void)
{
    static const char* paths[] = {
        "DH0:" BB_HARNESS_RESULT_FILE,
        "SYS:" BB_HARNESS_RESULT_FILE,
        "PROGDIR:" BB_HARNESS_RESULT_FILE,
        "a1200:" BB_HARNESS_RESULT_FILE,
        "RAM:" BB_HARNESS_RESULT_FILE,
        NULL};
    BPTR file;
    int i;
    ANA_RunStats run_stats;
    ANA_RenderStats render_stats;
    BB_RenderStats sprite_stats;

    file = (BPTR)0;
    for (i = 0; paths[i] != NULL; i++) {
        file = Open((STRPTR)paths[i], MODE_NEWFILE);
        if (file != (BPTR)0) {
            break;
        }
    }
    if (file == (BPTR)0) {
        printf("Byte Brothers could not write harness result file.\n");
        return;
    }

    run_stats = ana_last_run_stats();
    render_stats = ana_render_stats();
    sprite_stats = bb_render_stats();

    bb_harness_write_int_line(file, "ana_byte_brothers_result", 1);
    bb_harness_write_string_line(file, "phase", bb_harness_result_phase);
    bb_harness_write_string_line(file, "version", ANA_VERSION_STRING);
    bb_harness_write_int_line(file, "harness_scenario_id", BB_HARNESS_SCENARIO);
    bb_harness_write_string_line(
        file,
        "harness_scenario",
        bb_harness_scenario_name());
    bb_harness_write_int_line(file, "harness_frames", BB_HARNESS_FRAMES);
    bb_harness_write_int_line(file, "bb_frame", bb_frame);
    bb_harness_write_int_line(file, "level", bb_level_index + 1);
    bb_harness_write_int_line(file, "score", bb_score);
    bb_harness_write_int_line(file, "lives", bb_lives);
    bb_harness_write_int_line(file, "fragments_left", bb_fragments_left);
    bb_harness_write_int_line(file, "enemy_count", bb_enemy_count);
    bb_harness_write_int_line(file, "player_x", bb_player.x);
    bb_harness_write_int_line(file, "player_y", bb_player.y);
    bb_harness_write_int_line(file, "camera_x", bb_camera.x);
    bb_harness_write_int_line(file, "camera_y", bb_camera.y);
    bb_harness_write_int_line(
        file,
        "start_player_x",
        bb_harness_start_player_x);
    bb_harness_write_int_line(
        file,
        "start_player_y",
        bb_harness_start_player_y);
    bb_harness_write_int_line(
        file,
        "player_delta_x",
        bb_player.x - bb_harness_start_player_x);
    bb_harness_write_int_line(
        file,
        "player_delta_y",
        bb_player.y - bb_harness_start_player_y);
    bb_harness_write_int_line(
        file,
        "start_camera_x",
        bb_harness_start_camera_x);
    bb_harness_write_int_line(
        file,
        "start_camera_y",
        bb_harness_start_camera_y);
    bb_harness_write_int_line(
        file,
        "camera_delta_x",
        bb_camera.x - bb_harness_start_camera_x);
    bb_harness_write_int_line(
        file,
        "camera_delta_y",
        bb_camera.y - bb_harness_start_camera_y);
    bb_harness_write_int_line(
        file,
        "scroll_frames",
        bb_harness_scroll_frames);
    bb_harness_write_int_line(
        file,
        "frames_with_visible_enemies",
        bb_harness_frames_with_visible_enemies);
    bb_harness_write_int_line(
        file,
        "max_visible_enemies",
        bb_harness_max_visible_enemies);
    bb_harness_write_int_line(
        file,
        "input_jump_seen",
        bb_harness_input_jump_seen);
    bb_harness_write_int_line(
        file,
        "input_dash_seen",
        bb_harness_input_dash_seen);
    bb_harness_write_int_line(
        file,
        "input_move_seen",
        bb_harness_input_move_seen);
    bb_harness_write_int_line(
        file,
        "input_right_sent",
        bb_harness_input_right_sent);
    bb_harness_write_int_line(
        file,
        "input_right_down_seen",
        bb_harness_input_right_down_seen);
    bb_harness_write_int_line(
        file,
        "input_right_start_x",
        bb_harness_input_right_start_x);
    bb_harness_write_int_line(
        file,
        "input_right_vx",
        bb_harness_input_right_vx);
    bb_harness_write_int_line(
        file,
        "input_quit_scheduled",
        bb_harness_input_quit_scheduled);
    bb_harness_write_int_line(
        file,
        "input_quit_frame",
        bb_harness_input_quit_frame);
    bb_harness_write_int_line(file, "assets_loaded", bb_assets_loaded());
    bb_harness_write_long_line(file, "run_frames", run_stats.frames);
    bb_harness_write_long_line(file, "elapsed_ticks", run_stats.elapsed_ticks);
    bb_harness_write_long_line(
        file,
        "ticks_per_second",
        run_stats.ticks_per_second);
    bb_harness_write_long_line(
        file,
        "average_fps_x100",
        run_stats.average_fps_x100);
    bb_harness_write_long_line(
        file,
        "perf_ticks_per_second",
        run_stats.perf_ticks_per_second);
    bb_harness_write_long_line(
        file,
        "input_perf_ticks",
        run_stats.input_perf_ticks);
    bb_harness_write_long_line(
        file,
        "update_perf_ticks",
        run_stats.update_perf_ticks);
    bb_harness_write_long_line(
        file,
        "draw_perf_ticks",
        run_stats.draw_perf_ticks);
    bb_harness_write_long_line(
        file,
        "present_perf_ticks",
        run_stats.present_perf_ticks);
    bb_harness_write_long_line(file, "render_frames", render_stats.frames);
    bb_harness_write_long_line(
        file,
        "dirty_rects",
        render_stats.dirty_rects);
    bb_harness_write_long_line(
        file,
        "max_dirty_rects",
        render_stats.max_dirty_rects);
    bb_harness_write_long_line(
        file,
        "converted_pixels",
        render_stats.converted_pixels);
    bb_harness_write_long_line(
        file,
        "max_converted_pixels",
        render_stats.max_converted_pixels);
    bb_harness_write_long_line(
        file,
        "max_converted_rect_w",
        render_stats.max_converted_rect_w);
    bb_harness_write_long_line(
        file,
        "max_converted_rect_h",
        render_stats.max_converted_rect_h);
    bb_harness_write_long_line(
        file,
        "planar_clear_pixels",
        render_stats.planar_clear_pixels);
    bb_harness_write_long_line(
        file,
        "chunky_clear_pixels",
        render_stats.chunky_clear_pixels);
    bb_harness_write_long_line(
        file,
        "present_total_perf_ticks",
        render_stats.present_total_perf_ticks);
    bb_harness_write_long_line(
        file,
        "present_clear_perf_ticks",
        render_stats.present_clear_perf_ticks);
    bb_harness_write_long_line(
        file,
        "present_convert_perf_ticks",
        render_stats.present_convert_perf_ticks);
    bb_harness_write_long_line(
        file,
        "present_flip_perf_ticks",
        render_stats.present_flip_perf_ticks);
    bb_harness_write_long_line(
        file,
        "screen_buffer_flips",
        render_stats.screen_buffer_flips);
    bb_harness_write_long_line(file, "direct_flips", render_stats.direct_flips);
    bb_harness_write_long_line(
        file,
        "full_dirty_fill_rects",
        render_stats.full_dirty_fill_rects);
    bb_harness_write_long_line(
        file,
        "full_dirty_scroll_rects",
        render_stats.full_dirty_scroll_rects);
    bb_harness_write_long_line(
        file,
        "full_dirty_image_rects",
        render_stats.full_dirty_image_rects);
    bb_harness_write_long_line(
        file,
        "full_dirty_text_rects",
        render_stats.full_dirty_text_rects);
    bb_harness_write_long_line(
        file,
        "full_dirty_mask_fill_rects",
        render_stats.full_dirty_mask_fill_rects);
    bb_harness_write_long_line(
        file,
        "full_dirty_generic_rects",
        render_stats.full_dirty_generic_rects);
    bb_harness_write_long_line(
        file,
        "dirty_fill_rects",
        render_stats.dirty_fill_rects);
    bb_harness_write_long_line(
        file,
        "dirty_fill_pixels",
        render_stats.dirty_fill_pixels);
    bb_harness_write_long_line(
        file,
        "hardware_fill_rects",
        render_stats.hardware_fill_rects);
    bb_harness_write_long_line(
        file,
        "hardware_fill_pixels",
        render_stats.hardware_fill_pixels);
    bb_harness_write_long_line(
        file,
        "hardware_restore_rects",
        render_stats.hardware_restore_rects);
    bb_harness_write_long_line(
        file,
        "hardware_restore_pixels",
        render_stats.hardware_restore_pixels);
    bb_harness_write_long_line(
        file,
        "hardware_restore_fallbacks",
        render_stats.hardware_restore_fallbacks);
    bb_harness_write_long_line(
        file,
        "hardware_redraw_rects",
        render_stats.hardware_redraw_rects);
    bb_harness_write_long_line(
        file,
        "hardware_redraw_pixels",
        render_stats.hardware_redraw_pixels);
    bb_harness_write_long_line(
        file,
        "hardware_scroll_alloc_attempts",
        render_stats.hardware_scroll_alloc_attempts);
    bb_harness_write_long_line(
        file,
        "hardware_scroll_alloc_failures",
        render_stats.hardware_scroll_alloc_failures);
    bb_harness_write_long_line(
        file,
        "hardware_scroll_alloc_width",
        render_stats.hardware_scroll_alloc_width);
    bb_harness_write_long_line(
        file,
        "hardware_scroll_alloc_failed_buffer",
        render_stats.hardware_scroll_alloc_failed_buffer);
    bb_harness_write_long_line(
        file,
        "hardware_scroll_alloc_failed_plane",
        render_stats.hardware_scroll_alloc_failed_plane);
    bb_harness_write_long_line(
        file,
        "amiga_chip_avail_before_scroll_alloc",
        render_stats.amiga_chip_avail_before_scroll_alloc);
    bb_harness_write_long_line(
        file,
        "amiga_chip_largest_before_scroll_alloc",
        render_stats.amiga_chip_largest_before_scroll_alloc);
    bb_harness_write_long_line(
        file,
        "hw_enemy_update_calls",
        sprite_stats.hw_enemy_update_calls);
    bb_harness_write_long_line(
        file,
        "hw_enemy_visible_moves",
        sprite_stats.hw_enemy_visible_moves);
    bb_harness_write_int_line(
        file,
        "hw_enemy_ready",
        sprite_stats.hw_enemy_ready);
    bb_harness_write_int_line(
        file,
        "hw_enemy_failed",
        sprite_stats.hw_enemy_failed);
    bb_harness_write_int_line(
        file,
        "hw_enemy_sprite_count",
        sprite_stats.hw_enemy_sprite_count);
    bb_harness_write_int_line(
        file,
        "hw_enemy_slot_count",
        sprite_stats.hw_enemy_slot_count);
    bb_harness_write_long_line(
        file,
        "bitmap_enemy_draws",
        sprite_stats.bitmap_enemy_draws);
    bb_harness_write_long_line(file, "full_redraws", sprite_stats.full_redraws);
    bb_harness_write_long_line(
        file,
        "tile_layer_draws",
        sprite_stats.tile_layer_draws);
    bb_harness_write_long_line(
        file,
        "tile_hardware_unavailable_draws",
        sprite_stats.tile_hardware_unavailable_draws);
    bb_harness_write_long_line(
        file,
        "tile_hardware_draws",
        sprite_stats.tile_hardware_draws);
    bb_harness_write_long_line(
        file,
        "tile_software_draws",
        sprite_stats.tile_software_draws);
    bb_harness_write_long_line(
        file,
        "tile_redraw_world_rects",
        sprite_stats.tile_redraw_world_rects);
    bb_harness_write_long_line(
        file,
        "tile_restore_world_rects",
        sprite_stats.tile_restore_world_rects);
    bb_harness_write_long_line(
        file,
        "actor_redraw_passes",
        sprite_stats.actor_redraw_passes);
    bb_harness_write_long_line(file, "player_draws", sprite_stats.player_draws);
    bb_harness_write_long_line(
        file,
        "bb_tile_perf_ticks",
        sprite_stats.tile_perf_ticks);
    bb_harness_write_long_line(
        file,
        "bb_hud_perf_ticks",
        sprite_stats.hud_perf_ticks);
    bb_harness_write_long_line(
        file,
        "bb_actor_perf_ticks",
        sprite_stats.actor_perf_ticks);
    bb_harness_write_long_line(
        file,
        "bb_hw_player_perf_ticks",
        sprite_stats.hw_player_perf_ticks);
    bb_harness_write_long_line(
        file,
        "bb_hw_enemy_perf_ticks",
        sprite_stats.hw_enemy_perf_ticks);
    bb_harness_write_long_line(
        file,
        "hw_player_update_calls",
        sprite_stats.hw_player_update_calls);
    bb_harness_write_long_line(
        file,
        "hw_player_visible_moves",
        sprite_stats.hw_player_visible_moves);
    bb_harness_write_int_line(
        file,
        "hw_player_ready",
        sprite_stats.hw_player_ready);
    bb_harness_write_int_line(
        file,
        "hw_player_failed",
        sprite_stats.hw_player_failed);
    bb_harness_write_int_line(
        file,
        "hw_player_sprite_count",
        sprite_stats.hw_player_sprite_count);
    bb_harness_write_string_line(
        file,
        "hw_player_status",
        sprite_stats.hw_player_status != NULL ?
            sprite_stats.hw_player_status :
            "unknown");
    bb_harness_write_string_line(
        file,
        "hw_enemy_status",
        sprite_stats.hw_enemy_status != NULL ?
            sprite_stats.hw_enemy_status :
            "unknown");
    bb_harness_write_int_line(file, "result_complete", bb_harness_result_complete);

    Close(file);
    printf("Byte Brothers wrote harness result to %s.\n", paths[i]);
}

#elif defined(BB_EMULATOR_HARNESS)
static void bb_harness_write_result(void)
{
}
#endif

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
}

static unsigned char bb_tile_flags_for_char(char tile)
{
    switch (tile) {
    case BB_TILE_SOLID:
    case BB_TILE_POWER:
    case BB_TILE_BONUS:
    case BB_TILE_GATE:
        return BB_TILE_FLAG_SOLID;
    case BB_TILE_PLATFORM:
        return BB_TILE_FLAG_PLATFORM;
    case BB_TILE_SPIKE:
    case BB_TILE_WATER:
        return BB_TILE_FLAG_HAZARD;
    case BB_TILE_FRAGMENT:
    case BB_TILE_RARE_FRAGMENT:
    case BB_TILE_CODE_FRAGMENT:
        return BB_TILE_FLAG_COLLECTIBLE;
    case BB_TILE_EXIT:
        return BB_TILE_FLAG_GOAL;
    default:
        return 0u;
    }
}

static unsigned char bb_tile_flags_at(int tx, int ty)
{
    if (tx < 0 || tx >= BB_MAP_W || ty < 0 || ty >= BB_MAP_H) {
        return BB_TILE_FLAG_SOLID;
    }

    return bb_map_flags[ty][tx];
}

char bb_tile_at(int tx, int ty)
{
    if (tx < 0 || tx >= BB_MAP_W || ty < 0 || ty >= BB_MAP_H) {
        return BB_TILE_SOLID;
    }

    return bb_map[ty][tx];
}

void bb_set_tile(int tx, int ty, char value)
{
    if (tx < 0 || tx >= BB_MAP_W || ty < 0 || ty >= BB_MAP_H) {
        return;
    }

    if (bb_map[ty][tx] == value) {
        return;
    }

    bb_map[ty][tx] = value;
    bb_map_flags[ty][tx] = bb_tile_flags_for_char(value);
    bb_render_tile_changed(tx, ty);
}

int bb_tile_is_solid(char tile)
{
    return (bb_tile_flags_for_char(tile) & BB_TILE_FLAG_SOLID) != 0u;
}

int bb_tile_is_platform(char tile)
{
    return (bb_tile_flags_for_char(tile) & BB_TILE_FLAG_PLATFORM) != 0u;
}

int bb_tile_is_hazard(char tile)
{
    return (bb_tile_flags_for_char(tile) & BB_TILE_FLAG_HAZARD) != 0u;
}

int bb_tile_is_collectible(char tile)
{
    return (bb_tile_flags_for_char(tile) & BB_TILE_FLAG_COLLECTIBLE) != 0u;
}

int bb_tile_is_goal(char tile)
{
    return (bb_tile_flags_for_char(tile) & BB_TILE_FLAG_GOAL) != 0u;
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

static ANA_Rect bb_tiles_for_rect(int x, int y, int w, int h)
{
    return ana_tile_rect_for_world_rect(
        ana_rect_make(x, y, w, h),
        BB_TILE,
        BB_TILE);
}

static int bb_rect_hits_solid(int x, int y, int w, int h)
{
    ANA_Rect tiles;
    int tx;
    int ty;
    unsigned char flags;

    tiles = bb_tiles_for_rect(x, y, w, h);

    for (ty = tiles.y; ty < tiles.y + tiles.h; ty++) {
        if (ty < 0) {
            continue;
        }
        for (tx = tiles.x; tx < tiles.x + tiles.w; tx++) {
            flags = bb_tile_flags_at(tx, ty);
            if ((flags & BB_TILE_FLAG_SOLID) != 0u) {
                return 1;
            }
        }
    }

    return 0;
}

static int bb_has_floor_at(int x, int y)
{
    ANA_Rect tile_rect;
    int tx;
    int ty;
    unsigned char flags;

    tile_rect = bb_tiles_for_rect(x, y, 1, 1);
    tx = tile_rect.x;
    ty = tile_rect.y;
    flags = bb_tile_flags_at(tx, ty);
    return (flags & (BB_TILE_FLAG_SOLID | BB_TILE_FLAG_PLATFORM)) != 0u;
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
    ANA_Rect tiles;

    old_bottom = old_y + h;
    new_bottom = new_y + h;
    if (new_bottom <= old_bottom) {
        return 0;
    }

    tiles = bb_tiles_for_rect(x + 1, new_bottom, w - 2, 1);
    tx0 = tiles.x;
    tx1 = tiles.x + tiles.w - 1;
    ty = tiles.y;
    platform_top = ty * BB_TILE;

    if (old_bottom > platform_top || new_bottom < platform_top) {
        return 0;
    }

    for (tx = tx0; tx <= tx1; tx++) {
        if ((bb_tile_flags_at(tx, ty) & BB_TILE_FLAG_PLATFORM) != 0u) {
            return platform_top;
        }
    }

    return 0;
}

static int bb_bump_head_tiles(int head_y)
{
    ANA_Rect tiles;
    int tx;
    int ty;
    char tile;
    int bumped;

    tiles = bb_tiles_for_rect(bb_player.x + 1, head_y, BB_PLAYER_W - 2, 1);
    ty = tiles.y;
    bumped = 0;

    for (tx = tiles.x; tx < tiles.x + tiles.w; tx++) {
        tile = bb_tile_at(tx, ty);
        if (tile == BB_TILE_POWER ||
                tile == BB_TILE_HIDDEN ||
                tile == BB_TILE_BONUS) {
            bb_set_tile(tx, ty, BB_TILE_EMPTY);
            bb_score += tile == BB_TILE_BONUS ? 5 : 25;
            bb_assets_play_power();
            bumped = 1;
        }
    }

    return bumped;
}

static int bb_find_solid_in_column(int tx, int y0, int y1)
{
    int ty0;
    int ty1;
    int ty;

    ty0 = y0 / BB_TILE;
    ty1 = y1 / BB_TILE;
    for (ty = ty0; ty <= ty1; ty++) {
        if (ty < 0) {
            continue;
        }
        if ((bb_tile_flags_at(tx, ty) & BB_TILE_FLAG_SOLID) != 0u) {
            return ty;
        }
    }

    return -1;
}

static int bb_find_solid_in_row(int x0, int x1, int ty)
{
    int tx0;
    int tx1;
    int tx;

    if (ty < 0) {
        return -1;
    }

    tx0 = x0 / BB_TILE;
    tx1 = x1 / BB_TILE;
    for (tx = tx0; tx <= tx1; tx++) {
        if ((bb_tile_flags_at(tx, ty) & BB_TILE_FLAG_SOLID) != 0u) {
            return tx;
        }
    }

    return -1;
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
    int new_x;
    int tx;
    int hit_ty;

    if (dx == 0) {
        return;
    }

    new_x = bb_player.x + dx;
    if (dx > 0) {
        tx = (new_x + BB_PLAYER_W - 1) / BB_TILE;
        hit_ty = bb_find_solid_in_column(
            tx,
            bb_player.y,
            bb_player.y + BB_PLAYER_H - 1);
        if (hit_ty >= 0) {
            bb_player.x = tx * BB_TILE - BB_PLAYER_W;
            bb_player.vx = 0;
            return;
        }
    } else {
        tx = new_x / BB_TILE;
        hit_ty = bb_find_solid_in_column(
            tx,
            bb_player.y,
            bb_player.y + BB_PLAYER_H - 1);
        if (hit_ty >= 0) {
            bb_player.x = (tx + 1) * BB_TILE;
            bb_player.vx = 0;
            return;
        }
    }

    bb_player.x = new_x;
}

static void bb_move_player_v(int dy)
{
    int new_y;
    int platform_top;
    int ty;
    int hit_tx;

    if (dy == 0) {
        return;
    }

    new_y = bb_player.y + dy;
    bb_player.on_ground = 0;

    if (dy > 0) {
        platform_top = bb_hits_platform_on_descent(
            bb_player.x,
            bb_player.y,
            new_y,
            BB_PLAYER_W,
            BB_PLAYER_H);
        if (platform_top != 0) {
            bb_player.y = platform_top - BB_PLAYER_H;
            bb_player.vy = 0;
            bb_player.on_ground = 1;
            return;
        }

        ty = (new_y + BB_PLAYER_H - 1) / BB_TILE;
        hit_tx = bb_find_solid_in_row(
            bb_player.x,
            bb_player.x + BB_PLAYER_W - 1,
            ty);
        if (hit_tx >= 0) {
            bb_player.y = ty * BB_TILE - BB_PLAYER_H;
            bb_player.vy = 0;
            bb_player.on_ground = 1;
            return;
        }
    } else {
        if (bb_bump_head_tiles(new_y)) {
            bb_player.vy = 0;
            return;
        }

        if (new_y >= 0) {
            ty = new_y / BB_TILE;
            hit_tx = bb_find_solid_in_row(
                bb_player.x,
                bb_player.x + BB_PLAYER_W - 1,
                ty);
            if (hit_tx >= 0) {
                bb_bump_head_tiles(new_y);
                bb_player.y = (ty + 1) * BB_TILE;
                bb_player.vy = 0;
                return;
            }
        }
    }

    bb_player.y = new_y;
}

static void bb_clamp_player_to_world(void)
{
    bb_player.x = ana_clamp_int(bb_player.x, 0, BB_WORLD_W - BB_PLAYER_W);
    if (bb_player.y > BB_WORLD_H) {
        bb_player_hit();
    }
}

static void bb_scan_player_tiles(int* hit_hazard, int* reached_goal)
{
    ANA_Rect tiles;
    int tx;
    int ty;
    char tile;
    unsigned char flags;

    if (hit_hazard != 0) {
        *hit_hazard = 0;
    }
    if (reached_goal != 0) {
        *reached_goal = 0;
    }

    tiles = bb_tiles_for_rect(
        bb_player.x,
        bb_player.y,
        BB_PLAYER_W,
        BB_PLAYER_H);

    for (ty = tiles.y; ty < tiles.y + tiles.h; ty++) {
        for (tx = tiles.x; tx < tiles.x + tiles.w; tx++) {
            flags = bb_tile_flags_at(tx, ty);
            if ((flags & BB_TILE_FLAG_COLLECTIBLE) != 0u) {
                tile = bb_tile_at(tx, ty);
                if (tile == BB_TILE_FRAGMENT) {
                    bb_score += 10;
                    bb_fragments_left--;
                    bb_set_tile(tx, ty, BB_TILE_EMPTY);
                    bb_assets_play_collect();
                } else if (tile == BB_TILE_RARE_FRAGMENT) {
                    bb_score += 100;
                    bb_fragments_left--;
                    bb_set_tile(tx, ty, BB_TILE_EMPTY);
                    bb_assets_play_power();
                } else if (tile == BB_TILE_CODE_FRAGMENT) {
                    bb_score += 50;
                    bb_fragments_left--;
                    bb_set_tile(tx, ty, BB_TILE_EMPTY);
                    bb_assets_play_power();
                }
            }
            if (hit_hazard != 0 &&
                    (flags & BB_TILE_FLAG_HAZARD) != 0u) {
                *hit_hazard = 1;
            }
            if (reached_goal != 0 &&
                    (flags & BB_TILE_FLAG_GOAL) != 0u) {
                *reached_goal = 1;
            }
        }
    }
}

static void bb_update_camera(void)
{
    int target;

    target = bb_player.x - BB_CAMERA_TARGET_X;
    ana_camera_set_position(&bb_camera, target, 0);
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

        if (ana_rect_intersects(
                ana_rect_make(
                    bb_player.x,
                    bb_player.y,
                    BB_PLAYER_W,
                    BB_PLAYER_H),
                ana_rect_make(
                    enemy->x,
                    enemy->y,
                    BB_ENEMY_W,
                    BB_ENEMY_H))) {
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
        memcpy(bb_map[y], bb_levels[level][y], BB_MAP_W);
        bb_map[y][BB_MAP_W] = '\0';
        for (x = 0; x < BB_MAP_W; x++) {
            tile = bb_map[y][x];
            if (tile == BB_TILE_PLAYER_START) {
                bb_start_x = bb_actor_tile_x(x, BB_PLAYER_W);
                bb_start_y = bb_player_tile_y(y);
                bb_map[y][x] = BB_TILE_EMPTY;
            } else if (tile == BB_TILE_ENEMY || tile == BB_TILE_ENEMY_ALT) {
                if (bb_enemy_count < BB_MAX_ENEMIES &&
                        bb_enemy_count < BB_MAX_ACTIVE_ENEMIES) {
                    bb_enemies[bb_enemy_count].x =
                        bb_actor_tile_x(x, BB_ENEMY_W);
                    bb_enemies[bb_enemy_count].y = bb_enemy_tile_y(y);
                    bb_enemies[bb_enemy_count].vx = (bb_enemy_count & 1) ? -1 : 1;
                    bb_enemies[bb_enemy_count].alive = 1;
                    bb_enemy_count++;
                }
                bb_map[y][x] = BB_TILE_EMPTY;
            } else if (bb_tile_is_collectible(tile)) {
                bb_fragments_left++;
            }
            bb_map_flags[y][x] = bb_tile_flags_for_char(bb_map[y][x]);
        }
    }

    bb_reset_player();
    bb_jump_latched = 0;
    bb_render_reset();
    printf(
        "Byte Brothers level %d loaded: enemies=%d, fragments=%d\n",
        level + 1,
        bb_enemy_count,
        bb_fragments_left);
}

void byte_brothers_init(void)
{
    bb_map_input();
}

void byte_brothers_load(void)
{
    bb_set_palette();
    bb_assets_load();
    bb_level_index = 0;
    bb_score = 0;
    bb_lives = 3;
    bb_frame = 0;
    bb_load_level(bb_level_index);
#ifdef BB_EMULATOR_HARNESS
    bb_harness_begin();
#endif
}

void byte_brothers_update(ANA_Time time)
{
    int move;
    int dash_pressed;
    int hit_hazard;
    int reached_goal;
    int jump_pressed;
    int jump_down;

    (void)time;

    if (ana_quit_requested()) {
        ana_quit();
        return;
    }

    bb_frame++;

    move = 0;
    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        move--;
    }
    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        move++;
    }
#ifdef BB_EMULATOR_HARNESS
    bb_harness_apply_controls(&move);
#endif

    if (move < 0) {
        bb_player.facing = -1;
    } else if (move > 0) {
        bb_player.facing = 1;
    }

    jump_down =
        ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_1) ||
        ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_4) ||
        ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_UP);
    jump_pressed =
        ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_1) ||
        ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_4) ||
        ana_input_direction_pressed(ANA_INPUT_DEVICE_0, ANA_INPUT_UP) ||
        (jump_down && !bb_jump_latched);
    dash_pressed =
        ana_input_direction_pressed(ANA_INPUT_DEVICE_0, ANA_INPUT_DOWN) ||
        ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_2);

    if (!jump_down) {
        bb_jump_latched = 0;
    }

    if (jump_pressed && bb_player.on_ground) {
        bb_player.vy = BB_JUMP_SPEED;
        bb_player.on_ground = 0;
        bb_jump_latched = 1;
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

    bb_scan_player_tiles(&hit_hazard, &reached_goal);
    if (hit_hazard) {
        bb_player_hit();
    }

    bb_update_enemies();

    if (reached_goal) {
        bb_assets_play_goal();
        bb_level_index++;
        if (bb_level_index >= BB_LEVEL_COUNT) {
            bb_level_index = 0;
        }
        bb_load_level(bb_level_index);
    }

    bb_update_camera();
#ifdef BB_EMULATOR_HARNESS
    bb_harness_update_stats();
    bb_harness_schedule_input();
#endif

#ifdef BB_EMULATOR_HARNESS
    if (ana_render_stats().frames >= BB_HARNESS_FRAMES) {
        ana_quit();
    }
#endif

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
#ifdef BB_EMULATOR_HARNESS
    bb_harness_write_result();
#endif
    bb_render_shutdown();
    bb_assets_unload();
}

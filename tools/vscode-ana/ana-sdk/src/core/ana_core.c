#include "ana/ana_core.h"

#include "ana_internal.h"

#include <stddef.h>
#include <stdio.h>

static int ana_runtime_quit_requested = 0;
static int ana_runtime_running = 0;
static ANA_RunStats ana_runtime_last_stats =
    { 0L, 0L, 1L, 0L, 1L, 0L, 0L, 0L, 0L };

static int ana_game_value_or_default(int value, int default_value)
{
    if (value == 0) {
        return default_value;
    }

    return value;
}

static ANA_ScreenMode ana_game_screen_mode_or_default(ANA_ScreenMode mode)
{
    if (mode == 0) {
        return ANA_SCREEN_PAL_LORES;
    }

    return mode;
}

static ANA_RenderMode ana_game_render_mode_or_default(ANA_RenderMode mode)
{
    if (mode == ANA_RENDER_DEFAULT) {
        return ANA_RENDER_DIRTY;
    }

    return mode;
}

static ANA_Profile ana_profile_from_game(const ANA_Game* game)
{
    ANA_Profile profile;

    profile.width = ana_game_value_or_default(game->width, ANA_DEFAULT_WIDTH);
    profile.height = ana_game_value_or_default(game->height, ANA_DEFAULT_HEIGHT);
    profile.fps = ana_game_value_or_default(game->fps, ANA_DEFAULT_FPS);
    profile.colors = ana_game_value_or_default(game->colors, ANA_DEFAULT_COLORS);
    profile.bitplanes = ANA_DEFAULT_BITPLANES;
    profile.screen_mode = ana_game_screen_mode_or_default(game->screen_mode);
    profile.render_mode = ana_game_render_mode_or_default(game->render_mode);
    profile.target_flags = ANA_TARGET_OCS_ECS;

    return profile;
}

static void ana_runtime_reset_stats(void)
{
    ana_runtime_last_stats.frames = 0L;
    ana_runtime_last_stats.elapsed_ticks = 0L;
    ana_runtime_last_stats.ticks_per_second =
        ana_platform_time_ticks_per_second();
    ana_runtime_last_stats.average_fps_x100 = 0L;
    ana_runtime_last_stats.perf_ticks_per_second =
        (long)ana_platform_perf_ticks_per_second();
    ana_runtime_last_stats.input_perf_ticks = 0L;
    ana_runtime_last_stats.update_perf_ticks = 0L;
    ana_runtime_last_stats.draw_perf_ticks = 0L;
    ana_runtime_last_stats.present_perf_ticks = 0L;
}

static void ana_runtime_record_stats(long start_ticks, long end_ticks)
{
    long elapsed_ticks;

    elapsed_ticks = end_ticks - start_ticks;
    if (elapsed_ticks < 0L) {
        elapsed_ticks = 0L;
    }

    ana_runtime_last_stats.frames = (long)ana_gfx_present_count();
    ana_runtime_last_stats.elapsed_ticks = elapsed_ticks;
    ana_runtime_last_stats.ticks_per_second =
        ana_platform_time_ticks_per_second();

    if (elapsed_ticks > 0L &&
            ana_runtime_last_stats.ticks_per_second > 0L) {
        ana_runtime_last_stats.average_fps_x100 =
            (ana_runtime_last_stats.frames *
                ana_runtime_last_stats.ticks_per_second *
                100L) /
            elapsed_ticks;
    } else {
        ana_runtime_last_stats.average_fps_x100 = 0L;
    }
}

#ifdef ANA_DEBUG_STATS
static void ana_runtime_record_perf_ticks(
    long* total,
    unsigned long start_ticks,
    unsigned long end_ticks)
{
    unsigned long elapsed;

    if (total == NULL) {
        return;
    }

    elapsed = end_ticks - start_ticks;
    *total += (long)elapsed;
}

static long ana_runtime_average_us(
    long total_perf_ticks,
    long frames,
    long perf_ticks_per_second)
{
    long avg_ticks;

    if (total_perf_ticks <= 0L || frames <= 0L ||
            perf_ticks_per_second <= 0L) {
        return 0L;
    }

    avg_ticks = total_perf_ticks / frames;
    if (perf_ticks_per_second >= 1000L) {
        return (avg_ticks * 1000L) / (perf_ticks_per_second / 1000L);
    }

    return (avg_ticks * 1000000L) / perf_ticks_per_second;
}

static void ana_runtime_print_ms(long microseconds)
{
    printf("%ld.%03ld", microseconds / 1000L, microseconds % 1000L);
}

static void ana_runtime_print_debug_stats(void)
{
    ANA_RunStats stats;
    ANA_RenderStats render_stats;
    long elapsed_seconds_x100;
    long dirty_rects_per_frame_x100;
    long converted_pixels_per_frame;
    long planar_clear_pixels_per_frame;
    long chunky_clear_pixels_per_frame;
    long input_us;
    long update_us;
    long draw_us;
    long present_us;
    long present_total_us;
    long present_clear_us;
    long present_convert_us;
    long present_flip_us;

    stats = ana_last_run_stats();
    render_stats = ana_render_stats();
    elapsed_seconds_x100 = 0L;
    dirty_rects_per_frame_x100 = 0L;
    converted_pixels_per_frame = 0L;
    planar_clear_pixels_per_frame = 0L;
    chunky_clear_pixels_per_frame = 0L;
    input_us = ana_runtime_average_us(
        stats.input_perf_ticks,
        stats.frames,
        stats.perf_ticks_per_second);
    update_us = ana_runtime_average_us(
        stats.update_perf_ticks,
        stats.frames,
        stats.perf_ticks_per_second);
    draw_us = ana_runtime_average_us(
        stats.draw_perf_ticks,
        stats.frames,
        stats.perf_ticks_per_second);
    present_us = ana_runtime_average_us(
        stats.present_perf_ticks,
        stats.frames,
        stats.perf_ticks_per_second);
    present_total_us = ana_runtime_average_us(
        render_stats.present_total_perf_ticks,
        render_stats.frames,
        render_stats.perf_ticks_per_second);
    present_clear_us = ana_runtime_average_us(
        render_stats.present_clear_perf_ticks,
        render_stats.frames,
        render_stats.perf_ticks_per_second);
    present_convert_us = ana_runtime_average_us(
        render_stats.present_convert_perf_ticks,
        render_stats.frames,
        render_stats.perf_ticks_per_second);
    present_flip_us = ana_runtime_average_us(
        render_stats.present_flip_perf_ticks,
        render_stats.frames,
        render_stats.perf_ticks_per_second);

    if (stats.ticks_per_second > 0L) {
        elapsed_seconds_x100 =
            (stats.elapsed_ticks * 100L) / stats.ticks_per_second;
    }

    if (render_stats.frames > 0L) {
        dirty_rects_per_frame_x100 =
            (render_stats.dirty_rects * 100L) / render_stats.frames;
        converted_pixels_per_frame =
            render_stats.converted_pixels / render_stats.frames;
        planar_clear_pixels_per_frame =
            render_stats.planar_clear_pixels / render_stats.frames;
        chunky_clear_pixels_per_frame =
            render_stats.chunky_clear_pixels / render_stats.frames;
    }

    printf("Frames presented: %ld\n", stats.frames);
    printf(
        "Elapsed time: %ld.%02ld sec\n",
        elapsed_seconds_x100 / 100L,
        elapsed_seconds_x100 % 100L);
    printf(
        "Average FPS: %ld.%02ld\n",
        stats.average_fps_x100 / 100L,
        stats.average_fps_x100 % 100L);
    printf(
        "Dirty rects/frame: %ld.%02ld (max %ld)\n",
        dirty_rects_per_frame_x100 / 100L,
        dirty_rects_per_frame_x100 % 100L,
        render_stats.max_dirty_rects);
    printf(
        "Converted pixels/frame: %ld (max %ld)\n",
        converted_pixels_per_frame,
        render_stats.max_converted_pixels);
    printf(
        "Planar clear pixels/frame: %ld (max %ld)\n",
        planar_clear_pixels_per_frame,
        render_stats.max_planar_clear_pixels);
    printf(
        "Chunky clear pixels/frame: %ld (max %ld)\n",
        chunky_clear_pixels_per_frame,
        render_stats.max_chunky_clear_pixels);
    printf("Avg input/update/draw/present ms: ");
    ana_runtime_print_ms(input_us);
    printf("/");
    ana_runtime_print_ms(update_us);
    printf("/");
    ana_runtime_print_ms(draw_us);
    printf("/");
    ana_runtime_print_ms(present_us);
    printf("\n");
    printf("Avg present clear/c2p/flip/total ms: ");
    ana_runtime_print_ms(present_clear_us);
    printf("/");
    ana_runtime_print_ms(present_convert_us);
    printf("/");
    ana_runtime_print_ms(present_flip_us);
    printf("/");
    ana_runtime_print_ms(present_total_us);
    printf("\n");
    printf(
        "Flip paths screen/direct: %ld/%ld\n",
        render_stats.screen_buffer_flips,
        render_stats.direct_flips);
}
#endif

static void ana_runtime_wait_for_next_frame(
    long start_ticks,
    long frame,
    long fps)
{
    long ticks_per_second;
    long target_ticks;

    if (fps <= 0L) {
        return;
    }

    ticks_per_second = ana_platform_time_ticks_per_second();
    if (ticks_per_second <= 0L) {
        return;
    }

    target_ticks = start_ticks + ((frame * ticks_per_second) / fps);
    ana_platform_wait_until_time_tick(target_ticks);
}

int ana_run(const ANA_Game* game)
{
    ANA_Profile profile;
    ANA_Result result;
    ANA_Time time;
    long start_ticks;
    long end_ticks;
#ifdef ANA_DEBUG_STATS
    unsigned long perf_start;
#endif

    ana_runtime_reset_stats();

    if (game == NULL) {
        return ANA_ERROR_INVALID_ARGUMENT;
    }

    if (ana_runtime_running) {
        return ANA_ERROR_INVALID_ARGUMENT;
    }

    profile = ana_profile_from_game(game);
    result = ana_validate_profile(&profile);
    if (result != ANA_OK) {
        return result;
    }

    result = ana_gfx_open(&profile);
    if (result != ANA_OK) {
        return result;
    }

    result = ana_sound_open(&profile);
    if (result != ANA_OK) {
        ana_gfx_close();
        return result;
    }

    ana_input_reset();
    ana_runtime_running = 1;
    ana_runtime_quit_requested = 0;

    if (game->init != NULL) {
        game->init();
    }

    if (!ana_runtime_quit_requested && game->load != NULL) {
        game->load();
    }

    time.tick = 0;
    time.fps = profile.fps;
    start_ticks = ana_platform_time_ticks();

    while (!ana_runtime_quit_requested) {
#ifdef ANA_DEBUG_STATS
        perf_start = ana_platform_perf_ticks();
#endif
        ana_input_update();
#ifdef ANA_DEBUG_STATS
        ana_runtime_record_perf_ticks(
            &ana_runtime_last_stats.input_perf_ticks,
            perf_start,
            ana_platform_perf_ticks());
#endif

        if (ana_quit_requested()) {
            ana_quit();
        }

        if (!ana_runtime_quit_requested && game->update != NULL) {
#ifdef ANA_DEBUG_STATS
            perf_start = ana_platform_perf_ticks();
#endif
            game->update(time);
#ifdef ANA_DEBUG_STATS
            ana_runtime_record_perf_ticks(
                &ana_runtime_last_stats.update_perf_ticks,
                perf_start,
                ana_platform_perf_ticks());
#endif
        }

        if (!ana_runtime_quit_requested && game->draw != NULL) {
#ifdef ANA_DEBUG_STATS
            perf_start = ana_platform_perf_ticks();
#endif
            game->draw();
#ifdef ANA_DEBUG_STATS
            ana_runtime_record_perf_ticks(
                &ana_runtime_last_stats.draw_perf_ticks,
                perf_start,
                ana_platform_perf_ticks());
            perf_start = ana_platform_perf_ticks();
#endif
            ana_present();
#ifdef ANA_DEBUG_STATS
            ana_runtime_record_perf_ticks(
                &ana_runtime_last_stats.present_perf_ticks,
                perf_start,
                ana_platform_perf_ticks());
#endif
        }

        if (!ana_runtime_quit_requested) {
            ana_runtime_wait_for_next_frame(
                start_ticks,
                time.tick + 1L,
                (long)profile.fps);
            ana_sound_update();
        }

        time.tick++;
    }

    end_ticks = ana_platform_time_ticks();
    ana_runtime_record_stats(start_ticks, end_ticks);

#ifdef ANA_DEBUG_STATS
    if (game->debug_stats) {
        ana_runtime_print_debug_stats();
    }
#endif

    if (game->shutdown != NULL) {
        game->shutdown();
    }

    ana_runtime_quit_requested = 0;
    ana_runtime_running = 0;
    ana_sound_close();
    ana_gfx_close();

    return ANA_OK;
}

void ana_quit(void)
{
    ana_runtime_quit_requested = 1;
}

ANA_RunStats ana_last_run_stats(void)
{
    return ana_runtime_last_stats;
}

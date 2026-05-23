#include "ana/ana_core.h"

#include "ana_internal.h"

#include <stddef.h>

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

static ANA_Profile ana_profile_from_game(const ANA_Game* game)
{
    ANA_Profile profile;

    profile.width = ana_game_value_or_default(game->width, ANA_DEFAULT_WIDTH);
    profile.height = ana_game_value_or_default(game->height, ANA_DEFAULT_HEIGHT);
    profile.fps = ana_game_value_or_default(game->fps, ANA_DEFAULT_FPS);
    profile.colors = ana_game_value_or_default(game->colors, ANA_DEFAULT_COLORS);
    profile.bitplanes = ANA_DEFAULT_BITPLANES;
    profile.screen_mode = ana_game_screen_mode_or_default(game->screen_mode);
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
    unsigned long perf_start;

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
        perf_start = ana_platform_perf_ticks();
        ana_input_update();
        ana_runtime_record_perf_ticks(
            &ana_runtime_last_stats.input_perf_ticks,
            perf_start,
            ana_platform_perf_ticks());

        if (ana_quit_requested()) {
            ana_quit();
        }

        if (!ana_runtime_quit_requested && game->update != NULL) {
            perf_start = ana_platform_perf_ticks();
            game->update(time);
            ana_runtime_record_perf_ticks(
                &ana_runtime_last_stats.update_perf_ticks,
                perf_start,
                ana_platform_perf_ticks());
        }

        if (!ana_runtime_quit_requested && game->draw != NULL) {
            perf_start = ana_platform_perf_ticks();
            game->draw();
            ana_runtime_record_perf_ticks(
                &ana_runtime_last_stats.draw_perf_ticks,
                perf_start,
                ana_platform_perf_ticks());
            perf_start = ana_platform_perf_ticks();
            ana_present();
            ana_runtime_record_perf_ticks(
                &ana_runtime_last_stats.present_perf_ticks,
                perf_start,
                ana_platform_perf_ticks());
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

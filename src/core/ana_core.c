#include "ana/ana_core.h"

#include "ana_internal.h"

#include <stddef.h>

static int ana_runtime_quit_requested = 0;
static int ana_runtime_running = 0;
static ANA_RunStats ana_runtime_last_stats = { 0L, 0L, 1L, 0L };

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

int ana_run(const ANA_Game* game)
{
    ANA_Profile profile;
    ANA_Result result;
    ANA_Time time;
    long start_ticks;
    long end_ticks;

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
        ana_input_update();

        if (ana_quit_requested()) {
            ana_quit();
        }

        if (!ana_runtime_quit_requested && game->update != NULL) {
            game->update(time);
        }

        if (!ana_runtime_quit_requested && game->draw != NULL) {
            game->draw();
            ana_present();
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

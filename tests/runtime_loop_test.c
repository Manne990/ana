#include "ana.h"
#include "ana_internal.h"

#include <assert.h>

static int init_count = 0;
static int load_count = 0;
static int update_count = 0;
static int draw_count = 0;
static int shutdown_count = 0;
static int first_tick_fps = 0;

static void runtime_init(void)
{
    init_count++;
}

static void runtime_load(void)
{
    load_count++;
}

static void runtime_update(ANA_Time time)
{
    if (update_count == 0) {
        assert(time.tick == 0);
        first_tick_fps = time.fps;
    }

    update_count++;

    if (time.tick >= 2) {
        ana_quit();
    }
}

static void runtime_draw(void)
{
    draw_count++;
    ana_clear((unsigned char)draw_count);
}

static void runtime_shutdown(void)
{
    shutdown_count++;
}

static void reset_counts(void)
{
    init_count = 0;
    load_count = 0;
    update_count = 0;
    draw_count = 0;
    shutdown_count = 0;
    first_tick_fps = 0;
}

static ANA_Game make_game(void)
{
    ANA_Game game = {0};

    game.init = runtime_init;
    game.load = runtime_load;
    game.update = runtime_update;
    game.draw = runtime_draw;
    game.shutdown = runtime_shutdown;
    game.width = ANA_DEFAULT_WIDTH;
    game.height = ANA_DEFAULT_HEIGHT;
    game.fps = ANA_DEFAULT_FPS;
    game.colors = ANA_DEFAULT_COLORS;
    game.screen_mode = ANA_SCREEN_PAL_LORES;
    game.debug_stats = 0;

    return game;
}

static void test_runtime_loop(void)
{
    ANA_Game game;
    ANA_RunStats stats;

    reset_counts();
    game = make_game();

    assert(ana_run(&game) == ANA_OK);
    assert(init_count == 1);
    assert(load_count == 1);
    assert(update_count == 3);
    assert(draw_count == 2);
    assert(shutdown_count == 1);
    assert(first_tick_fps == ANA_DEFAULT_FPS);
    assert(!ana_gfx_is_open());

    stats = ana_last_run_stats();
    assert(stats.frames == 2);
    assert(stats.ticks_per_second > 0);
    assert(stats.perf_ticks_per_second > 0);
}

static void test_runtime_rejects_bad_profile(void)
{
    ANA_Game game;
    ANA_RunStats stats;

    reset_counts();
    game = make_game();
    game.colors = 32;

    assert(ana_run(&game) == ANA_ERROR_UNSUPPORTED_PROFILE);
    assert(init_count == 0);
    assert(load_count == 0);
    assert(update_count == 0);
    assert(draw_count == 0);
    assert(shutdown_count == 0);

    stats = ana_last_run_stats();
    assert(stats.frames == 0);
}

static void test_runtime_uses_default_profile_values(void)
{
    ANA_Game game;

    reset_counts();
    game = make_game();
    game.width = 0;
    game.height = 0;
    game.fps = 0;
    game.colors = 0;
    game.screen_mode = 0;

    assert(ana_run(&game) == ANA_OK);
    assert(update_count == 3);
    assert(first_tick_fps == ANA_DEFAULT_FPS);
}

int main(void)
{
    test_runtime_loop();
    test_runtime_rejects_bad_profile();
    test_runtime_uses_default_profile_values();
    assert(ana_run(0) == ANA_ERROR_INVALID_ARGUMENT);

    return 0;
}

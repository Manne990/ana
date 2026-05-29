#include "ana/ana.h"

#include <assert.h>

static void test_default_profile(void)
{
    const ANA_Profile* profile;

    profile = ana_default_profile();

    assert(profile != 0);
    assert(profile->width == ANA_DEFAULT_WIDTH);
    assert(profile->height == ANA_DEFAULT_HEIGHT);
    assert(profile->fps == ANA_DEFAULT_FPS);
    assert(profile->colors == ANA_DEFAULT_COLORS);
    assert(profile->bitplanes == ANA_DEFAULT_BITPLANES);
    assert(profile->screen_mode == ANA_SCREEN_PAL_LORES);
    assert(profile->render_mode == ANA_RENDER_DIRTY);
    assert(profile->target_flags == ANA_TARGET_OCS_ECS);
    assert(ana_validate_profile(profile) == ANA_OK);
    assert(ana_profile_is_supported(profile));
}

static void test_rejects_unsupported_profiles(void)
{
    ANA_Profile profile;

    profile = *ana_default_profile();
    profile.colors = 32;
    assert(ana_validate_profile(&profile) == ANA_ERROR_UNSUPPORTED_PROFILE);
    assert(!ana_profile_is_supported(&profile));

    profile = *ana_default_profile();
    profile.width = 640;
    assert(ana_validate_profile(&profile) == ANA_ERROR_UNSUPPORTED_PROFILE);

    profile = *ana_default_profile();
    profile.target_flags = 0u;
    assert(ana_validate_profile(&profile) == ANA_ERROR_UNSUPPORTED_PROFILE);

    profile = *ana_default_profile();
    profile.render_mode = (ANA_RenderMode)99;
    assert(ana_validate_profile(&profile) == ANA_ERROR_UNSUPPORTED_PROFILE);

    assert(ana_validate_profile(0) == ANA_ERROR_INVALID_ARGUMENT);
}

static void test_accepts_supported_render_modes(void)
{
    ANA_Profile profile;

    profile = *ana_default_profile();
    profile.render_mode = ANA_RENDER_FULL_FRAME;
    assert(ana_validate_profile(&profile) == ANA_OK);

    profile.render_mode = ANA_RENDER_TILE_SCROLL;
    assert(ana_validate_profile(&profile) == ANA_OK);

    profile.render_mode = ANA_RENDER_BLITTER_BOBS;
    assert(ana_validate_profile(&profile) == ANA_OK);

    profile.render_mode = ANA_RENDER_SIDE_SCROLL;
    assert(ana_validate_profile(&profile) == ANA_OK);

    profile.render_mode = ANA_RENDER_VERTICAL_SCROLL;
    assert(ana_validate_profile(&profile) == ANA_OK);

    profile.render_mode = ANA_RENDER_TILE_4WAY;
    assert(ana_validate_profile(&profile) == ANA_OK);

    profile.render_mode = ANA_RENDER_RAYCAST;
    assert(ana_validate_profile(&profile) == ANA_OK);
}

static void test_public_handle_names_are_available(void)
{
    ANA_Image image;
    ANA_Font font;
    ANA_Sound sound;
    ANA_Game game = {0};
    ANA_Time time;

    image = 0;
    font = 0;
    sound = 0;

    game.init = 0;
    game.load = 0;
    game.update = 0;
    game.draw = 0;
    game.shutdown = 0;
    game.width = ANA_DEFAULT_WIDTH;
    game.height = ANA_DEFAULT_HEIGHT;
    game.fps = ANA_DEFAULT_FPS;
    game.colors = ANA_DEFAULT_COLORS;
    game.screen_mode = ANA_SCREEN_PAL_LORES;
    game.render_mode = ANA_RENDER_DIRTY;
    game.debug_stats = 0;

    time.tick = 0;
    time.fps = ANA_DEFAULT_FPS;

    assert(image == 0);
    assert(font == 0);
    assert(sound == 0);
    assert(game.width == ANA_DEFAULT_WIDTH);
    assert(game.render_mode == ANA_RENDER_DIRTY);
    assert(time.fps == ANA_DEFAULT_FPS);
}

int main(void)
{
    test_default_profile();
    test_rejects_unsupported_profiles();
    test_accepts_supported_render_modes();
    test_public_handle_names_are_available();

    return 0;
}

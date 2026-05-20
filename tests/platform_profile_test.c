#include "ana.h"

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

    assert(ana_validate_profile(0) == ANA_ERROR_INVALID_ARGUMENT);
}

static void test_public_handle_names_are_available(void)
{
    ANA_Image image;
    ANA_Font font;
    ANA_Sound sound;
    ANA_Game game;
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

    time.tick = 0;
    time.fps = ANA_DEFAULT_FPS;

    assert(image == 0);
    assert(font == 0);
    assert(sound == 0);
    assert(game.width == ANA_DEFAULT_WIDTH);
    assert(time.fps == ANA_DEFAULT_FPS);
}

int main(void)
{
    test_default_profile();
    test_rejects_unsupported_profiles();
    test_public_handle_names_are_available();

    return 0;
}


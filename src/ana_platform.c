#include "ana/ana_platform.h"

#include <stddef.h>

static const ANA_Profile ana_ocs_ecs_pal_lores_profile = {
    ANA_DEFAULT_WIDTH,
    ANA_DEFAULT_HEIGHT,
    ANA_DEFAULT_FPS,
    ANA_DEFAULT_COLORS,
    ANA_DEFAULT_BITPLANES,
    ANA_SCREEN_PAL_LORES,
    ANA_TARGET_OCS_ECS
};

const ANA_Profile* ana_default_profile(void)
{
    return &ana_ocs_ecs_pal_lores_profile;
}

ANA_Result ana_validate_profile(const ANA_Profile* profile)
{
    if (profile == NULL) {
        return ANA_ERROR_INVALID_ARGUMENT;
    }

    if (profile->width != ANA_DEFAULT_WIDTH) {
        return ANA_ERROR_UNSUPPORTED_PROFILE;
    }

    if (profile->height != ANA_DEFAULT_HEIGHT) {
        return ANA_ERROR_UNSUPPORTED_PROFILE;
    }

    if (profile->fps != ANA_DEFAULT_FPS) {
        return ANA_ERROR_UNSUPPORTED_PROFILE;
    }

    if (profile->colors != ANA_DEFAULT_COLORS) {
        return ANA_ERROR_UNSUPPORTED_PROFILE;
    }

    if (profile->bitplanes != ANA_DEFAULT_BITPLANES) {
        return ANA_ERROR_UNSUPPORTED_PROFILE;
    }

    if (profile->screen_mode != ANA_SCREEN_PAL_LORES) {
        return ANA_ERROR_UNSUPPORTED_PROFILE;
    }

    if (profile->target_flags == 0u) {
        return ANA_ERROR_UNSUPPORTED_PROFILE;
    }

    if ((profile->target_flags & ~ANA_TARGET_OCS_ECS) != 0u) {
        return ANA_ERROR_UNSUPPORTED_PROFILE;
    }

    return ANA_OK;
}

int ana_profile_is_supported(const ANA_Profile* profile)
{
    return ana_validate_profile(profile) == ANA_OK;
}


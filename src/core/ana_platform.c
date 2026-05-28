#include "ana/ana_platform.h"

#include <stddef.h>

#ifdef ANA_TARGET_AMIGA
#include <dos/dos.h>
#include <exec/types.h>
#include <intuition/intuitionbase.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#else
#include <time.h>
#endif

static const ANA_Profile ana_ocs_ecs_pal_lores_profile = {
    ANA_DEFAULT_WIDTH,
    ANA_DEFAULT_HEIGHT,
    ANA_DEFAULT_FPS,
    ANA_DEFAULT_COLORS,
    ANA_DEFAULT_BITPLANES,
    ANA_SCREEN_PAL_LORES,
    ANA_RENDER_DIRTY,
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

    switch (profile->render_mode) {
        case ANA_RENDER_DIRTY:
        case ANA_RENDER_FULL_FRAME:
        case ANA_RENDER_TILE_SCROLL:
        case ANA_RENDER_BLITTER_BOBS:
            break;
        default:
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

long ana_platform_time_ticks(void)
{
#ifdef ANA_TARGET_AMIGA
    struct DateStamp stamp;

    DateStamp(&stamp);
    return (stamp.ds_Days * 24L * 60L * 50L) +
        (stamp.ds_Minute * 60L * 50L) +
        stamp.ds_Tick;
#else
    return (long)clock();
#endif
}

long ana_platform_time_ticks_per_second(void)
{
#ifdef ANA_TARGET_AMIGA
    return 50L;
#else
    if ((long)CLOCKS_PER_SEC <= 0L) {
        return 1L;
    }

    return (long)CLOCKS_PER_SEC;
#endif
}

unsigned long ana_platform_perf_ticks(void)
{
#ifdef ANA_TARGET_AMIGA
    ULONG seconds;
    ULONG micros;

    if (IntuitionBase != NULL) {
        CurrentTime(&seconds, &micros);
        return (seconds * 1000000UL) + micros;
    }

    return (unsigned long)ana_platform_time_ticks() * 20000UL;
#else
    return (unsigned long)clock();
#endif
}

unsigned long ana_platform_perf_ticks_per_second(void)
{
#ifdef ANA_TARGET_AMIGA
    return 1000000UL;
#else
    if ((long)CLOCKS_PER_SEC <= 0L) {
        return 1UL;
    }

    return (unsigned long)CLOCKS_PER_SEC;
#endif
}

void ana_platform_wait_until_time_tick(long target_tick)
{
#ifdef ANA_TARGET_AMIGA
    while (ana_platform_time_ticks() < target_tick) {
        WaitTOF();
    }
#else
    (void)target_tick;
#endif
}

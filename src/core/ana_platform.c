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
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#endif

#ifndef ANA_TARGET_AMIGA
#define ANA_HOST_TICKS_PER_SECOND 1000000L

static int ana_host_clock_started = 0;
static long ana_host_clock_start_seconds = 0L;
static long ana_host_clock_start_micros = 0L;
static long ana_host_clock_last_tick = 0L;

static long ana_host_time_ticks(void)
{
    struct timeval now;
    long ticks;

    if (gettimeofday(&now, NULL) != 0) {
        return ana_host_clock_last_tick;
    }

    if (!ana_host_clock_started) {
        ana_host_clock_started = 1;
        ana_host_clock_start_seconds = (long)now.tv_sec;
        ana_host_clock_start_micros = (long)now.tv_usec;
    }

    ticks = ((long)now.tv_sec - ana_host_clock_start_seconds) *
        ANA_HOST_TICKS_PER_SECOND;
    ticks += (long)now.tv_usec - ana_host_clock_start_micros;
    if (ticks < ana_host_clock_last_tick) {
        ticks = ana_host_clock_last_tick;
    }
    ana_host_clock_last_tick = ticks;
    return ticks;
}
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
        case ANA_RENDER_SIDE_SCROLL:
        case ANA_RENDER_VERTICAL_SCROLL:
        case ANA_RENDER_TILE_4WAY:
        case ANA_RENDER_RAYCAST:
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
    return ana_host_time_ticks();
#endif
}

long ana_platform_time_ticks_per_second(void)
{
#ifdef ANA_TARGET_AMIGA
    return 50L;
#else
    return ANA_HOST_TICKS_PER_SECOND;
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
    return (unsigned long)ana_host_time_ticks();
#endif
}

unsigned long ana_platform_perf_ticks_per_second(void)
{
#ifdef ANA_TARGET_AMIGA
    return 1000000UL;
#else
    return (unsigned long)ANA_HOST_TICKS_PER_SECOND;
#endif
}

void ana_platform_wait_until_time_tick(long target_tick)
{
#ifdef ANA_TARGET_AMIGA
    while (ana_platform_time_ticks() < target_tick) {
        WaitTOF();
    }
#else
    long remaining;
    struct timeval delay;

    if (getenv("ANA_HOST_UNPACED") != NULL) {
        return;
    }

    remaining = target_tick - ana_host_time_ticks();
    while (remaining > 0L) {
        delay.tv_sec = (long)(remaining / ANA_HOST_TICKS_PER_SECOND);
        delay.tv_usec = (long)(remaining % ANA_HOST_TICKS_PER_SECOND);
        select(0, NULL, NULL, NULL, &delay);
        remaining = target_tick - ana_host_time_ticks();
    }
#endif
}

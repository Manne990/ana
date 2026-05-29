#ifndef ANA_PLATFORM_H
#define ANA_PLATFORM_H

#include "ana_result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ANA_DEFAULT_WIDTH 320
#define ANA_DEFAULT_HEIGHT 256
#define ANA_DEFAULT_FPS 50
#define ANA_DEFAULT_COLORS 16
#define ANA_DEFAULT_BITPLANES 4

#define ANA_TARGET_OCS 0x01u
#define ANA_TARGET_ECS 0x02u
#define ANA_TARGET_OCS_ECS (ANA_TARGET_OCS | ANA_TARGET_ECS)

typedef enum ANA_ScreenMode {
    ANA_SCREEN_PAL_LORES = 1
} ANA_ScreenMode;

typedef enum ANA_RenderMode {
    ANA_RENDER_DEFAULT = 0,
    ANA_RENDER_DIRTY = 1,
    ANA_RENDER_FULL_FRAME = 2,
    ANA_RENDER_TILE_SCROLL = 3,
    ANA_RENDER_BLITTER_BOBS = 4,
    ANA_RENDER_SIDE_SCROLL = 5,
    ANA_RENDER_VERTICAL_SCROLL = 6,
    ANA_RENDER_TILE_4WAY = 7,
    ANA_RENDER_RAYCAST = 8
} ANA_RenderMode;

typedef struct ANA_Profile {
    int width;
    int height;
    int fps;
    int colors;
    int bitplanes;
    ANA_ScreenMode screen_mode;
    ANA_RenderMode render_mode;
    unsigned int target_flags;
} ANA_Profile;

const ANA_Profile* ana_default_profile(void);
ANA_Result ana_validate_profile(const ANA_Profile* profile);
int ana_profile_is_supported(const ANA_Profile* profile);

#ifdef __cplusplus
}
#endif

#endif

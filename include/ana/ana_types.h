#ifndef ANA_TYPES_H
#define ANA_TYPES_H

#include "ana_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ANA_ImageData* ANA_Image;
typedef struct ANA_FontData* ANA_Font;
typedef struct ANA_SoundData* ANA_Sound;
typedef struct ANA_MusicData* ANA_Music;

typedef struct ANA_Time {
    int tick;
    int fps;
} ANA_Time;

typedef struct ANA_Game {
    void (*init)(void);
    void (*load)(void);
    void (*update)(ANA_Time time);
    void (*draw)(void);
    void (*shutdown)(void);

    int width;
    int height;
    int fps;
    int colors;
    ANA_ScreenMode screen_mode;
} ANA_Game;

#ifdef __cplusplus
}
#endif

#endif

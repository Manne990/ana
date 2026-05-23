#ifndef ANA_SOUND_H
#define ANA_SOUND_H

#include "ana_types.h"

#ifdef __cplusplus
extern "C" {
#endif

ANA_Sound ana_load_sound(const char* path);
ANA_Sound ana_load_sound_data(const unsigned char* bytes, long size);
void ana_free_sound(ANA_Sound sound);
void ana_play_sound(ANA_Sound sound);
void ana_stop_all_sounds(void);
void ana_set_sound_volume(int volume);

#ifdef __cplusplus
}
#endif

#endif

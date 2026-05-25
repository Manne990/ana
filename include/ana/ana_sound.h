#ifndef ANA_SOUND_H
#define ANA_SOUND_H

#include "ana_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char ANA_AudioChannels;

#define ANA_AUDIO_CH_0 0x01u
#define ANA_AUDIO_CH_1 0x02u
#define ANA_AUDIO_CH_2 0x04u
#define ANA_AUDIO_CH_3 0x08u
#define ANA_AUDIO_ALL_CHANNELS 0x0fu

#define ANA_MUSIC_ONCE 0
#define ANA_MUSIC_LOOP 1

typedef struct ANA_AudioConfig {
    ANA_AudioChannels music_channels;
    ANA_AudioChannels sfx_channels;
    int sfx_can_steal_music;
    int music_can_use_free_sfx_channels;
} ANA_AudioConfig;

void ana_configure_audio(const ANA_AudioConfig* config);
ANA_AudioConfig ana_audio_config(void);

ANA_Sound ana_load_sound(const char* path);
ANA_Sound ana_load_sound_data(const unsigned char* bytes, long size);
void ana_free_sound(ANA_Sound sound);
void ana_play_sound(ANA_Sound sound);
void ana_stop_all_sounds(void);
void ana_set_sound_volume(int volume);

ANA_Music ana_load_music(const char* path);
ANA_Music ana_load_music_data(const unsigned char* bytes, long size);
void ana_free_music(ANA_Music music);
void ana_play_music(ANA_Music music, int loop);
void ana_stop_music(void);
void ana_pause_music(void);
void ana_resume_music(void);
void ana_set_music_volume(int volume);

#ifdef __cplusplus
}
#endif

#endif

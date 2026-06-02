#include "byte_brothers_assets.h"

#include <stddef.h>

/* Audio setup for the Byte Brothers platform sample. */

#ifdef ANA_TARGET_AMIGA
#define BB_ASSET_ROOT "assets/"
#else
#define BB_ASSET_ROOT "build/assets/byte_brothers/assets/"
#endif

static ANA_Sound bb_jump_sound = NULL;
static ANA_Sound bb_collect_sound = NULL;
static ANA_Sound bb_power_sound = NULL;
static ANA_Sound bb_hit_sound = NULL;
static ANA_Sound bb_goal_sound = NULL;
static ANA_Music bb_theme_music = NULL;
static int bb_loaded = 0;

static void bb_build_asset_path(char* path, const char* name, int capacity)
{
    int pos;

    pos = 0;
    while (BB_ASSET_ROOT[pos] != '\0' && pos < capacity - 1) {
        path[pos] = BB_ASSET_ROOT[pos];
        pos++;
    }

    while (*name != '\0' && pos < capacity - 1) {
        path[pos] = *name;
        name++;
        pos++;
    }

    path[pos] = '\0';
}

static ANA_Sound bb_load_sound_file(const char* name)
{
    char path[96];

    bb_build_asset_path(path, name, (int)sizeof(path));
    return ana_load_sound(path);
}

static ANA_Music bb_load_music_file(const char* name)
{
    char path[96];

    bb_build_asset_path(path, name, (int)sizeof(path));
    return ana_load_music(path);
}

void bb_assets_load(void)
{
    ANA_AudioConfig config;

    bb_loaded = 0;

    config.music_channels = ANA_AUDIO_CH_0 | ANA_AUDIO_CH_1;
    config.sfx_channels = ANA_AUDIO_CH_2 | ANA_AUDIO_CH_3;
    config.sfx_can_steal_music = 0;
    config.music_can_use_free_sfx_channels = 0;
    ana_configure_audio(&config);
    ana_set_music_volume(8);
    ana_set_sound_volume(64);

    bb_jump_sound = bb_load_sound_file("jump.anasnd");
    bb_collect_sound = bb_load_sound_file("collect.anasnd");
    bb_power_sound = bb_load_sound_file("power.anasnd");
    bb_hit_sound = bb_load_sound_file("hit.anasnd");
    bb_goal_sound = bb_load_sound_file("goal.anasnd");
    bb_theme_music = bb_load_music_file("theme.mod");

    if (bb_theme_music != NULL) {
        ana_play_music(bb_theme_music, ANA_MUSIC_LOOP);
    }

    bb_loaded = bb_jump_sound != NULL &&
        bb_collect_sound != NULL &&
        bb_power_sound != NULL &&
        bb_hit_sound != NULL &&
        bb_goal_sound != NULL &&
        bb_theme_music != NULL;
}

void bb_assets_unload(void)
{
    ana_stop_music();
    ana_stop_all_sounds();

    if (bb_jump_sound != NULL) {
        ana_free_sound(bb_jump_sound);
    }
    if (bb_collect_sound != NULL) {
        ana_free_sound(bb_collect_sound);
    }
    if (bb_power_sound != NULL) {
        ana_free_sound(bb_power_sound);
    }
    if (bb_hit_sound != NULL) {
        ana_free_sound(bb_hit_sound);
    }
    if (bb_goal_sound != NULL) {
        ana_free_sound(bb_goal_sound);
    }
    if (bb_theme_music != NULL) {
        ana_free_music(bb_theme_music);
    }

    bb_jump_sound = NULL;
    bb_collect_sound = NULL;
    bb_power_sound = NULL;
    bb_hit_sound = NULL;
    bb_goal_sound = NULL;
    bb_theme_music = NULL;
}

void bb_assets_play_jump(void)
{
    if (bb_jump_sound != NULL) {
        ana_play_sound(bb_jump_sound);
    }
}

void bb_assets_play_collect(void)
{
    if (bb_collect_sound != NULL) {
        ana_play_sound(bb_collect_sound);
    }
}

void bb_assets_play_power(void)
{
    if (bb_power_sound != NULL) {
        ana_play_sound(bb_power_sound);
    }
}

void bb_assets_play_hit(void)
{
    if (bb_hit_sound != NULL) {
        ana_play_sound(bb_hit_sound);
    }
}

void bb_assets_play_goal(void)
{
    if (bb_goal_sound != NULL) {
        ana_play_sound(bb_goal_sound);
    }
}

int bb_assets_loaded(void)
{
    return bb_loaded;
}

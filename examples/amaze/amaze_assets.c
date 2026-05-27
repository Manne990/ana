/* Asset loading and ownership for AMAze. Keeps file paths, audio channel
   setup, music volume, SFX volume, and resource cleanup in one module. */

#include "amaze_assets.h"

#ifdef ANA_TARGET_AMIGA
#define AMAZE_ASSET_ROOT "assets/"
#else
#define AMAZE_ASSET_ROOT "build/assets/amaze/assets/"
#endif

#define AMAZE_ASSET_PATH(name) AMAZE_ASSET_ROOT name

static int amaze_assets_loaded = 0;
ANA_Image amaze_businessman_image = 0;
ANA_Image amaze_taxman_red_image = 0;
ANA_Image amaze_taxman_pink_image = 0;
ANA_Image amaze_taxman_cyan_image = 0;
ANA_Image amaze_taxman_blue_image = 0;
ANA_Image amaze_coin_image = 0;
ANA_Image amaze_gold_bag_image = 0;
static ANA_Sound amaze_coin_sound = 0;
static ANA_Sound amaze_bag_sound = 0;
static ANA_Sound amaze_death_sound = 0;
static ANA_Sound amaze_yippie_sound = 0;
static ANA_Music amaze_theme_music = 0;

int amaze_load_assets(void)
{
    ANA_AudioConfig audio;

    amaze_free_assets();
    amaze_assets_loaded = 0;

    audio.music_channels = ANA_AUDIO_CH_0 | ANA_AUDIO_CH_1;
    audio.sfx_channels = ANA_AUDIO_CH_2 | ANA_AUDIO_CH_3;
    audio.sfx_can_steal_music = 0;
    audio.music_can_use_free_sfx_channels = 0;
    ana_configure_audio(&audio);
    ana_set_sound_volume(64);
    ana_set_music_volume(8);

    amaze_businessman_image = ana_load_image(AMAZE_ASSET_PATH("businessman.anaimg"));
    amaze_taxman_red_image = ana_load_image(AMAZE_ASSET_PATH("taxman_red.anaimg"));
    amaze_taxman_pink_image = ana_load_image(AMAZE_ASSET_PATH("taxman_pink.anaimg"));
    amaze_taxman_cyan_image = ana_load_image(AMAZE_ASSET_PATH("taxman_cyan.anaimg"));
    amaze_taxman_blue_image = ana_load_image(AMAZE_ASSET_PATH("taxman_blue.anaimg"));
    amaze_coin_image = ana_load_image(AMAZE_ASSET_PATH("coin.anaimg"));
    amaze_gold_bag_image = ana_load_image(AMAZE_ASSET_PATH("gold_bag.anaimg"));
    amaze_coin_sound = ana_load_sound(AMAZE_ASSET_PATH("coin.anasnd"));
    amaze_bag_sound = ana_load_sound(AMAZE_ASSET_PATH("bag.anasnd"));
    amaze_death_sound = ana_load_sound(AMAZE_ASSET_PATH("death.anasnd"));
    amaze_yippie_sound = ana_load_sound(AMAZE_ASSET_PATH("yippie.anasnd"));
    amaze_theme_music = ana_load_music(AMAZE_ASSET_PATH("theme.mod"));

    amaze_assets_loaded = (amaze_businessman_image != 0 &&
            amaze_taxman_red_image != 0 &&
            amaze_taxman_pink_image != 0 &&
            amaze_taxman_cyan_image != 0 &&
            amaze_taxman_blue_image != 0 &&
            amaze_coin_image != 0 &&
            amaze_gold_bag_image != 0 &&
            amaze_coin_sound != 0 &&
            amaze_bag_sound != 0 &&
            amaze_death_sound != 0 &&
            amaze_yippie_sound != 0 &&
            amaze_theme_music != 0);

    if (amaze_theme_music != 0) {
        ana_play_music(amaze_theme_music, ANA_MUSIC_LOOP);
    }

    return amaze_assets_loaded;
}

int amaze_assets_are_loaded(void)
{
    return amaze_assets_loaded;
}

void amaze_free_assets(void)
{
    ana_stop_music();
    ana_free_music(amaze_theme_music);
    ana_free_sound(amaze_yippie_sound);
    ana_free_sound(amaze_death_sound);
    ana_free_sound(amaze_bag_sound);
    ana_free_sound(amaze_coin_sound);
    ana_free_image(amaze_gold_bag_image);
    ana_free_image(amaze_coin_image);
    ana_free_image(amaze_taxman_blue_image);
    ana_free_image(amaze_taxman_cyan_image);
    ana_free_image(amaze_taxman_pink_image);
    ana_free_image(amaze_taxman_red_image);
    ana_free_image(amaze_businessman_image);

    amaze_theme_music = 0;
    amaze_yippie_sound = 0;
    amaze_death_sound = 0;
    amaze_bag_sound = 0;
    amaze_coin_sound = 0;
    amaze_gold_bag_image = 0;
    amaze_coin_image = 0;
    amaze_taxman_blue_image = 0;
    amaze_taxman_cyan_image = 0;
    amaze_taxman_pink_image = 0;
    amaze_taxman_red_image = 0;
    amaze_businessman_image = 0;
}

void amaze_play_coin_sound(void)
{
    if (amaze_coin_sound != 0) {
        ana_play_sound(amaze_coin_sound);
    }
}

void amaze_play_bag_sound(void)
{
    if (amaze_bag_sound != 0) {
        ana_play_sound(amaze_bag_sound);
    }
}

void amaze_play_death_sound(void)
{
    if (amaze_death_sound != 0) {
        ana_play_sound(amaze_death_sound);
    }
}

void amaze_play_yippie_sound(void)
{
    if (amaze_yippie_sound != 0) {
        ana_play_sound(amaze_yippie_sound);
    }
}

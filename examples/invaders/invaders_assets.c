#include "invaders_assets.h"

#ifdef ANA_TARGET_AMIGA
#define INVADERS_ASSET_ROOT "assets/"
#else
#define INVADERS_ASSET_ROOT "build/assets/invaders/assets/"
#endif

#define INVADERS_ASSET_PATH(name) INVADERS_ASSET_ROOT name

ANA_Image player_image = 0;
ANA_Image bullet_image = 0;
ANA_Image invader_image = 0;
ANA_Image explosion_image = 0;
ANA_Font hud_font = 0;
ANA_Sound fire_sound = 0;
ANA_Sound explosion_sound = 0;
ANA_Sound step_sound = 0;
ANA_Sound game_over_sound = 0;
ANA_Music theme_music = 0;

int invaders_load_assets(void)
{
    ANA_AudioConfig audio;

    invaders_free_assets();

    audio.music_channels = ANA_AUDIO_CH_0 | ANA_AUDIO_CH_1;
    audio.sfx_channels = ANA_AUDIO_CH_2 | ANA_AUDIO_CH_3;
    audio.sfx_can_steal_music = 0;
    audio.music_can_use_free_sfx_channels = 0;
    ana_configure_audio(&audio);

    ana_set_sound_volume(52);

    player_image = ana_load_image(INVADERS_ASSET_PATH("player.anaimg"));
    bullet_image = ana_load_image(INVADERS_ASSET_PATH("bullet.anaimg"));
    invader_image = ana_load_image(INVADERS_ASSET_PATH("invader.anaimg"));
    explosion_image = ana_load_image(INVADERS_ASSET_PATH("explosion.anaimg"));
    hud_font = ana_load_font(INVADERS_ASSET_PATH("hud.anafnt"));
    fire_sound = ana_load_sound(INVADERS_ASSET_PATH("fire.anasnd"));
    explosion_sound = ana_load_sound(INVADERS_ASSET_PATH("explosion.anasnd"));
    step_sound = ana_load_sound(INVADERS_ASSET_PATH("step.anasnd"));
    game_over_sound = ana_load_sound(INVADERS_ASSET_PATH("gameover.anasnd"));
    theme_music = ana_load_music(INVADERS_ASSET_PATH("theme.mod"));

    if (hud_font != 0) {
        ana_set_font_color(hud_font, 5);
    }

    if (player_image == 0 ||
            bullet_image == 0 ||
            invader_image == 0 ||
            explosion_image == 0 ||
            hud_font == 0 ||
            fire_sound == 0 ||
            explosion_sound == 0 ||
            step_sound == 0 ||
            game_over_sound == 0) {
        invaders_free_assets();
        return 0;
    }

    if (theme_music != 0) {
        ana_set_music_volume(38);
        ana_play_music(theme_music, ANA_MUSIC_LOOP);
    }

    return 1;
}

void invaders_free_assets(void)
{
    ana_stop_music();
    ana_free_music(theme_music);
    ana_free_sound(game_over_sound);
    ana_free_sound(step_sound);
    ana_free_sound(explosion_sound);
    ana_free_sound(fire_sound);
    ana_free_font(hud_font);
    ana_free_image(explosion_image);
    ana_free_image(invader_image);
    ana_free_image(bullet_image);
    ana_free_image(player_image);

    theme_music = 0;
    game_over_sound = 0;
    step_sound = 0;
    explosion_sound = 0;
    fire_sound = 0;
    hud_font = 0;
    explosion_image = 0;
    invader_image = 0;
    bullet_image = 0;
    player_image = 0;
}

#ifndef INVADERS_ASSETS_H
#define INVADERS_ASSETS_H

#include "ana.h"

extern ANA_Image player_image;
extern ANA_Image bullet_image;
extern ANA_Image invader_image;
extern ANA_Image explosion_image;
extern ANA_Font hud_font;
extern ANA_Sound fire_sound;
extern ANA_Sound explosion_sound;
extern ANA_Sound step_sound;
extern ANA_Sound game_over_sound;
extern ANA_Music theme_music;

int invaders_load_assets(void);
void invaders_free_assets(void);

#endif

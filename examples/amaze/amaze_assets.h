/* Public asset loading, playback, and cleanup helpers for the AMAze sample. */

#ifndef AMAZE_ASSETS_H
#define AMAZE_ASSETS_H

#include "ana.h"

extern ANA_Image amaze_businessman_image;
extern ANA_Image amaze_taxman_red_image;
extern ANA_Image amaze_taxman_pink_image;
extern ANA_Image amaze_taxman_cyan_image;
extern ANA_Image amaze_taxman_blue_image;
extern ANA_Image amaze_coin_image;
extern ANA_Image amaze_gold_bag_image;

int amaze_load_assets(void);
int amaze_assets_are_loaded(void);
void amaze_free_assets(void);

void amaze_play_coin_sound(void);
void amaze_play_bag_sound(void);
void amaze_play_death_sound(void);
void amaze_play_yippie_sound(void);

#endif

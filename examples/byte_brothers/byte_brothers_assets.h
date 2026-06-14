#ifndef BYTE_BROTHERS_ASSETS_H
#define BYTE_BROTHERS_ASSETS_H

/* Loads and owns Byte Brothers image and audio assets. */

#include "ana.h"

extern ANA_Image bb_player_image;
extern ANA_Image bb_enemy_image;

void bb_assets_load(void);
void bb_assets_unload(void);
void bb_assets_play_jump(void);
void bb_assets_play_collect(void);
void bb_assets_play_power(void);
void bb_assets_play_hit(void);
void bb_assets_play_goal(void);
void bb_assets_play_plopp(void);
int bb_assets_loaded(void);

#endif

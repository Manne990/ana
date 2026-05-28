#ifndef BYTE_BROTHERS_ASSETS_H
#define BYTE_BROTHERS_ASSETS_H

/* Loads and owns Byte Brothers audio assets. */

#include "ana.h"

void bb_assets_load(void);
void bb_assets_unload(void);
void bb_assets_play_jump(void);
void bb_assets_play_collect(void);
void bb_assets_play_power(void);
void bb_assets_play_hit(void);
void bb_assets_play_goal(void);
int bb_assets_loaded(void);

#endif

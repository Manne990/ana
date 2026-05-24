#ifndef INVADERS_GAME_H
#define INVADERS_GAME_H

#include "ana.h"

void invaders_init(void);
void invaders_load(void);
void invaders_update(ANA_Time time);
void invaders_draw(void);
void invaders_shutdown(void);

void invaders_print_run_summary(void);
#ifdef ANA_INVADERS_DEBUG_STATS
void invaders_print_run_stats(void);
#endif

int invaders_player_x(void);
int invaders_score(void);
int invaders_remaining_count(void);
int invaders_assets_are_loaded(void);

#endif

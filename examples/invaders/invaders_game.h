/* Public lifecycle callbacks for registering ANA Invaders with the ANA runtime. */

#ifndef INVADERS_GAME_H
#define INVADERS_GAME_H

#include "ana.h"

void invaders_init(void);
void invaders_load(void);
void invaders_update(ANA_Time time);
void invaders_draw(void);
void invaders_shutdown(void);

int invaders_assets_are_loaded(void);

#endif

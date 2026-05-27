/* Public lifecycle callbacks for registering AMAze with the ANA runtime. */

#ifndef AMAZE_GAME_H
#define AMAZE_GAME_H

#include "ana.h"

void amaze_init(void);
void amaze_load(void);
void amaze_update(ANA_Time time);
void amaze_draw(void);
void amaze_shutdown(void);

int amaze_assets_are_loaded(void);

#endif

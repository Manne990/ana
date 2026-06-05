#ifndef BYTE_BROTHERS_GAME_H
#define BYTE_BROTHERS_GAME_H

/* ANA callbacks for Byte Brothers gameplay and lifecycle. */

#include "ana.h"

void byte_brothers_init(void);
void byte_brothers_load(void);
void byte_brothers_update(ANA_Time time);
void byte_brothers_draw(void);
void byte_brothers_shutdown(void);

#endif

/* Shared internal state, constants, and helper declarations for AMAze gameplay
   and rendering modules. Not part of the public example API. */

#ifndef AMAZE_INTERNAL_H
#define AMAZE_INTERNAL_H

#include "ana.h"
#include "amaze_assets.h"
#include "amaze_game.h"

#define AMAZE_MAP_W 21
#define AMAZE_MAP_H 17
#define AMAZE_TILE 8
#define AMAZE_ORIGIN_X 76
#define AMAZE_ORIGIN_Y 48
#define AMAZE_PLAYER_STEP_TICKS 20
#define AMAZE_CHASER_STEP_TICKS 32
#define AMAZE_SMOKE_TICKS 900
#define AMAZE_CHASERS 3
#define AMAZE_POWER_TICKS 600
#define AMAZE_DOT_SCORE 1
#define AMAZE_POWER_SCORE 10
#define AMAZE_CAPTURE_SCORE 20
#define AMAZE_CHASER_FLEE_COLOR 7
#define AMAZE_SUIT_COLOR 8
#define AMAZE_SKIN_COLOR 9
#define AMAZE_TIE_COLOR 10
#define AMAZE_COLLISION_NONE 0
#define AMAZE_COLLISION_CAPTURE 1
#define AMAZE_COLLISION_DEATH 2

typedef struct AMAzeActor {
    int tx;
    int ty;
    unsigned char color;
} AMAzeActor;

extern char amaze_map[AMAZE_MAP_H][AMAZE_MAP_W + 1];
extern AMAzeActor amaze_player;
extern AMAzeActor amaze_chasers[AMAZE_CHASERS];
extern int amaze_score;
extern int amaze_lives;
extern int amaze_power_ticks;

void amaze_render_request_full_redraw(void);

#endif

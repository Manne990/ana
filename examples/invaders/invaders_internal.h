/* Shared internal state, constants, and helper declarations for the ANA
   Invaders gameplay and renderer modules. Not part of the public example API. */

#ifndef INVADERS_INTERNAL_H
#define INVADERS_INTERNAL_H

#include "ana.h"
#include "invaders_assets.h"
#include "invaders_game.h"

#include <stdio.h>
#include <string.h>

#define PLAYER_SPEED 2
#define PLAYER_BULLET_SPEED 4
#define PLAYER_BULLET_SLOTS 4
#define ALIEN_BULLET_SPEED 2
#define ALIEN_BULLET_SLOTS 6
#define GAME_TICK_LIMIT 3000

#define INVADER_COLUMNS 10
#define INVADER_ROWS 5
#define INVADER_COUNT (INVADER_COLUMNS * INVADER_ROWS)
#define INVADER_WIDTH 16
#define INVADER_HEIGHT 8
#define INVADER_FRAMES 2
#define INVADER_SPACING_X 22
#define INVADER_SPACING_Y 16
#define INVADER_FORMATION_WIDTH \
    (INVADER_WIDTH + ((INVADER_COLUMNS - 1) * INVADER_SPACING_X))
#define INVADER_FORMATION_HEIGHT \
    (INVADER_HEIGHT + ((INVADER_ROWS - 1) * INVADER_SPACING_Y))
#define INVADER_START_X 52
#define INVADER_START_Y 54
#define INVADER_STEP_PIXELS 2
#define INVADER_DROP_PIXELS 8
#define INVADER_MIN_X 8
#define INVADER_MAX_X (ANA_DEFAULT_WIDTH - 8)
#define INVADER_BASE_INTERVAL 24
#define INVADER_MIN_INTERVAL 5
#define ALIEN_FIRE_BASE_INTERVAL 45
#define ALIEN_FIRE_MIN_INTERVAL 10

#define EXPLOSION_WIDTH 16
#define EXPLOSION_HEIGHT 10
#define EXPLOSION_FRAMES 2
#define EXPLOSION_SLOTS 4
#define EXPLOSION_TICKS_PER_FRAME 5
#define EXPLOSION_TICKS (EXPLOSION_FRAMES * EXPLOSION_TICKS_PER_FRAME)

#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_DIRECT_PRESENT)
#define INVADERS_DRAW_SLOTS 1
#else
#define INVADERS_DRAW_SLOTS 2
#endif
#define INVADERS_REMOVED_ENEMY_SLOTS INVADER_COUNT
#define HUD_SCORE_X 16
#define HUD_SCORE_Y 16
#define HUD_LIVES_X 200
#define HUD_LIVES_Y 16
#define HUD_STATUS_X 16
#define HUD_STATUS_Y 30
#define HUD_FONT_HEIGHT 7
#define HUD_SCORE_CLEAR_WIDTH 90
#define HUD_LIVES_CLEAR_WIDTH 70
#define HUD_STATUS_CLEAR_WIDTH 150

#define SHIELD_COUNT 4
#define SHIELD_COLUMNS 8
#define SHIELD_ROWS 5
#define SHIELD_CELL_SIZE 4
#define SHIELD_WIDTH (SHIELD_COLUMNS * SHIELD_CELL_SIZE)
#define SHIELD_HEIGHT (SHIELD_ROWS * SHIELD_CELL_SIZE)
#define SHIELD_START_X 30
#define SHIELD_Y 178
#define SHIELD_SPACING_X 72
#define SHIELD_COLOR 3
#define SHIELD_DIRTY_ALL ((1 << SHIELD_COUNT) - 1)

typedef enum InvadersGameState {
    INVADERS_STATE_TITLE,
    INVADERS_STATE_PLAYING,
    INVADERS_STATE_CLEAR,
    INVADERS_STATE_GAME_OVER
} InvadersGameState;

typedef struct InvadersExplosion {
    int active;
    int x;
    int y;
    int age;
} InvadersExplosion;

typedef struct InvadersBullet {
    int active;
    int x;
    int y;
} InvadersBullet;

typedef struct InvadersDrawSlot {
    ANA_Bob player_bob;
    InvadersBullet bullets[PLAYER_BULLET_SLOTS];
    InvadersBullet alien_bullets[ALIEN_BULLET_SLOTS];
    InvadersExplosion explosions[EXPLOSION_SLOTS];
    ANA_Label score_label;
    ANA_Label lives_label;
    ANA_Label status_label;
    int hud_labels_ready;
} InvadersDrawSlot;

extern unsigned char invader_alive[INVADER_ROWS][INVADER_COLUMNS];
extern InvadersExplosion explosions[EXPLOSION_SLOTS];
extern InvadersBullet player_bullets[PLAYER_BULLET_SLOTS];
extern InvadersBullet alien_bullets[ALIEN_BULLET_SLOTS];
extern InvadersDrawSlot draw_slots[INVADERS_DRAW_SLOTS];
extern ANA_Rect removed_enemy_rects[INVADERS_DRAW_SLOTS][INVADERS_REMOVED_ENEMY_SLOTS];
extern int removed_enemy_counts[INVADERS_DRAW_SLOTS];
extern int formation_dirty_slots[INVADERS_DRAW_SLOTS];
extern int formation_drawn_slots[INVADERS_DRAW_SLOTS];
extern int formation_slot_x[INVADERS_DRAW_SLOTS];
extern int formation_slot_y[INVADERS_DRAW_SLOTS];
extern int formation_row_min_slots[INVADERS_DRAW_SLOTS][INVADER_ROWS];
extern int formation_row_max_slots[INVADERS_DRAW_SLOTS][INVADER_ROWS];
extern int shield_dirty_slots[INVADERS_DRAW_SLOTS];
extern int full_clear_slots[INVADERS_DRAW_SLOTS];
extern unsigned char shield_cells[SHIELD_COUNT][SHIELD_ROWS][SHIELD_COLUMNS];
extern int player_x;
extern int player_y;
extern int invader_formation_x;
extern int invader_formation_y;
extern int invader_direction;
extern int invader_frame;
extern ANA_Timer invader_move_timer;
extern ANA_Timer alien_fire_timer;
extern int invaders_remaining;
extern int score;
extern int lives;
extern InvadersGameState game_state;
extern int demo_ticks;
extern int invaders_assets_loaded;
extern unsigned long invaders_rng_state;

int invaders_shield_x(int shield);
ANA_Rect invaders_shield_cell_rect(int shield, int row, int col);
ANA_Rect invaders_shield_rect(int shield);
int invaders_enemy_x(int col);
int invaders_enemy_y(int row);
int invaders_rect_overlaps_alive_shield(ANA_Rect rect);
const char* invaders_status_text(void);

#endif

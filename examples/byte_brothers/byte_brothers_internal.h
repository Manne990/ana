#ifndef BYTE_BROTHERS_INTERNAL_H
#define BYTE_BROTHERS_INTERNAL_H

/* Shared state and constants for the Byte Brothers platform sample. */

#include "ana.h"

#define BB_SCREEN_W 320
#define BB_SCREEN_H 256
#define BB_HUD_H 16
#define BB_TILE 16
#define BB_MAP_W 64
#define BB_MAP_H 16
#define BB_LEVEL_COUNT 4
#define BB_MAX_ENEMIES 8
#define BB_CAMERA_SNAP_X 1
#define BB_CAMERA_SNAP_Y 1
#define BB_PLAYER_W 20
#define BB_PLAYER_H 28
#define BB_ENEMY_W 24
#define BB_ENEMY_H 24
#define BB_PLAYER_SPEED 2
#define BB_DASH_SPEED 5
#define BB_DASH_TICKS 8
#define BB_DASH_JUMP_TICKS 10
#define BB_JUMP_SPEED -11
#define BB_DASH_JUMP_SPEED -13
#define BB_STOMP_BOUNCE_SPEED -7
#define BB_STOMP_SCORE 25
#define BB_STOMP_GRACE 6
#define BB_GRAVITY 1
#define BB_MAX_FALL 6
#define BB_INVULN_TICKS 50
#define BB_CAMERA_TARGET_X 136
#define BB_WORLD_W (BB_MAP_W * BB_TILE)
#define BB_WORLD_H (BB_MAP_H * BB_TILE)

#define BB_TILE_EMPTY '.'
#define BB_TILE_SOLID '#'
#define BB_TILE_PLATFORM '-'
#define BB_TILE_POWER '?'
#define BB_TILE_BONUS 'B'
#define BB_TILE_GATE 'G'
#define BB_TILE_FRAGMENT '*'
#define BB_TILE_RARE_FRAGMENT 'o'
#define BB_TILE_CODE_FRAGMENT 'D'
#define BB_TILE_HIDDEN 'H'
#define BB_TILE_SPIKE '^'
#define BB_TILE_WATER '~'
#define BB_TILE_EXIT 'E'
#define BB_TILE_PLAYER_START 'S'
#define BB_TILE_ENEMY 'v'
#define BB_TILE_ENEMY_ALT 'T'

typedef struct BB_Player {
    int x;
    int y;
    int vx;
    int vy;
    int on_ground;
    int dash_ticks;
    int facing;
    int invuln_ticks;
} BB_Player;

typedef struct BB_Enemy {
    int x;
    int y;
    int vy;
    int on_ground;
    int vx;
    int alive;
} BB_Enemy;

extern char bb_map[BB_MAP_H][BB_MAP_W + 1];
extern BB_Player bb_player;
extern BB_Enemy bb_enemies[BB_MAX_ENEMIES];
extern int bb_enemy_count;
extern int bb_level_index;
extern int bb_score;
extern int bb_lives;
extern int bb_fragments_left;
extern ANA_Camera bb_camera;
extern int bb_frame;

char bb_tile_at(int tx, int ty);
void bb_set_tile(int tx, int ty, char value);
int bb_tile_is_solid(char tile);
int bb_tile_is_platform(char tile);
int bb_tile_is_hazard(char tile);
int bb_tile_is_collectible(char tile);
int bb_tile_is_goal(char tile);
void bb_render_invalidate(void);

#endif

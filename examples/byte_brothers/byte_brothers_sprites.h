#ifndef BYTE_BROTHERS_SPRITES_H
#define BYTE_BROTHERS_SPRITES_H

#include "ana.h"

typedef struct BB_HardwareSpriteStats {
    ANA_AmigaSpriteUpdateStats update;
    long player_update_calls;
    long player_visible_moves;
    long enemy_update_calls;
    long enemy_visible_moves;
    int player_ready;
    int player_failed;
    int player_slots;
    const char* player_status;
    int enemy_ready;
    int enemy_failed;
    int enemy_slots;
    const char* enemy_status;
} BB_HardwareSpriteStats;

void bb_hardware_sprites_reset(void);
void bb_hardware_sprites_sync(void);
void bb_hardware_sprites_shutdown(void);
int bb_hardware_player_active(void);
int bb_hardware_enemy_active(int index);
int bb_hardware_enemy_ready(void);
int bb_hardware_enemy_failed(void);
int bb_hardware_enemy_capacity(void);
void bb_hardware_sprites_get_stats(BB_HardwareSpriteStats* stats);

#endif

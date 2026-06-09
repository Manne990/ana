#ifndef BYTE_BROTHERS_RENDER_H
#define BYTE_BROTHERS_RENDER_H

/* Rendering entry points for the Byte Brothers platform sample. */

typedef struct BB_RenderStats {
    long hw_enemy_update_calls;
    long hw_enemy_visible_moves;
    long bitmap_enemy_draws;
    long full_redraws;
    long tile_layer_draws;
    long tile_redraw_world_rects;
    long tile_restore_world_rects;
    long actor_redraw_passes;
    long player_draws;
    int hw_enemy_ready;
    int hw_enemy_failed;
    int hw_enemy_sprite_count;
    int hw_enemy_slot_count;
    const char* hw_enemy_status;
} BB_RenderStats;

void bb_render_reset(void);
void bb_render_invalidate(void);
void bb_render_tile_changed(int tx, int ty);
void bb_render_draw(void);
BB_RenderStats bb_render_stats(void);
void bb_render_shutdown(void);

#endif

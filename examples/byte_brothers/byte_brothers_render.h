#ifndef BYTE_BROTHERS_RENDER_H
#define BYTE_BROTHERS_RENDER_H

/* Rendering entry points for the Byte Brothers platform sample. */

typedef struct BB_RenderStats {
    long hw_enemy_update_calls;
    long hw_enemy_visible_moves;
    long bitmap_enemy_draws;
    long full_redraws;
    long tile_layer_draws;
    long tile_hardware_unavailable_draws;
    long tile_hardware_draws;
    long tile_software_draws;
    long tile_redraw_world_rects;
    long tile_restore_world_rects;
    long actor_redraw_passes;
    long player_draws;
    long tile_perf_ticks;
    long hud_perf_ticks;
    long actor_perf_ticks;
    long hw_player_perf_ticks;
    long hw_enemy_perf_ticks;
    long hw_sprite_position_checks;
    long hw_sprite_position_mismatches;
    long hw_sprite_zero_control_words;
    long hw_sprite_raster_checks;
    long hw_sprite_visible_raster_writes;
    long hw_sprite_safe_wait_calls;
    long hw_sprite_safe_top_hits;
    long hw_sprite_safe_bottom_hits;
    long hw_sprite_safe_visible_waits;
    long hw_sprite_safe_write_waits;
    long hw_sprite_span_checks;
    long hw_sprite_span_waits;
    long hw_sprite_unsafe_span_writes;
    int hw_sprite_min_raster_line;
    int hw_sprite_max_raster_line;
    int hw_sprite_last_raster_line;
    long hw_player_update_calls;
    long hw_player_visible_moves;
    int hw_player_ready;
    int hw_player_failed;
    int hw_player_sprite_count;
    const char* hw_player_status;
    int hw_enemy_ready;
    int hw_enemy_failed;
    int hw_enemy_sprite_count;
    int hw_enemy_slot_count;
    const char* hw_enemy_status;
} BB_RenderStats;

void bb_render_reset(void);
void bb_render_invalidate(void);
void bb_render_tile_changed(int tx, int ty);
void bb_render_sync_hardware_sprites(void);
void bb_render_draw(void);
BB_RenderStats bb_render_stats(void);
void bb_render_shutdown(void);

#endif

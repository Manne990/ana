#ifndef ANA_AMIGA_SPRITE_H
#define ANA_AMIGA_SPRITE_H

#include "ana_gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ANA_AMIGA_SPRITE_WIDTH 16
#define ANA_AMIGA_SPRITE_MAX_SLOTS 8

typedef int (*ANA_AmigaSpriteColorMap)(
    int source_color,
    void* user_data);

typedef struct ANA_AmigaSpriteBatch ANA_AmigaSpriteBatch;

typedef struct ANA_AmigaSpriteBatchConfig {
    ANA_Image image;
    int requested_slots;
    int minimum_slots;
    const int* preferred_channels;
    int preferred_channel_count;
    ANA_Color colors[3];
    ANA_AmigaSpriteColorMap color_map;
    void* color_map_user_data;
    unsigned short* fallback_data;
    long fallback_words;
    int trace_raster;
    ANA_AmigaSpriteUpdateStats* update_stats;
} ANA_AmigaSpriteBatchConfig;

/*
 * Converts one ANA image frame into an Amiga 16-pixel, two-bitplane sprite.
 * The output includes the two control words and two trailing zero words.
 */
int ana_amiga_sprite_encode_image_frame(
    ANA_Image image,
    int frame,
    ANA_AmigaSpriteColorMap color_map,
    void* color_map_user_data,
    unsigned short* target_data,
    long target_words);

#ifdef ANA_TARGET_AMIGA
void ana_amiga_sprite_update_stats_reset(ANA_AmigaSpriteUpdateStats* stats);
#endif

/*
 * Batch objects reserve one hardware sprite channel per slot. On non-Amiga
 * hosts creation returns NULL; the encoder remains available for tests and
 * asset validation.
 */
ANA_AmigaSpriteBatch* ana_amiga_sprite_batch_create(
    const ANA_AmigaSpriteBatchConfig* config);
void ana_amiga_sprite_batch_destroy(ANA_AmigaSpriteBatch* batch);
void ana_amiga_sprite_batch_begin(ANA_AmigaSpriteBatch* batch);
void ana_amiga_sprite_batch_set(
    ANA_AmigaSpriteBatch* batch,
    int slot,
    int visible,
    int frame,
    int x,
    int y);
void ana_amiga_sprite_batch_commit(ANA_AmigaSpriteBatch* batch);
void ana_amiga_sprite_batch_hide_all(ANA_AmigaSpriteBatch* batch);
int ana_amiga_sprite_batch_ready(const ANA_AmigaSpriteBatch* batch);
int ana_amiga_sprite_batch_failed(const ANA_AmigaSpriteBatch* batch);
int ana_amiga_sprite_batch_slot_count(const ANA_AmigaSpriteBatch* batch);
const char* ana_amiga_sprite_batch_status(const ANA_AmigaSpriteBatch* batch);
long ana_amiga_sprite_batch_update_calls(const ANA_AmigaSpriteBatch* batch);
long ana_amiga_sprite_batch_visible_moves(const ANA_AmigaSpriteBatch* batch);

#ifdef __cplusplus
}
#endif

#endif

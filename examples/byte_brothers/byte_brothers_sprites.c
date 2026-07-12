#include "byte_brothers_sprites.h"

#include "byte_brothers_assets.h"
#include "byte_brothers_internal.h"

#include <stdio.h>
#include <string.h>

#ifdef ANA_TARGET_AMIGA

#ifndef BB_HW_RASTER_TRACE
#define BB_HW_RASTER_TRACE 0
#endif

#define BB_HW_PLAYER_MAX_FRAMES 8
#define BB_HW_PLAYER_WORDS ((BB_PLAYER_H * 2) + 4)
#define BB_HW_ENEMY_MAX 4
#define BB_HW_ENEMY_WORDS ((BB_ENEMY_H * 2) + 4)

static ANA_AmigaSpriteBatch* bb_player_batch = NULL;
static ANA_AmigaSpriteBatch* bb_enemy_batch = NULL;
static unsigned short bb_player_fallback
    [BB_HW_PLAYER_MAX_FRAMES][BB_HW_PLAYER_WORDS];
static unsigned short bb_enemy_fallback[BB_HW_ENEMY_MAX][BB_HW_ENEMY_WORDS];
static ANA_AmigaSpriteUpdateStats bb_sprite_update_stats;
static int bb_player_failure_reported = 0;
static int bb_enemy_failure_reported = 0;

static int bb_hardware_player_color(int color, void* user_data)
{
    (void)user_data;
    switch (color) {
    case 13:
    case 3:
        return 1;
    case 1:
    case 4:
        return 2;
    case 5:
    case 6:
        return 3;
    default:
        return color > 0 ? 1 : 0;
    }
}

static int bb_hardware_enemy_color(int color, void* user_data)
{
    (void)user_data;
    switch (color) {
    case 7:
    case 15:
        return 1;
    case 1:
        return 2;
    case 13:
        return 3;
    default:
        return color > 0 ? 1 : 0;
    }
}

static int bb_actor_visible(int x, int y, int width, int height)
{
    return x < bb_camera.x + bb_camera.view_w &&
        x + width > bb_camera.x &&
        y < bb_camera.y + bb_camera.view_h &&
        y + height > bb_camera.y;
}

static int bb_enemy_visible(const BB_Enemy* enemy)
{
    return enemy != NULL && enemy->alive &&
        bb_actor_visible(enemy->x, enemy->y, BB_ENEMY_W, BB_ENEMY_H);
}

static void bb_create_player_batch(void)
{
    static const int channels[] = {6, 7, 0, 1};
    ANA_AmigaSpriteBatchConfig config;

    if (bb_player_batch != NULL || bb_player_image == NULL) {
        return;
    }

    memset(&config, 0, sizeof(config));
    config.image = bb_player_image;
    config.requested_slots = 1;
    config.minimum_slots = 1;
    config.preferred_channels = channels;
    config.preferred_channel_count =
        (int)(sizeof(channels) / sizeof(channels[0]));
    config.colors[0].r = 68;
    config.colors[0].g = 68;
    config.colors[0].b = 85;
    config.colors[1].r = 51;
    config.colors[1].g = 204;
    config.colors[1].b = 255;
    config.colors[2].r = 255;
    config.colors[2].g = 238;
    config.colors[2].b = 0;
    config.color_map = bb_hardware_player_color;
    config.trace_raster = BB_HW_RASTER_TRACE;
    config.fallback_data = &bb_player_fallback[0][0];
    config.fallback_words =
        (long)(sizeof(bb_player_fallback) / sizeof(unsigned short));
    config.update_stats = &bb_sprite_update_stats;
    bb_player_batch = ana_amiga_sprite_batch_create(&config);
}

static void bb_create_enemy_batch(void)
{
    static const int channels[] = {2, 3, 4, 5};
    ANA_AmigaSpriteBatchConfig config;

    if (bb_enemy_batch != NULL || bb_enemy_image == NULL) {
        return;
    }

    memset(&config, 0, sizeof(config));
    config.image = bb_enemy_image;
    config.requested_slots = BB_HW_ENEMY_MAX;
    config.minimum_slots = 1;
    config.preferred_channels = channels;
    config.preferred_channel_count =
        (int)(sizeof(channels) / sizeof(channels[0]));
    config.colors[0].r = 255;
    config.colors[0].g = 34;
    config.colors[0].b = 68;
    config.colors[1].r = 255;
    config.colors[1].g = 255;
    config.colors[1].b = 255;
    config.colors[2].r = 68;
    config.colors[2].g = 68;
    config.colors[2].b = 85;
    config.color_map = bb_hardware_enemy_color;
    config.trace_raster = BB_HW_RASTER_TRACE;
    config.fallback_data = &bb_enemy_fallback[0][0];
    config.fallback_words =
        (long)(sizeof(bb_enemy_fallback) / sizeof(unsigned short));
    config.update_stats = &bb_sprite_update_stats;
    bb_enemy_batch = ana_amiga_sprite_batch_create(&config);
}

static void bb_report_sprite_failures(void)
{
    if (bb_player_batch == NULL) {
        if (!bb_player_failure_reported) {
            printf("Byte Brothers hardware player sprite unavailable: allocation\n");
            bb_player_failure_reported = 1;
        }
    } else if (ana_amiga_sprite_batch_failed(bb_player_batch) &&
            !bb_player_failure_reported) {
        printf(
            "Byte Brothers hardware player sprite unavailable: %s\n",
            ana_amiga_sprite_batch_status(bb_player_batch));
        bb_player_failure_reported = 1;
    }

    if (bb_enemy_batch == NULL) {
        if (!bb_enemy_failure_reported) {
            printf("Byte Brothers hardware enemy sprites unavailable: allocation\n");
            bb_enemy_failure_reported = 1;
        }
    } else if (ana_amiga_sprite_batch_failed(bb_enemy_batch) &&
            !bb_enemy_failure_reported) {
        printf(
            "Byte Brothers hardware enemy sprites unavailable: %s\n",
            ana_amiga_sprite_batch_status(bb_enemy_batch));
        bb_enemy_failure_reported = 1;
    }
}

static void bb_begin_batches(void)
{
    bb_create_player_batch();
    bb_create_enemy_batch();
    ana_amiga_sprite_batch_begin(bb_player_batch);
    ana_amiga_sprite_batch_begin(bb_enemy_batch);
    bb_report_sprite_failures();
}

void bb_hardware_sprites_reset(void)
{
    ana_amiga_sprite_update_stats_reset(&bb_sprite_update_stats);
    bb_begin_batches();
    ana_amiga_sprite_batch_commit(bb_player_batch);
    ana_amiga_sprite_batch_commit(bb_enemy_batch);
    ana_amiga_sprite_update_stats_reset(&bb_sprite_update_stats);
}

void bb_hardware_sprites_sync(void)
{
    int capacity;
    int i;
    int slot;

    bb_begin_batches();
    if (ana_amiga_sprite_batch_ready(bb_player_batch) &&
            bb_actor_visible(
                bb_player.x,
                bb_player.y,
                BB_PLAYER_W,
                BB_PLAYER_H)) {
        ana_amiga_sprite_batch_set(
            bb_player_batch,
            0,
            1,
            0,
            bb_player.x - bb_camera.x + bb_camera.view_x,
            bb_player.y - bb_camera.y + bb_camera.view_y);
    }

    capacity = ana_amiga_sprite_batch_slot_count(bb_enemy_batch);
    slot = 0;
    for (i = 0; i < bb_enemy_count && slot < capacity; i++) {
        if (!bb_enemy_visible(&bb_enemies[i])) {
            continue;
        }
        ana_amiga_sprite_batch_set(
            bb_enemy_batch,
            slot,
            1,
            0,
            bb_enemies[i].x - bb_camera.x + bb_camera.view_x,
            bb_enemies[i].y - bb_camera.y + bb_camera.view_y);
        slot++;
    }

    ana_amiga_sprite_batch_commit(bb_player_batch);
    ana_amiga_sprite_batch_commit(bb_enemy_batch);
}

void bb_hardware_sprites_shutdown(void)
{
    ana_amiga_sprite_batch_destroy(bb_player_batch);
    ana_amiga_sprite_batch_destroy(bb_enemy_batch);
    bb_player_batch = NULL;
    bb_enemy_batch = NULL;
}

int bb_hardware_player_active(void)
{
    return ana_amiga_sprite_batch_ready(bb_player_batch) &&
        bb_actor_visible(
            bb_player.x,
            bb_player.y,
            BB_PLAYER_W,
            BB_PLAYER_H);
}

int bb_hardware_enemy_active(int index)
{
    int i;
    int slot;
    int capacity;

    if (index < 0 || index >= bb_enemy_count ||
            !ana_amiga_sprite_batch_ready(bb_enemy_batch) ||
            !bb_enemy_visible(&bb_enemies[index])) {
        return 0;
    }

    capacity = ana_amiga_sprite_batch_slot_count(bb_enemy_batch);
    slot = 0;
    for (i = 0; i <= index; i++) {
        if (!bb_enemy_visible(&bb_enemies[i])) {
            continue;
        }
        if (i == index) {
            return slot < capacity;
        }
        slot++;
    }
    return 0;
}

int bb_hardware_enemy_ready(void)
{
    return ana_amiga_sprite_batch_ready(bb_enemy_batch);
}

int bb_hardware_enemy_failed(void)
{
    return ana_amiga_sprite_batch_failed(bb_enemy_batch);
}

int bb_hardware_enemy_capacity(void)
{
    return ana_amiga_sprite_batch_slot_count(bb_enemy_batch);
}

void bb_hardware_sprites_get_stats(BB_HardwareSpriteStats* stats)
{
    if (stats == NULL) {
        return;
    }

    memset(stats, 0, sizeof(*stats));
    stats->update = bb_sprite_update_stats;
    stats->player_update_calls =
        ana_amiga_sprite_batch_update_calls(bb_player_batch);
    stats->player_visible_moves =
        ana_amiga_sprite_batch_visible_moves(bb_player_batch);
    stats->enemy_update_calls =
        ana_amiga_sprite_batch_update_calls(bb_enemy_batch);
    stats->enemy_visible_moves =
        ana_amiga_sprite_batch_visible_moves(bb_enemy_batch);
    stats->player_ready = ana_amiga_sprite_batch_ready(bb_player_batch);
    stats->player_failed = ana_amiga_sprite_batch_failed(bb_player_batch);
    stats->player_slots = ana_amiga_sprite_batch_slot_count(bb_player_batch);
    stats->player_status = ana_amiga_sprite_batch_status(bb_player_batch);
    stats->enemy_ready = ana_amiga_sprite_batch_ready(bb_enemy_batch);
    stats->enemy_failed = ana_amiga_sprite_batch_failed(bb_enemy_batch);
    stats->enemy_slots = ana_amiga_sprite_batch_slot_count(bb_enemy_batch);
    stats->enemy_status = ana_amiga_sprite_batch_status(bb_enemy_batch);
}

#else

void bb_hardware_sprites_reset(void)
{
}

void bb_hardware_sprites_sync(void)
{
}

void bb_hardware_sprites_shutdown(void)
{
}

int bb_hardware_player_active(void)
{
    return 0;
}

int bb_hardware_enemy_active(int index)
{
    (void)index;
    return 0;
}

int bb_hardware_enemy_ready(void)
{
    return 0;
}

int bb_hardware_enemy_failed(void)
{
    return 0;
}

int bb_hardware_enemy_capacity(void)
{
    return 0;
}

void bb_hardware_sprites_get_stats(BB_HardwareSpriteStats* stats)
{
    if (stats == NULL) {
        return;
    }
    memset(stats, 0, sizeof(*stats));
    stats->update.min_raster_line = -1;
    stats->update.max_raster_line = -1;
    stats->update.last_raster_line = -1;
    stats->player_status = "host";
    stats->enemy_status = "host";
}

#endif

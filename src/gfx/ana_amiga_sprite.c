#include "ana/ana_amiga_sprite.h"

#include <stddef.h>

int ana_amiga_sprite_encode_image_frame(
    ANA_Image image,
    int frame,
    ANA_AmigaSpriteColorMap color_map,
    void* color_map_user_data,
    unsigned short* target_data,
    long target_words)
{
    int source_width;
    int source_height;
    int source_x;
    int color;
    int code;
    int x;
    int y;
    long required_words;
    unsigned short bit;
    unsigned short plane0;
    unsigned short plane1;

    source_width = ana_image_width(image);
    source_height = ana_image_height(image);
    required_words = ((long)source_height * 2L) + 4L;
    if (image == NULL || color_map == NULL || target_data == NULL ||
            frame < 0 || frame >= ana_image_frame_count(image) ||
            source_width <= 0 || source_height <= 0 ||
            target_words < required_words) {
        return 0;
    }

    target_data[0] = 0u;
    target_data[1] = 0u;
    for (y = 0; y < source_height; y++) {
        plane0 = 0u;
        plane1 = 0u;
        for (x = 0; x < ANA_AMIGA_SPRITE_WIDTH; x++) {
            source_x =
                ((x * source_width) + (ANA_AMIGA_SPRITE_WIDTH / 2)) /
                ANA_AMIGA_SPRITE_WIDTH;
            if (source_x >= source_width ||
                    !ana_image_pixel_visible(image, frame, source_x, y)) {
                continue;
            }

            color = ana_image_pixel_index(image, frame, source_x, y);
            code = color_map(color, color_map_user_data) & 3;
            bit = (unsigned short)(0x8000u >> x);
            if ((code & 1) != 0) {
                plane0 = (unsigned short)(plane0 | bit);
            }
            if ((code & 2) != 0) {
                plane1 = (unsigned short)(plane1 | bit);
            }
        }

        target_data[2 + (y * 2)] = plane0;
        target_data[3 + (y * 2)] = plane1;
    }

    target_data[2 + (source_height * 2)] = 0u;
    target_data[3 + (source_height * 2)] = 0u;
    return 1;
}

#ifdef ANA_TARGET_AMIGA

#include <stdlib.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/sprite.h>
#include <graphics/view.h>
#include <hardware/custom.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#define ANA_AMIGA_SPRITE_VISIBLE_START_X 128
#define ANA_AMIGA_SPRITE_VISIBLE_START_LINE 44
#define ANA_AMIGA_SPRITE_VISIBLE_END_LINE \
    (ANA_AMIGA_SPRITE_VISIBLE_START_LINE + ANA_DEFAULT_HEIGHT)
#define ANA_AMIGA_SPRITE_SAFE_TOP_END_LINE 32
#define ANA_AMIGA_SPRITE_RASTER_GUARD_LINES 16

extern struct Custom custom;

void ana_amiga_sprite_update_stats_reset(ANA_AmigaSpriteUpdateStats* stats)
{
    if (stats == NULL) {
        return;
    }

    memset(stats, 0, sizeof(*stats));
    stats->min_raster_line = 9999;
    stats->max_raster_line = -1;
    stats->last_raster_line = -1;
}

static int ana_amiga_sprite_raster_line(void)
{
    UWORD vpos;
    UWORD vhpos;

    vpos = custom.vposr;
    vhpos = custom.vhposr;
    return (int)(((vpos & 0x0001u) << 8) | ((vhpos >> 8) & 0x00ffu));
}

static int ana_amiga_sprite_line_overlaps_y(int line, int y, int height)
{
    int start;
    int end;

    start = y + ANA_AMIGA_SPRITE_VISIBLE_START_LINE;
    end = start + height;
    return line >= start - ANA_AMIGA_SPRITE_RASTER_GUARD_LINES &&
        line < end + ANA_AMIGA_SPRITE_RASTER_GUARD_LINES;
}

static int ana_amiga_sprite_current_y(const struct SimpleSprite* sprite)
{
    if (sprite == NULL) {
        return -1000;
    }

    return (int)sprite->y;
}

static int ana_amiga_sprite_line_unsafe(
    const struct SimpleSprite* sprite,
    int new_y,
    int height,
    int line)
{
    int old_y;

    old_y = ana_amiga_sprite_current_y(sprite);
    return ana_amiga_sprite_line_overlaps_y(line, old_y, height) ||
        ana_amiga_sprite_line_overlaps_y(line, new_y, height);
}

static void ana_amiga_sprite_wait_until_safe(
    const struct SimpleSprite* sprite,
    int y,
    int height,
    ANA_AmigaSpriteUpdateStats* stats)
{
    int line;

    if (height <= 0) {
        return;
    }

    line = ana_amiga_sprite_raster_line();
    if (stats != NULL) {
        stats->safe_wait_calls++;
        stats->span_checks++;
    }
    if (!ana_amiga_sprite_line_unsafe(sprite, y, height, line)) {
        if (stats != NULL) {
            if (line < ANA_AMIGA_SPRITE_SAFE_TOP_END_LINE) {
                stats->safe_top_hits++;
            } else if (line >= ANA_AMIGA_SPRITE_VISIBLE_END_LINE) {
                stats->safe_bottom_hits++;
            }
        }
        return;
    }

    if (stats != NULL) {
        stats->safe_write_waits++;
        stats->span_waits++;
        if (line >= ANA_AMIGA_SPRITE_SAFE_TOP_END_LINE &&
                line < ANA_AMIGA_SPRITE_VISIBLE_END_LINE) {
            stats->safe_visible_waits++;
        }
    }
    while (ana_amiga_sprite_line_unsafe(sprite, y, height, line)) {
        line = ana_amiga_sprite_raster_line();
    }
}

/*
 * Disable interrupts after the raster wait and recheck while locked. The
 * critical section contains only the sprite pointer/control update and keeps
 * the beam from crossing into an actor span between validation and storage.
 */
static void ana_amiga_sprite_lock_safe_write(
    const struct SimpleSprite* sprite,
    int y,
    int height,
    ANA_AmigaSpriteUpdateStats* stats)
{
    int line;

    for (;;) {
        ana_amiga_sprite_wait_until_safe(sprite, y, height, stats);
        Disable();
        line = ana_amiga_sprite_raster_line();
        if (!ana_amiga_sprite_line_unsafe(sprite, y, height, line)) {
            return;
        }
        Enable();
    }
}

static void ana_amiga_sprite_record_write_raster(
    const struct SimpleSprite* sprite,
    int y,
    int height,
    int trace_raster,
    ANA_AmigaSpriteUpdateStats* stats)
{
    int line;

    if (!trace_raster || stats == NULL || height <= 0) {
        return;
    }

    line = ana_amiga_sprite_raster_line();
    stats->raster_checks++;
    stats->last_raster_line = line;
    if (line < stats->min_raster_line) {
        stats->min_raster_line = line;
    }
    if (line > stats->max_raster_line) {
        stats->max_raster_line = line;
    }
    if (line >= ANA_AMIGA_SPRITE_VISIBLE_START_LINE &&
            line < ANA_AMIGA_SPRITE_VISIBLE_END_LINE) {
        stats->visible_raster_writes++;
    }
    if (ana_amiga_sprite_line_unsafe(sprite, y, height, line)) {
        stats->unsafe_span_writes++;
    }
}

static void ana_amiga_sprite_copy_control_words(
    const struct SimpleSprite* sprite,
    unsigned short* target_data)
{
    if (sprite == NULL || sprite->posctldata == NULL || target_data == NULL) {
        return;
    }

    target_data[0] = sprite->posctldata[0];
    target_data[1] = sprite->posctldata[1];
}

static void ana_amiga_sprite_expected_control_words(
    int x,
    int y,
    int height,
    unsigned short* word0,
    unsigned short* word1)
{
    int hstart;
    int vstart;
    int vstop;

    if (word0 == NULL || word1 == NULL) {
        return;
    }

    hstart = x + ANA_AMIGA_SPRITE_VISIBLE_START_X;
    vstart = y + ANA_AMIGA_SPRITE_VISIBLE_START_LINE;
    vstop = vstart + height;
    if (hstart < 0) {
        hstart = 0;
    }
    if (vstart < 0) {
        vstart = 0;
    }
    if (vstop < 0) {
        vstop = 0;
    }

    *word0 = (unsigned short)(((vstart & 0xff) << 8) |
        ((hstart >> 1) & 0xff));
    *word1 = (unsigned short)(((vstop & 0xff) << 8) |
        ((vstart & 0x100) != 0 ? 0x0004 : 0x0000) |
        ((vstop & 0x100) != 0 ? 0x0002 : 0x0000) |
        (hstart & 1));
}

static void ana_amiga_sprite_set_position_safe(
    struct SimpleSprite* sprite,
    int x,
    int y,
    int height,
    int trace_raster,
    ANA_AmigaSpriteUpdateStats* stats)
{
    UWORD* data;
    unsigned short word0;
    unsigned short word1;

    if (sprite == NULL || sprite->posctldata == NULL || height <= 0) {
        return;
    }

    ana_amiga_sprite_lock_safe_write(sprite, y, height, stats);
    ana_amiga_sprite_record_write_raster(
        sprite,
        y,
        height,
        trace_raster,
        stats);
    ana_amiga_sprite_expected_control_words(
        x,
        y,
        height,
        &word0,
        &word1);

    data = sprite->posctldata;
    data[0] = (UWORD)word0;
    data[1] = (UWORD)word1;
    sprite->x = (WORD)x;
    sprite->y = (WORD)y;
    Enable();
}

static void ana_amiga_sprite_record_control_check(
    const struct SimpleSprite* sprite,
    int x,
    int y,
    int height,
    ANA_AmigaSpriteUpdateStats* stats)
{
    unsigned short expected0;
    unsigned short expected1;
    const UWORD* data;

    if (stats == NULL || sprite == NULL || sprite->posctldata == NULL ||
            height <= 0) {
        return;
    }

    data = sprite->posctldata;
    stats->position_checks++;
    if (data[0] == 0u && data[1] == 0u) {
        stats->zero_control_words++;
    }

    ana_amiga_sprite_expected_control_words(
        x,
        y,
        height,
        &expected0,
        &expected1);
    if (data[0] != expected0 || data[1] != expected1) {
        stats->position_mismatches++;
    }
}

typedef struct ANA_AmigaSpriteSlot {
    struct SimpleSprite sprite;
    unsigned short* data;
    int channel;
    int current_frame;
    int visible;
    int last_x;
    int last_y;
    int pending_visible;
    int pending_frame;
    int pending_x;
    int pending_y;
} ANA_AmigaSpriteSlot;

struct ANA_AmigaSpriteBatch {
    ANA_Image image;
    int requested_slots;
    int minimum_slots;
    int preferred_channels[ANA_AMIGA_SPRITE_MAX_SLOTS];
    int preferred_channel_count;
    ANA_Color colors[3];
    ANA_AmigaSpriteColorMap color_map;
    void* color_map_user_data;
    unsigned short* fallback_data;
    long fallback_words;
    int trace_raster;
    ANA_AmigaSpriteUpdateStats* update_stats;
    ANA_AmigaSpriteSlot slots[ANA_AMIGA_SPRITE_MAX_SLOTS];
    unsigned short* data;
    long data_words;
    int data_allocated;
    int frame_count;
    int frame_words;
    int height;
    int slot_count;
    int initialized;
    int ready;
    int failed;
    int pending_valid;
    const char* status;
    long update_calls;
    long visible_moves;
};

static unsigned short* ana_amiga_sprite_batch_frame_data(
    ANA_AmigaSpriteBatch* batch,
    int slot,
    int frame)
{
    long offset;

    if (batch == NULL || batch->data == NULL ||
            slot < 0 || slot >= batch->requested_slots ||
            frame < 0 || frame >= batch->frame_count) {
        return NULL;
    }

    offset = ((long)slot * (long)batch->frame_count + (long)frame) *
        (long)batch->frame_words;
    return batch->data + offset;
}

static void ana_amiga_sprite_batch_move_slot(
    ANA_AmigaSpriteBatch* batch,
    int slot,
    int x,
    int y)
{
    ANA_AmigaSpriteSlot* item;

    if (batch == NULL || slot < 0 || slot >= batch->slot_count) {
        return;
    }

    item = &batch->slots[slot];
    ana_amiga_sprite_set_position_safe(
        &item->sprite,
        x,
        y,
        batch->height,
        batch->trace_raster,
        batch->update_stats);
    ana_amiga_sprite_record_control_check(
        &item->sprite,
        x,
        y,
        batch->height,
        batch->update_stats);
}

static void ana_amiga_sprite_batch_hide_slot(
    ANA_AmigaSpriteBatch* batch,
    int slot)
{
    ANA_AmigaSpriteSlot* item;

    if (batch == NULL || slot < 0 || slot >= batch->slot_count) {
        return;
    }

    item = &batch->slots[slot];
    if (item->visible) {
        ana_amiga_sprite_batch_move_slot(batch, slot, -32, 0);
    }
    item->visible = 0;
    item->last_x = -1000;
    item->last_y = -1000;
}

static void ana_amiga_sprite_batch_release_resources(
    ANA_AmigaSpriteBatch* batch)
{
    int i;

    if (batch == NULL) {
        return;
    }

    for (i = 0; i < batch->slot_count; i++) {
        if (batch->slots[i].visible) {
            ana_amiga_sprite_batch_hide_slot(batch, i);
        }
        FreeSprite((LONG)batch->slots[i].channel);
        batch->slots[i].channel = -1;
    }
    batch->slot_count = 0;

    if (batch->data != NULL && batch->data_allocated) {
        FreeMem(
            batch->data,
            (ULONG)(batch->data_words * (long)sizeof(unsigned short)));
    }
    batch->data = NULL;
    batch->data_words = 0L;
    batch->data_allocated = 0;
    batch->ready = 0;
}

static int ana_amiga_sprite_batch_fail(
    ANA_AmigaSpriteBatch* batch,
    const char* status)
{
    ana_amiga_sprite_batch_release_resources(batch);
    batch->initialized = 1;
    batch->failed = 1;
    batch->status = status;
    return 0;
}

static int ana_amiga_sprite_batch_channel_already_used(
    const ANA_AmigaSpriteBatch* batch,
    int channel)
{
    int i;

    for (i = 0; i < batch->slot_count; i++) {
        if (batch->slots[i].channel == channel) {
            return 1;
        }
    }
    return 0;
}

static void ana_amiga_sprite_batch_set_colors(
    const ANA_AmigaSpriteBatch* batch,
    struct ViewPort* viewport,
    int channel)
{
    int base;
    int i;

    base = 17 + ((channel >> 1) * 4);
    for (i = 0; i < 3; i++) {
        SetRGB4(
            viewport,
            (LONG)(base + i),
            (LONG)(batch->colors[i].r >> 4),
            (LONG)(batch->colors[i].g >> 4),
            (LONG)(batch->colors[i].b >> 4));
    }
}

static int ana_amiga_sprite_batch_initialize(ANA_AmigaSpriteBatch* batch)
{
    struct ViewPort* viewport;
    ANA_AmigaSpriteSlot* item;
    unsigned short* frame_data;
    long required_words;
    int candidate_slot;
    int channel;
    int channel_index;
    int frame;
    int slot;

    if (batch == NULL) {
        return 0;
    }
    if (batch->ready) {
        return 1;
    }
    if (batch->initialized || batch->failed) {
        return 0;
    }

    viewport = (struct ViewPort*)ana_gfx_native_viewport();
    if (viewport == NULL) {
        return ana_amiga_sprite_batch_fail(batch, "no viewport");
    }

    batch->height = ana_image_height(batch->image);
    batch->frame_count = ana_image_frame_count(batch->image);
    if (batch->image == NULL || ana_image_width(batch->image) <= 0 ||
            batch->height <= 0 || batch->height > 255 ||
            batch->frame_count <= 0) {
        return ana_amiga_sprite_batch_fail(batch, "invalid image");
    }

    batch->frame_words = (batch->height * 2) + 4;
    required_words = (long)batch->requested_slots *
        (long)batch->frame_count * (long)batch->frame_words;
    if (required_words <= 0L) {
        return ana_amiga_sprite_batch_fail(batch, "sprite data size");
    }
    batch->data_words = required_words;
    batch->data = (unsigned short*)AllocMem(
        (ULONG)(required_words * (long)sizeof(unsigned short)),
        MEMF_CHIP | MEMF_CLEAR);
    if (batch->data != NULL) {
        batch->data_allocated = 1;
    } else {
        if (batch->fallback_data == NULL ||
                batch->fallback_words < required_words ||
                (TypeOfMem(batch->fallback_data) & MEMF_CHIP) == 0u) {
            return ana_amiga_sprite_batch_fail(
                batch,
                "sprite chip allocation");
        }
        batch->data = batch->fallback_data;
        batch->data_allocated = 0;
        memset(
            batch->data,
            0,
            (size_t)(required_words * (long)sizeof(unsigned short)));
    }

    for (slot = 0; slot < batch->requested_slots; slot++) {
        for (frame = 0; frame < batch->frame_count; frame++) {
            frame_data = ana_amiga_sprite_batch_frame_data(batch, slot, frame);
            if (!ana_amiga_sprite_encode_image_frame(
                    batch->image,
                    frame,
                    batch->color_map,
                    batch->color_map_user_data,
                    frame_data,
                    batch->frame_words)) {
                return ana_amiga_sprite_batch_fail(
                    batch,
                    "sprite data build");
            }
        }
    }

    for (channel_index = 0;
            channel_index < batch->preferred_channel_count &&
                batch->slot_count < batch->requested_slots;
            channel_index++) {
        channel = batch->preferred_channels[channel_index];
        if (channel < 0 || channel >= ANA_AMIGA_SPRITE_MAX_SLOTS ||
                ana_amiga_sprite_batch_channel_already_used(batch, channel)) {
            continue;
        }

        candidate_slot = batch->slot_count;
        item = &batch->slots[candidate_slot];
        frame_data = ana_amiga_sprite_batch_frame_data(batch, candidate_slot, 0);
        item->sprite.posctldata = (UWORD*)frame_data;
        item->sprite.height = (WORD)batch->height;
        item->sprite.x = 0;
        item->sprite.y = 0;
        item->sprite.num = 0;
        channel = (int)GetSprite(&item->sprite, (LONG)channel);
        if (channel < 0) {
            continue;
        }

        item->data = frame_data;
        item->channel = channel;
        item->current_frame = 0;
        item->visible = 0;
        item->last_x = -1000;
        item->last_y = -1000;
        batch->slot_count++;
        ana_amiga_sprite_batch_set_colors(batch, viewport, channel);
        ChangeSprite(viewport, &item->sprite, (UWORD*)frame_data);
    }

    if (batch->slot_count < batch->minimum_slots) {
        return ana_amiga_sprite_batch_fail(batch, "sprite channels");
    }

    batch->initialized = 1;
    batch->ready = 1;
    batch->status = "ready";
    return 1;
}

ANA_AmigaSpriteBatch* ana_amiga_sprite_batch_create(
    const ANA_AmigaSpriteBatchConfig* config)
{
    ANA_AmigaSpriteBatch* batch;
    int i;

    if (config == NULL || config->image == NULL ||
            config->requested_slots <= 0 ||
            config->requested_slots > ANA_AMIGA_SPRITE_MAX_SLOTS ||
            config->minimum_slots <= 0 ||
            config->minimum_slots > config->requested_slots ||
            config->preferred_channels == NULL ||
            config->preferred_channel_count <= 0 ||
            config->preferred_channel_count > ANA_AMIGA_SPRITE_MAX_SLOTS ||
            config->color_map == NULL) {
        return NULL;
    }

    batch = (ANA_AmigaSpriteBatch*)calloc(1u, sizeof(*batch));
    if (batch == NULL) {
        return NULL;
    }

    batch->image = config->image;
    batch->requested_slots = config->requested_slots;
    batch->minimum_slots = config->minimum_slots;
    batch->preferred_channel_count = config->preferred_channel_count;
    for (i = 0; i < config->preferred_channel_count; i++) {
        batch->preferred_channels[i] = config->preferred_channels[i];
    }
    for (i = 0; i < 3; i++) {
        batch->colors[i] = config->colors[i];
    }
    for (i = 0; i < ANA_AMIGA_SPRITE_MAX_SLOTS; i++) {
        batch->slots[i].channel = -1;
        batch->slots[i].current_frame = -1;
        batch->slots[i].last_x = -1000;
        batch->slots[i].last_y = -1000;
    }
    batch->color_map = config->color_map;
    batch->color_map_user_data = config->color_map_user_data;
    batch->fallback_data = config->fallback_data;
    batch->fallback_words = config->fallback_words;
    batch->trace_raster = config->trace_raster;
    batch->update_stats = config->update_stats;
    batch->status = "not initialized";
    return batch;
}

void ana_amiga_sprite_batch_destroy(ANA_AmigaSpriteBatch* batch)
{
    if (batch == NULL) {
        return;
    }

    ana_amiga_sprite_batch_release_resources(batch);
    free(batch);
}

void ana_amiga_sprite_batch_begin(ANA_AmigaSpriteBatch* batch)
{
    int i;

    if (batch == NULL) {
        return;
    }

    batch->update_calls++;
    batch->pending_valid = 0;
    if (!ana_amiga_sprite_batch_initialize(batch)) {
        return;
    }

    for (i = 0; i < batch->slot_count; i++) {
        batch->slots[i].pending_visible = 0;
        batch->slots[i].pending_frame = 0;
        batch->slots[i].pending_x = 0;
        batch->slots[i].pending_y = 0;
    }
    batch->pending_valid = 1;
}

void ana_amiga_sprite_batch_set(
    ANA_AmigaSpriteBatch* batch,
    int slot,
    int visible,
    int frame,
    int x,
    int y)
{
    ANA_AmigaSpriteSlot* item;

    if (batch == NULL || !batch->pending_valid ||
            slot < 0 || slot >= batch->slot_count) {
        return;
    }

    item = &batch->slots[slot];
    item->pending_visible = visible != 0;
    item->pending_frame = frame;
    item->pending_x = x;
    item->pending_y = y;
}

void ana_amiga_sprite_batch_commit(ANA_AmigaSpriteBatch* batch)
{
    struct ViewPort* viewport;
    ANA_AmigaSpriteSlot* item;
    unsigned short* frame_data;
    int frame_changed;
    int slot;

    if (batch == NULL || !batch->pending_valid || !batch->ready) {
        return;
    }

    viewport = (struct ViewPort*)ana_gfx_native_viewport();
    if (viewport == NULL) {
        return;
    }

    for (slot = 0; slot < batch->slot_count; slot++) {
        item = &batch->slots[slot];
        if (!item->pending_visible || item->pending_frame < 0 ||
                item->pending_frame >= batch->frame_count) {
            ana_amiga_sprite_batch_hide_slot(batch, slot);
            continue;
        }

        frame_changed = item->pending_frame != item->current_frame;
        if (frame_changed) {
            frame_data = ana_amiga_sprite_batch_frame_data(
                batch,
                slot,
                item->pending_frame);
            ana_amiga_sprite_lock_safe_write(
                &item->sprite,
                item->pending_y,
                batch->height,
                batch->update_stats);
            ana_amiga_sprite_copy_control_words(&item->sprite, frame_data);
            ana_amiga_sprite_record_write_raster(
                &item->sprite,
                item->pending_y,
                batch->height,
                batch->trace_raster,
                batch->update_stats);
            ChangeSprite(viewport, &item->sprite, (UWORD*)frame_data);
            item->data = frame_data;
            item->current_frame = item->pending_frame;
            Enable();
        }

        if (frame_changed || !item->visible ||
                item->last_x != item->pending_x ||
                item->last_y != item->pending_y) {
            ana_amiga_sprite_batch_move_slot(
                batch,
                slot,
                item->pending_x,
                item->pending_y);
            item->visible = 1;
            item->last_x = item->pending_x;
            item->last_y = item->pending_y;
            batch->visible_moves++;
        }
    }
    batch->pending_valid = 0;
}

void ana_amiga_sprite_batch_hide_all(ANA_AmigaSpriteBatch* batch)
{
    int i;

    if (batch == NULL || !batch->ready) {
        return;
    }
    for (i = 0; i < batch->slot_count; i++) {
        ana_amiga_sprite_batch_hide_slot(batch, i);
    }
}

int ana_amiga_sprite_batch_ready(const ANA_AmigaSpriteBatch* batch)
{
    return batch != NULL && batch->ready;
}

int ana_amiga_sprite_batch_failed(const ANA_AmigaSpriteBatch* batch)
{
    return batch != NULL && batch->failed;
}

int ana_amiga_sprite_batch_slot_count(const ANA_AmigaSpriteBatch* batch)
{
    return batch != NULL ? batch->slot_count : 0;
}

const char* ana_amiga_sprite_batch_status(const ANA_AmigaSpriteBatch* batch)
{
    return batch != NULL ? batch->status : "not created";
}

long ana_amiga_sprite_batch_update_calls(const ANA_AmigaSpriteBatch* batch)
{
    return batch != NULL ? batch->update_calls : 0L;
}

long ana_amiga_sprite_batch_visible_moves(const ANA_AmigaSpriteBatch* batch)
{
    return batch != NULL ? batch->visible_moves : 0L;
}

#else

struct ANA_AmigaSpriteBatch {
    int unused;
};

ANA_AmigaSpriteBatch* ana_amiga_sprite_batch_create(
    const ANA_AmigaSpriteBatchConfig* config)
{
    (void)config;
    return NULL;
}

void ana_amiga_sprite_batch_destroy(ANA_AmigaSpriteBatch* batch)
{
    (void)batch;
}

void ana_amiga_sprite_batch_begin(ANA_AmigaSpriteBatch* batch)
{
    (void)batch;
}

void ana_amiga_sprite_batch_set(
    ANA_AmigaSpriteBatch* batch,
    int slot,
    int visible,
    int frame,
    int x,
    int y)
{
    (void)batch;
    (void)slot;
    (void)visible;
    (void)frame;
    (void)x;
    (void)y;
}

void ana_amiga_sprite_batch_commit(ANA_AmigaSpriteBatch* batch)
{
    (void)batch;
}

void ana_amiga_sprite_batch_hide_all(ANA_AmigaSpriteBatch* batch)
{
    (void)batch;
}

int ana_amiga_sprite_batch_ready(const ANA_AmigaSpriteBatch* batch)
{
    (void)batch;
    return 0;
}

int ana_amiga_sprite_batch_failed(const ANA_AmigaSpriteBatch* batch)
{
    (void)batch;
    return 0;
}

int ana_amiga_sprite_batch_slot_count(const ANA_AmigaSpriteBatch* batch)
{
    (void)batch;
    return 0;
}

const char* ana_amiga_sprite_batch_status(const ANA_AmigaSpriteBatch* batch)
{
    (void)batch;
    return "host";
}

long ana_amiga_sprite_batch_update_calls(const ANA_AmigaSpriteBatch* batch)
{
    (void)batch;
    return 0L;
}

long ana_amiga_sprite_batch_visible_moves(const ANA_AmigaSpriteBatch* batch)
{
    (void)batch;
    return 0L;
}

#endif

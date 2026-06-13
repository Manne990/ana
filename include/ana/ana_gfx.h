#ifndef ANA_GFX_H
#define ANA_GFX_H

#include "ana_helpers.h"
#include "ana_platform.h"
#include "ana_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ANA_Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} ANA_Color;

typedef struct ANA_RenderStats {
    long frames;
    long perf_ticks_per_second;
    long dirty_rects;
    long converted_pixels;
    long planar_clear_rects;
    long planar_clear_pixels;
    long chunky_clear_rects;
    long chunky_clear_pixels;
    long max_dirty_rects;
    long max_converted_pixels;
    long max_converted_rect_w;
    long max_converted_rect_h;
    long max_planar_clear_pixels;
    long max_chunky_clear_pixels;
    long present_total_perf_ticks;
    long present_clear_perf_ticks;
    long present_convert_perf_ticks;
    long present_flip_perf_ticks;
    long screen_buffer_flips;
    long direct_flips;
    long full_dirty_fill_rects;
    long full_dirty_scroll_rects;
    long full_dirty_image_rects;
    long full_dirty_text_rects;
    long full_dirty_mask_fill_rects;
    long full_dirty_generic_rects;
    long dirty_fill_rects;
    long dirty_fill_pixels;
    long hardware_fill_rects;
    long hardware_fill_pixels;
    long hardware_restore_rects;
    long hardware_restore_pixels;
    long hardware_restore_fallbacks;
    long hardware_redraw_rects;
    long hardware_redraw_pixels;
    long hardware_scroll_alloc_attempts;
    long hardware_scroll_alloc_failures;
    long hardware_scroll_alloc_width;
    long hardware_scroll_alloc_failed_buffer;
    long hardware_scroll_alloc_failed_plane;
    long amiga_chip_avail_before_scroll_alloc;
    long amiga_chip_largest_before_scroll_alloc;
} ANA_RenderStats;

typedef void (*ANA_RedrawCallback)(ANA_Rect rect, void* user_data);
typedef unsigned char (*ANA_TileReadCallback)(
    int tx,
    int ty,
    void* user_data);
typedef void (*ANA_TileDrawCallback)(
    unsigned char tile,
    int x,
    int y,
    void* user_data);

typedef enum ANA_LayerKind {
    ANA_LAYER_STATIC = 0,
    ANA_LAYER_SIDE_SCROLL = 1,
    ANA_LAYER_VERTICAL_SCROLL = 2,
    ANA_LAYER_PARALLAX = 3,
    ANA_LAYER_SPRITES = 4,
    ANA_LAYER_HUD = 5,
    ANA_LAYER_DEBUG = 6,
    ANA_LAYER_MENU = 7,
    ANA_LAYER_TILE_4WAY = 8,
    ANA_LAYER_RAYCAST_VIEW = 9
} ANA_LayerKind;

typedef enum ANA_ScrollBackend {
    /* Let ANA choose the fastest available backend for the layer. */
    ANA_SCROLL_BACKEND_AUTO = 0,
    /* Portable redraw path; useful for comparisons and conservative builds. */
    ANA_SCROLL_BACKEND_SOFTWARE = 1,
    /*
     * Transitional Amiga direct-present blitter-visible-bitmap scroll bridge.
     * This is useful for backend experiments, but it is not true hardware
     * fine scroll.
     */
    ANA_SCROLL_BACKEND_NATIVE = 2,
    /*
     * Dedicated native hardware scroll path. If it is unavailable, ANA uses a
     * conservative software tile-layer path instead of falling through to the
     * experimental bridge.
     */
    ANA_SCROLL_BACKEND_HARDWARE = 3
} ANA_ScrollBackend;

typedef enum ANA_ScrollSync {
    /* Let ANA choose a conservative source-buffer sync policy. */
    ANA_SCROLL_SYNC_AUTO = 0,
    /* Keep the chunky source buffer synchronized after scrolls. */
    ANA_SCROLL_SYNC_CHUNKY = 1,
    /* Redraw exposed and dirty regions from callbacks after native scrolls. */
    ANA_SCROLL_SYNC_DIRTY = 2
} ANA_ScrollSync;

typedef struct ANA_RetainedLayer {
    ANA_RedrawCallback redraw;
    void* user_data;
} ANA_RetainedLayer;

typedef struct ANA_Bob {
    ANA_Image image;
    ANA_Image previous_image;
    int frame;
    int x;
    int y;
    int previous_x;
    int previous_y;
    int previous_frame;
    int previous_w;
    int previous_h;
    int visible;
    int previous_visible;
    unsigned char clear_color;
} ANA_Bob;

typedef struct ANA_Layer {
    ANA_RedrawCallback redraw;
    void* user_data;
    int dirty;
    ANA_LayerKind kind;
    ANA_RenderMode render_mode;
    ANA_Rect viewport;
    ANA_Camera camera;
    int z_order;
    int disabled;
} ANA_Layer;

typedef ANA_Layer ANA_DrawLayer;

typedef struct ANA_TileLayer {
    ANA_Layer layer;
    ANA_TileReadCallback read_tile;
    ANA_TileDrawCallback draw_tile;
    void* user_data;
    int tile_width;
    int tile_height;
    int map_width;
    int map_height;
    int previous_camera_x;
    int previous_camera_y;
    ANA_ScrollBackend scroll_backend;
    ANA_ScrollSync scroll_sync;
    int native_scroll_active;
    int hardware_scroll_active;
    unsigned char clear_color;
} ANA_TileLayer;

typedef struct ANA_Label {
    ANA_Font font;
    int x;
    int y;
    unsigned char color;
    unsigned char clear_color;
    int clear_width;
    char text[32];
    int dirty;
} ANA_Label;

#ifdef ANA_TARGET_AMIGA
struct SimpleSprite;

typedef struct ANA_AmigaSpriteUpdateStats {
    long position_checks;
    long position_mismatches;
    long zero_control_words;
    long raster_checks;
    long visible_raster_writes;
    long safe_wait_calls;
    long safe_top_hits;
    long safe_bottom_hits;
    long safe_visible_waits;
    long safe_write_waits;
    long span_checks;
    long span_waits;
    long unsafe_span_writes;
    int min_raster_line;
    int max_raster_line;
    int last_raster_line;
} ANA_AmigaSpriteUpdateStats;
#endif

void ana_set_palette(const ANA_Color* colors, int count);
void ana_clear(unsigned char color_index);
void ana_fill_rect(unsigned char color_index, int x, int y, int width, int height);
void ana_scroll_rect(
    int x,
    int y,
    int width,
    int height,
    int dx,
    int dy,
    unsigned char clear_color);
void ana_present(void);
ANA_RenderStats ana_render_stats(void);
void* ana_gfx_native_viewport(void);

#ifdef ANA_TARGET_AMIGA
void ana_amiga_sprite_update_stats_reset(ANA_AmigaSpriteUpdateStats* stats);
void ana_amiga_sprite_wait_until_safe(
    const struct SimpleSprite* sprite,
    int y,
    int height,
    ANA_AmigaSpriteUpdateStats* stats);
void ana_amiga_sprite_record_write_raster(
    const struct SimpleSprite* sprite,
    int y,
    int height,
    int trace_raster,
    ANA_AmigaSpriteUpdateStats* stats);
void ana_amiga_sprite_copy_control_words(
    const struct SimpleSprite* sprite,
    unsigned short* target_data);
void ana_amiga_sprite_expected_control_words(
    int x,
    int y,
    int height,
    unsigned short* word0,
    unsigned short* word1);
void ana_amiga_sprite_set_position_safe(
    struct SimpleSprite* sprite,
    int x,
    int y,
    int height,
    int trace_raster,
    ANA_AmigaSpriteUpdateStats* stats);
void ana_amiga_sprite_record_control_check(
    const struct SimpleSprite* sprite,
    int x,
    int y,
    int height,
    ANA_AmigaSpriteUpdateStats* stats);
#endif

ANA_Image ana_load_image(const char* path);
ANA_Image ana_load_image_data(const unsigned char* bytes, long size);
void ana_free_image(ANA_Image image);
void ana_draw_image(ANA_Image image, int x, int y);
void ana_draw_image_frame(ANA_Image image, int frame, int x, int y);
int ana_image_width(ANA_Image image);
int ana_image_height(ANA_Image image);
int ana_image_frame_count(ANA_Image image);
int ana_image_pixel_index(ANA_Image image, int frame, int x, int y);
int ana_image_pixel_visible(ANA_Image image, int frame, int x, int y);

ANA_Rect ana_retained_clear_rect(
    ANA_Rect rect,
    unsigned char clear_color,
    const ANA_RetainedLayer* layers,
    int layer_count);
ANA_Rect ana_retained_clear_rect_x8(
    ANA_Rect rect,
    unsigned char clear_color,
    int min_x,
    int max_x,
    const ANA_RetainedLayer* layers,
    int layer_count);

void ana_bob_init(ANA_Bob* bob, ANA_Image image);
void ana_bob_set_image(ANA_Bob* bob, ANA_Image image);
void ana_bob_set_position(ANA_Bob* bob, int x, int y);
void ana_bob_set_frame(ANA_Bob* bob, int frame);
void ana_bob_set_visible(ANA_Bob* bob, int visible);
int ana_bob_is_unchanged(const ANA_Bob* bob);
ANA_Rect ana_bob_rect(const ANA_Bob* bob);
ANA_Rect ana_bob_previous_rect(const ANA_Bob* bob);
void ana_bob_clear_previous(const ANA_Bob* bob);
void ana_bob_clear_previous_with_layers(
    const ANA_Bob* bob,
    const ANA_RetainedLayer* layers,
    int layer_count);
ANA_Rect ana_bob_clear_previous_x8_with_layers(
    const ANA_Bob* bob,
    int min_x,
    int max_x,
    const ANA_RetainedLayer* layers,
    int layer_count);
ANA_Rect ana_bob_clear_previous_masked_x8_with_layers(
    const ANA_Bob* bob,
    int min_x,
    int max_x,
    const ANA_RetainedLayer* layers,
    int layer_count);
void ana_bob_draw(const ANA_Bob* bob);
void ana_bob_commit(ANA_Bob* bob);

ANA_Font ana_load_font(const char* path);
ANA_Font ana_load_font_data(const unsigned char* bytes, long size);
void ana_free_font(ANA_Font font);
void ana_set_font_color(ANA_Font font, unsigned char color_index);
void ana_draw_text(ANA_Font font, int x, int y, const char* text);
void ana_draw_int(ANA_Font font, int x, int y, int value);
int ana_text_width(ANA_Font font, const char* text);

ANA_RenderMode ana_layer_default_render_mode(ANA_LayerKind kind);
void ana_layer_init(ANA_Layer* layer, ANA_LayerKind kind, int z_order);
void ana_layer_set_viewport(ANA_Layer* layer, ANA_Rect viewport);
ANA_Rect ana_layer_viewport(const ANA_Layer* layer);
void ana_layer_set_camera(ANA_Layer* layer, const ANA_Camera* camera);
ANA_Camera ana_layer_camera(const ANA_Layer* layer);
void ana_layer_set_redraw(
    ANA_Layer* layer,
    ANA_RedrawCallback redraw,
    void* user_data);
void ana_layer_set_enabled(ANA_Layer* layer, int enabled);
int ana_layer_is_enabled(const ANA_Layer* layer);
int ana_layer_is_dirty(const ANA_Layer* layer);
void ana_layer_mark_dirty(ANA_Layer* layer);
void ana_layer_draw_if_dirty(ANA_Layer* layer);

void ana_tile_layer_init(
    ANA_TileLayer* tile_layer,
    ANA_LayerKind kind,
    int z_order,
    int tile_width,
    int tile_height,
    int map_width,
    int map_height);
void ana_tile_layer_set_callbacks(
    ANA_TileLayer* tile_layer,
    ANA_TileReadCallback read_tile,
    ANA_TileDrawCallback draw_tile,
    void* user_data);
void ana_tile_layer_set_viewport(ANA_TileLayer* tile_layer, ANA_Rect viewport);
void ana_tile_layer_set_camera(
    ANA_TileLayer* tile_layer,
    const ANA_Camera* camera);
void ana_tile_layer_set_clear_color(
    ANA_TileLayer* tile_layer,
    unsigned char clear_color);
void ana_tile_layer_set_scroll_backend(
    ANA_TileLayer* tile_layer,
    ANA_ScrollBackend backend);
ANA_ScrollBackend ana_tile_layer_scroll_backend(const ANA_TileLayer* tile_layer);
void ana_tile_layer_set_scroll_sync(
    ANA_TileLayer* tile_layer,
    ANA_ScrollSync sync);
ANA_ScrollSync ana_tile_layer_scroll_sync(const ANA_TileLayer* tile_layer);
int ana_tile_layer_native_scroll_available(const ANA_TileLayer* tile_layer);
int ana_tile_layer_native_scroll_active(const ANA_TileLayer* tile_layer);
int ana_tile_layer_hardware_scroll_available(const ANA_TileLayer* tile_layer);
int ana_tile_layer_hardware_scroll_active(const ANA_TileLayer* tile_layer);
int ana_tile_layer_hardware_scroll_frame_slot(const ANA_TileLayer* tile_layer);
int ana_tile_layer_hardware_scroll_update_view(ANA_TileLayer* tile_layer);
void ana_tile_layer_invalidate(ANA_TileLayer* tile_layer);
void ana_tile_layer_draw(ANA_TileLayer* tile_layer);
void ana_tile_layer_redraw_world_rect(
    ANA_TileLayer* tile_layer,
    ANA_Rect world_rect);
void ana_tile_layer_restore_world_rect(
    ANA_TileLayer* tile_layer,
    ANA_Rect world_rect);

void ana_label_init(
    ANA_Label* label,
    ANA_Font font,
    int x,
    int y,
    int clear_width);
void ana_label_set_text(ANA_Label* label, const char* text);
void ana_label_draw_if_dirty(ANA_Label* label);

#ifdef __cplusplus
}
#endif

#endif

# ANA 0.1 API overview

This is the public API surface for the 0.1 release. Later releases may still
revise names and add higher-level helpers as the framework matures.

Include ANA with:

```c
#include <ana.h>
```

## Runtime

Core types and functions:

- `ANA_Game`
- `ANA_Time`
- `ana_run`
- `ana_quit`
- `ana_last_run_stats`

`ANA_Game` owns the high-level lifecycle. Prefer zero-initializing it with
`ANA_Game game = {0};` before assigning fields, so future optional fields default
to off:

- `init`
- `load`
- `update`
- `draw`
- `shutdown`

The runtime currently uses a fixed tick model. `ANA_Time.tick` increases once
per simulated frame. `ANA_Time.fps` is the configured target rate.

Set `ANA_Game.debug_stats` to `1` to ask the runtime to print full run/render
statistics after `ana_run`. The flag only has an effect when ANA is built with
`ANA_DEBUG_STATS`.

Set `ANA_Game.render_mode` to describe the renderer strategy the game expects:

- `ANA_RENDER_DIRTY`: static viewport with retained/dirty moving objects.
- `ANA_RENDER_FULL_FRAME`: simple full redraw every frame.
- `ANA_RENDER_BLITTER_BOBS`: future mode for BOB-heavy renderers that want a
  blitter-oriented backend.
- `ANA_RENDER_TILE_SCROLL`: legacy/generic tile-scroll contract.
- `ANA_RENDER_SIDE_SCROLL`: side-scrolling tilemap/playfield games.
- `ANA_RENDER_VERTICAL_SCROLL`: vertical shooters, runners, and upward/downward
  strip scrolling.
- `ANA_RENDER_TILE_4WAY`: four-direction tilemap games such as maze crawlers or
  Gauntlet-like examples.
- `ANA_RENDER_RAYCAST`: future first-person/raycast view contract.

`ANA_RENDER_DEFAULT` is accepted on `ANA_Game` and resolves to
`ANA_RENDER_DIRTY`. The scrolling and raycast modes are currently contracts and
profiling signals; specialized Amiga backends are still planned work.

## Platform profile

The 0.1 profile is PAL lores:

- width: `320`
- height: `256`
- fps: `50`
- colors: `16`
- bitplanes: `4`
- screen mode: `ANA_SCREEN_PAL_LORES`
- default render mode: `ANA_RENDER_DIRTY`

Useful constants:

- `ANA_DEFAULT_WIDTH`
- `ANA_DEFAULT_HEIGHT`
- `ANA_DEFAULT_FPS`
- `ANA_DEFAULT_COLORS`
- `ANA_SCREEN_PAL_LORES`
- `ANA_RENDER_DIRTY`
- `ANA_RENDER_TILE_SCROLL`
- `ANA_RENDER_SIDE_SCROLL`
- `ANA_RENDER_VERTICAL_SCROLL`
- `ANA_RENDER_TILE_4WAY`

Validation helpers:

- `ana_default_profile`
- `ana_validate_profile`
- `ana_profile_is_supported`

## Graphics

Core drawing functions:

- `ana_set_palette`
- `ana_clear`
- `ana_fill_rect`
- `ana_present`
- `ana_render_stats`

When a game runs through `ana_run`, the runtime calls `ana_present` after each
`draw` callback. Normal game code should not call `ana_present` from `draw`;
doing so presents twice for one game update and breaks frame pacing. Direct
`ana_present` calls are reserved for lower-level/manual loops and tests.

Images:

- `ANA_Image`
- `ana_load_image`
- `ana_load_image_data`
- `ana_free_image`
- `ana_draw_image`
- `ana_draw_image_frame`
- `ana_image_width`
- `ana_image_height`
- `ana_image_frame_count`

Fonts:

- `ANA_Font`
- `ana_load_font`
- `ana_load_font_data`
- `ana_free_font`
- `ana_set_font_color`
- `ana_draw_text`
- `ana_draw_int`
- `ana_text_width`

Images and fonts are expected to be converted before runtime. The current file
formats are `.anaimg` and `.anafnt`.

Retained rendering helpers:

- `ANA_Bob`
- `ana_bob_init`
- `ana_bob_set_image`
- `ana_bob_set_position`
- `ana_bob_set_frame`
- `ana_bob_set_visible`
- `ana_bob_is_unchanged`
- `ana_bob_rect`
- `ana_bob_previous_rect`
- `ana_bob_clear_previous`
- `ana_bob_clear_previous_with_layers`
- `ana_bob_clear_previous_x8_with_layers`
- `ana_bob_clear_previous_masked_x8_with_layers`
- `ana_bob_draw`
- `ana_bob_commit`
- `ANA_RetainedLayer`
- `ana_retained_clear_rect`
- `ana_retained_clear_rect_x8`
- `ANA_LayerKind`
- `ANA_Layer`
- `ANA_DrawLayer`
- `ana_layer_default_render_mode`
- `ana_layer_init`
- `ana_layer_set_viewport`
- `ana_layer_viewport`
- `ana_layer_set_camera`
- `ana_layer_camera`
- `ana_layer_set_redraw`
- `ana_layer_set_enabled`
- `ana_layer_is_enabled`
- `ana_layer_is_dirty`
- `ana_layer_mark_dirty`
- `ana_layer_draw_if_dirty`
- `ANA_TileReadCallback`
- `ANA_TileDrawCallback`
- `ANA_TileLayer`
- `ana_tile_layer_init`
- `ana_tile_layer_set_callbacks`
- `ana_tile_layer_set_viewport`
- `ana_tile_layer_set_camera`
- `ana_tile_layer_set_clear_color`
- `ana_tile_layer_invalidate`
- `ana_tile_layer_draw`
- `ana_tile_layer_redraw_world_rect`
- `ANA_Label`
- `ana_label_init`
- `ana_label_set_text`
- `ana_label_draw_if_dirty`

`ANA_DrawLayer` is kept as a compatibility typedef for `ANA_Layer`.
Layer kinds let a game describe whether a layer is static, scrolling, sprites,
HUD, debug, or menu. `ANA_TileLayer` is the first tilemap-owned draw helper. It
stores viewport, camera, tile size, clear color, and tile read/draw callbacks,
and can redraw exposed strips when the camera moves. The current Amiga
implementation still uses ANA's chunky/software path, so native hardware
scroll remains backend work. These helpers keep rendering intent explicit. They
do not replace a game's draw order, collision rules, or custom performance work
yet.

## Input

ANA uses semantic input names rather than device-specific names.

Directions:

- `ANA_INPUT_LEFT`
- `ANA_INPUT_RIGHT`
- `ANA_INPUT_UP`
- `ANA_INPUT_DOWN`

Actions:

- `ANA_ACTION_1`
- `ANA_ACTION_2`
- `ANA_ACTION_3`
- `ANA_ACTION_4`

Devices:

- `ANA_INPUT_DEVICE_0`
- `ANA_INPUT_DEVICE_1`

Polling:

- `ana_input_direction`
- `ana_input_direction_pressed`
- `ana_input_direction_released`
- `ana_input_action`
- `ana_input_action_pressed`
- `ana_input_action_released`
- `ana_quit_requested`

Keyboard mapping:

- `ana_input_clear_key_map`
- `ana_input_map_key_to_direction`
- `ana_input_map_key_to_action`
- `ana_input_map_key_to_quit`
- `ana_input_map_default_keys`

The intent is that joystick, keyboard, and later gamepad-style input can map to
the same game-facing directions and actions.

## Sound

0.1 sound is focused on short sound effects plus explicit Paula channel policy:

- `ANA_Sound`
- `ana_load_sound`
- `ana_load_sound_data`
- `ana_free_sound`
- `ana_play_sound`
- `ana_stop_all_sounds`
- `ana_set_sound_volume`
- `ANA_AudioConfig`
- `ana_configure_audio`
- `ana_audio_config`

Music API for MOD assets:

- `ANA_Music`
- `ana_load_music`
- `ana_load_music_data`
- `ana_free_music`
- `ana_play_music`
- `ana_stop_music`
- `ana_pause_music`
- `ana_resume_music`
- `ana_set_music_volume`

The host implementation validates and stores MOD data and applies channel
policy state for tests. The Amiga implementation uses a vendored ProTracker
replayer to play MOD data through Paula.

## Helpers

Low-level graphics helpers:

- `ana_scroll_rect`

`ana_scroll_rect` moves an existing screen rectangle by a signed delta and
fills newly exposed strips with a clear color. It is intended as the first
portable primitive for scrolling samples.

Tilemap scrolling is exposed through `ANA_TileLayer`. The game supplies tile
read/draw callbacks and ANA owns camera movement, exposed strip redraw, and
backend selection:

- `ana_tile_layer_set_scroll_backend`
- `ana_tile_layer_scroll_backend`
- `ana_tile_layer_native_scroll_available`
- `ana_tile_layer_native_scroll_active`
- `ana_tile_layer_hardware_scroll_available`
- `ana_tile_layer_hardware_scroll_active`

Use `ANA_SCROLL_BACKEND_HARDWARE` when a scrolling tile layer should use a
dedicated Amiga hardware-scroll path. On eligible A1200 direct-present
side-scroll layers this path uses a wide planar playfield bitmap plus
`RasInfo->RxOffset` camera scroll, direct planar `ana_fill_rect` drawing for
playfield/actors, and a small planar HUD cache for fixed screen layers. Layers
that opt into `ANA_SCROLL_SYNC_DIRTY` can skip chunky-shadow synchronization in
this path. `SOFTWARE` forces portable redraw, and `NATIVE` explicitly requests
the older direct-present visible-bitmap bridge for backend experiments. The
next high-performance step is a bounded overdraw or ringbuffer playfield with
blitter-updated incoming tile strips.

Scrolling games should choose the most specific render mode available:
`ANA_RENDER_SIDE_SCROLL`, `ANA_RENDER_VERTICAL_SCROLL`, or
`ANA_RENDER_TILE_4WAY`. `ANA_RENDER_TILE_SCROLL` remains available as a generic
compatibility contract. These modes make game intent visible to the runtime,
docs, tests, and backend selection.

Small game helpers:

- `ANA_Rect`
- `ANA_Camera`
- `ANA_Timer`
- `ana_rect_make`
- `ana_rect_clip`
- `ana_rect_union`
- `ana_rect_align_x8`
- `ana_rect_is_empty`
- `ana_rect_contains`
- `ana_rect_intersects`
- `ana_clamp_int`
- `ana_camera_init`
- `ana_camera_set_snap`
- `ana_camera_set_position`
- `ana_camera_scroll_by`
- `ana_camera_follow_rect`
- `ana_camera_world_view`
- `ana_camera_world_to_screen_rect`
- `ana_timer_reset`
- `ana_timer_tick`

These are intentionally simple. More complex game logic should stay in the game
unless it becomes common enough to justify framework support.

## Error results

Result values:

- `ANA_OK`
- `ANA_ERROR_INVALID_ARGUMENT`
- `ANA_ERROR_UNSUPPORTED_PROFILE`
- `ANA_ERROR_NOT_IMPLEMENTED`

Use `ana_result_name` for readable output.

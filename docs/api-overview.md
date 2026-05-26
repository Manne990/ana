# ANA 0.1 API overview

This is the public API surface that is stable enough to document for the 0.1
preview. Names may still change before a final 0.1 release.

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

## Platform profile

The 0.1 profile is PAL lores:

- width: `320`
- height: `256`
- fps: `50`
- colors: `16`
- bitplanes: `4`
- screen mode: `ANA_SCREEN_PAL_LORES`

Useful constants:

- `ANA_DEFAULT_WIDTH`
- `ANA_DEFAULT_HEIGHT`
- `ANA_DEFAULT_FPS`
- `ANA_DEFAULT_COLORS`
- `ANA_SCREEN_PAL_LORES`

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
- `ana_bob_draw`
- `ana_bob_commit`
- `ANA_RetainedLayer`
- `ana_retained_clear_rect`
- `ana_retained_clear_rect_x8`
- `ANA_DrawLayer`
- `ana_layer_mark_dirty`
- `ana_layer_draw_if_dirty`
- `ANA_Label`
- `ana_label_init`
- `ana_label_set_text`
- `ana_label_draw_if_dirty`

These helpers keep dirty rendering explicit. They do not replace a game's draw
order, collision rules, or custom performance work.

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

Small game helpers:

- `ANA_Rect`
- `ANA_Timer`
- `ana_rect_make`
- `ana_rect_clip`
- `ana_rect_union`
- `ana_rect_align_x8`
- `ana_rect_is_empty`
- `ana_rect_contains`
- `ana_rect_intersects`
- `ana_clamp_int`
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

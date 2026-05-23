#ifndef ANA_INTERNAL_H
#define ANA_INTERNAL_H

#include "ana.h"

#define ANA_INPUT_LEFT_MASK (1u << 0)
#define ANA_INPUT_RIGHT_MASK (1u << 1)
#define ANA_INPUT_UP_MASK (1u << 2)
#define ANA_INPUT_DOWN_MASK (1u << 3)
#define ANA_ACTION_1_MASK (1u << 4)
#define ANA_ACTION_2_MASK (1u << 5)
#define ANA_ACTION_3_MASK (1u << 6)
#define ANA_ACTION_4_MASK (1u << 7)

ANA_Result ana_gfx_open(const ANA_Profile* profile);
void ana_gfx_close(void);
int ana_gfx_is_open(void);
int ana_gfx_present_count(void);
unsigned char ana_gfx_front_pixel(int x, int y);
unsigned char ana_gfx_draw_pixel(int x, int y);
void* ana_gfx_native_window(void);

void ana_input_reset(void);
unsigned int ana_input_state_from_amiga_joydat(
    unsigned short joydat,
    int fire_down);
void ana_input_set_pending_state(ANA_InputDevice device, unsigned int state);
void ana_input_set_pending_key_state(ANA_Key key, int is_down);
void ana_input_set_pending_quit(int requested);

ANA_Result ana_sound_open(const ANA_Profile* profile);
void ana_sound_close(void);
void ana_sound_update(void);
int ana_sound_active_channel_count(void);
int ana_sound_global_volume(void);

long ana_platform_time_ticks(void);
long ana_platform_time_ticks_per_second(void);
unsigned long ana_platform_perf_ticks(void);
unsigned long ana_platform_perf_ticks_per_second(void);
void ana_platform_wait_until_time_tick(long target_tick);

#endif

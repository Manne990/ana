#ifndef ANA_INTERNAL_H
#define ANA_INTERNAL_H

#include "ana.h"

#define ANA_INPUT_JOY_LEFT_MASK (1u << 0)
#define ANA_INPUT_JOY_RIGHT_MASK (1u << 1)
#define ANA_INPUT_JOY_UP_MASK (1u << 2)
#define ANA_INPUT_JOY_DOWN_MASK (1u << 3)
#define ANA_INPUT_JOY_FIRE_MASK (1u << 4)
#define ANA_INPUT_JOY_START_MASK (1u << 5)

ANA_Result ana_gfx_open(const ANA_Profile* profile);
void ana_gfx_close(void);
int ana_gfx_is_open(void);
int ana_gfx_present_count(void);
unsigned char ana_gfx_front_pixel(int x, int y);
unsigned char ana_gfx_draw_pixel(int x, int y);

void ana_input_reset(void);
void ana_input_set_pending_joy_state(int port, unsigned int state);
void ana_input_set_pending_quit(int requested);

#endif


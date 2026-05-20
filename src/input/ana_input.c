#include "ana/ana_input.h"

#include "ana_internal.h"

static unsigned int ana_current_joy[ANA_JOY_PORTS];
static unsigned int ana_previous_joy[ANA_JOY_PORTS];
static unsigned int ana_pending_joy[ANA_JOY_PORTS];
static int ana_current_quit_requested = 0;
static int ana_pending_quit_requested = 0;

static unsigned int ana_joy_button_mask(ANA_JoyButton button)
{
    switch (button) {
    case ANA_JOY_LEFT:
        return ANA_INPUT_JOY_LEFT_MASK;
    case ANA_JOY_RIGHT:
        return ANA_INPUT_JOY_RIGHT_MASK;
    case ANA_JOY_UP:
        return ANA_INPUT_JOY_UP_MASK;
    case ANA_JOY_DOWN:
        return ANA_INPUT_JOY_DOWN_MASK;
    case ANA_JOY_FIRE:
        return ANA_INPUT_JOY_FIRE_MASK;
    case ANA_JOY_START:
        return ANA_INPUT_JOY_START_MASK;
    default:
        return 0u;
    }
}

static int ana_valid_port(int port)
{
    return port >= 0 && port < ANA_JOY_PORTS;
}

void ana_input_reset(void)
{
    int i;

    for (i = 0; i < ANA_JOY_PORTS; i++) {
        ana_current_joy[i] = 0u;
        ana_previous_joy[i] = 0u;
        ana_pending_joy[i] = 0u;
    }

    ana_current_quit_requested = 0;
    ana_pending_quit_requested = 0;
}

void ana_input_update(void)
{
    int i;

    for (i = 0; i < ANA_JOY_PORTS; i++) {
        ana_previous_joy[i] = ana_current_joy[i];
        ana_current_joy[i] = ana_pending_joy[i];
    }

    ana_current_quit_requested = ana_pending_quit_requested;
}

int ana_joy_down(int port, ANA_JoyButton button)
{
    unsigned int mask;

    if (!ana_valid_port(port)) {
        return 0;
    }

    mask = ana_joy_button_mask(button);
    if (mask == 0u) {
        return 0;
    }

    return (ana_current_joy[port] & mask) != 0u;
}

int ana_joy_pressed(int port, ANA_JoyButton button)
{
    unsigned int mask;

    if (!ana_valid_port(port)) {
        return 0;
    }

    mask = ana_joy_button_mask(button);
    if (mask == 0u) {
        return 0;
    }

    return ((ana_current_joy[port] & mask) != 0u) &&
        ((ana_previous_joy[port] & mask) == 0u);
}

int ana_joy_released(int port, ANA_JoyButton button)
{
    unsigned int mask;

    if (!ana_valid_port(port)) {
        return 0;
    }

    mask = ana_joy_button_mask(button);
    if (mask == 0u) {
        return 0;
    }

    return ((ana_current_joy[port] & mask) == 0u) &&
        ((ana_previous_joy[port] & mask) != 0u);
}

int ana_quit_requested(void)
{
    return ana_current_quit_requested;
}

void ana_input_set_pending_joy_state(int port, unsigned int state)
{
    if (!ana_valid_port(port)) {
        return;
    }

    ana_pending_joy[port] = state;
}

void ana_input_set_pending_quit(int requested)
{
    ana_pending_quit_requested = requested != 0;
}


#include "ana/ana_input.h"

#include "ana_internal.h"

static unsigned int ana_current_input[ANA_INPUT_DEVICES];
static unsigned int ana_previous_input[ANA_INPUT_DEVICES];
static unsigned int ana_pending_input[ANA_INPUT_DEVICES];
static int ana_current_quit_requested = 0;
static int ana_pending_quit_requested = 0;

static unsigned int ana_input_direction_mask(ANA_InputDirection direction)
{
    switch (direction) {
    case ANA_INPUT_LEFT:
        return ANA_INPUT_LEFT_MASK;
    case ANA_INPUT_RIGHT:
        return ANA_INPUT_RIGHT_MASK;
    case ANA_INPUT_UP:
        return ANA_INPUT_UP_MASK;
    case ANA_INPUT_DOWN:
        return ANA_INPUT_DOWN_MASK;
    default:
        return 0u;
    }
}

static unsigned int ana_input_action_mask(ANA_InputAction action)
{
    switch (action) {
    case ANA_ACTION_1:
        return ANA_ACTION_1_MASK;
    case ANA_ACTION_2:
        return ANA_ACTION_2_MASK;
    case ANA_ACTION_3:
        return ANA_ACTION_3_MASK;
    case ANA_ACTION_4:
        return ANA_ACTION_4_MASK;
    default:
        return 0u;
    }
}

static int ana_valid_device(ANA_InputDevice device)
{
    return device >= 0 && device < ANA_INPUT_DEVICES;
}

static int ana_input_mask_down(ANA_InputDevice device, unsigned int mask)
{
    if (!ana_valid_device(device) || mask == 0u) {
        return 0;
    }

    return (ana_current_input[device] & mask) != 0u;
}

static int ana_input_mask_pressed(ANA_InputDevice device, unsigned int mask)
{
    if (!ana_valid_device(device) || mask == 0u) {
        return 0;
    }

    return ((ana_current_input[device] & mask) != 0u) &&
        ((ana_previous_input[device] & mask) == 0u);
}

static int ana_input_mask_released(ANA_InputDevice device, unsigned int mask)
{
    if (!ana_valid_device(device) || mask == 0u) {
        return 0;
    }

    return ((ana_current_input[device] & mask) == 0u) &&
        ((ana_previous_input[device] & mask) != 0u);
}

void ana_input_reset(void)
{
    int i;

    for (i = 0; i < ANA_INPUT_DEVICES; i++) {
        ana_current_input[i] = 0u;
        ana_previous_input[i] = 0u;
        ana_pending_input[i] = 0u;
    }

    ana_current_quit_requested = 0;
    ana_pending_quit_requested = 0;
}

void ana_input_update(void)
{
    int i;

    for (i = 0; i < ANA_INPUT_DEVICES; i++) {
        ana_previous_input[i] = ana_current_input[i];
        ana_current_input[i] = ana_pending_input[i];
    }

    ana_current_quit_requested = ana_pending_quit_requested;
}

int ana_input_direction(ANA_InputDevice device, ANA_InputDirection direction)
{
    return ana_input_mask_down(device, ana_input_direction_mask(direction));
}

int ana_input_direction_pressed(ANA_InputDevice device, ANA_InputDirection direction)
{
    return ana_input_mask_pressed(device, ana_input_direction_mask(direction));
}

int ana_input_direction_released(ANA_InputDevice device, ANA_InputDirection direction)
{
    return ana_input_mask_released(device, ana_input_direction_mask(direction));
}

int ana_input_action(ANA_InputDevice device, ANA_InputAction action)
{
    return ana_input_mask_down(device, ana_input_action_mask(action));
}

int ana_input_action_pressed(ANA_InputDevice device, ANA_InputAction action)
{
    return ana_input_mask_pressed(device, ana_input_action_mask(action));
}

int ana_input_action_released(ANA_InputDevice device, ANA_InputAction action)
{
    return ana_input_mask_released(device, ana_input_action_mask(action));
}

int ana_quit_requested(void)
{
    return ana_current_quit_requested;
}

void ana_input_set_pending_state(ANA_InputDevice device, unsigned int state)
{
    if (!ana_valid_device(device)) {
        return;
    }

    ana_pending_input[device] = state;
}

void ana_input_set_pending_quit(int requested)
{
    ana_pending_quit_requested = requested != 0;
}

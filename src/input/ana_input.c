#include "ana/ana_input.h"

#include "ana_internal.h"

static unsigned int ana_current_input[ANA_INPUT_DEVICES];
static unsigned int ana_previous_input[ANA_INPUT_DEVICES];
static unsigned int ana_pending_input[ANA_INPUT_DEVICES];
static int ana_pending_key_state[ANA_KEY_COUNT];
static unsigned int ana_key_direction_map[ANA_KEY_COUNT][ANA_INPUT_DEVICES];
static unsigned int ana_key_action_map[ANA_KEY_COUNT][ANA_INPUT_DEVICES];
static int ana_key_quit_map[ANA_KEY_COUNT];
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

static int ana_valid_key(ANA_Key key)
{
    return key > ANA_KEY_UNKNOWN && key < ANA_KEY_COUNT;
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

static unsigned int ana_input_mapped_key_state(ANA_InputDevice device)
{
    ANA_Key key;
    unsigned int state;

    state = 0u;

    for (key = ANA_KEY_UNKNOWN; key < ANA_KEY_COUNT; key++) {
        if (ana_pending_key_state[key]) {
            state |= ana_key_direction_map[key][device];
            state |= ana_key_action_map[key][device];
        }
    }

    return state;
}

static int ana_input_mapped_quit_requested(void)
{
    ANA_Key key;

    for (key = ANA_KEY_UNKNOWN; key < ANA_KEY_COUNT; key++) {
        if (ana_pending_key_state[key] && ana_key_quit_map[key]) {
            return 1;
        }
    }

    return 0;
}

void ana_input_reset(void)
{
    int i;

    for (i = 0; i < ANA_INPUT_DEVICES; i++) {
        ana_current_input[i] = 0u;
        ana_previous_input[i] = 0u;
        ana_pending_input[i] = 0u;
    }

    for (i = 0; i < ANA_KEY_COUNT; i++) {
        ana_pending_key_state[i] = 0;
    }

    ana_input_clear_key_map();
    ana_input_map_default_keys(ANA_INPUT_DEVICE_0);

    ana_current_quit_requested = 0;
    ana_pending_quit_requested = 0;
}

void ana_input_update(void)
{
    int i;

    for (i = 0; i < ANA_INPUT_DEVICES; i++) {
        ana_previous_input[i] = ana_current_input[i];
        ana_current_input[i] = ana_pending_input[i] |
            ana_input_mapped_key_state((ANA_InputDevice)i);
    }

    ana_current_quit_requested = ana_pending_quit_requested ||
        ana_input_mapped_quit_requested();
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

void ana_input_clear_key_map(void)
{
    int key;
    int device;

    for (key = 0; key < ANA_KEY_COUNT; key++) {
        ana_key_quit_map[key] = 0;

        for (device = 0; device < ANA_INPUT_DEVICES; device++) {
            ana_key_direction_map[key][device] = 0u;
            ana_key_action_map[key][device] = 0u;
        }
    }
}

void ana_input_map_key_to_direction(
    ANA_Key key,
    ANA_InputDevice device,
    ANA_InputDirection direction)
{
    unsigned int mask;

    if (!ana_valid_key(key) || !ana_valid_device(device)) {
        return;
    }

    mask = ana_input_direction_mask(direction);
    if (mask == 0u) {
        return;
    }

    ana_key_direction_map[key][device] |= mask;
}

void ana_input_map_key_to_action(
    ANA_Key key,
    ANA_InputDevice device,
    ANA_InputAction action)
{
    unsigned int mask;

    if (!ana_valid_key(key) || !ana_valid_device(device)) {
        return;
    }

    mask = ana_input_action_mask(action);
    if (mask == 0u) {
        return;
    }

    ana_key_action_map[key][device] |= mask;
}

void ana_input_map_key_to_quit(ANA_Key key)
{
    if (!ana_valid_key(key)) {
        return;
    }

    ana_key_quit_map[key] = 1;
}

void ana_input_map_default_keys(ANA_InputDevice device)
{
    if (!ana_valid_device(device)) {
        return;
    }

    ana_input_map_key_to_direction(ANA_KEY_LEFT, device, ANA_INPUT_LEFT);
    ana_input_map_key_to_direction(ANA_KEY_A, device, ANA_INPUT_LEFT);
    ana_input_map_key_to_direction(ANA_KEY_RIGHT, device, ANA_INPUT_RIGHT);
    ana_input_map_key_to_direction(ANA_KEY_D, device, ANA_INPUT_RIGHT);
    ana_input_map_key_to_direction(ANA_KEY_UP, device, ANA_INPUT_UP);
    ana_input_map_key_to_direction(ANA_KEY_W, device, ANA_INPUT_UP);
    ana_input_map_key_to_direction(ANA_KEY_DOWN, device, ANA_INPUT_DOWN);
    ana_input_map_key_to_direction(ANA_KEY_S, device, ANA_INPUT_DOWN);

    ana_input_map_key_to_action(ANA_KEY_SPACE, device, ANA_ACTION_1);
    ana_input_map_key_to_action(ANA_KEY_CTRL, device, ANA_ACTION_1);
    ana_input_map_key_to_action(ANA_KEY_RETURN, device, ANA_ACTION_4);

    ana_input_map_key_to_quit(ANA_KEY_ESCAPE);
}

void ana_input_set_pending_state(ANA_InputDevice device, unsigned int state)
{
    if (!ana_valid_device(device)) {
        return;
    }

    ana_pending_input[device] = state;
}

void ana_input_set_pending_key_state(ANA_Key key, int is_down)
{
    if (!ana_valid_key(key)) {
        return;
    }

    ana_pending_key_state[key] = is_down != 0;
}

void ana_input_set_pending_quit(int requested)
{
    ana_pending_quit_requested = requested != 0;
}

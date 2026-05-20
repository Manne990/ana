#include "ana.h"
#include "ana_internal.h"

#include <assert.h>

static void test_direction_transitions(void)
{
    ana_input_reset();

    ana_input_set_pending_state(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT_MASK);
    ana_input_update();
    assert(ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));
    assert(ana_input_direction_pressed(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));
    assert(!ana_input_direction_released(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));

    ana_input_update();
    assert(ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));
    assert(!ana_input_direction_pressed(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));
    assert(!ana_input_direction_released(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));

    ana_input_set_pending_state(ANA_INPUT_DEVICE_0, 0u);
    ana_input_update();
    assert(!ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));
    assert(!ana_input_direction_pressed(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));
    assert(ana_input_direction_released(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));
}

static void test_action_transitions(void)
{
    ana_input_reset();

    ana_input_set_pending_state(ANA_INPUT_DEVICE_0, ANA_ACTION_1_MASK);
    ana_input_update();
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
    assert(ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
    assert(!ana_input_action_released(ANA_INPUT_DEVICE_0, ANA_ACTION_1));

    ana_input_update();
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
    assert(!ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
    assert(!ana_input_action_released(ANA_INPUT_DEVICE_0, ANA_ACTION_1));

    ana_input_set_pending_state(ANA_INPUT_DEVICE_0, 0u);
    ana_input_update();
    assert(!ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
    assert(!ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
    assert(ana_input_action_released(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
}

static void test_invalid_inputs_are_safe(void)
{
    ana_input_reset();
    ana_input_set_pending_state((ANA_InputDevice)3, ANA_ACTION_1_MASK);
    ana_input_update();

    assert(!ana_input_action((ANA_InputDevice)3, ANA_ACTION_1));
    assert(!ana_input_action_pressed((ANA_InputDevice)3, ANA_ACTION_1));
    assert(!ana_input_action_released((ANA_InputDevice)3, ANA_ACTION_1));
    assert(!ana_input_direction(ANA_INPUT_DEVICE_0, (ANA_InputDirection)99));
    assert(!ana_input_action(ANA_INPUT_DEVICE_0, (ANA_InputAction)99));
}

static void test_quit_request(void)
{
    ana_input_reset();
    assert(!ana_quit_requested());

    ana_input_set_pending_quit(1);
    ana_input_update();
    assert(ana_quit_requested());

    ana_input_set_pending_quit(0);
    ana_input_update();
    assert(!ana_quit_requested());
}

int main(void)
{
    test_direction_transitions();
    test_action_transitions();
    test_invalid_inputs_are_safe();
    test_quit_request();

    return 0;
}

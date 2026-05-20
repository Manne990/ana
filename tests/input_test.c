#include "ana.h"
#include "ana_internal.h"

#include <assert.h>

static void test_joy_transitions(void)
{
    ana_input_reset();

    ana_input_set_pending_joy_state(0, ANA_INPUT_JOY_LEFT_MASK);
    ana_input_update();
    assert(ana_joy_down(0, ANA_JOY_LEFT));
    assert(ana_joy_pressed(0, ANA_JOY_LEFT));
    assert(!ana_joy_released(0, ANA_JOY_LEFT));

    ana_input_update();
    assert(ana_joy_down(0, ANA_JOY_LEFT));
    assert(!ana_joy_pressed(0, ANA_JOY_LEFT));
    assert(!ana_joy_released(0, ANA_JOY_LEFT));

    ana_input_set_pending_joy_state(0, 0u);
    ana_input_update();
    assert(!ana_joy_down(0, ANA_JOY_LEFT));
    assert(!ana_joy_pressed(0, ANA_JOY_LEFT));
    assert(ana_joy_released(0, ANA_JOY_LEFT));
}

static void test_invalid_inputs_are_safe(void)
{
    ana_input_reset();
    ana_input_set_pending_joy_state(3, ANA_INPUT_JOY_FIRE_MASK);
    ana_input_update();

    assert(!ana_joy_down(3, ANA_JOY_FIRE));
    assert(!ana_joy_pressed(3, ANA_JOY_FIRE));
    assert(!ana_joy_released(3, ANA_JOY_FIRE));
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
    test_joy_transitions();
    test_invalid_inputs_are_safe();
    test_quit_request();

    return 0;
}


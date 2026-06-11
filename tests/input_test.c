#include "ana.h"
#include "ana_internal.h"

#include <assert.h>
#include <stddef.h>

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

static void test_default_keyboard_mapping(void)
{
    ana_input_reset();

    ana_input_set_pending_key_state(ANA_KEY_LEFT, 1);
    ana_input_set_pending_key_state(ANA_KEY_SPACE, 1);
    ana_input_update();
    assert(ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));
    assert(ana_input_direction_pressed(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
    assert(ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_1));

    ana_input_set_pending_key_state(ANA_KEY_X, 1);
    ana_input_set_pending_key_state(ANA_KEY_C, 1);
    ana_input_update();
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_3));

    ana_input_set_pending_key_state(ANA_KEY_X, 0);
    ana_input_set_pending_key_state(ANA_KEY_C, 0);
    ana_input_set_pending_key_state(ANA_KEY_LEFT, 0);
    ana_input_set_pending_key_state(ANA_KEY_SPACE, 0);
    ana_input_update();
    assert(!ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));
    assert(ana_input_direction_released(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));
    assert(!ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
    assert(ana_input_action_released(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
}

static void test_default_keyboard_mapping_covers_common_game_keys(void)
{
    ana_input_reset();

    ana_input_set_pending_key_state(ANA_KEY_A, 1);
    ana_input_set_pending_key_state(ANA_KEY_D, 1);
    ana_input_set_pending_key_state(ANA_KEY_W, 1);
    ana_input_set_pending_key_state(ANA_KEY_S, 1);
    ana_input_set_pending_key_state(ANA_KEY_Z, 1);
    ana_input_set_pending_key_state(ANA_KEY_V, 1);
    ana_input_set_pending_key_state(ANA_KEY_RETURN, 1);
    ana_input_set_pending_key_state(ANA_KEY_CTRL, 1);
    ana_input_update();

    assert(ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));
    assert(ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT));
    assert(ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_UP));
    assert(ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_DOWN));
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_3));
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_4));
}

static void test_custom_keyboard_mapping(void)
{
    ana_input_reset();
    ana_input_clear_key_map();

    ana_input_set_pending_key_state(ANA_KEY_A, 1);
    ana_input_update();
    assert(!ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT));

    ana_input_map_key_to_direction(ANA_KEY_A, ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT);
    ana_input_map_key_to_action(ANA_KEY_X, ANA_INPUT_DEVICE_0, ANA_ACTION_2);

    ana_input_update();
    assert(ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT));
    assert(!ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));

    ana_input_set_pending_key_state(ANA_KEY_X, 1);
    ana_input_update();
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
}

static void test_keyboard_quit_mapping(void)
{
    ana_input_reset();

    ana_input_set_pending_key_state(ANA_KEY_ESCAPE, 1);
    ana_input_update();
    assert(ana_quit_requested());

    ana_input_reset();
    ana_input_clear_key_map();
    ana_input_set_pending_key_state(ANA_KEY_ESCAPE, 1);
    ana_input_update();
    assert(ana_quit_requested());

    ana_input_set_pending_key_state(ANA_KEY_ESCAPE, 0);
    ana_input_set_pending_key_state(ANA_KEY_Q, 1);
    ana_input_update();
    assert(!ana_quit_requested());

    ana_input_map_key_to_quit(ANA_KEY_Q);
    ana_input_update();
    assert(ana_quit_requested());

    ana_input_unmap_key_from_quit(ANA_KEY_Q);
    ana_input_update();
    assert(!ana_quit_requested());
}

static void test_keyboard_pulse_events(void)
{
    ana_input_reset();

    ana_input_pulse_key_event(ANA_KEY_X);
    ana_input_update();
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_2));

    ana_input_update();
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(!ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(!ana_input_action_released(ANA_INPUT_DEVICE_0, ANA_ACTION_2));

    ana_input_update();
    assert(!ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(ana_input_action_released(ANA_INPUT_DEVICE_0, ANA_ACTION_2));

    ana_input_map_key_to_quit(ANA_KEY_Q);
    ana_input_pulse_key_event(ANA_KEY_Q);
    ana_input_update();
    assert(ana_quit_requested());

    ana_input_update();
    assert(ana_quit_requested());

    ana_input_update();
    assert(!ana_quit_requested());
}

static void test_reset_clears_keyboard_pulse_events(void)
{
    ana_input_reset();

    ana_input_pulse_key_event(ANA_KEY_X);
    ana_input_map_key_to_quit(ANA_KEY_Q);
    ana_input_pulse_key_event(ANA_KEY_Q);
    ana_input_reset();
    ana_input_update();

    assert(!ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(!ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(!ana_quit_requested());
}

static void test_amiga_joydat_mapping(void)
{
    unsigned int state;

    state = ana_input_state_from_amiga_joydat(0x0300u, 0, 0, 0);
    assert((state & ANA_INPUT_LEFT_MASK) != 0u);
    assert((state & ANA_INPUT_UP_MASK) == 0u);

    state = ana_input_state_from_amiga_joydat(0x0100u, 0, 0, 0);
    assert((state & ANA_INPUT_UP_MASK) != 0u);
    assert((state & ANA_INPUT_LEFT_MASK) == 0u);

    state = ana_input_state_from_amiga_joydat(0x0003u, 0, 0, 0);
    assert((state & ANA_INPUT_RIGHT_MASK) != 0u);
    assert((state & ANA_INPUT_DOWN_MASK) == 0u);

    state = ana_input_state_from_amiga_joydat(0x0001u, 0, 0, 0);
    assert((state & ANA_INPUT_DOWN_MASK) != 0u);
    assert((state & ANA_INPUT_RIGHT_MASK) == 0u);

    state = ana_input_state_from_amiga_joydat(0x0200u, 1, 0, 0);
    assert((state & ANA_INPUT_LEFT_MASK) != 0u);
    assert((state & ANA_INPUT_UP_MASK) != 0u);
    assert((state & ANA_INPUT_RIGHT_MASK) == 0u);
    assert((state & ANA_INPUT_DOWN_MASK) == 0u);
    assert((state & ANA_ACTION_1_MASK) != 0u);

    state = ana_input_state_from_amiga_joydat(0x0002u, 0, 1, 1);
    assert((state & ANA_INPUT_LEFT_MASK) == 0u);
    assert((state & ANA_INPUT_UP_MASK) == 0u);
    assert((state & ANA_INPUT_RIGHT_MASK) != 0u);
    assert((state & ANA_INPUT_DOWN_MASK) != 0u);
    assert((state & ANA_ACTION_1_MASK) == 0u);
    assert((state & ANA_ACTION_2_MASK) != 0u);
    assert((state & ANA_ACTION_3_MASK) != 0u);
}

static void test_amiga_key_matrix_mapping(void)
{
    unsigned char matrix[16];
    int i;

    for (i = 0; i < 16; i++) {
        matrix[i] = 0u;
    }

    matrix[0x45 >> 3] = (unsigned char)(1u << (0x45 & 7));
    assert(ana_input_key_matrix_raw_down(matrix, 16, 0x45));
    assert(!ana_input_key_matrix_raw_down(matrix, 16, 0x44));
    assert(!ana_input_key_matrix_raw_down(matrix, 8, 0x45));
    assert(!ana_input_key_matrix_raw_down(NULL, 16, 0x45));
    assert(!ana_input_key_matrix_raw_down(matrix, 16, 0x80));

    matrix[0x10 >> 3] = (unsigned char)(1u << (0x10 & 7));
    assert(ana_input_key_matrix_raw_down(matrix, 16, 0x10));
}

static void test_amiga_key_code_mapping(void)
{
    assert(ana_input_key_from_amiga_raw_code(0x4f) == ANA_KEY_LEFT);
    assert(ana_input_key_from_amiga_raw_code(0x4e) == ANA_KEY_RIGHT);
    assert(ana_input_key_from_amiga_raw_code(0x4c) == ANA_KEY_UP);
    assert(ana_input_key_from_amiga_raw_code(0x4d) == ANA_KEY_DOWN);
    assert(ana_input_key_from_amiga_raw_code(0x40) == ANA_KEY_SPACE);
    assert(ana_input_key_from_amiga_raw_code(0x45) == ANA_KEY_ESCAPE);
    assert(ana_input_key_from_amiga_raw_code(0x10) == ANA_KEY_Q);
    assert(ana_input_key_from_amiga_raw_code(0x32) == ANA_KEY_X);
    assert(ana_input_key_from_amiga_raw_code(0x33) == ANA_KEY_C);
    assert(ana_input_key_from_amiga_raw_code(0x7f) == ANA_KEY_UNKNOWN);

    assert(ana_input_key_from_amiga_vanilla_code(' ') == ANA_KEY_SPACE);
    assert(ana_input_key_from_amiga_vanilla_code(0x1b) == ANA_KEY_ESCAPE);
    assert(ana_input_key_from_amiga_vanilla_code('q') == ANA_KEY_Q);
    assert(ana_input_key_from_amiga_vanilla_code('Q') == ANA_KEY_Q);
    assert(ana_input_key_from_amiga_vanilla_code('x') == ANA_KEY_X);
    assert(ana_input_key_from_amiga_vanilla_code('C') == ANA_KEY_C);
    assert(ana_input_key_from_amiga_vanilla_code('?') == ANA_KEY_UNKNOWN);
}

static void test_amiga_key_event_handling(void)
{
    ana_input_reset();

    assert(ana_input_handle_amiga_raw_key_event(0x32, 1) == ANA_KEY_X);
    ana_input_update();
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_2));

    ana_input_update();
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(!ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_2));

    assert(ana_input_handle_amiga_raw_key_event(0x32, 0) == ANA_KEY_X);
    ana_input_update();
    assert(!ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(ana_input_action_released(ANA_INPUT_DEVICE_0, ANA_ACTION_2));

    assert(ana_input_handle_amiga_vanilla_key_event('x') == ANA_KEY_X);
    ana_input_update();
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_2));

    ana_input_update();
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(!ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_2));

    ana_input_update();
    assert(!ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2));
    assert(ana_input_action_released(ANA_INPUT_DEVICE_0, ANA_ACTION_2));

    ana_input_map_key_to_quit(ANA_KEY_Q);
    assert(ana_input_handle_amiga_raw_key_event(0x10, 1) == ANA_KEY_Q);
    ana_input_update();
    assert(ana_quit_requested());
    assert(ana_input_handle_amiga_raw_key_event(0x10, 0) == ANA_KEY_Q);
    ana_input_update();
    assert(ana_quit_requested());
    ana_input_update();
    assert(!ana_quit_requested());
}

static void test_action_quit_mapping(void)
{
    ana_input_reset();
    ana_input_map_action_to_quit(ANA_INPUT_DEVICE_0, ANA_ACTION_3);

    ana_input_set_pending_state(ANA_INPUT_DEVICE_0, ANA_ACTION_3_MASK);
    ana_input_update();
    assert(ana_quit_requested());

    ana_input_set_pending_state(ANA_INPUT_DEVICE_0, 0u);
    ana_input_update();
    assert(!ana_quit_requested());

    ana_input_clear_key_map();
    ana_input_set_pending_state(ANA_INPUT_DEVICE_0, ANA_ACTION_3_MASK);
    ana_input_update();
    assert(!ana_quit_requested());
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

    ana_input_map_key_to_direction(ANA_KEY_UNKNOWN, ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT);
    ana_input_map_key_to_action((ANA_Key)99, ANA_INPUT_DEVICE_0, ANA_ACTION_1);
    ana_input_map_key_to_quit((ANA_Key)99);
    ana_input_set_pending_key_state((ANA_Key)99, 1);
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

static void test_advance_without_poll(void)
{
    ana_input_reset();

    ana_input_set_pending_state(ANA_INPUT_DEVICE_0, ANA_ACTION_1_MASK);
    ana_input_advance_without_poll();
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
    assert(ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_1));

    ana_input_advance_without_poll();
    assert(ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
    assert(!ana_input_action_pressed(ANA_INPUT_DEVICE_0, ANA_ACTION_1));

    ana_input_set_pending_state(ANA_INPUT_DEVICE_0, 0u);
    ana_input_advance_without_poll();
    assert(!ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
    assert(ana_input_action_released(ANA_INPUT_DEVICE_0, ANA_ACTION_1));
}

int main(void)
{
    test_direction_transitions();
    test_action_transitions();
    test_default_keyboard_mapping();
    test_default_keyboard_mapping_covers_common_game_keys();
    test_custom_keyboard_mapping();
    test_keyboard_quit_mapping();
    test_keyboard_pulse_events();
    test_reset_clears_keyboard_pulse_events();
    test_amiga_joydat_mapping();
    test_amiga_key_matrix_mapping();
    test_amiga_key_code_mapping();
    test_amiga_key_event_handling();
    test_action_quit_mapping();
    test_invalid_inputs_are_safe();
    test_quit_request();
    test_advance_without_poll();

    return 0;
}

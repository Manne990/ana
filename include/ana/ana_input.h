#ifndef ANA_INPUT_H
#define ANA_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#define ANA_INPUT_DEVICES 2

typedef enum ANA_InputDevice {
    ANA_INPUT_DEVICE_0 = 0,
    ANA_INPUT_DEVICE_1 = 1
} ANA_InputDevice;

typedef enum ANA_InputDirection {
    ANA_INPUT_LEFT = 0,
    ANA_INPUT_RIGHT,
    ANA_INPUT_UP,
    ANA_INPUT_DOWN
} ANA_InputDirection;

typedef enum ANA_InputAction {
    ANA_ACTION_1 = 0,
    ANA_ACTION_2,
    ANA_ACTION_3,
    ANA_ACTION_4
} ANA_InputAction;

typedef enum ANA_Key {
    ANA_KEY_UNKNOWN = 0,
    ANA_KEY_LEFT,
    ANA_KEY_RIGHT,
    ANA_KEY_UP,
    ANA_KEY_DOWN,
    ANA_KEY_SPACE,
    ANA_KEY_RETURN,
    ANA_KEY_ESCAPE,
    ANA_KEY_Q,
    ANA_KEY_A,
    ANA_KEY_D,
    ANA_KEY_W,
    ANA_KEY_S,
    ANA_KEY_Z,
    ANA_KEY_X,
    ANA_KEY_C,
    ANA_KEY_V,
    ANA_KEY_CTRL,
    ANA_KEY_COUNT
} ANA_Key;

typedef struct ANA_InputDebug {
    int event_count;
    int raw_count;
    int vanilla_count;
    int matrix_count;
    int matrix_ready;
    int last_class;
    int last_code;
    int last_key;
    int last_is_down;
    unsigned int seen_key_bits;
    unsigned int current_state[ANA_INPUT_DEVICES];
    unsigned int backend_state[ANA_INPUT_DEVICES];
    int key_c_down;
    int key_q_down;
    int key_escape_down;
    int key_space_down;
    int key_x_down;
    int quit_requested;
} ANA_InputDebug;

void ana_input_update(void);
int ana_input_direction(ANA_InputDevice device, ANA_InputDirection direction);
int ana_input_direction_pressed(ANA_InputDevice device, ANA_InputDirection direction);
int ana_input_direction_released(ANA_InputDevice device, ANA_InputDirection direction);
int ana_input_action(ANA_InputDevice device, ANA_InputAction action);
int ana_input_action_pressed(ANA_InputDevice device, ANA_InputAction action);
int ana_input_action_released(ANA_InputDevice device, ANA_InputAction action);
int ana_quit_requested(void);
void ana_input_debug_snapshot(ANA_InputDebug* debug);

void ana_input_clear_key_map(void);
void ana_input_map_key_to_direction(
    ANA_Key key,
    ANA_InputDevice device,
    ANA_InputDirection direction);
void ana_input_map_key_to_action(
    ANA_Key key,
    ANA_InputDevice device,
    ANA_InputAction action);
void ana_input_map_key_to_quit(ANA_Key key);
void ana_input_unmap_key_from_quit(ANA_Key key);
void ana_input_map_action_to_quit(
    ANA_InputDevice device,
    ANA_InputAction action);
void ana_input_map_default_keys(ANA_InputDevice device);

#ifdef __cplusplus
}
#endif

#endif

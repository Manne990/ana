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

void ana_input_update(void);
int ana_input_direction(ANA_InputDevice device, ANA_InputDirection direction);
int ana_input_direction_pressed(ANA_InputDevice device, ANA_InputDirection direction);
int ana_input_direction_released(ANA_InputDevice device, ANA_InputDirection direction);
int ana_input_action(ANA_InputDevice device, ANA_InputAction action);
int ana_input_action_pressed(ANA_InputDevice device, ANA_InputAction action);
int ana_input_action_released(ANA_InputDevice device, ANA_InputAction action);
int ana_quit_requested(void);

#ifdef __cplusplus
}
#endif

#endif

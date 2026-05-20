#ifndef ANA_INPUT_H
#define ANA_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#define ANA_JOY_PORTS 2

typedef enum ANA_JoyButton {
    ANA_JOY_LEFT = 0,
    ANA_JOY_RIGHT,
    ANA_JOY_UP,
    ANA_JOY_DOWN,
    ANA_JOY_FIRE,
    ANA_JOY_START
} ANA_JoyButton;

void ana_input_update(void);
int ana_joy_down(int port, ANA_JoyButton button);
int ana_joy_pressed(int port, ANA_JoyButton button);
int ana_joy_released(int port, ANA_JoyButton button);
int ana_quit_requested(void);

#ifdef __cplusplus
}
#endif

#endif


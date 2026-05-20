# Spec 007: Input

## Syfte

Ge spelkod en enkel och stabil inputmodell for joystick, gamepad och tangentbord, utan att varje spel laser hardvara direkt.

## Mal for 0.1

- Joystick- eller gamepadstyrning for Invaders.
- Tangentbordsstyrning for emulator och utveckling.
- Current/down-state.
- Pressed/released-state per frame.
- Enkel kontrollmodell som passar actionspel.

## Foreslaget API

```c
typedef enum ANA_InputDevice {
    ANA_INPUT_DEVICE_0,
    ANA_INPUT_DEVICE_1
} ANA_InputDevice;

typedef enum ANA_InputDirection {
    ANA_INPUT_LEFT,
    ANA_INPUT_RIGHT,
    ANA_INPUT_UP,
    ANA_INPUT_DOWN
} ANA_InputDirection;

typedef enum ANA_InputAction {
    ANA_ACTION_1,
    ANA_ACTION_2,
    ANA_ACTION_3,
    ANA_ACTION_4
} ANA_InputAction;

typedef enum ANA_Key {
    ANA_KEY_LEFT,
    ANA_KEY_RIGHT,
    ANA_KEY_UP,
    ANA_KEY_DOWN,
    ANA_KEY_SPACE,
    ANA_KEY_RETURN,
    ANA_KEY_ESCAPE,
    ANA_KEY_A,
    ANA_KEY_D,
    ANA_KEY_W,
    ANA_KEY_S,
    ANA_KEY_Z,
    ANA_KEY_X,
    ANA_KEY_C,
    ANA_KEY_V,
    ANA_KEY_CTRL
} ANA_Key;

void ana_input_update(void);
int ana_input_direction(ANA_InputDevice device, ANA_InputDirection direction);
int ana_input_direction_pressed(ANA_InputDevice device, ANA_InputDirection direction);
int ana_input_direction_released(ANA_InputDevice device, ANA_InputDirection direction);
int ana_input_action(ANA_InputDevice device, ANA_InputAction action);
int ana_input_action_pressed(ANA_InputDevice device, ANA_InputAction action);
int ana_input_action_released(ANA_InputDevice device, ANA_InputAction action);
int ana_quit_requested(void);

void ana_input_clear_key_map(void);
void ana_input_map_key_to_direction(ANA_Key key, ANA_InputDevice device, ANA_InputDirection direction);
void ana_input_map_key_to_action(ANA_Key key, ANA_InputDevice device, ANA_InputAction action);
void ana_input_map_key_to_quit(ANA_Key key);
void ana_input_map_default_keys(ANA_InputDevice device);
```

`ana_input_update` kan anropas internt av runtime-loop. Det publika API:t behover bara exponera funktionen om det ar anvandbart for tester eller specialfall.

`ANA_InputDirection` beskriver digitala riktningar. `ANA_INPUT_LEFT` kan komma fran en klassisk joystick-riktning, en gamepad-dpad, en analog stick som oversatts till digital riktning eller tangentbordsmappning.

`ANA_InputAction` beskriver ovriga actionknappar. Pa en klassisk Amiga-joystick mappar fire normalt till `ANA_ACTION_1`. Gamepads kan senare mappa fler knappar till `ANA_ACTION_2`, `ANA_ACTION_3` och `ANA_ACTION_4`.

Spel kan sjalva mappa tangentbordet till samma direction/action-modell:

```c
ana_input_clear_key_map();
ana_input_map_key_to_direction(ANA_KEY_LEFT, ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT);
ana_input_map_key_to_direction(ANA_KEY_A, ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT);
ana_input_map_key_to_action(ANA_KEY_SPACE, ANA_INPUT_DEVICE_0, ANA_ACTION_1);
ana_input_map_key_to_quit(ANA_KEY_ESCAPE);
```

## Standardmapping

0.1 bor definiera en standardmapping:

- joystick/gamepad vanster/hoger till `ANA_INPUT_LEFT` och `ANA_INPUT_RIGHT`
- joystick fire eller gamepad primary button till `ANA_ACTION_1`
- tangentbordspilar eller A/D som utvecklingsalternativ
- Space eller Ctrl som `ANA_ACTION_1`
- Escape som quit via `ana_quit_requested()`
- Return kan senare mappas till en action, exempelvis `ANA_ACTION_4`

Exakta tangenter kan justeras efter Amiga-konvention och implementation.

## Prestanda och beteende

- Input-state ska uppdateras en gang per frame.
- `pressed` ska bara vara sann for overgangen upp -> ner.
- `released` ska bara vara sann for overgangen ner -> upp.
- Lasning av input ska inte allokera minne.

## Inte i 0.1

- Full keycode-API for alla tangenter bortom vanliga spelkontroller.
- Musinput.
- Remapping-UI.
- Flera spelare om det forsinkar Invaders.
- Textinmatning.

## Acceptanskriterier

- Invaders kan spelas med joystick eller gamepad dar plattformen stoder det.
- Invaders kan testas i emulator med tangentbord.
- `pressed` och `released` fungerar deterministiskt i 50 Hz-loop.
- Spelkod behover inte lasa joystick-, gamepad- eller tangentbordshardvara direkt.

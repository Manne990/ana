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
    ANA_KEY_Q,
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
void ana_input_map_action_to_quit(ANA_InputDevice device, ANA_InputAction action);
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
```

`ana_input_clear_key_map()` rensar direction/action-mapping och custom
quit-tangenter, men Escape ar alltid global quit i ANA. Ett spel ska alltsa
inte behova lagga tillbaka ESC efter att det har byggt sin egen mapping.
`ana_input_map_action_to_quit()` kan anvandas nar en emulator eller gamepad
levererar quit som en joystick/gamepad-knapp i stallet for som en tangent.

## Standardmapping

0.1 bor definiera en standardmapping:

- joystick/gamepad vanster/hoger till `ANA_INPUT_LEFT` och `ANA_INPUT_RIGHT`
- joystick fire eller gamepad primary button till `ANA_ACTION_1`
- joystick/gamepad tredje knapp kan mappas till quit via `ana_input_map_action_to_quit`
- tangentbordspilar eller A/D som utvecklingsalternativ
- Space eller Ctrl som `ANA_ACTION_1`
- X eller Z som `ANA_ACTION_2`
- C eller V som `ANA_ACTION_3`
- Escape som quit via `ana_quit_requested()`
- Return som `ANA_ACTION_4`

Exakta tangenter kan justeras efter Amiga-konvention och implementation.

## Implementerad Amiga-backend

Amiga-bygget laser tangentbordets aktuella matrix via `keyboard.device` och
`KBD_READMATRIX`, och kombinerar den med `IDCMP_RAWKEY` nar Intuition skickar
riktiga key down/up-events. Matrix och RAWKEY ar alltsa parallella kallor till
samma key-state. `IDCMP_VANILLAKEY` anvands som pulse/fallback eftersom den
inte har release-events. Tangenterna uppdaterar samma key-state som
host/test-backenden:

- cursor left/right/up/down
- A/D/W/S
- Space
- Return
- Escape
- Z/X/C/V
- Ctrl

Amiga-bygget laser ocksa de klassiska joystick-portarna varje frame.
`ANA_INPUT_DEVICE_0` mappar till den vanliga spelarporten (`JOY1DAT`, fire via
CIAA PRA bit 7) och `ANA_INPUT_DEVICE_1` mappar till den andra porten
(`JOY0DAT`, fire via CIAA PRA bit 6). Det gor att en emulator som mappar
host-tangentbordets pilar till joystick-porten fungerar utan att spelet
behover skilja pa joystick och tangentbord.

FS-UAE kan leverera host-tangenter som riktiga Amiga-tangenter eller som
joystick-hardware. ANA stodjer bada vagen, men tangentbordsstyrning ska i
forsta hand mappas som `action_key_*` och inte samtidigt anvanda samma
host-tangentbord som `Keyboard`-joystick. Det gor tangenterna event-queue:ade
via Intuition/keyboard.device och undviker att korta tryck missas nar spelet
har en tung render frame. Anvand en riktig joystick/gamepad for joystickporten
nar du vill testa joystick, eller en separat emulatorprofil for
keyboard-as-joystick.

En rekommenderad FS-UAE-keyboardprofil mappar host-pilar till samma Amiga-
tangenter som WASD (`left=A`, `right=D`, `up=W`, `down=S`) och mappar aven
A/D/W/S direkt till sina Amiga-tangenter. Space/Ctrl/Return mappas till
`action_key_space`/`action_key_ctrl`/`action_key_return`, X/Z/C/V till
`action_key_x`/`action_key_z`/`action_key_c`/`action_key_v`, och Esc/Q till
`action_key_esc`/`action_key_q`. ANA stodjer fortfarande riktiga Amiga
cursor-key-events, men FS-UAE:s `action_key_cursor_*` har varit mindre stabila
i manuella Launcher-profiler. Joystick actions som
`action_joy_1_fire_button` och `action_joy_1_2nd_button` passar for riktiga
joystick/gamepad-signaler. De kan fortfarande mappas till ANA-actions, men bor
inte vara forstahandsvalet for tangentbordstryck som ska fungera stabilt vid
lag FPS.

`tools/emulator/configure_fsuae_keyboard.py` kan patcha lokala FS-UAE-profiler
med denna mapping och tar backup innan den skriver om filerna.
`make byte-brothers-a1200-debug-fsuae-config` skriver dessutom en reproducerbar
launch-konfig till `build/fs-uae/byte-brothers-a1200-debug.fs-uae`. Den konfigen
ar avsedd for manuell test nar FS-UAE Launchers sparade profiler riskerar att
aterstalla joystick- eller keyboard-mapping.

`tools/emulator/run_input_probe.py` ar strikt nar den far skicka host-tangenter:
om macOS stoppar key injection, eller om proben inte ser de skickade
tangenterna, returnerar kommandot fel. Den automatiska macOS-sendern klickar
forst i FS-UAE-fonstret innan den skickar CoreGraphics-key events; FS-UAE kan
annars vara frontmost utan att SDL/input-grab faktiskt tar emot syntetiska
tangenttryck. `--no-send-keys` verifierar bara boot, runtime och att
keyboard-matrix kan pollas.
`make emulator-input-probe-synthetic` kor dessutom ett Amiga-side selftest som
inte skickar host-tangenter alls, utan verifierar rawkey/vanillakey-mappning,
rawkey down/up-hantering, vanillakey-pulser och ANA-actions inne i
m68k-programmet.
`make emulator-byte-brothers-input` kor ett spelharness som pulsar ANA-keys for
Right, Space, X och C och verifierar att Byte Brothers reagerar med gang,
hopp, dash och quit.

## Prestanda och beteende

- Input-state ska uppdateras innan varje simulation update.
- `pressed` ska bara vara sann for overgangen upp -> ner.
- `released` ska bara vara sann for overgangen ner -> upp.
- Event-only tangenttryck halles kvar i en kort grace period internt sa att
  korta `IDCMP_RAWKEY`/`IDCMP_VANILLAKEY`-pulser inte tappas under catch-up
  updates eller tunga render frames. `pressed` ar fortfarande bara sann for
  forsta simulation update dar pulsen syns.
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

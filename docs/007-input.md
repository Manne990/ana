# Spec 007: Input

## Syfte

Ge spelkod en enkel och stabil inputmodell for joystick och tangentbord, utan att varje spel laser Amiga-hardvara direkt.

## Mal for 0.1

- Joystickstyrning for Invaders.
- Tangentbordsstyrning for emulator och utveckling.
- Current/down-state.
- Pressed/released-state per frame.
- Enkel knappmodell som passar actionspel.

## Foreslaget API

```c
typedef enum ANA_Button {
    ANA_BUTTON_LEFT,
    ANA_BUTTON_RIGHT,
    ANA_BUTTON_UP,
    ANA_BUTTON_DOWN,
    ANA_BUTTON_FIRE,
    ANA_BUTTON_START,
    ANA_BUTTON_QUIT
} ANA_Button;

void ana_input_update(void);
int ana_input_down(ANA_Button button);
int ana_input_pressed(ANA_Button button);
int ana_input_released(ANA_Button button);
```

`ana_input_update` kan anropas internt av runtime-loop. Det publika API:t behover bara exponera funktionen om det ar anvandbart for tester eller specialfall.

## Standardmapping

0.1 bor definiera en standardmapping:

- joystick vanster/hoger till `ANA_BUTTON_LEFT` och `ANA_BUTTON_RIGHT`
- joystick fire till `ANA_BUTTON_FIRE`
- tangentbordspilar eller A/D som utvecklingsalternativ
- Space eller Ctrl som fire
- Escape som quit
- Return som start

Exakta tangenter kan justeras efter Amiga-konvention och implementation.

## Prestanda och beteende

- Input-state ska uppdateras en gang per frame.
- `pressed` ska bara vara sann for overgangen upp -> ner.
- `released` ska bara vara sann for overgangen ner -> upp.
- Lasning av input ska inte allokera minne.

## Inte i 0.1

- Full keycode-API for alla tangenter.
- Musinput.
- Remapping-UI.
- Flera spelare om det forsinkar Invaders.
- Textinmatning.

## Acceptanskriterier

- Invaders kan spelas med joystick.
- Invaders kan testas i emulator med tangentbord.
- `pressed` och `released` fungerar deterministiskt i 50 Hz-loop.
- Spelkod behover inte lasa joystick- eller tangentbordshardvara direkt.


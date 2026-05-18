# Spec 010: Sma spelhjalpare

## Syfte

Erbjuda ett litet antal hjalpfunktioner som gor exempelspel tydligare utan att ANA blir en stor motor.

## Mal for 0.1

- Rektangelkollision.
- Enkla mattehjalpare.
- Timer/counter-hjalpare.
- Eventuellt enkel fixed point-typ om det behovs.
- API:t ska vara litet och latt att ignorera.

## Foreslaget API

```c
typedef struct ANA_Rect {
    int x;
    int y;
    int w;
    int h;
} ANA_Rect;

int ana_rect_intersects(ANA_Rect a, ANA_Rect b);
int ana_clamp_int(int value, int min_value, int max_value);
```

Timerhjalpare kan tillkomma om Invaders-koden blir tydligare av det:

```c
typedef struct ANA_Timer {
    int ticks;
    int interval;
} ANA_Timer;

void ana_timer_reset(ANA_Timer* timer, int interval);
int ana_timer_tick(ANA_Timer* timer);
```

## Riktlinjer

- Hjalpare ska vara sma och forutsagbara.
- De ska inte aga spelets state.
- De ska inte allokera minne.
- De ska inte introducera stora abstraktioner som scener, entities eller komponenter i 0.1.

## Inte i 0.1

- Fysikmotor.
- Entity component system.
- Generell collision world.
- Pathfinding.
- AI-system.
- Avancerad tweening/animation framework.

## Acceptanskriterier

- Invaders kan anvanda ANA:s rektangelkollision for skott, spelare och fiender.
- Hjalpfunktionerna minskar exempelspelskod utan att gomma spelets regler.
- API:t ar tillrackligt litet for att rymmas i en overskadlig header.

# Spec 010: Sma spelhjalpare

## Syfte

Erbjuda ett litet antal hjalpfunktioner som gor exempelspel tydligare utan att ANA blir en stor motor.

## Mal for 0.1

- Rektangelkollision.
- Enkla mattehjalpare.
- Timer/counter-hjalpare.
- Eventuellt enkel fixed point-typ om det behovs.
- API:t ska vara litet och latt att ignorera.

## API i 0.1

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

`ana_rect_intersects` returnerar sant nar rektanglarna har overlappande yta.
Rektanglar med noll eller negativ bredd/hojd raknas aldrig som traffar. Kanter
som bara vidror varandra raknas inte som overlapp.

`ana_clamp_int` begransar ett heltal till intervallet. Om `min_value` och
`max_value` skickas in i fel ordning byter funktionen ordning internt.

Efter Spec 015 finns ocksa sma rektangelhjalpare:

```c
ANA_Rect ana_rect_make(int x, int y, int w, int h);
ANA_Rect ana_rect_clip(ANA_Rect rect, ANA_Rect bounds);
ANA_Rect ana_rect_union(ANA_Rect a, ANA_Rect b);
ANA_Rect ana_rect_align_x8(ANA_Rect rect, int min_x, int max_x);
int ana_rect_is_empty(ANA_Rect rect);
int ana_rect_contains(ANA_Rect outer, ANA_Rect inner);
```

`ana_rect_align_x8` ar framfor allt till for Amiga-vanlig dirty rendering dar
bitplane-arbete ofta blir billigare och sakrare nar x-koordinater alignas mot
8 pixlar.

```c
typedef struct ANA_Timer {
    int ticks;
    int interval;
} ANA_Timer;

void ana_timer_reset(ANA_Timer* timer, int interval);
int ana_timer_tick(ANA_Timer* timer);
```

`ana_timer_tick` raknar upp timern ett tick och returnerar sant nar intervallet
har natts. Intervall under 1 normaliseras till 1.

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
- Invaders kan anvanda ANA:s timerhjalpare for aterkommande handelser.
- Invaders kan anvanda ANA:s clamp-hjalpare for enklare granslogik.
- Hjalpfunktionerna minskar exempelspelskod utan att gomma spelets regler.
- API:t ar tillrackligt litet for att rymmas i en overskadlig header.

## Implementationsstatus

- `ANA_Rect`, `ana_rect_intersects` och `ana_clamp_int` finns i
  `include/ana/ana_helpers.h`.
- `ana_rect_make`, `ana_rect_clip`, `ana_rect_union`, `ana_rect_align_x8`,
  `ana_rect_is_empty` och `ana_rect_contains` finns i samma header.
- `ANA_Timer`, `ana_timer_reset` och `ana_timer_tick` finns i samma header.
- Invaders anvander hjalparna for kollisioner, spelarkoordinater och
  formationens rorelse-/skott-timers, samt for dirty rendering.

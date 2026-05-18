# Spec 003: Core runtime och game loop

## Syfte

Ge ANA den centrala XNA/MonoGame-kanslan: spelkod beskriver initiering, laddning, uppdatering och ritning, medan ANA ager runtime-loop och plattformsinitiering.

## Mal for 0.1

- Enkel game loop i C.
- Fast 50 Hz update-loop.
- Separata callbacks for init, load, update, draw och shutdown.
- Kontrollerad avslutning fran spelkod.
- Grundlaggande felkoder.

## Foreslaget API

```c
typedef struct ANA_Game {
    void (*init)(void);
    void (*load)(void);
    void (*update)(void);
    void (*draw)(void);
    void (*shutdown)(void);
} ANA_Game;

int ana_run(const ANA_Game* game);
void ana_quit(void);
```

Senare kan `update` fa en tidsparameter om det visar sig praktiskt:

```c
void (*update)(ANA_Time time);
```

For 0.1 ar en strikt 50 Hz-loop sannolikt enklare och mer Amiga-vanlig.

## Runtime-ansvar

`ana_run` ansvarar for:

- initiera ANA:s subsystem i korrekt ordning
- anropa callbacks i korrekt ordning
- driva update/draw-loop
- hantera quit-flagga
- stanga ner subsystem i korrekt ordning
- returnera felkod vid initieringsfel

## Spelkodens ansvar

Spelkoden ansvarar for:

- skapa spelets state
- ladda egna assets via ANA API
- uppdatera spelregler i `update`
- rita aktuell frame i `draw`
- frigora egna resurser i `shutdown`, om API:t kraver det

## Inte i 0.1

- Variabel timestep.
- Multitradad loop.
- Avancerad scenhantering.
- Coroutine-/jobbsystem.
- Automatisk minneshantering.

## Acceptanskriterier

- Ett `hello`-exempel kan starta ANA, visa bild och avsluta rent.
- `update` anropas stabilt i 50 Hz pa malprofilen.
- Spelet kan anropa `ana_quit()` for att avsluta.
- Runtime gor inga dolda allokeringar i den lopande `update`/`draw`-fasen.


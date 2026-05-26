# Spec 003: Core runtime och game loop

## Syfte

Ge ANA den centrala XNA/MonoGame-kanslan: spelkod beskriver initiering, laddning, uppdatering och ritning, medan ANA ager runtime-loop och plattformsinitiering.

## Mal for 0.1

- Enkel game loop i C.
- Fast 50 Hz update-loop.
- Separata callbacks for init, load, update, draw och shutdown.
- Enkel konfiguration av skarmstorlek, fps, fargantal och screen mode i `ANA_Game`.
- Kontrollerad avslutning fran spelkod.
- Grundlaggande felkoder.

## Foreslaget API

```c
typedef struct ANA_Time {
    int tick;
    int fps;
} ANA_Time;

typedef enum ANA_ScreenMode {
    ANA_SCREEN_PAL_LORES
} ANA_ScreenMode;

typedef struct ANA_Game {
    void (*init)(void);
    void (*load)(void);
    void (*update)(ANA_Time time);
    void (*draw)(void);
    void (*shutdown)(void);

    int width;
    int height;
    int fps;
    int colors;
    ANA_ScreenMode screen_mode;
    int debug_stats;
} ANA_Game;

int ana_run(const ANA_Game* game);
void ana_quit(void);
ANA_RunStats ana_last_run_stats(void);
```

For 0.1 ar `ANA_Time` fortfarande baserad pa en strikt fixed timestep. Den finns for att gora koden tydlig och framtidssaker, inte for att introducera variabel timestep.

Spelkod bor skapa `ANA_Game` med `ANA_Game game = {0};` innan falt satts.
Da defaultar framtida frivilliga falt till avstangt beteende.

## Runtime-ansvar

`ana_run` ansvarar for:

- initiera ANA:s subsystem i korrekt ordning
- anropa callbacks i korrekt ordning
- driva update/draw-loop
- satta upp skarm enligt `ANA_Game`-konfiguration
- hantera quit-flagga
- skriva runtime-/render-statistik om `debug_stats` ar satt och ANA ar byggt
  med `ANA_DEBUG_STATS`
- stanga ner subsystem i korrekt ordning
- returnera felkod vid initieringsfel

Efter `ana_run` kan spelet hamta `ANA_RunStats` med
`ana_last_run_stats()`. I 0.1 anvands det framfor allt internt av ANA:s
debug-utskrift, men ar fortfarande tillgangligt for avancerad matning:

- `frames`: antal presenterade frames
- `elapsed_ticks`: uppmatt tid i plattformens ticks
- `ticks_per_second`: upplosning for tidskallan
- `average_fps_x100`: genomsnittlig FPS multiplicerad med 100, sa spelkod kan
  skriva `50.00` utan floating point

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

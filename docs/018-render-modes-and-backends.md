# Spec 018: Render modes och backends

## Bakgrund

Invaders och AMAze fungerar bra med statisk viewport, retained BOBs och dirty
rectangles. Byte Brothers visar ett annat problem: nar kameran scrollar blir
stora delar av spelvyn i praktiken dirty, och dagens C2P/direct-present-modell
far ojamt tempo pa stock A1200 utan Fast RAM.

ANA behover darfor lata spelet beskriva vilken renderstrategi det ar byggt for.
Det ska inte vara en magisk prestandaflagga, utan ett tydligt kontrakt mellan
spel och runtime. Samtidigt ar ett enda render mode for hela spelet bara en
forsta approximation: ett riktigt spel kan kombinera flera strategier i samma
frame.

Ramverkets princip ar att anvandaren uttrycker speltypen pa hog niva, medan
ANA valjer den snabbaste korrekta implementationen for aktuell Amiga-profil.
Ett scrollande spel ska alltsa inte sjalv programmera bitplane-pekare eller
BPLCON1 for att fa bra prestanda. Det ska deklarera scrollbehovet, anvanda
ANA:s kamera-/tilemap-API, och lata Amiga-backenden anvanda hardware scroll,
blitter och overdraw-buffertar dar det ar ratt teknik.

`ANA_Game.render_mode` ar darfor default eller primar strategi for spelets
huvudvy, inte en slutgiltig begransning. Senare API ska kunna lagga render mode
pa scen- eller layer-niva.

## Mal

- Gora rendererstrategi explicit i `ANA_Game`.
- Behalla zero-init som sakert defaultbeteende.
- Ge dokumentation, tester och examples ett gemensamt sprak for renderfall.
- Bana vag for Amiga-specifika backends utan att spelkodens high-level API
  maste bytas.

## API

```c
typedef enum ANA_RenderMode {
    ANA_RENDER_DEFAULT = 0,
    ANA_RENDER_DIRTY = 1,
    ANA_RENDER_FULL_FRAME = 2,
    ANA_RENDER_TILE_SCROLL = 3,
    ANA_RENDER_BLITTER_BOBS = 4
} ANA_RenderMode;

typedef struct ANA_Game {
    ...
    ANA_ScreenMode screen_mode;
    ANA_RenderMode render_mode;
    int debug_stats;
} ANA_Game;
```

`ANA_RENDER_DEFAULT` far anvandas i `ANA_Game` och normaliseras till
`ANA_RENDER_DIRTY`.

`ANA_Profile` innehaller alltid ett konkret render mode efter runtime-
normalisering. `ana_validate_profile` ska neka okanda render modes.

## Modes

### `ANA_RENDER_DIRTY`

For spel med statisk viewport och relativt fa rorliga objekt.

Passar:

- Invaders
- AMAze
- pusselspel
- single-screen action

Strategin ar retained redraw: rensa gamla objektpositioner, reparera statiska
lager vid behov och rita rorliga objekt igen.

Detta ar prestanda-vagen for spel dar storre delen av viewporten ar stabil.
Att tvinga in hardware-scroll-tanket har skulle normalt ge mer komplexitet
utan att vinna nagot.

### `ANA_RENDER_FULL_FRAME`

For enkla prototyper eller spel dar hela bilden anda ritas om varje frame.

Det ar enklare men normalt dyrare. Det ska finnas som uttryckt val, inte som
implicit fallback for allt.

### `ANA_RENDER_TILE_SCROLL`

For spel dar kameran eller bakgrunden ror sig ofta.

Passar:

- plattformsspel
- vertikala shooters
- racing-/runner-spel
- parallaxbakgrunder

I forsta steget ar detta ett API-kontrakt. Den riktiga Amiga-backenden ska
senare kunna anvanda tilemap-redraw, bitplane-offset/fine scroll, overdraw-
marginaler och nyritade strips i stallet for full viewport redraw.

Detta ar prestanda-vagen for stora scrollande bakgrunder. Spelet ska inte
rita om hela viewporten for varje kamerapixel om Amiga-backenden kan flytta
visningen med hardware scroll och bara rita in nya tile-kolumner eller rader.

### `ANA_RENDER_BLITTER_BOBS`

Reserverad for BOB-tunga spel dar Amiga-backenden senare kan anvanda mer
blitter-orienterad intern strategi. Mode:t finns for att vi inte ska blanda
ihop scrolling och sprite-intensiva single-screen-spel.

## Kombinerade renderstrategier

Ett normalt spel behover ofta flera renderstrategier samtidigt.

Exempel: Byte Brothers kan ha:

- ett hardware-scrollat tilemap-lager for banan
- ett BOB/sprite-lager for spelare, fiender, pickups och projektiler
- en statisk HUD-overlay med dirty labels for score, liv och items
- en separat title/menu-scene som kanske ar full-frame eller dirty-renderad

Ramverket ska pa sikt gora detta explicit genom layers/scenes. Spelkoden ska i
forsta hand valja lager-typ, inte lag-niva renderingsteknik.

```c
ANA_Layer playfield; /* sidescroll/tilemap */
ANA_Layer actors;    /* sprites/BOBs */
ANA_Layer hud;       /* HUD overlay */
```

Det viktiga ar att spelkoden inte ska behova blanda ihop scrolllogik,
sprite-redraw och HUD-cache for hand. Den ska uttrycka lagerordning och syfte;
ANA-backenden valjer effektiv implementation.

### Foreslagna layer-typer

```c
typedef enum ANA_LayerKind {
    ANA_LAYER_STATIC = 1,
    ANA_LAYER_SIDE_SCROLL = 2,
    ANA_LAYER_VERTICAL_SCROLL = 3,
    ANA_LAYER_PARALLAX = 4,
    ANA_LAYER_SPRITES = 5,
    ANA_LAYER_HUD = 6,
    ANA_LAYER_DEBUG = 7,
    ANA_LAYER_MENU = 8
} ANA_LayerKind;
```

`ANA_LayerKind` beskriver vad lagret ar till for. `ANA_RenderMode` beskriver
eller valjer implementationen. I manga fall kan ANA sjalv mappa layer-typ till
ratt render mode:

- `ANA_LAYER_SIDE_SCROLL` -> hardware/tile scroll dar Amiga-backenden kan.
- `ANA_LAYER_VERTICAL_SCROLL` -> strip/tile scroll for shooters och runners.
- `ANA_LAYER_PARALLAX` -> billigare scroll-lager, ofta fa farger eller enklare
  tiledata.
- `ANA_LAYER_SPRITES` -> BOB/blitter/dirty sprite-hantering.
- `ANA_LAYER_HUD` -> statiska/dirty labels och sma icons, inte del av
  playfield-scroll.
- `ANA_LAYER_MENU` -> full-frame eller dirty beroende pa scenens innehall.

En mojlig lagerordning for plattformsspel:

1. bakgrund/parallax
2. scrollande tilemap
3. BOBs/sprites
4. foreground tiles
5. HUD overlay
6. debug overlay

For single-screen-spel kan samma modell anvandas, men playfield-lagret blir
dirty/static i stallet for hardware-scrollat.

### API-skiss

```c
ANA_Layer* ana_layer_create(ANA_LayerKind kind, int z_order);
void ana_layer_set_viewport(ANA_Layer* layer, ANA_Rect viewport);
void ana_layer_set_camera(ANA_Layer* layer, const ANA_Camera* camera);
void ana_layer_set_tilemap(ANA_Layer* layer, const ANA_Tilemap* tilemap);
void ana_layer_draw_image(ANA_Layer* layer, ANA_Image image, int x, int y);
void ana_layer_draw_text(ANA_Layer* layer, ANA_Font font, int x, int y, const char* text);
```

Detta ar inte ett forslag om en tung scene graph. Det ar en tunn struktur som
later ANA separera playfield, sprites, HUD och debug sa varje del kan renderas
med ratt Amiga-teknik.

## Forsta implementation

- Lagg `ANA_RenderMode` i public API.
- Lagg `render_mode` i `ANA_Game` och `ANA_Profile`.
- Defaulta `ANA_RENDER_DEFAULT` till `ANA_RENDER_DIRTY`.
- Validera okanda render modes.
- Satt examples:
  - Invaders: `ANA_RENDER_DIRTY`
  - AMAze: `ANA_RENDER_DIRTY`
  - Byte Brothers: `ANA_RENDER_TILE_SCROLL`
  - Hello: `ANA_RENDER_DIRTY`

Ingen hardware-scroll implementeras i denna spec. Det hor till Spec 017 och
kraver en separat Amiga-backend.

## Acceptanskriterier

- Befintliga tester passerar.
- En ny test verifierar default-render-mode och ogiltigt render mode.
- Examples bygger med explicita render modes.
- README, API-dokumentation och performance guide beskriver skillnaden mellan
  dirty-rendering och scroll-rendering.
- Byte Brothers kan fortfarande vara korrekt men inte perfekt optimerad; det
  viktiga ar att samplet inte langre ser ut som ett vanligt dirty-rect-spel for
  ramverket.

## Senare arbete

- Exponera debugrad som visar aktivt render mode.
- Koppla `ANA_RENDER_TILE_SCROLL` till `ANA_Tilemap`/`ANA_ScrollLayer`.
- Flytta renderstrategi fran endast `ANA_Game` till komponerbara scenes/layers.
- Lat HUD overlays och debug overlays vara egna lager sa de inte blandas ihop
  med scrollande playfields.
- Bygg Amiga hardware-scroll-backend med bitplane-pointer/fine-scroll dar det
  passar.
- Utvardera om `ANA_RENDER_FULL_FRAME` ska ha en enklare, snabbare host path.

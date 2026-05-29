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

A1200-bygget defaultar for narvarande till den sakra direct-present/chunky-C2P-
vagen for side-/tile-scroll. Det finns en experimentell visible-scroll-
overgangsbackend bakom `ANA_AMIGA_EXPERIMENTAL_VISIBLE_SCROLL`, men den ar inte
default eftersom den kan ge stale-pixel-artefakter. Den ska inte forvaxlas med
den slutliga BPLCON1/bitplane-pointer-hardware-scrollen.

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
    ANA_RENDER_BLITTER_BOBS = 4,
    ANA_RENDER_SIDE_SCROLL = 5,
    ANA_RENDER_VERTICAL_SCROLL = 6,
    ANA_RENDER_TILE_4WAY = 7,
    ANA_RENDER_RAYCAST = 8
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

Legacy/generiskt tile-scroll-kontrakt. Ny kod bor valja ett mer specifikt
render mode nar scrollriktningen ar kand.

Det finns kvar for kompatibilitet och for enklare prototyper som annu inte
vill lova side-, vertical- eller 4-way-scroll.

### `ANA_RENDER_BLITTER_BOBS`

Reserverad for BOB-tunga spel dar Amiga-backenden senare kan anvanda mer
blitter-orienterad intern strategi. Mode:t finns for att vi inte ska blanda
ihop scrolling och sprite-intensiva single-screen-spel.

### `ANA_RENDER_SIDE_SCROLL`

For plattformsspel och andra spel dar huvudkameran normalt ror sig horisontellt.

Passar:

- Byte Brothers
- Giana/Mario-liknande plattformsspel
- sidscrollande actionspel

Detta ar mode:t som ska kopplas till Amiga-hardware-scroll med bredare
bitplane-buffer, BPLCON1-finscroll, bitplane-pekar-coarsescroll och blitter-
ritade tile-strips.

### `ANA_RENDER_VERTICAL_SCROLL`

For spel dar banan scrollar uppat eller nedat.

Passar:

- vertikala shooters
- runners
- kartor som trycks kontinuerligt i en riktning

Backenden kan optimera for rad-/strip-uppdateringar i stallet for att anta
samma kostnader som en full 2D-kamera.

### `ANA_RENDER_TILE_4WAY`

For tilemap-spel med kamera i flera riktningar.

Passar:

- Gauntlet-liknande spel
- Zelda-/maze-liknande spel
- storre kartor med fri kamera

Det ar sannolikt dyrare an side- eller vertical-scroll pa Amiga, men API:t
gor att ANA kan valja en separat strategi i stallet for att behandla allt som
plattformsscroll.

### `ANA_RENDER_RAYCAST`

Framtida kontrakt for raycast/first-person-rendering. Det ar inte en
scroll-backend; det ar en egen huvudvy dar tilemap-data kan vara input men
renderingen ar kolumn-/spanbaserad.

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
    ANA_LAYER_STATIC = 0,
    ANA_LAYER_SIDE_SCROLL = 1,
    ANA_LAYER_VERTICAL_SCROLL = 2,
    ANA_LAYER_PARALLAX = 3,
    ANA_LAYER_SPRITES = 4,
    ANA_LAYER_HUD = 5,
    ANA_LAYER_DEBUG = 6,
    ANA_LAYER_MENU = 7,
    ANA_LAYER_TILE_4WAY = 8,
    ANA_LAYER_RAYCAST_VIEW = 9
} ANA_LayerKind;
```

`ANA_LayerKind` beskriver vad lagret ar till for. `ANA_RenderMode` beskriver
eller valjer implementationen. I manga fall kan ANA sjalv mappa layer-typ till
ratt render mode:

- `ANA_LAYER_SIDE_SCROLL` -> `ANA_RENDER_SIDE_SCROLL`.
- `ANA_LAYER_VERTICAL_SCROLL` -> `ANA_RENDER_VERTICAL_SCROLL`.
- `ANA_LAYER_TILE_4WAY` -> `ANA_RENDER_TILE_4WAY`.
- `ANA_LAYER_RAYCAST_VIEW` -> `ANA_RENDER_RAYCAST`.
- `ANA_LAYER_PARALLAX` -> billigare scroll-lager, ofta fa farger eller enklare
  tiledata. Defaultar fortsatt till det generiska `ANA_RENDER_TILE_SCROLL`.
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
ANA_Layer playfield;

ana_layer_init(&playfield, ANA_LAYER_SIDE_SCROLL, 0);
void ana_layer_set_viewport(ANA_Layer* layer, ANA_Rect viewport);
void ana_layer_set_camera(ANA_Layer* layer, const ANA_Camera* camera);
void ana_layer_set_redraw(
    ANA_Layer* layer,
    ANA_RedrawCallback redraw,
    void* user_data);
void ana_layer_mark_dirty(ANA_Layer* layer);
void ana_layer_draw_if_dirty(ANA_Layer* layer);
```

Detta ar inte ett forslag om en tung scene graph. Det ar en tunn struktur som
later ANA separera playfield, sprites, HUD och debug sa varje del kan renderas
med ratt Amiga-teknik.

Forsta implementationen ar stackallokerad och har inga dolda allokeringar.
Den lagrar layer-kind, render-mode-mapping, viewport, kamera, z-order och en
dirty redraw callback. `ANA_TileLayer` bygger vidare pa detta med tile
read/draw callbacks, viewport, camera, tile size och software strip redraw.
Den ar den forsta tilemap-agda draw-funktionen, men den ar fortfarande en
portabel/chunky fallback och inte den slutliga Amiga hardware-scroll-backenden.

## Forsta implementation

- Lagg `ANA_RenderMode` i public API.
- Lagg `render_mode` i `ANA_Game` och `ANA_Profile`.
- Defaulta `ANA_RENDER_DEFAULT` till `ANA_RENDER_DIRTY`.
- Validera okanda render modes.
- Lagg `ANA_LayerKind`, `ANA_Layer` och enkla layer helpers i public API.
- Lat `ANA_LAYER_SIDE_SCROLL` defaulta till `ANA_RENDER_SIDE_SCROLL`.
- Lat `ANA_LAYER_VERTICAL_SCROLL` defaulta till `ANA_RENDER_VERTICAL_SCROLL`.
- Lat `ANA_LAYER_TILE_4WAY` defaulta till `ANA_RENDER_TILE_4WAY`.
- Lat `ANA_LAYER_RAYCAST_VIEW` defaulta till `ANA_RENDER_RAYCAST`.
- Lat `ANA_LAYER_PARALLAX` defaulta till `ANA_RENDER_TILE_SCROLL`.
- Lat `ANA_LAYER_SPRITES` defaulta till `ANA_RENDER_BLITTER_BOBS`.
- Lat `ANA_LAYER_HUD`, `ANA_LAYER_STATIC` och `ANA_LAYER_DEBUG` defaulta till
  `ANA_RENDER_DIRTY`.
- Lagg `ANA_TileLayer` som forsta framework-owned tilemap draw helper.
- Satt examples:
  - Invaders: `ANA_RENDER_DIRTY`
  - AMAze: `ANA_RENDER_DIRTY`
  - Byte Brothers: `ANA_RENDER_SIDE_SCROLL`
  - Hello: `ANA_RENDER_DIRTY`

Ingen native Amiga hardware-scroll implementeras i denna spec. Det hor till
Spec 017 och kraver en separat Amiga-backend.

## Acceptanskriterier

- Befintliga tester passerar.
- En ny test verifierar default-render-mode och ogiltigt render mode.
- Examples bygger med explicita render modes.
- Byte Brothers anvander `ANA_TileLayer` for playfield redraw.
- README, API-dokumentation och performance guide beskriver skillnaden mellan
  dirty-rendering och scroll-rendering.
- Byte Brothers kan fortfarande vara korrekt men inte perfekt optimerad; det
  viktiga ar att samplet inte langre ser ut som ett vanligt dirty-rect-spel for
  ramverket.

## Senare arbete

- Exponera debugrad som visar aktivt render mode.
- Ersatt den portabla `ANA_TileLayer`-scrollbackenden med native Amiga
  hardware-scroll for `ANA_RENDER_SIDE_SCROLL` och senare vertical/4-way.
- Flytta renderstrategi fran endast `ANA_Game` till komponerbara scenes/layers.
- Lat HUD overlays och debug overlays vara egna lager sa de inte blandas ihop
  med scrollande playfields.
- Bygg Amiga hardware-scroll-backend med bitplane-pointer/fine-scroll dar det
  passar.
- Utvardera om `ANA_RENDER_FULL_FRAME` ska ha en enklare, snabbare host path.

# Spec 017: Scroll, camera och tilemap

## Bakgrund

Byte Brothers visar en viktig grans for ANA:s nuvarande renderer. Dirty rects ar
effektivt nar bakgrunden ar statisk, som i Invaders och AMAze. En scrollande
plattformare eller vertikal shooter gor daremot stora delar av viewporten dirty
nar kameran ror sig. Om spelet losser det genom att rensa och rita om hela
spelvyn fran C varje frame blir C2P- och minneskostnaden for hog pa en stock
A1200 utan Fast RAM.

Nuvarande Byte Brothers-scroll anvander `ANA_Camera` for
world/screen-konvertering och `ANA_TileLayer` for det scrollande playfieldet.
Spelet levererar tile read/draw callbacks, medan ramverket ager viewport,
kamera, exponerade strips och redraw. Detta har flyttat tile-synlighet och
strip-redraw ut ur exemplet. Byte Brothers begar nu
`ANA_SCROLL_BACKEND_HARDWARE` for playfieldet. Det ar ramverkets publika
"snabbaste native scroll"-begaran. Den riktiga Amiga-backenden finns annu inte:
den ska bygga pa dedikerade planar playfields, BPLCON1-finscroll,
bitplane-pekarjustering och blitterritade inkommande tile-kolumner/rader. Tills
den ar klar faller `HARDWARE` tillbaka till den konservativa tile-layer-vagen.
Den gamla visible-bitmap-bryggan ar bara tillganglig via explicit
`ANA_SCROLL_BACKEND_NATIVE`, eftersom testerna visade stale-pixel-artefakter vid
scroll. Detta ar ett viktigt API-steg, men ska inte betraktas som ANA:s slutliga
scrollmodell.

Byte Brothers satter `ANA_Game.render_mode = ANA_RENDER_SIDE_SCROLL`. Det ar
inte en fardig hardware-scroll-backend, men det gor side-scrollbehovet explicit
och ger ANA en plats att valja ratt backend senare. Den riktiga Amiga-backenden
ska bygga pa bredare bitplanes, BPLCON1-finscroll, bitplane-pekarjustering och
blitterritade inkommande tile-kolumner/rader.

Detta ska inte tolkas som att hela spelet alltid bara har ett render mode. Ett
scrollspel behover normalt ett `ANA_LAYER_SIDE_SCROLL`-playfield,
ett `ANA_LAYER_SPRITES`-lager for aktorer och ett `ANA_LAYER_HUD`-lager.
`ANA_RENDER_SIDE_SCROLL` beskriver huvudvyn/playfieldet i Byte Brothers; HUD,
menyer och aktorer ska pa sikt kunna ha egna renderstrategier.
`ANA_RENDER_TILE_SCROLL` finns kvar som generiskt/legacy-kontrakt, men nya spel
ska valja mer specifikt renderlage nar det gar: side scroll, vertical scroll
eller 4-way tilemap.

## Mal

- Ge spelaren ett enkelt hog-niva-API for kamera och scrollande tilemaps.
- Stodja bade spelarstyrd kamera, t.ex. plattformsspel, och kontinuerlig
  scroll, t.ex. vertikala shooters.
- Undvika att spelkod behover rakna tile-intervall, screen/world-offsets och
  exposed strips sjalv.
- Halla HUD och spelviewport separerade sa en statisk HUD inte blir dirty pa
  varje scrollsteg.
- Bygga en modell som kan optimeras for Amiga utan att spel-API:t maste andras.
- Behalla C- och Amiga-escape hatches for spel som behover egen hot path.

## Icke-mal for forsta versionen

- Ingen komplett scene graph.
- Ingen generell fysikmotor.
- Ingen godtycklig zoom/rotation.
- Ingen garanti for pixelperfekt fri scroll pa alla maskinprofiler.
- Ingen A500/68000-baseline for fullskarmscroll i 0.2.

## Grundprincip

ANA ska skilja pa flera renderfall:

1. Statisk viewport med dirty objekt (`ANA_RENDER_DIRTY`).
   Detta ar dagens starka fall: rorliga objekt rensar sin gamla rektangel,
   reparerar statiska lager och ritas pa nytt.

2. Snappad riktad tile-/byte-scroll (`ANA_RENDER_SIDE_SCROLL`,
   `ANA_RENDER_VERTICAL_SCROLL` eller `ANA_RENDER_TILE_4WAY`).
   Kameran ror sig i steg som passar Amigans minneslayout, normalt 8 pixlar
   horisontellt och tilehojd eller mindre vertikalt. ANA ritar bara nya strips
   som kommer in i viewporten och ritar om rorliga objekt.

3. Hardware-scroll-backend for riktade scroll-lagen med Amiga-specifik backend.
   Senare Amiga-backend kan anvanda bitplane-offset/fine scroll och overdraw-
   marginaler, sa skarmen flyttas av hardvaran och ANA bara ritar nya kolumner
   eller rader. Detta ar den langsiktigt ratta modellen for jamn scroll.

4. Generisk tile-scroll (`ANA_RENDER_TILE_SCROLL`).
   Detta finns kvar som kompatibilitetslage for kod som bara vill saga
   "scrollande tilemap" utan att specificera axel eller kameramodell.

Dagens C2P/direct-present-modell ska vara fallback, inte huvudstrategi for
kontinuerlig fullskarmscroll.

## API-skiss

`ANA_Camera` och `ANA_TileLayer` finns i ramverket. API:t ar avsiktligt tunt:
det flyttar tilemap/camera-bokforing ur spelkoden utan att lova en tung scene
graph.

```c
typedef struct ANA_Camera {
    int x;
    int y;
    int view_x;
    int view_y;
    int view_w;
    int view_h;
    int world_w;
    int world_h;
    int snap_x;
    int snap_y;
} ANA_Camera;

void ana_camera_init(
    ANA_Camera* camera,
    int view_x,
    int view_y,
    int view_w,
    int view_h,
    int world_w,
    int world_h);

void ana_camera_set_snap(ANA_Camera* camera, int snap_x, int snap_y);
void ana_camera_set_position(ANA_Camera* camera, int x, int y);
void ana_camera_scroll_by(ANA_Camera* camera, int dx, int dy);
void ana_camera_follow_rect(
    ANA_Camera* camera,
    ANA_Rect target,
    int margin_x,
    int margin_y);

ANA_Rect ana_camera_world_view(const ANA_Camera* camera);
ANA_Rect ana_camera_world_to_screen_rect(
    const ANA_Camera* camera,
    ANA_Rect world_rect);

void ana_scroll_rect(
    int x,
    int y,
    int width,
    int height,
    int dx,
    int dy,
    unsigned char clear_color);
```

Tilemap:

```c
typedef unsigned char (*ANA_TileReadCallback)(
    int tx,
    int ty,
    void* user_data);

typedef void (*ANA_TileDrawCallback)(
    unsigned char tile,
    int x,
    int y,
    void* user_data);

typedef struct ANA_TileLayer {
    ANA_Layer layer;
    ANA_TileReadCallback read_tile;
    ANA_TileDrawCallback draw_tile;
    void* user_data;
    int tile_width;
    int tile_height;
    int map_width;
    int map_height;
    int previous_camera_x;
    int previous_camera_y;
    unsigned char clear_color;
} ANA_TileLayer;

void ana_tile_layer_init(...);
void ana_tile_layer_set_callbacks(...);
void ana_tile_layer_set_viewport(...);
void ana_tile_layer_set_camera(...);
void ana_tile_layer_invalidate(...);
void ana_tile_layer_draw(...);
```

Forsta implementationen har ett lag-niva-API som kan flytta en befintlig
viewport-rektangel och rita exponerade strips via tile callbacks. Det viktiga
ar att spelkod redan borjar uttrycka scroll som kamera + tile layer. Sedan kan
implementationen bytas till hardware-scroll utan att Byte Brothers eller
framtida spel behover rakna om sin rendering.

## Prestandamodell

For en 320x240 viewport ar full redraw varje frame dyrt:

- manga chunky writes
- stor C2P-yta
- manga tile-draw-callbacks
- risk for ojamt frame pacing nar scroll och sprites hander samtidigt

En stripbaserad modell vid horisontell scroll ska i stallet gora:

- flytta eller hardvaruscrolla befintlig bild
- rita ny kolumn/kolumner som exponeras
- rita om movers och HUD endast nar de andras

For vertikal shooter-scroll ar samma princip:

- scrolla bakgrundslagret kontinuerligt
- rita ny rad/rader i framkant
- hall objekt/HUD separat fran bakgrundslagret

## Backends

### Portable fallback

Alla plattformar kan anvanda viewport redraw. Den ar enkel och korrekt men inte
prestationsmalet for stora scrollande spel pa Amiga.

### Scroll backend-val

`ANA_SCROLL_BACKEND_NATIVE` aktiverar den experimentella
A1200/direct-present-bron:

- flytta befintlig synlig bitmap med blittern
- flytta chunky-kallan pa samma satt
- rita om exponerade strips i chunky-kallan
- rita om moving-object restore-regioner i chunky-kallan
- C2P-konvertera bara dessa dirty regioner

`ANA_SCROLL_BACKEND_HARDWARE` ar den publika intentionen: anvand en dedikerad
native hardware-scroll-backend nar en sadan finns. I nuvarande
A1200/direct-present-backend ar den riktiga BPLCON1/BPLxPTR-vagen inte klar,
sa `HARDWARE` faller tillbaka till den konservativa tile-layer-vagen i stallet
for den experimentella visible-bitmap-bron. `ANA_SCROLL_BACKEND_NATIVE` begar
uttryckligen den bridgen for backend-experiment, `ANA_SCROLL_BACKEND_AUTO` far
valja snabbaste stabila tillgangliga backend och `ANA_SCROLL_BACKEND_SOFTWARE`
tvingar portabel redraw. Den slutliga vagen ska anvanda BPLCON1-finscroll,
bitplane-pekarjustering och blitterritade inkommande tile-kolumner/rader.

### Snapped dirty strips

Amiga-fallback for 0.2:

- kamera snappar till 8 pixlar horisontellt
- ANA identifierar exponerade strips
- bara strips och rorliga objekt markeras dirty
- HUD ar ett separat statiskt lager

Detta passar tilebaserade plattformsspel och ar betydligt battre an att rita om
hela viewporten.

### Hardware scroll

Langsiktig backend:

- anvand bitplane-start/fine-scroll dar det ar praktiskt
- ha en overdraw-marginal utanfor synlig viewport
- rita nya tile strips i marginalen
- behall samma `ANA_Camera`/`ANA_TileLayer`-API

Detta ar troligen nodvandigt for stabil, mjuk fullskarmscroll pa stock A1200
utan att offra for mycket CPU till C2P.

## Byte Brothers-migrering

Byte Brothers ska stegvis sluta aga egen scrolllogik:

1. Ersatt egen `bb_camera_x`-hantering med `ANA_Camera`. Klart for
   world/screen-konvertering och positionering; gamla kompatibilitetsfalt finns
   kvar internt tills scroll-layer-delen tar over.
2. Flytta viewport-scroll till ramverket och rita bara exponerade strips.
   Klart for den portabla/software backenden genom `ANA_TileLayer`.
3. Ersatt egen tile-intervalberakning med tile-layer callbacks. Klart for
   playfield rendering.
4. Flytta viewport redraw/camera change state till ramverket. Klart for
   software backenden; native hardware-scroll aterstar.
5. Dela upp scrollande playfield, aktorer och HUD i separata render-lager:
   `ANA_LAYER_SIDE_SCROLL`, `ANA_LAYER_SPRITES` och `ANA_LAYER_HUD`. Klart som
   API/form for Byte Brothers.
6. Lat spelet endast beskriva tilemap-data, collision och entities.
7. Nar strip/hardware-backend finns ska Byte Brothers fa den utan att
   spelreglerna andras.

## Acceptanskriterier

- Byte Brothers anvander `ANA_Camera` for world/screen-konvertering. Klart.
- Byte Brothers deklarerar `ANA_RENDER_SIDE_SCROLL` i `ANA_Game`. Klart.
- Byte Brothers anvander `ANA_TileLayer` for tile redraw.
- Spelkod raknar inte langre synliga tile-intervall sjalv.
- Debug stats visar tydligt nar scroll faller tillbaka till full viewport redraw.
- Kontinuerlig vertikal scroll kan demonstreras med ett enkelt test eller sample.
- Ingen regression for Invaders/AMAze, som fortsatt ska kunna anvanda dirty
  retained rendering utan scrollsystemet.

## Oppna fragor

- Hur mycket av hardware scroll kan kombineras med nuvarande chunky/C2P-modell?
- Ska hardware scroll forst stodja endast horisontell scroll, eller bade X/Y?
- Ska tilemaps ritas med callbacks, preconverted tile images, eller bada?
- Ska parallax vara flera `ANA_TileLayer`/`ANA_Layer`, eller ett separat hogre API?
- Vilken scrollgranularitet ar rimlig for stock A1200: 1, 2, 4, 8 eller
  tilebaserade steg?
